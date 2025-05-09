/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_CONCEPTS_H_
#define VIR_SIMD_CONCEPTS_H_

/** \file vir/simd_concepts.h
 * \brief C++20 concepts extending the Parallelism TS 2 (which is limited to C++17).
 */

#if defined DOXYGEN \
  || (defined __cpp_concepts && __cpp_concepts >= 201907 && __has_include(<concepts>) \
  && (__GNUC__ > 10 || defined __clang__))
#define VIR_HAVE_SIMD_CONCEPTS 1
#include <concepts>

#include "simd.h"

namespace vir
{
  using std::size_t;

  /// This concept matches the core language defintion of an arithmetic type.
  template <typename T>
    concept arithmetic = std::floating_point<T> or std::integral<T>;

  /**
   * \brief Satisfied for all arithmetic types except `bool`.
   *
   * This concept matches the *vectorizable type* as introduced in the Parallelism TS 2.
   *
   * \note The C++26 definition of a *vectorizable type* is different.
   */
  template <typename T>
    concept vectorizable = arithmetic<T> and not std::same_as<T, bool>;

  /// Satisfied if `T` is a SIMD ABI tag.
  template <typename T>
    concept simd_abi_tag = stdx::is_abi_tag_v<T>;

  /**
   * \brief Satisfied if `V` is a (valid) specialization of `simd<T, Abi>`.
   */
  template <typename V>
    concept any_simd
      = stdx::is_simd_v<V> and vectorizable<typename V::value_type>
	  and simd_abi_tag<typename V::abi_type>;

  /**
   * \brief Satisfied if `V` is a (valid) specialization of `simd_mask<T, Abi>`.
   */
  template <typename V>
    concept any_simd_mask
      = stdx::is_simd_mask_v<V> and any_simd<typename V::simd_type>
	  and simd_abi_tag<typename V::abi_type>;

  /// Satisfied if `V` is either a `simd` or a `simd_mask`.
  template <typename V>
    concept any_simd_or_mask = any_simd<V> or any_simd_mask<V>;

  /// Satisfied if `V` is a `simd<T, Abi>` with arbitrary but valid ABI tag `Abi`.
  template <typename V, typename T>
    concept typed_simd = any_simd<V> and std::same_as<T, typename V::value_type>;

  /// Satisfied if `V` is a `simd` with the given size `Width`.
  template <typename V, size_t Width>
    concept sized_simd = any_simd<V> and V::size() == Width;

  /// Satisfied if `V` is a `simd_mask` with the given size `Width`.
  template <typename V, size_t Width>
    concept sized_simd_mask = any_simd_mask<V> and V::size() == Width;
}

#endif // has concepts
#endif // VIR_SIMD_CONCEPTS_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
