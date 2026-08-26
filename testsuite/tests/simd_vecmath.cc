/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 * Copyright © 2026 Axel Huebl <axelhuebl@lbl.gov>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *
#include "bits/main.h"
#include <vir/simd_vecmath.h>

#include <cstdint>
#include <cstring>

/* Coverage for vir/simd_vecmath.h
 *
 * Everything here is inside VIR_HAVE_SIMD_VECMATH. Where the header is inert
 * -- another architecture, another libc, a glibc too old, vir's own simd, or
 * the opt-out -- vir::vecmath does not exist and there is nothing of this
 * header's to test. Testing the underlying implementation instead would only
 * measure how well libstdc++ reduces arguments, which it does badly enough to
 * fail (its vector cos returns inf for finite_max).
 */
#ifdef VIR_HAVE_SIMD_VECMATH

/* Which functions actually reach the vector math library depends on the glibc
 * that declared them, so the tolerance has to follow the same split rather
 * than the simd's width alone.
 */
#ifdef VIR_HAVE_SIMD_VECMATH_EXTENDED
constexpr bool extended_routed = true;
#else
constexpr bool extended_routed = false;
#endif

#define VECMATH_TESTER(name_)                                                                      \
  make_tester("vir::vecmath::" #name_,                                                             \
	      [](auto... xs) { return vir::vecmath::name_(xs...); },                               \
	      [](auto... xs) { return std::name_(xs...); }, __FILE__, __LINE__)

template <typename T>
  int
  ulp_distance(T a, T b)
  {
    constexpr int far = 1 << 30;
    if (a == b or (std::isnan(a) and std::isnan(b)))
      return 0;
    if (std::isnan(a) != std::isnan(b))
      return far;

    static_assert(sizeof(T) == 4 or sizeof(T) == 8, "no integer type to step through");
    using U = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
    constexpr U sign = U(1) << (sizeof(U) * 8 - 1);
    U ua, ub;
    std::memcpy(&ua, &a, sizeof(T));
    std::memcpy(&ub, &b, sizeof(T));

    // monotonic unsigned key, so that a plain difference counts steps
    const auto key = [sign](U u) { return (u & sign) ? U(~u) : U(u | sign); };
    const U ka = key(ua);
    const U kb = key(ub);
    const U d = ka > kb ? ka - kb : kb - ka;
    return d > U(far) ? far : int(d);
  }

/* Width sweep
 *
 * Which path a simd takes, and how many chunks the vector math library is
 * called with, is decided entirely by its width. The harness instantiates only
 * the scalar and the native ABI unless the expensive tests are enabled, and
 * its ULP helper does not work with fixed_size ABIs on top of libstdc++'s
 * simd, so the widths are swept here, lane by lane, with nothing from the
 * harness involved.
 */
template <typename T, int Width, typename FSimd, typename FScalar>
  void
  test_one_width(const char* name, bool routed, FSimd&& fsimd, FScalar&& fscalar,
		 std::initializer_list<T> inputs)
  {
    using V = vir::stdx::fixed_size_simd<T, Width>;
    /* 4 ULP is what libmvec documents. Where the call is not routed the answer
     * comes from the underlying implementation, which is not the scalar
     * routine either: libstdc++ carries its own sin and cos, good to 1 ULP.
     */
    const int allowed
      = (routed and vir::vecmath_detail::use_vecmath<T, typename V::abi_type>) ? 4 : 1;

    alignas(vir::stdx::memory_alignment_v<V>) T lane[Width];
    auto it = inputs.begin();
    for (int i = 0; i < Width; ++i, ++it)
      {
	if (it == inputs.end())
	  it = inputs.begin();
	lane[i] = *it;
      }

    V x;
    x.copy_from(lane, vir::stdx::vector_aligned);
    const V got = fsimd(x);

    for (int i = 0; i < Width; ++i)
      {
	const T expect = fscalar(lane[i]);
	const int d = ulp_distance(T(got[i]), expect);
	VERIFY(d <= allowed)
	  << name << " at width " << Width << ", lane " << i << ": got " << T(got[i])
	  << ", expected " << expect << " (" << d << " ULP, allowed " << allowed << ')';
      }
  }

/* How wide a chunk is decides how many calls into the vector math library a
 * simd costs. Narrowing it keeps every result correct, so no value comparison
 * can notice; assert the selection directly instead.
 */
template <typename T, int Width>
  void
  check_chunk_width()
  {
    using vir::vecmath_detail::chunk_width;
    using vir::vecmath_detail::is_supported_width;
    using vir::vecmath_detail::native_lanes;

    if constexpr (is_supported_width<T, Width>)
      {
	constexpr int chunk = chunk_width<T, Width>::value;
	constexpr int native = native_lanes<T>::value;
	constexpr int smallest = 16 / int(sizeof(T));

	static_assert(Width % chunk == 0,
		      "a chunk that does not divide the width would run off the end");
	static_assert(chunk <= native,
		      "a chunk wider than the target's registers is not callable");
	static_assert(chunk >= smallest,
		      "the narrowest entry point takes a full 128-bit register");
	static_assert(Width % native != 0 or chunk == native,
		      "a width the native register divides has to use the native chunk");
      }
  }

