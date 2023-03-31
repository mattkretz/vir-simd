/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2022-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_BIT_H
#define VIR_SIMD_BIT_H

#include "simd.h"
#include "detail.h"

namespace vir
{
  template <typename To, typename From>
    constexpr
    std::enable_if_t<std::conjunction_v<std::integral_constant<bool, sizeof(To) == sizeof(From)>,
                                        meta::is_simd_or_mask<To>,
                                        meta::is_simd_or_mask<From>>, To>
    simd_bit_cast(const From& x)
    {
#if VIR_GLIBCXX_STDX_SIMD
      return std::experimental::__proposed::simd_bit_cast<To>(x);
#else
      return detail::bit_cast<To>(x);
#endif
    }
}

#endif // VIR_SIMD_BIT_H
