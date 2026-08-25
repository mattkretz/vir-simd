/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *
#include "bits/main.h"
#include <vir/simd_vecmath.h>

#include <cstdint>
#include <cstring>

/* Coverage for vir/simd_vecmath.h
 *
 * Every function the header defines is checked against the scalar routine, for
 * every element type and ABI the harness iterates. Which of the two code paths
 * a given simd takes is decided by its width, so running the extended widths
 * (1 to 32) covers both: the vector math library for the widths libmvec has,
 * and the forward to the underlying implementation for everything else.
 *
 * The overloads live in vir::stdx, so they have to be named. An unqualified
 * call would land on the underlying implementation through argument-dependent
 * lookup and test nothing.
 */
#define VECMATH_TESTER(name_)                                                                      \
  make_tester("vir::stdx::" #name_,                                                                \
	      [](auto... xs) { return vir::stdx::name_(xs...); },                                  \
	      [](auto... xs) { return std::name_(xs...); }, __FILE__, __LINE__)

/* Width sweep
 *
 * Which of the two paths a simd takes, and how many chunks the vector math
 * library is called with, is decided entirely by its width. The harness only
 * instantiates the scalar and the native ABI unless the expensive tests are
 * enabled, and its ULP helper does not currently work with fixed_size ABIs on
 * top of libstdc++'s simd, so sweep the widths here instead, with a lane-wise
 * comparison that needs nothing from the harness.
 */
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

