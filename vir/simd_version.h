/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2024–2025 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_VERSION_H_
#define VIR_SIMD_VERSION_H_

/** \file vir/simd_version.h
 * \brief Version macros and version constant.
 */

#if __cpp_impl_three_way_comparison >= 201907L and __cpp_lib_three_way_comparison >= 201907L
#define VIR_HAVE_SPACESHIP 1
#include <compare>
#else
#define VIR_HAVE_SPACESHIP 0
#endif

/** \brief The current version as a single integer
 *
 * The least significant 8 bits represent the patchlevel:
 * -     release >= 0x00 and even
 * - development >= 0x00 and odd
 * -       alpha >= 0xbe
 * -        beta >= 0xc8
 */
#define VIR_SIMD_VERSION 0x0'04'bd

/** \brief The major version number component.
 * \see vir::simd_version_t::major
 */
#define VIR_SIMD_VERSION_MAJOR (VIR_SIMD_VERSION / 0x10000)

/** \brief The minor version number component.
 * \see vir::simd_version_t::minor
 */
#define VIR_SIMD_VERSION_MINOR ((VIR_SIMD_VERSION % 0x10000) / 0x100)

/** \brief The patchlevel version number component.
 * \see vir::simd_version_t::patchlevel
 */
#define VIR_SIMD_VERSION_PATCHLEVEL (VIR_SIMD_VERSION % 0x100)

/**
 * \brief This namespace collects libraries and tools authored by Matthias Kretz.
 *
 * This is just a name used for avoiding name collisions, without any deeper meaning.
 *
 * \see vir::simd_version
 */
namespace vir
{
  /// Represents the vir-simd version of major, minor, and patchlevel components.
  struct simd_version_t
  {
    /// An increment of the major version number implies a breaking change.
    int major;
    /// An increment of the minor version number implies new features without breaking changes.
    int minor;
    /// An increment of the patchlevel is used for bug fixes. Odd numbers indicate a development version.
    int patchlevel;

    /// Check for equality.
    friend constexpr bool
    operator==(simd_version_t a, simd_version_t b)
    { return a.major == b.major and a.minor == b.minor and a.patchlevel == b.patchlevel; }

    /// Check for inequality.
    friend constexpr bool
    operator!=(simd_version_t a, simd_version_t b)
    { return not (a == b); }

#if VIR_HAVE_SPACESHIP
    /// Relational operators.
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

  /**
   * \brief The current version.
   *
   * Example: Check that you're compiling against vir-simd >= 0.5.0
    ```c++
    static_assert(vir::simd_version >= vir::simd_version_t{0,5,0});
    ```
   */
  inline constexpr simd_version_t
    simd_version = { VIR_SIMD_VERSION_MAJOR, VIR_SIMD_VERSION_MINOR, VIR_SIMD_VERSION_PATCHLEVEL };
}

#endif  // VIR_SIMD_VERSION_H_
