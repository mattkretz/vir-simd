/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// expensive: * [1-9] * *
#include "bits/main.h"

template <typename V>
  void
  test()
  {
    using M = typename V::mask_type;
    static_assert(std::is_convertible<typename M::reference, bool>::value,
		  "A smart_reference<simd_mask> must be convertible to bool.");
    static_assert(
      std::is_same<bool, decltype(std::declval<const typename M::reference&>()
				  == true)>::value,
      "A smart_reference<simd_mask> must be comparable against bool.");
    static_assert(
      vir::test::sfinae_is_callable<typename M::reference&&, bool>(
	[](auto&& a, auto&& b) -> decltype(std::declval<decltype(a)>()
					   == std::declval<decltype(b)>()) {
	  return {};
	}),
      "A smart_reference<simd_mask> must be comparable against bool.");
    VERIFY(std::experimental::is_simd_mask_v<M>);

    {
      M x;     // uninitialized
      x = M{}; // default broadcasts 0
      COMPARE(x, M(false));
      COMPARE(x, M());
      COMPARE(x, M{});
      x = M(); // default broadcasts 0
      COMPARE(x, M(false));
      COMPARE(x, M());
      COMPARE(x, M{});
      x = x;
      for (std::size_t i = 0; i < M::size(); ++i)
	{
	  COMPARE(x[i], false);
	}
    }

    M x(true);
    M y(false);
    for (std::size_t i = 0; i < M::size(); ++i)
      {
	COMPARE(x[i], true);
	COMPARE(y[i], false);
      }
    y = M(true);
    COMPARE(x, y);
  }
