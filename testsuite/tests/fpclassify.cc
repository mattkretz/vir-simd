/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *
#include "bits/main.h"
#include <cfenv>

template <typename F>
  auto
  verify_no_fp_exceptions(F&& fun)
  {
    std::feclearexcept(FE_ALL_EXCEPT);
    auto r = fun();
    COMPARE(std::fetestexcept(FE_ALL_EXCEPT), 0);
    return r;
  }

#if VIR_HAVE_STD_SIMD
#define NOFPEXCEPT(...) verify_no_fp_exceptions([&]() { return __VA_ARGS__; })
#elif VIR_HAVE_VIR_SIMD
// With the array-based simd implementation, GCC might auto-vectorize the classification functions,
// hitting PR94413 (auto-vectorization of isfinite raises FP exception). Therefore, simply ignore FP
// exceptions then.
#define NOFPEXCEPT(...) [&]() { return __VA_ARGS__; }()
#endif

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;
    using intv = std::experimental::fixed_size_simd<int, V::size()>;
#if COMPLETE_IEC559_SUPPORT
    constexpr T inf = vir::infinity_v<T>;
    constexpr T denorm_min = vir::infinity_v<T>;
    constexpr T nan = vir::quiet_NaN_v<T>;
#endif
    constexpr T max = vir::finite_max_v<T>;
    constexpr T norm_min = vir::norm_min_v<T>;
    test_values<V>(
      {0., 1., -1.,
#if COMPLETE_IEC559_SUPPORT
       -0., inf, -inf, denorm_min, -denorm_min, nan,
       norm_min * 0.9, -norm_min * 0.9,
#endif
       max, -max, norm_min, -norm_min
      },
      [](const V input) {
	COMPARE(NOFPEXCEPT(isfinite(input)),
		!V([&](auto i) { return std::isfinite(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isinf(input)),
		!V([&](auto i) { return std::isinf(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isnan(input)),
		!V([&](auto i) { return std::isnan(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isnormal(input)),
		!V([&](auto i) { return std::isnormal(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(signbit(input)),
		!V([&](auto i) { return std::signbit(input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isunordered(input, V())),
		!V([&](auto i) { return std::isunordered(input[i], 0) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(isunordered(V(), input)),
		!V([&](auto i) { return std::isunordered(0, input[i]) ? 0 : 1; }))
	  << input;
	COMPARE(NOFPEXCEPT(fpclassify(input)),
		intv([&](auto i) { return std::fpclassify(input[i]); }))
	  << input;
      });
#ifdef __SUPPORT_SNAN__
    const V snan = vir::signaling_NaN_v<T>;
    COMPARE(isfinite(snan),
	    !V([&](auto i) { return std::isfinite(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(isinf(snan), !V([&](auto i) { return std::isinf(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(isnan(snan), !V([&](auto i) { return std::isnan(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(isnormal(snan),
	    !V([&](auto i) { return std::isnormal(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(signbit(snan),
	    !V([&](auto i) { return std::signbit(snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(isunordered(snan, V()),
	    !V([&](auto i) { return std::isunordered(snan[i], 0) ? 0 : 1; }))
      << snan;
    COMPARE(isunordered(V(), snan),
	    !V([&](auto i) { return std::isunordered(0, snan[i]) ? 0 : 1; }))
      << snan;
    COMPARE(fpclassify(snan),
	    intv([&](auto i) { return std::fpclassify(snan[i]); }))
      << snan;
#endif
  }
