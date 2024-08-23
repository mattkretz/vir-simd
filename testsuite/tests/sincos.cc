/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// only: float|double|ldouble * * *
// xfail: run * * * * 'test ! -r reference-sincos-dp.dat'
// expensive: * [1-9] * *
#include "bits/main.h"

template <typename V>
  void
  test()
  {
    using std::cos;
    using std::sin;
    using T = typename V::value_type;

    vir::test::setFuzzyness<float>(2);
    vir::test::setFuzzyness<double>(1);

    const auto& testdata = referenceData<function::sincos, T>();
    vir::simd_view<V>(testdata).for_each(
      [&](const V input, const V expected_sin, const V expected_cos) {
	FUZZY_COMPARE(sin(input), expected_sin) << " input = " << input;
	FUZZY_COMPARE(sin(-input), -expected_sin) << " input = " << input;
	FUZZY_COMPARE(cos(input), expected_cos) << " input = " << input;
	FUZZY_COMPARE(cos(-input), expected_cos) << " input = " << input;
      });
  }