template <typename T, int Width, typename FSimd, typename FScalar>
  void
  test_one_width(const char* name, FSimd&& fsimd, FScalar&& fscalar,
		 std::initializer_list<T> inputs)
  {
    using V = vir::stdx::fixed_size_simd<T, Width>;
    /* 4 ULP is what libmvec documents. On the fallback path the answer comes
     * from the underlying implementation, which is not the scalar routine
     * either: libstdc++ carries its own sin and cos, good to 1 ULP rather than
     * correctly rounded.
     */
#ifdef VIR_HAVE_SIMD_VECMATH
    constexpr int allowed = vir::vecmath_detail::use_vecmath<T, typename V::abi_type> ? 4 : 1;
#else
    constexpr int allowed = 1;
#endif
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

#define SWEEP_1(name_, values_)                                                                    \
  (test_one_width<T, Widths>(#name_, [](auto v) { return vir::stdx::name_(v); },                   \
			     [](T v) { return std::name_(v); }, values_), ...)

#define SWEEP_2(name_, second_, values_)                                                           \
  (test_one_width<T, Widths>(#name_,                                                               \
			     [](auto v) { return vir::stdx::name_(v, T(second_)); },               \
			     [](T v) { return std::name_(v, T(second_)); }, values_), ...)

#ifdef VIR_HAVE_SIMD_VECMATH
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
#endif

template <typename T, int... Widths>
  void
  sweep_widths()
  {
#ifdef VIR_HAVE_SIMD_VECMATH
    (check_chunk_width<T, Widths>(), ...);
#endif

    // a spread of values, cycled to fill each width, inside the domain of each group
    const std::initializer_list<T> general
      = {T(0.5), T(-0.5), T(1), T(-1), T(0), T(2), T(-2), T(0.25)};
    const std::initializer_list<T> positive
      = {T(0.5), T(1), T(1.5), T(2), T(3), T(0.25), T(10), T(1.25)};
    const std::initializer_list<T> unit
      = {T(0.5), T(-0.5), T(1), T(-1), T(0), T(0.25), T(-0.25), T(0.75)};
    const std::initializer_list<T> above_one
      = {T(1), T(1.5), T(2), T(3), T(10), T(1.25), T(5), T(100)};

    SWEEP_1(sin, general); SWEEP_1(cos, general); SWEEP_1(tan, general);
    SWEEP_1(atan, general); SWEEP_1(sinh, general); SWEEP_1(cosh, general);
    SWEEP_1(tanh, general); SWEEP_1(asinh, general); SWEEP_1(cbrt, general);
    SWEEP_1(erf, general); SWEEP_1(erfc, general); SWEEP_1(exp, general);
    SWEEP_1(exp2, general); SWEEP_1(expm1, general);
    SWEEP_1(asin, unit); SWEEP_1(acos, unit); SWEEP_1(atanh, unit);
    SWEEP_1(acosh, above_one);
    SWEEP_1(log, positive); SWEEP_1(log2, positive); SWEEP_1(log10, positive);
    SWEEP_1(log1p, positive);
    SWEEP_2(pow, 2.5, positive); SWEEP_2(atan2, 2, general);
  }

#undef SWEEP_1
#undef SWEEP_2

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;
    using Abi = typename V::abi_type;

#ifdef VIR_HAVE_SIMD_VECMATH
    constexpr bool vecmath = vir::vecmath_detail::use_vecmath<T, Abi>;
#else
    constexpr bool vecmath = false;
#endif

    /* libmvec is accurate to 4 ULP. Where these overloads forward instead, the
     * result has to be exactly what calling the underlying implementation
     * would have given, so demand that.
     */
    vir::test::setFuzzyness<float>(vecmath ? 4 : 1);
    vir::test::setFuzzyness<double>(vecmath ? 4 : 1);
    vir::test::setFuzzyness<long double>(1);

    // ... and it does not reproduce the scalar routines' exception flags
    FloatExceptCompare::ignore = vecmath;

    constexpr T inf = vir::infinity_v<T>;
    constexpr T nan = vir::quiet_NaN_v<T>;
    constexpr T denorm_min = vir::denorm_min_v<T>;
    constexpr T norm_min = vir::norm_min_v<T>;
    constexpr T max = vir::finite_max_v<T>;

    // values every function has to survive, whatever its domain
    const std::initializer_list<T> edge_values
      = {+0., -0., 1., -1., 0.5, -0.5, 2., -2., inf, -inf, nan,
	 denorm_min, -denorm_min, norm_min, norm_min / 3, max, -max};

    // unrestricted domain
    test_values<V>(edge_values, {5000},
		   VECMATH_TESTER(sin), VECMATH_TESTER(cos), VECMATH_TESTER(tan),
		   VECMATH_TESTER(atan), VECMATH_TESTER(erf), VECMATH_TESTER(erfc));

    test_values<V>(edge_values, {5000},
		   VECMATH_TESTER(sinh), VECMATH_TESTER(cosh), VECMATH_TESTER(tanh),
		   VECMATH_TESTER(asinh), VECMATH_TESTER(cbrt));

    test_values<V>(edge_values, {5000},
		   VECMATH_TESTER(exp), VECMATH_TESTER(exp2), VECMATH_TESTER(expm1));

    /* Domain-restricted functions get inputs inside their domain, so that the
     * comparison exercises the computation rather than agreeing on NaN. The
     * edge list above already covered the out-of-domain answers.
     */
    test_values<V>({-1., -0.5, +0., -0., 0.5, 1., denorm_min, nan},
		   {5000, T(-1), T(1)},
		   VECMATH_TESTER(asin), VECMATH_TESTER(acos), VECMATH_TESTER(atanh));

    test_values<V>({1., 1.5, 2., max, inf, nan}, {5000, T(1), max},
		   VECMATH_TESTER(acosh));

    test_values<V>({norm_min, denorm_min, 0.5, 1., 2., max, inf, nan}, {5000, denorm_min, max},
		   VECMATH_TESTER(log), VECMATH_TESTER(log2), VECMATH_TESTER(log10));

    test_values<V>({-1., -0.5, +0., -0., 0.5, 1., max, inf, nan}, {5000, T(-1), max},
		   VECMATH_TESTER(log1p));

    // two-argument functions
    test_values_2arg<V>({+0., -0., 0.5, 1., 2., 3., inf, -inf, nan, norm_min, max},
			{5000}, VECMATH_TESTER(atan2));

    test_values_2arg<V>({+0., -0., 0.5, 1., 2., 3., -2., inf, -inf, nan, norm_min},
			{2000, T(0), T(10)}, VECMATH_TESTER(pow));

    FloatExceptCompare::ignore = false;
    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);

    /* The two-argument overloads must keep accepting a scalar on either side.
     * Deducing both parameters instead of only the first compiles fine and
     * silently takes these four spellings away from every caller.
     */
    {
      /* Both sides have to reach the vector math library at run time. Left to
       * itself the compiler folds one of the two spellings with the scalar
       * routine, and the comparison then measures libmvec's 4 ULP rather than
       * whether the two spellings agree.
       */
      const V x = make_value_unknown(V([](auto i) { return T(1) + T(i) * T(0.25); }));
      const V two = make_value_unknown(V(T(2)));
      const T two_scalar = make_value_unknown(T(2));

      COMPARE(vir::stdx::pow(x, two_scalar), vir::stdx::pow(x, two));
      COMPARE(vir::stdx::pow(two_scalar, x), vir::stdx::pow(two, x));
      COMPARE(vir::stdx::atan2(x, two_scalar), vir::stdx::atan2(x, two));
      COMPARE(vir::stdx::atan2(two_scalar, x), vir::stdx::atan2(two, x));

      // an argument that merely converts to the element type has to work too
      COMPARE(vir::stdx::pow(x, make_value_unknown(2)), vir::stdx::pow(x, two));
      COMPARE(vir::stdx::pow(make_value_unknown(2), x), vir::stdx::pow(two, x));
    }

    /* Names the header does not define must keep every overload the underlying
     * implementation gives them. A declaration in vir::stdx hides the lot, so
     * this is what says hypot was left alone.
     */
    {
      const V x = V(T(3));
      const V y = V(T(4));

      COMPARE(vir::stdx::hypot(x, y), V(T(5)));
      VERIFY(all_of(vir::stdx::hypot(x, y, V(T(0))) == V(T(5))));
      COMPARE(vir::stdx::sqrt(V(T(4))), V(T(2)));
      COMPARE(vir::stdx::abs(V(T(-2))), V(T(2)));
      COMPARE(vir::stdx::fabs(V(T(-2))), V(T(2)));
    }

    /* Every width that decides chunking, once per element type. Hung off the
     * scalar ABI so the sweep runs once rather than once per instantiated ABI.
     */
    if constexpr (std::is_same_v<Abi, vir::stdx::simd_abi::scalar>
		    and (sizeof(T) == 4 or sizeof(T) == 8))
      sweep_widths<T, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 15, 16, 17, 20, 31, 32>();

    /* Taking a function's address forces the out-of-line copy that the ISA
     * discriminator in the signature exists to keep apart. It also pins the
     * signature: the discriminator has to be a defaulted parameter, or naming
     * the function with two explicit arguments stops working.
     */
    {
      using Fn = V (*)(const V&);
      const Fn f = &vir::stdx::sin<T, Abi>;
      COMPARE(f(V(T(0))), V(T(0)));
    }
  }
