/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_IOTA_H_
#define VIR_SIMD_IOTA_H_

#include "simd.h"
#include "simd_concepts.h"
#if VIR_HAVE_SIMD_CONCEPTS
#define VIR_HAVE_SIMD_IOTA 1
#include <array>

namespace vir
{
  namespace detail
  {
    template <typename T, typename>
      struct iota_array;

    template <typename T, std::size_t... Is>
      struct iota_array<T, std::index_sequence<Is...>>
      { static constexpr T data[sizeof...(Is)] = {static_cast<T>(Is)...}; };
  }

  template <typename T>
    inline constexpr T
    iota_v;

  template <typename T>
    requires(std::is_arithmetic_v<T>)
    inline constexpr T
    iota_v<T> = T();

  template <vir::any_simd T>
    inline constexpr T
    iota_v<T> = T([](auto i) { return static_cast<typename T::value_type>(i); });

  template <typename T, std::size_t N>
    inline constexpr std::array<T, N>
    iota_v<std::array<T, N>> = []<std::size_t... Is>(std::index_sequence<Is...>) {
				   return std::array<T, N>{static_cast<T>(Is)...};
				 }(std::make_index_sequence<N>());

  template <typename T, std::size_t N>
    inline constexpr auto&
      iota_v<T[N]> = detail::iota_array<T, decltype(std::make_index_sequence<N>())>::data;
}

#endif  // VIR_HAVE_SIMD_CONCEPTS
#endif  // VIR_SIMD_IOTA_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
