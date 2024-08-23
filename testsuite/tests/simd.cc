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

    // V must store V::size() values of type T giving us the lower bound on the
    // sizeof
    VERIFY(sizeof(V) >= sizeof(T) * V::size());

    // For fixed_size, V should not pad more than to the next-power-of-2 of
    // sizeof(T) * V::size() (for ABI stability of V), giving us the upper bound
    // on the sizeof. For non-fixed_size we give the implementation a bit more
    // slack to trade space vs. efficiency.
    auto n = sizeof(T) * V::size();
    if (n & (n - 1))
      {
	n = ((n << 1) & ~n) & ~((n >> 1) | (n >> 3));
	while (n & (n - 1))
	  n &= n - 1;
      }
    if constexpr (
      !std::is_same_v<typename V::abi_type,
		      std::experimental::simd_abi::fixed_size<V::size()>>)
      n *= 2;
    VERIFY(sizeof(V) <= n) << "\nsizeof(V): " << sizeof(V) << "\nn: " << n;
  }
