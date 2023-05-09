/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_CONCEPTS_H_
#define VIR_SIMD_CONCEPTS_H_

#if defined __cpp_concepts && __cpp_concepts >= 201907 && __has_include(<concepts>)
#define VIR_HAVE_SIMD_CONCEPTS 1
#include <concepts>

#include "simd.h"

namespace vir
{
  using std::size_t;

  template <typename T>
    concept arithmetic = std::floating_point<T> or std::integral<T>;

  template <typename T>
    concept vectorizable = arithmetic<T> and not std::same_as<T, bool>;

  template <typename T>
    concept simd_abi_tag = stdx::is_abi_tag_v<T>;

  template <typename V>
    concept any_simd
      = stdx::is_simd_v<V> and vectorizable<typename V::value_type>
	  and simd_abi_tag<typename V::abi_type>;

  template <typename V>
    concept any_simd_mask
      = stdx::is_simd_mask_v<V> and any_simd<typename V::simd_type>
	  and simd_abi_tag<typename V::abi_type>;

  template <typename V>
    concept any_simd_or_mask = any_simd<V> or any_simd_mask<V>;

  template <typename V, typename T>
    concept typed_simd = any_simd<V> and std::same_as<T, typename V::value_type>;

  template <typename V, size_t Width>
    concept sized_simd = any_simd<V> and V::size() == Width;

  template <typename V, size_t Width>
    concept sized_simd_mask = any_simd_mask<V> and V::size() == Width;
}

#endif // has concepts
#endif // VIR_SIMD_CONCEPTS_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
