/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2022–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */
// expensive: * [1-9] * *
#include "bits/main.h"

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;
    using A = typename V::abi_type;
    using M = typename V::mask_type;

    M k0{true};
    std::bitset b0 = vir::to_bitset(k0);
    VERIFY(b0.all());
    vir::stdx::simd_mask k1 = vir::to_simd_mask<T, A>(b0);
    COMPARE(k0, k1);
  }
