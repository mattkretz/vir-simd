// Copyright (C) 2020-2022 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

// expensive: * [1-9] * *
#include "bits/verify.h"
#include <vir/simd_resize.h>
#include <vir/simd_bitset.h>
#include <vir/simd_cast.h>

namespace stdx = std::experimental;

template <typename From, typename To>
  void
  conversions()
  {
    using ToV = typename To::simd_type;
    using ToT = typename ToV::value_type;

    using stdx::simd_cast;
    using stdx::static_simd_cast;
    using vir::simd_size_cast;

    auto x = simd_size_cast<To>(vir::static_simd_cast<ToT>(From()));
    COMPARE(typeid(x), typeid(To));
    COMPARE(x, To());

    x = simd_size_cast<To>(vir::static_simd_cast<ToT>(From(true)));
    const To ref = ToV([](auto i) { return i; }) < int(From::size());
    COMPARE(x, ref) << "converted from: " << From(true);

    const ullong all_bits = ~ullong() >> (64 - From::size());
    for (ullong bit_pos = 1; bit_pos /*until overflow*/; bit_pos *= 2)
      {
	for (ullong bits : {bit_pos & all_bits, ~bit_pos & all_bits})
	  {
	    const auto from = vir::to_simd_mask<From>(bits);
	    const auto to = simd_size_cast<To>(vir::static_simd_cast<ToT>(from));
	    COMPARE(to, vir::to_simd_mask<To>(bits))
	      << "\nfrom: " << from << "\nbits: " << std::hex << bits << std::dec;
	    for (std::size_t i = 0; i < To::size(); ++i)
	      {
		COMPARE(to[i], (bits >> i) & 1)
		  << "\nfrom: " << from << "\nto: " << to
		  << "\nbits: " << std::hex << bits << std::dec << "\ni: " << i;
	      }
	  }
      }
  }

template <typename T, typename V, typename = void>
  struct rebind_or_max_fixed
  {
    using type = stdx::rebind_simd_t<
      T, stdx::resize_simd_t<stdx::simd_abi::max_fixed_size<T>, V>>;
  };

template <typename T, typename V>
  struct rebind_or_max_fixed<T, V, std::void_t<stdx::rebind_simd_t<T, V>>>
  {
    using type = stdx::rebind_simd_t<T, V>;
  };

template <typename From, typename To>
  void
  apply_abis()
  {
    using M0 = typename rebind_or_max_fixed<To, From>::type;
    using M1 = stdx::native_simd_mask<To>;
    using M2 = stdx::simd_mask<To>;
    using M3 = stdx::simd_mask<To, stdx::simd_abi::scalar>;

    using std::is_same_v;
    conversions<From, M0>();
    if constexpr (!is_same_v<M1, M0>)
      conversions<From, M1>();
    if constexpr (!is_same_v<M2, M0> && !is_same_v<M2, M1>)
      conversions<From, M2>();
    if constexpr (!is_same_v<M3, M0> && !is_same_v<M3, M1> && !is_same_v<M3, M2>)
      conversions<From, M3>();
  }

template <typename V>
  void
  test()
  {
    using M = typename V::mask_type;
    apply_abis<M, ldouble>();
    apply_abis<M, double>();
    apply_abis<M, float>();
    apply_abis<M, ullong>();
    apply_abis<M, llong>();
    apply_abis<M, ulong>();
    apply_abis<M, long>();
    apply_abis<M, uint>();
    apply_abis<M, int>();
    apply_abis<M, ushort>();
    apply_abis<M, short>();
    apply_abis<M, uchar>();
    apply_abis<M, schar>();
    apply_abis<M, char>();
    apply_abis<M, wchar_t>();
    apply_abis<M, char16_t>();
    apply_abis<M, char32_t>();
  }
