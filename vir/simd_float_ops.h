/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2022-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_FLOAT_OPS_H_
#define VIR_SIMD_FLOAT_OPS_H_

#include "detail.h"
#include <type_traits>

namespace vir
{
  namespace simd_float_ops
  {
    template <typename T, typename A>
      constexpr inline stdx::simd<T, A>
      operator&(const stdx::simd<detail::FloatingPoint<T>, A> a,
                const stdx::simd<T, A> b) noexcept
      {
        if constexpr (sizeof(T) <= sizeof(long long))
          {
            using V = stdx::simd<T, A>;
            using I = stdx::rebind_simd_t<meta::as_unsigned_t<T>, V>;
            return detail::bit_cast<V>(detail::bit_cast<I>(a) & detail::bit_cast<I>(b));
          }
        else
          return stdx::simd<T, A>([&](auto i) {
            using I = meta::as_unsigned_t<T>;
            return detail::bit_cast<T>(detail::bit_cast<I>(a[i]) & detail::bit_cast<I>(b[i]));
          });
      }

    template <typename T, typename A>
      constexpr inline stdx::simd<T, A>
      operator|(const stdx::simd<detail::FloatingPoint<T>, A> a,
                const stdx::simd<T, A> b) noexcept
      {
        if constexpr (sizeof(T) <= sizeof(long long))
          {
            using V = stdx::simd<T, A>;
            using I = stdx::rebind_simd_t<meta::as_unsigned_t<T>, V>;
            return detail::bit_cast<V>(detail::bit_cast<I>(a) | detail::bit_cast<I>(b));
          }
        else
          return stdx::simd<T, A>([&](auto i) {
            using I = meta::as_unsigned_t<T>;
            return detail::bit_cast<T>(detail::bit_cast<I>(a[i]) | detail::bit_cast<I>(b[i]));
          });
      }

    template <typename T, typename A>
      constexpr inline stdx::simd<T, A>
      operator^(const stdx::simd<detail::FloatingPoint<T>, A> a,
                const stdx::simd<T, A> b) noexcept
      {
        if constexpr (sizeof(T) <= sizeof(long long))
          {
            using V = stdx::simd<T, A>;
            using I = stdx::rebind_simd_t<meta::as_unsigned_t<T>, V>;
            return detail::bit_cast<V>(detail::bit_cast<I>(a) ^ detail::bit_cast<I>(b));
          }
        else
          return stdx::simd<T, A>([&](auto i) {
            using I = meta::as_unsigned_t<T>;
            return detail::bit_cast<T>(detail::bit_cast<I>(a[i]) ^ detail::bit_cast<I>(b[i]));
          });
      }
  }
}

#endif // VIR_SIMD_FLOAT_OPS_H_
