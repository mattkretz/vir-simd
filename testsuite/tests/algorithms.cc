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
    using T = typename V::value_type;
    V a{[](auto i) -> T { return i & 1u; }};
    V b{[](auto i) -> T { return (i + 1u) & 1u; }};
    COMPARE(min(a, b), V{0});
    COMPARE(max(a, b), V{1});
  }