#define SWEEP_1(name_, routed_, values_)                                                           \
  (test_one_width<T, Widths>(#name_, routed_, [](auto v) { return vir::vecmath::name_(v); },       \
			     [](T v) { return std::name_(v); }, values_), ...)

#define SWEEP_2(name_, routed_, second_, values_)                                                  \
  (test_one_width<T, Widths>(#name_, routed_,                                                      \
			     [](auto v) { return vir::vecmath::name_(v, T(second_)); },            \
			     [](T v) { return std::name_(v, T(second_)); }, values_), ...)

template <typename T, int... Widths>
  void
  sweep_widths()
  {
    (check_chunk_width<T, Widths>(), ...);

    // a spread of values, cycled to fill each width, inside the domain of each group
    const std::initializer_list<T> general
      = {T(0.5), T(-0.5), T(1), T(-1), T(0), T(2), T(-2), T(0.25)};
    const std::initializer_list<T> positive
      = {T(0.5), T(1), T(1.5), T(2), T(3), T(0.25), T(10), T(1.25)};
    const std::initializer_list<T> unit
      = {T(0.5), T(-0.5), T(1), T(-1), T(0), T(0.25), T(-0.25), T(0.75)};
    const std::initializer_list<T> above_one
      = {T(1), T(1.5), T(2), T(3), T(10), T(1.25), T(5), T(100)};

    constexpr bool base = true;             // sin cos exp log pow, since glibc 2.22
    constexpr bool ext = extended_routed;   // the rest, since glibc 2.35

    SWEEP_1(sin, base, general);      SWEEP_1(cos, base, general);
    SWEEP_1(exp, base, general);      SWEEP_1(log, base, positive);
    SWEEP_2(pow, base, 2.5, positive);

    SWEEP_1(tan, ext, general);       SWEEP_1(atan, ext, general);
    SWEEP_1(sinh, ext, general);      SWEEP_1(cosh, ext, general);
    SWEEP_1(tanh, ext, general);      SWEEP_1(asinh, ext, general);
    SWEEP_1(cbrt, ext, general);      SWEEP_1(erf, ext, general);
    SWEEP_1(erfc, ext, general);      SWEEP_1(exp2, ext, general);
    SWEEP_1(expm1, ext, general);     SWEEP_1(asin, ext, unit);
    SWEEP_1(acos, ext, unit);         SWEEP_1(atanh, ext, unit);
    SWEEP_1(acosh, ext, above_one);   SWEEP_1(log2, ext, positive);
    SWEEP_1(log10, ext, positive);    SWEEP_1(log1p, ext, positive);
    SWEEP_2(atan2, ext, 2, general);
  }

#undef SWEEP_1
#undef SWEEP_2
#endif // VIR_HAVE_SIMD_VECMATH

