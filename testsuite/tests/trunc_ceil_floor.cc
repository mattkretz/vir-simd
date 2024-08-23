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
    using T = typename V::value_type;
    constexpr T inf = vir::infinity_v<T>;
    constexpr T denorm_min = vir::denorm_min_v<T>;
    constexpr T norm_min = vir::norm_min_v<T>;
    constexpr T max = vir::finite_max_v<T>;
    constexpr T min = vir::finite_min_v<T>;
    test_values<V>(
      {2.1,
       2.0,
       2.9,
       2.5,
       2.499,
       1.5,
       1.499,
       1.99,
       0.99,
       0.5,
       0.499,
       0.,
       -2.1,
       -2.0,
       -2.9,
       -2.5,
       -2.499,
       -1.5,
       -1.499,
       -1.99,
       -0.99,
       -0.5,
       -0.499,
       3 << 21,
       3 << 22,
       3 << 23,
       -(3 << 21),
       -(3 << 22),
       -(3 << 23),
#ifdef __STDC_IEC_559__
       -0.,
       inf,
       -inf,
       denorm_min,
       norm_min * 0.9,
       -denorm_min,
       -norm_min * 0.9,
#endif
       max,
       norm_min,
       min,
       -norm_min
      },
      [](const V input) {
	const V expected([&](auto i) { return std::trunc(input[i]); });
	COMPARE(trunc(input), expected) << input;
      },
      [](const V input) {
	const V expected([&](auto i) { return std::ceil(input[i]); });
	COMPARE(ceil(input), expected) << input;
      },
      [](const V input) {
	const V expected([&](auto i) { return std::floor(input[i]); });
	COMPARE(floor(input), expected) << input;
      });

#ifdef __STDC_IEC_559__
    test_values<V>(
      {
#ifdef __SUPPORT_SNAN__
	vir::signaling_NaN_v<T>,
#endif
	vir::quiet_NaN_v<T>},
      [](const V input) {
	const V expected([&](auto i) { return std::trunc(input[i]); });
	COMPARE(isnan(trunc(input)), isnan(expected)) << input;
      },
      [](const V input) {
	const V expected([&](auto i) { return std::ceil(input[i]); });
	COMPARE(isnan(ceil(input)), isnan(expected)) << input;
      },
      [](const V input) {
	const V expected([&](auto i) { return std::floor(input[i]); });
	COMPARE(isnan(floor(input)), isnan(expected)) << input;
      });
#endif
  }
