/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_CVT_H_
#define VIR_SIMD_CVT_H_

#include "simd_concepts.h"
#if VIR_HAVE_SIMD_CONCEPTS
#define VIR_HAVE_SIMD_CVT 1

namespace vir
{
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

      template <typename U>
	requires std::convertible_to<T, U> or requires(const T&x)
	{
	  { stdx::static_simd_cast<U>(x) } -> std::same_as<U>;
	}
	constexpr
	operator U() const
	{
	  if constexpr (std::convertible_to<T, U>)
	    return ref;
	  else
	    return stdx::static_simd_cast<U>(ref);
	}
    };
}

#endif  // VIR_HAVE_SIMD_CONCEPTS
#endif  // VIR_SIMD_CVT_H_
// vim: noet cc=101 tw=100 sw=2 ts=8