template <typename V>
  void
  test()
  {
#ifdef VIR_HAVE_SIMD_VECMATH
    using T = typename V::value_type;
    using Abi = typename V::abi_type;

    /* test_values reaches vir::detail::bit_cast through the harness's ULP
     * helper, which does not accept fixed_size ABIs on top of libstdc++'s
     * simd. Those widths are covered by the sweep instead.
     */
    constexpr bool harness_usable
      = !std::is_same_v<Abi, vir::stdx::simd_abi::fixed_size<V::size()>>;

    constexpr bool vecmath = vir::vecmath_detail::use_vecmath<T, Abi>;

    if constexpr (harness_usable)
      {
	constexpr T inf = vir::infinity_v<T>;
	constexpr T nan = vir::quiet_NaN_v<T>;
	constexpr T denorm_min = vir::denorm_min_v<T>;
	constexpr T norm_min = vir::norm_min_v<T>;
	constexpr T max = vir::finite_max_v<T>;

	const std::initializer_list<T> edge_values
	  = {+0., -0., 1., -1., 0.5, -0.5, 2., -2., inf, -inf, nan,
	     denorm_min, -denorm_min, norm_min, norm_min / 3, max, -max};

	// libmvec does not reproduce the scalar routines' exception flags
	FloatExceptCompare::ignore = vecmath;

	// the functions glibc has had since 2.22
	vir::test::setFuzzyness<float>(vecmath ? 4 : 1);
	vir::test::setFuzzyness<double>(vecmath ? 4 : 1);
	vir::test::setFuzzyness<long double>(1);

	test_values<V>(edge_values, {5000},
		       VECMATH_TESTER(sin), VECMATH_TESTER(cos), VECMATH_TESTER(exp));
	test_values<V>({norm_min, denorm_min, 0.5, 1., 2., max, inf, nan}, {5000, denorm_min, max},
		       VECMATH_TESTER(log));

	// and the ones it grew in 2.35
	const bool ext = vecmath and extended_routed;
	FloatExceptCompare::ignore = ext;
	vir::test::setFuzzyness<float>(ext ? 4 : 1);
	vir::test::setFuzzyness<double>(ext ? 4 : 1);

	test_values<V>(edge_values, {5000},
		       VECMATH_TESTER(tan), VECMATH_TESTER(atan),
		       VECMATH_TESTER(erf), VECMATH_TESTER(erfc));
	test_values<V>(edge_values, {5000},
		       VECMATH_TESTER(sinh), VECMATH_TESTER(cosh), VECMATH_TESTER(tanh),
		       VECMATH_TESTER(asinh), VECMATH_TESTER(cbrt));
	test_values<V>(edge_values, {5000},
		       VECMATH_TESTER(exp2), VECMATH_TESTER(expm1));
	test_values<V>({-1., -0.5, +0., -0., 0.5, 1., denorm_min, nan}, {5000, T(-1), T(1)},
		       VECMATH_TESTER(asin), VECMATH_TESTER(acos), VECMATH_TESTER(atanh));
	test_values<V>({1., 1.5, 2., max, inf, nan}, {5000, T(1), max},
		       VECMATH_TESTER(acosh));
	test_values<V>({norm_min, denorm_min, 0.5, 1., 2., max, inf, nan}, {5000, denorm_min, max},
		       VECMATH_TESTER(log2), VECMATH_TESTER(log10));
	test_values<V>({-1., -0.5, +0., -0., 0.5, 1., max, inf, nan}, {5000, T(-1), max},
		       VECMATH_TESTER(log1p));
	test_values_2arg<V>({+0., -0., 0.5, 1., 2., 3., inf, -inf, nan, norm_min, max},
			    {5000}, VECMATH_TESTER(atan2));

	FloatExceptCompare::ignore = vecmath;
	vir::test::setFuzzyness<float>(vecmath ? 4 : 1);
	vir::test::setFuzzyness<double>(vecmath ? 4 : 1);
	test_values_2arg<V>({+0., -0., 0.5, 1., 2., 3., -2., inf, -inf, nan, norm_min},
			    {2000, T(0), T(10)}, VECMATH_TESTER(pow));

	FloatExceptCompare::ignore = false;
	vir::test::setFuzzyness<float>(0);
	vir::test::setFuzzyness<double>(0);
      }

    // every width that decides chunking, once per element type
    if constexpr (std::is_same_v<Abi, vir::stdx::simd_abi::scalar>
		    and (sizeof(T) == 4 or sizeof(T) == 8))
      sweep_widths<T, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 15, 16, 17, 20, 31, 32>();

    /* The two-argument overloads must accept a scalar on either side. Deducing
     * both parameters instead of only the first compiles fine and silently
     * takes these spellings away from every caller. The exponent is not a
     * whole number on purpose: with 2 the vector and scalar results agree bit
     * for bit, which makes the comparison vacuous.
     */
    {
      const V x = make_value_unknown(V([](auto i) { return T(1) + T(i) * T(0.25); }));
      const V e = make_value_unknown(V(T(2.5)));
      const T e_scalar = make_value_unknown(T(2.5));

      COMPARE(vir::vecmath::pow(x, e_scalar), vir::vecmath::pow(x, e));
      COMPARE(vir::vecmath::pow(e_scalar, x), vir::vecmath::pow(e, x));
      COMPARE(vir::vecmath::atan2(x, e_scalar), vir::vecmath::atan2(x, e));
      COMPARE(vir::vecmath::atan2(e_scalar, x), vir::vecmath::atan2(e, x));

      // an argument that merely converts to the element type has to work too
      COMPARE(vir::vecmath::pow(x, make_value_unknown(2)), vir::vecmath::pow(x, V(T(2))));
      COMPARE(vir::vecmath::pow(make_value_unknown(2), x), vir::vecmath::pow(V(T(2)), x));
    }

    /* These live in their own namespace now, so vir::stdx keeps every overload
     * it had. That is the whole point of not declaring them there.
     */
    {
      COMPARE(vir::stdx::hypot(V(T(3)), V(T(4))), V(T(5)));
      VERIFY(all_of(vir::stdx::hypot(V(T(3)), V(T(4)), V(T(0))) == V(T(5))));
      COMPARE(vir::stdx::sqrt(V(T(4))), V(T(2)));
      COMPARE(vir::stdx::abs(V(T(-2))), V(T(2)));
      COMPARE(vir::stdx::fabs(V(T(-2))), V(T(2)));
      COMPARE(vir::stdx::pow(V(T(2)), T(3)), V(T(8)));      // the scalar-broadcast form
      COMPARE(vir::stdx::pow(T(2), V(T(3))), V(T(8)));      // and the reversed one
    }

    // taking an address forces the out-of-line copy the ISA tag keeps apart
    {
      using Fn = V (*)(const V&);
      const Fn f = &vir::vecmath::sin<T, Abi>;
      COMPARE(f(V(T(0))), V(T(0)));
    }
#endif // VIR_HAVE_SIMD_VECMATH
  }
