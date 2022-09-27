/*
    Copyright © 2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
                     Matthias Kretz <m.kretz@gsi.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef VIR_SIMD_FLOAT_OPS_H_
#define VIR_SIMD_FLOAT_OPS_H_

#include "detail.h"
#include <type_traits>
#include <bit>

namespace vir
{
  namespace meta
  {
    template <typename T, typename U = long long, bool = (sizeof(T) == sizeof(U))>
      struct as_int;

    template <typename T, typename U>
      struct as_int<T, U, true>
      { using type = U; };

    template <typename T>
      struct as_int<T, long long, false>
      : as_int<T, long> {};

    template <typename T>
      struct as_int<T, long, false>
      : as_int<T, int> {};

    template <typename T>
      struct as_int<T, int, false>
      : as_int<T, short> {};

    template <typename T>
      struct as_int<T, short, false>
      : as_int<T, signed char> {};

    template <typename T>
      struct as_int<T, signed char, false>
  #ifdef __SIZEOF_INT128__
      : as_int<T, __int128> {};

    template <typename T>
      struct as_int<T, __int128, false>
  #endif // __SIZEOF_INT128__
      {};

    template <typename T>
      using as_int_t = typename as_int<T>::type;

    template <typename T, typename U = unsigned long long, bool = (sizeof(T) == sizeof(U))>
      struct as_unsigned;

    template <typename T, typename U>
      struct as_unsigned<T, U, true>
      { using type = U; };

    template <typename T>
      struct as_unsigned<T, unsigned long long, false>
      : as_unsigned<T, unsigned long> {};

    template <typename T>
      struct as_unsigned<T, unsigned long, false>
      : as_unsigned<T, unsigned int> {};

    template <typename T>
      struct as_unsigned<T, unsigned int, false>
      : as_unsigned<T, unsigned short> {};

    template <typename T>
      struct as_unsigned<T, unsigned short, false>
      : as_unsigned<T, unsigned char> {};

    template <typename T>
      struct as_unsigned<T, unsigned char, false>
  #ifdef __SIZEOF_INT128__
      : as_unsigned<T, unsigned __int128> {};

    template <typename T>
      struct as_unsigned<T, unsigned __int128, false>
  #endif // __SIZEOF_INT128__
      {};

    template <typename T>
      using as_unsigned_t = typename as_unsigned<T>::type;
  }

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
            return std::bit_cast<V>(std::bit_cast<I>(a) & std::bit_cast<I>(b));
          }
        else
          return stdx::simd<T, A>([&](auto i) {
            using I = meta::as_unsigned_t<T>;
            return std::bit_cast<T>(std::bit_cast<I>(a[i]) & std::bit_cast<I>(a[i]));
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
            return std::bit_cast<V>(std::bit_cast<I>(a) | std::bit_cast<I>(b));
          }
        else
          return stdx::simd<T, A>([&](auto i) {
            using I = meta::as_unsigned_t<T>;
            return std::bit_cast<T>(std::bit_cast<I>(a[i]) | std::bit_cast<I>(a[i]));
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
            return std::bit_cast<V>(std::bit_cast<I>(a) ^ std::bit_cast<I>(b));
          }
        else
          return stdx::simd<T, A>([&](auto i) {
            using I = meta::as_unsigned_t<T>;
            return std::bit_cast<T>(std::bit_cast<I>(a[i]) ^ std::bit_cast<I>(a[i]));
          });
      }
  }
}

#endif // VIR_SIMD_FLOAT_OPS_H_
