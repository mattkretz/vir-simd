/* SPDX-License-Identifier: LGPL-3.0-or-later WITH LGPL-3.0-linking-exception */
/* Copyright © 2023–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_CVT_H_
#define VIR_SIMD_CVT_H_

#include "simd_concepts.h"
#define VIR_HAVE_SIMD_CVT 1

namespace vir
{
#if not VIR_HAVE_SIMD_CONCEPTS
  namespace detail
  {
    template <typename From, typename To, typename = void>
      struct can_static_simd_cast
      : std::false_type
      {};

    template <typename From, typename To>
      struct can_static_simd_cast<From, To, std::void_t<decltype(stdx::static_simd_cast<To>(
                                                                   std::declval<const From&>()))>>
      : std::is_same<decltype(stdx::static_simd_cast<To>(std::declval<const From&>())), To>
      {};
  }
#endif

  template <typename T>
    class cvt
    {
      const T& ref;

    public:
      constexpr
      cvt(const T& x)
      : ref(x)
      {}

      cvt(const cvt&) = delete;

#if VIR_HAVE_SIMD_CONCEPTS
      template <typename U>
	requires std::convertible_to<T, U> or requires(const T&x)
	{
	  { stdx::static_simd_cast<U>(x) } -> std::same_as<U>;
	}
#else
      template <typename U, typename = std::enable_if_t<
                                         std::disjunction_v<std::is_convertible<T, U>,
                                                            detail::can_static_simd_cast<T, U>>>>
#endif
	constexpr
	operator U() const
	{
	  if constexpr (std::is_convertible_v<T, U>)
	    return ref;
	  else
	    return stdx::static_simd_cast<U>(ref);
	}
    };
}

#endif  // VIR_SIMD_CVT_H_
// vim: et cc=101 tw=100 sw=2 ts=8
