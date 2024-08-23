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
    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);

    using T = typename V::value_type;
    test_values<V>(
      {
#ifdef __STDC_IEC_559__
	vir::quiet_NaN_v<T>, vir::infinity_v<T>, -vir::infinity_v<T>, -0.,
	vir::denorm_min_v<T>, vir::norm_min_v<T> / 3,
#endif
	+0., vir::norm_min_v<T>, vir::finite_max_v<T>},
      {10000}, MAKE_TESTER(acos), MAKE_TESTER(tan), MAKE_TESTER(acosh),
      MAKE_TESTER(asinh), MAKE_TESTER(atanh), MAKE_TESTER(cosh),
      MAKE_TESTER(sinh), MAKE_TESTER(tanh));
  }
