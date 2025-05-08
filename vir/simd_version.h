/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2024–2025 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_VERSION_H_
#define VIR_SIMD_VERSION_H_

#if __cpp_impl_three_way_comparison >= 201907L and __cpp_lib_three_way_comparison >= 201907L
#define VIR_HAVE_SPACESHIP 1
#include <compare>
#else
#define VIR_HAVE_SPACESHIP 0
#endif

//     release >= 0x00 and even
// development >= 0x00 and odd
//       alpha >= 0xbe
//        beta >= 0xc8
#define VIR_SIMD_VERSION 0x0'04'bd

#define VIR_SIMD_VERSION_MAJOR (VIR_SIMD_VERSION / 0x10000)

#define VIR_SIMD_VERSION_MINOR ((VIR_SIMD_VERSION % 0x10000) / 0x100)

#define VIR_SIMD_VERSION_PATCHLEVEL (VIR_SIMD_VERSION % 0x100)

namespace vir
{
  struct simd_version_t
  {
    int major, minor, patchlevel;

    friend constexpr bool
    operator==(simd_version_t a, simd_version_t b)
    { return a.major == b.major and a.minor == b.minor and a.patchlevel == b.patchlevel; }

    friend constexpr bool
    operator!=(simd_version_t a, simd_version_t b)
    { return not (a == b); }

#if VIR_HAVE_SPACESHIP
    friend constexpr std::strong_ordering
    operator<=>(simd_version_t, simd_version_t) = default;
#else
    friend constexpr bool
    operator<(simd_version_t a, simd_version_t b)
    {
      return a.major < b.major
               or (a.major == b.major and a.minor < b.minor)
               or (a.major == b.major and a.minor == b.minor and a.patchlevel < b.patchlevel);
    }

    friend constexpr bool
    operator<=(simd_version_t a, simd_version_t b)
    {
      return a.major < b.major
               or (a.major == b.major and a.minor < b.minor)
               or (a.major == b.major and a.minor == b.minor and a.patchlevel <= b.patchlevel);
    }

    friend constexpr bool
    operator>(simd_version_t a, simd_version_t b)
    { return b < a; }

    friend constexpr bool
    operator>=(simd_version_t a, simd_version_t b)
    { return b <= a; }
#endif
  };

  inline constexpr simd_version_t
    simd_version = { VIR_SIMD_VERSION_MAJOR, VIR_SIMD_VERSION_MINOR, VIR_SIMD_VERSION_PATCHLEVEL };
}

#endif  // VIR_SIMD_VERSION_H_
