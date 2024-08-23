/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// expensive: * [1-9] * *
#include "bits/main.h"
#include <cmath>    // abs & sqrt
#include <cstdlib>  // integer abs

template <typename V>
  void
  test()
  {
    if constexpr (std::is_signed_v<typename V::value_type>)
      {
	using std::abs;
	using T = typename V::value_type;
	test_values<V>({vir::finite_max_v<T>, vir::norm_min_v<T>,
			-vir::norm_min_v<T>, vir::finite_min_v<T>,
			vir::finite_min_v<T> / 2, T(), -T(), T(-1), T(-2)},
		       {1000}, [](V input) {
			 const V expected(
			   [&](auto i) { return T(std::abs(T(input[i]))); });
			 COMPARE(abs(input), expected) << "input: " << input;
		       });
      }
  }
