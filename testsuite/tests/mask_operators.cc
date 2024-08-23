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
    { // compares
      M x(true), y(false);
      VERIFY(all_of(x == x));
      VERIFY(all_of(x != y));
      VERIFY(all_of(y != x));
      VERIFY(!all_of(x != x));
      VERIFY(!all_of(x == y));
      VERIFY(!all_of(y == x));
    }
    { // subscripting
      M x(true);
      for (std::size_t i = 0; i < M::size(); ++i)
	{
	  COMPARE(x[i], true) << "\nx: " << x << ", i: " << i;
	  x[i] = !x[i];
	}
      COMPARE(x, M{false});
      for (std::size_t i = 0; i < M::size(); ++i)
	{
	  COMPARE(x[i], false) << "\nx: " << x << ", i: " << i;
	  x[i] = !x[i];
	}
      COMPARE(x, M{true});
    }
    { // negation
      M x(false);
      M y = !x;
      COMPARE(y, M{true});
      COMPARE(!y, x);
    }
  }

