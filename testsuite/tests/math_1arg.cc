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
    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);

    using T = typename V::value_type;
    constexpr T inf = vir::infinity_v<T>;
    constexpr T nan = vir::quiet_NaN_v<T>;
    constexpr T denorm_min = vir::denorm_min_v<T>;
    constexpr T norm_min = vir::norm_min_v<T>;
    constexpr T max = vir::finite_max_v<T>;
#if defined __LONG_DOUBLE_IBM128__
    // On POWER with IBM128 long double, 1+eps and 2-eps is not a constant
    // expression. Until this is fixed, just use const instead of constexpr.
    // (error: '(1.0e+0l + 4.94065645841246544176568792868221e-324l)' is not a
    // constant expression)
    const T after_one = 1 + vir::epsilon_v<T>;
    const T before_one = (2 - vir::epsilon_v<T>) / 2;
#else
    constexpr T after_one = 1 + vir::epsilon_v<T>;
    constexpr T before_one = (2 - vir::epsilon_v<T>) / 2;
#endif
    const std::initializer_list<T>
      input_values = {+0.,
		      0.5,
		      -0.5,
		      before_one,
		      -before_one,
		      after_one,
		      -after_one,
		      1.5,
		      -1.5,
		      2 * before_one,
		      -2 * before_one,
		      2 * after_one,
		      -2 * after_one,
		      2.5,
		      -2.5,
		      0x1.fffffffffffffp52,
		      -0x1.fffffffffffffp52,
		      0x1.ffffffffffffep52,
		      -0x1.ffffffffffffep52,
		      0x1.ffffffffffffdp52,
		      -0x1.ffffffffffffdp52,
		      0x1.fffffep21,
		      -0x1.fffffep21,
		      0x1.fffffcp21,
		      -0x1.fffffcp21,
		      0x1.fffffep22,
		      -0x1.fffffep22,
		      0x1.fffffcp22,
		      -0x1.fffffcp22,
		      0x1.fffffep23,
		      -0x1.fffffep23,
		      0x1.fffffcp23,
		      -0x1.fffffcp23,
		      0x1.8p23,
		      -0x1.8p23,
		      inf,
		      -inf,
		      -0.,
		      nan,
		      denorm_min,
		      norm_min / 3,
		      norm_min,
		      max};

    test_values<V>(input_values, {10000}, MAKE_TESTER(erf), MAKE_TESTER(erfc), MAKE_TESTER(tgamma),
		   MAKE_TESTER(lgamma), MAKE_TESTER(nearbyint), MAKE_TESTER(rint),
		   MAKE_TESTER(lrint), MAKE_TESTER(llrint), MAKE_TESTER(ilogb));


#ifdef FE_INEXACT
    FloatExceptCompare::ignore_spurious = FE_INEXACT;
    FloatExceptCompare::ignore_missing = FE_INEXACT;
#endif

    test_values<V>(input_values, {10000}, MAKE_TESTER(ceil), MAKE_TESTER(floor), MAKE_TESTER(trunc),
		   MAKE_TESTER(round), MAKE_TESTER(lround), MAKE_TESTER(llround));

    FloatExceptCompare::ignore_spurious = 0;
    FloatExceptCompare::ignore_missing = 0;

    // sqrt(x) on x87 is precise in 80 bits, but the subsequent rounding can be
    // wrong (up to 1 ULP)
#if __FLT_EVAL_METHOD__ == 1
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(0);
#elif __FLT_EVAL_METHOD__ == 2
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);
#endif
    test_values<V>(input_values, {10000}, MAKE_TESTER(sqrt));
  }
