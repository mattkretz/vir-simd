/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *
#include "bits/main.h"

template <typename V>
  void
  test()
  {
    using int_v = std::experimental::fixed_size_simd<int, V::size()>;
    using T = typename V::value_type;
#if COMPLETE_IEC559_SUPPORT
    constexpr auto denorm_min = vir::denorm_min_v<T>;
    constexpr auto norm_min = vir::norm_min_v<T>;
    constexpr auto nan = vir::quiet_NaN_v<T>;
    constexpr auto inf = vir::infinity_v<T>;
#endif
    constexpr auto max = vir::finite_max_v<T>;
    test_values<V>(
      {0, 0.25, 0.5, 1, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
       20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 32, 31, -0., -0.25, -0.5, -1,
       -3, -4, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16, -17, -18,
       -19, -20, -21, -22, -23, -24, -25, -26, -27, -28, -29, -32, -31,
#if COMPLETE_IEC559_SUPPORT
       denorm_min, -denorm_min, norm_min / 2, -norm_min / 2,
#endif
       max, -max, max * 0.123f, -max * 0.123f},
      [](const V input) {
	V expectedFraction;
	const int_v expectedExponent([&](auto i) {
	  int exp;
	  expectedFraction[i] = std::frexp(input[i], &exp);
	  return exp;
	});
	int_v exponent = {};
	const V fraction = frexp(input, &exponent);
	COMPARE(fraction, expectedFraction) << ", input = " << input
	  << ", delta: " << fraction - expectedFraction;
	COMPARE(exponent, expectedExponent)
	  << "\ninput: " << input << ", fraction: " << fraction;
      });
#if COMPLETE_IEC559_SUPPORT
    test_values<V>(
      // If x is a NaN, a NaN is returned, and the value of *exp is unspecified.
      //
      // If x is positive  infinity  (negative  infinity),  positive  infinity
      // (negative infinity) is returned, and the value of *exp is unspecified.
      // This behavior is only guaranteed with C's Annex F when __STDC_IEC_559__
      // is defined.
      {nan, inf, -inf, denorm_min, denorm_min * 1.72, -denorm_min,
       -denorm_min * 1.72, 0., -0., 1, -1},
      [](const V input) {
	const V expectedFraction([&](auto i) {
	  int exp;
	  return std::frexp(input[i], &exp);
	});
	int_v exponent = {};
	const V fraction = frexp(input, &exponent);
	COMPARE(isnan(fraction), isnan(expectedFraction))
	  << fraction << ", input = " << input
	  << ", delta: " << fraction - expectedFraction;
	COMPARE(isinf(fraction), isinf(expectedFraction))
	  << fraction << ", input = " << input
	  << ", delta: " << fraction - expectedFraction;
	COMPARE(signbit(fraction), signbit(expectedFraction))
	  << fraction << ", input = " << input
	  << ", delta: " << fraction - expectedFraction;
      });
#endif
  }
