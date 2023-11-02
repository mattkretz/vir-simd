/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2022-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_DETAILS_H
#define VIR_DETAILS_H

#include "simd.h"
#include "constexpr_wrapper.h"
#include <type_traits>
#include <bit>

#if defined _GLIBCXX_EXPERIMENTAL_SIMD_H && defined __cpp_lib_experimental_parallel_simd
#define VIR_GLIBCXX_STDX_SIMD 1
#else
#define VIR_GLIBCXX_STDX_SIMD 0
#endif

#if defined __GNUC__ and not defined __clang__
#define VIR_LAMBDA_ALWAYS_INLINE __attribute__((__always_inline__))
#else
#define VIR_LAMBDA_ALWAYS_INLINE
#endif

#ifdef __has_builtin
// Clang 17 miscompiles permuting loads and stores for simdized types. I was unable to pin down the
// cause, but it seems highly likely that some __builtin_shufflevector calls get miscompiled. So
// far, not using __builtin_shufflevector has resolved all failures.
#if __has_builtin(__builtin_shufflevector) and __clang_major__ != 17
#define VIR_HAVE_WORKING_SHUFFLEVECTOR 1
#endif
#endif
#ifndef VIR_HAVE_WORKING_SHUFFLEVECTOR
#define VIR_HAVE_WORKING_SHUFFLEVECTOR 0
#endif


namespace vir::meta
{
  template <typename T>
    using is_simd_or_mask = std::disjunction<stdx::is_simd<T>, stdx::is_simd_mask<T>>;

  template <typename T>
    inline constexpr bool is_simd_or_mask_v = std::disjunction_v<stdx::is_simd<T>,
                                                                 stdx::is_simd_mask<T>>;

    template <typename T>
      struct type_identity
      { using type = T; };

    template <typename T>
      using type_identity_t = typename type_identity<T>::type;

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    template <typename T>
      struct as_int<T, signed char, false>
  #ifdef __SIZEOF_INT128__
      : as_int<T, __int128> {};

    template <typename T>
      struct as_int<T, __int128, false>
  #endif // __SIZEOF_INT128__
      {};
#pragma GCC diagnostic pop

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    template <typename T>
      struct as_unsigned<T, unsigned char, false>
  #ifdef __SIZEOF_INT128__
      : as_unsigned<T, unsigned __int128> {};

    template <typename T>
      struct as_unsigned<T, unsigned __int128, false>
  #endif // __SIZEOF_INT128__
      {};
#pragma GCC diagnostic pop

    template <typename T>
      using as_unsigned_t = typename as_unsigned<T>::type;
}

namespace vir::detail
{
  template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
    using FloatingPoint = T;

  using namespace vir::stdx;

    /**
     * Shortcut to determine the stdx::simd specialization with the most efficient ABI tag for the
     * requested element type T and width N.
     */
  template <typename T, int N>
    using deduced_simd = stdx::simd<T, stdx::simd_abi::deduce_t<T, N>>;

  template <typename T, int N>
    using deduced_simd_mask = stdx::simd_mask<T, stdx::simd_abi::deduce_t<T, N>>;

  template <typename To, typename From>
#ifdef __cpp_lib_bit_cast
    constexpr
#endif
    std::enable_if_t<sizeof(To) == sizeof(From), To>
    bit_cast(const From& x)
    {
#ifdef __cpp_lib_bit_cast
      return std::bit_cast<To>(x);
#else
      To r;
      std::memcpy(&r, &x, sizeof(x));
      return r;
#endif
    }

#if VIR_HAVE_CONSTEXPR_WRAPPER
  template <int Iterations, auto I = 0, typename F>
    [[gnu::always_inline, gnu::flatten]]
    constexpr void
    unroll(F&& fun)
    {
      if constexpr (Iterations != 0)
        {
          fun(vir::cw<I>);
          unroll<Iterations - 1, I + 1>(static_cast<F&&>(fun));
        }
    }

  template <int Iterations>
    [[gnu::always_inline, gnu::flatten]]
    constexpr void
    unroll2(auto&& fun0, auto&& fun1)
    {
      [&]<int... Is>(std::integer_sequence<int, Is...>) VIR_LAMBDA_ALWAYS_INLINE {
        [&](auto&&... r0s) VIR_LAMBDA_ALWAYS_INLINE {
          (fun1(vir::cw<Is>, static_cast<decltype(r0s)>(r0s)), ...);
        }(fun0(vir::cw<Is>)...);
      }(std::make_integer_sequence<int, Iterations>());
    }
#endif
}

#endif // VIR_DETAILS_H
