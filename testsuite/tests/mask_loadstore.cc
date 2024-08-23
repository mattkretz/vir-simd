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
    // loads
    constexpr size_t alignment = 2 * std::experimental::memory_alignment_v<M>;
    alignas(alignment) bool mem[3 * M::size()];
    std::memset(mem, 0, sizeof(mem));
    for (std::size_t i = 1; i < sizeof(mem) / sizeof(*mem); i += 2)
      {
	COMPARE(mem[i - 1], false);
	mem[i] = true;
      }
    using std::experimental::element_aligned;
    using std::experimental::vector_aligned;
    constexpr size_t stride_alignment
      = M::size() & 1
	  ? 1
	  : M::size() & 2
	  ? 2
	  : M::size() & 4
	  ? 4
	  : M::size() & 8
	  ? 8
	  : M::size() & 16
	  ? 16
	  : M::size() & 32
	  ? 32
	  : M::size() & 64
	  ? 64
	  : M::size() & 128 ? 128
			    : M::size() & 256 ? 256 : 512;
    using stride_aligned_t = std::conditional_t<
      M::size() == stride_alignment, decltype(vector_aligned),
      std::experimental::overaligned_tag<stride_alignment * sizeof(bool)>>;
    constexpr stride_aligned_t stride_aligned = {};
    constexpr auto overaligned = std::experimental::overaligned<alignment>;

    const M alternating_mask = make_alternating_mask<M>();

    M x(&mem[M::size()], stride_aligned);
    COMPARE(x, M::size() % 2 == 1 ? !alternating_mask : alternating_mask)
      << vir::to_bitset(x)
      << ", alternating_mask: " << vir::to_bitset(alternating_mask);
    x = {&mem[1], element_aligned};
    COMPARE(x, !alternating_mask);
    x = M{mem, overaligned};
    COMPARE(x, alternating_mask);

    x.copy_from(&mem[M::size()], stride_aligned);
    COMPARE(x, M::size() % 2 == 1 ? !alternating_mask : alternating_mask);
    x.copy_from(&mem[1], element_aligned);
    COMPARE(x, !alternating_mask);
    x.copy_from(mem, vector_aligned);
    COMPARE(x, alternating_mask);

    x = !alternating_mask;
    where(alternating_mask, x).copy_from(&mem[M::size()], stride_aligned);
    COMPARE(x, M::size() % 2 == 1 ? !alternating_mask : M{true});
    x = M(true);                                                    // 1111
    where(alternating_mask, x).copy_from(&mem[1], element_aligned); // load .0.0
    COMPARE(x, !alternating_mask);                                  // 1010
    where(alternating_mask, x).copy_from(mem, overaligned);         // load .1.1
    COMPARE(x, M{true});                                            // 1111

    // stores
    memset(mem, 0, sizeof(mem));
    x = M(true);
    x.copy_to(&mem[M::size()], stride_aligned);
    std::size_t i = 0;
    for (; i < M::size(); ++i)
      {
	COMPARE(mem[i], false);
      }
    for (; i < 2 * M::size(); ++i)
      {
	COMPARE(mem[i], true) << "i: " << i << ", x: " << x;
      }
    for (; i < 3 * M::size(); ++i)
      {
	COMPARE(mem[i], false);
      }
    memset(mem, 0, sizeof(mem));
    x.copy_to(&mem[1], element_aligned);
    COMPARE(mem[0], false);
    for (i = 1; i <= M::size(); ++i)
      {
	COMPARE(mem[i], true);
      }
    for (; i < 3 * M::size(); ++i)
      {
	COMPARE(mem[i], false);
      }
    memset(mem, 0, sizeof(mem));
    alternating_mask.copy_to(mem, overaligned);
    for (i = 0; i < M::size(); ++i)
      {
	COMPARE(mem[i], (i & 1) == 1);
      }
    for (; i < 3 * M::size(); ++i)
      {
	COMPARE(mem[i], false);
      }
    x.copy_to(mem, vector_aligned);
    where(alternating_mask, !x).copy_to(mem, overaligned);
    for (i = 0; i < M::size(); ++i)
      {
	COMPARE(mem[i], i % 2 == 0);
      }
    for (; i < 3 * M::size(); ++i)
      {
	COMPARE(mem[i], false);
      }
  }
