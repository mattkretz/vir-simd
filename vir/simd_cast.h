/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2022-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_CAST_H
#define VIR_SIMD_CAST_H

#include "simd.h"
#include "detail.h"

namespace vir
{
#if VIR_GLIBCXX_STDX_SIMD
  using std::experimental::parallelism_v2::__proposed::static_simd_cast;
#else
  using vir::stdx::static_simd_cast;
#endif
}

#endif // VIR_SIMD_CAST_H
