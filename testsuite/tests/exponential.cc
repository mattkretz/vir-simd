/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 * Copyright © 2026 The Regents of the University of California,
 *                  through Lawrence Berkeley National Laboratory
 *                  (subject to receipt of any required approvals from
 *                  the U.S. Dept. of Energy). All rights reserved.
 *                  Axel Huebl <axelhuebl@lbl.gov>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *
#include "bits/main.h"

template <typename V>
  void
  test()
  {
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);

    using T = typename V::value_type;
    constexpr T nan = vir::quiet_NaN_v<T>;
    constexpr T inf = vir::infinity_v<T>;
    constexpr T denorm_min = vir::denorm_min_v<T>;
    constexpr T norm_min = vir::norm_min_v<T>;
    constexpr T min = vir::finite_min_v<T>;
    constexpr T max = vir::finite_max_v<T>;

    // The exponentials, on a range that reaches the overflow and underflow
    // edges from both sides. expm1 is the reason for the small values: it
    // exists precisely to be accurate where exp(x) - 1 cancels.
    test_values<V>({0,
		    1,
		    -1,
		    2,
		    -2,
		    10,
		    -10,
		    100,
		    -100,
#ifdef __STDC_IEC_559__
		    nan,
		    inf,
		    -inf,
		    denorm_min,
		    -denorm_min,
		    norm_min,
		    -norm_min,
		    norm_min / 3,
		    -norm_min / 3,
		    -T(),
		    T(),
		    min,
		    max,
#endif
		    T(0.5),
		    T(-0.5)},
		   {10000, T(-100), T(100)},
		   MAKE_TESTER(exp), MAKE_TESTER(exp2), MAKE_TESTER(expm1));

    // cbrt takes the whole range, unlike the exponentials, and is defined for
    // negative arguments where sqrt is not.
    test_values<V>({0,
		    1,
		    -1,
		    8,
		    -8,
		    27,
		    -27,
#ifdef __STDC_IEC_559__
		    nan,
		    inf,
		    -inf,
		    denorm_min,
		    -denorm_min,
		    norm_min,
		    -norm_min,
		    -T(),
		    T(),
		    min,
		    max,
#endif
		    T(0.5),
		    T(-0.5)},
		   {10000,
#ifdef __STDC_IEC_559__
		    min / 2,
#else
		    norm_min,
#endif
		    max / 2},
		   MAKE_TESTER(cbrt));
  }
