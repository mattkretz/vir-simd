/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2026 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_VECMATH_H_
#define VIR_SIMD_VECMATH_H_

#include "simd.h"

#include <cstddef>
#include <cstring>
#include <type_traits>

/* Transcendental math functions evaluated by a vector math library.
 *
 * SIMD hardware has instructions for sqrt and abs, but not for the
 * transcendental functions. std::experimental::simd therefore falls back to
 * calling the scalar libm function once per lane, which makes a vectorized
 * kernel calling sin, exp or sinh slower than the scalar one it replaced.
 *
 * A vector math library computes a whole register worth of results per call.
 * This header routes the functions glibc's libmvec provides to it, and leaves
 * everything else to the underlying simd implementation.
 *
 * The calls go to the vector entry points directly, using the x86-64 vector
 * function ABI names, so no compiler flags and no auto-vectorization are
 * involved. Note that libmvec neither sets errno nor raises floating point
 * exceptions, and documents a maximum error of 4 ULP where the scalar routines
 * stay below 1 ULP.
 */

#if defined __x86_64__ && defined __GLIBC__ && defined VIR_HAVE_STD_SIMD
#include <features.h>
#ifdef __GLIBC_PREREQ

#if __GLIBC_PREREQ(2, 22)
#define VIR_HAVE_SIMD_VECMATH 1
#endif
#if __GLIBC_PREREQ(2, 35)
//! glibc 2.35 grew vector variants for everything beyond sin, cos, exp, log and pow
#define VIR_HAVE_SIMD_VECMATH_EXTENDED 1
#endif

#endif // __GLIBC_PREREQ
#endif // __x86_64__ && __GLIBC__ && VIR_HAVE_STD_SIMD

#ifdef VIR_HAVE_SIMD_VECMATH

namespace vir::vecmath_detail
{
  template <typename T, int Width>
    using vec [[gnu::vector_size(Width * sizeof(T))]] = T;

  using v2d = vec<double, 2>;
  using v4d = vec<double, 4>;
  using v8d = vec<double, 8>;
  using v4f = vec<float, 4>;
  using v8f = vec<float, 8>;
  using v16f = vec<float, 16>;

  /* Which lane counts can be handed to the vector math library
   *
   * The ISA letter in the symbol name says which instruction set the callee
   * uses: b is SSE2, c is AVX, d is AVX2 and e is AVX-512. A 256-bit call is
   * only available as AVX2 when the translation unit is built for AVX2, so the
   * letter follows the compiled-for ISA, while the lane count follows the
   * register width being passed.
   */
  template <typename T>
    struct native_lanes
    {
#if defined __AVX512F__
      static constexpr int value = 64 / int(sizeof(T));
#elif defined __AVX__
      static constexpr int value = 32 / int(sizeof(T));
#else
      static constexpr int value = 16 / int(sizeof(T));
#endif
    };

  //! true if a simd of Width elements of T can be evaluated in whole chunks
  template <typename T, int Width>
    inline constexpr bool is_supported_width
      = (std::is_same_v<T, float> || std::is_same_v<T, double>)
          && Width >= (16 / int(sizeof(T)))
          && Width % (16 / int(sizeof(T))) == 0;

  /* Chunk width used for a simd of Width elements
   *
   * The largest chunk the ISA supports that divides Width, so that a simd
   * wider than one register is evaluated in a few full calls rather than
   * falling back to scalar.
   */
  template <typename T, int Width>
    struct chunk_width
    {
      static constexpr int native = native_lanes<T>::value;
      static constexpr int value
        = Width % native == 0 ? native
            : (native > 16 / int(sizeof(T)) && Width % (native / 2) == 0) ? native / 2
            : 16 / int(sizeof(T));
    };
} // namespace vir::vecmath_detail

/* Naming the libmvec entry points
 *
 * The x86-64 vector function ABI spells them _ZGV<isa><mask><lanes><params>_<name>,
 * where the isa letter says which instruction set the callee uses: b is SSE2,
 * c is AVX, d is AVX2, e is AVX-512. A 256-bit call therefore has two spellings
 * and the right one is the one matching what this translation unit is built
 * for, while the lane count follows the register being passed.
 */
#if defined __AVX2__
#  define VIR_VECMATH_ISA256 d
#else
#  define VIR_VECMATH_ISA256 c
#endif

#define VIR_VECMATH_CAT_(a, b) a##b
#define VIR_VECMATH_CAT(a, b) VIR_VECMATH_CAT_(a, b)
#define VIR_VECMATH_SYM(isa, rest) VIR_VECMATH_CAT(VIR_VECMATH_CAT(_ZGV, isa), rest)

/* Declaring them
 *
 * Only the widths the target ISA actually has: declaring a function that
 * returns a 512-bit vector without AVX-512 enabled would change the ABI, which
 * GCC rightly warns about (-Wpsabi).
 */
#if defined __AVX512F__
#  define VIR_VECMATH_DECL_1_512(name)                                                             \
     vir::vecmath_detail::v8d  _ZGVeN8v_##name (vir::vecmath_detail::v8d);                         \
     vir::vecmath_detail::v16f _ZGVeN16v_##name##f (vir::vecmath_detail::v16f);
#  define VIR_VECMATH_DECL_2_512(name)                                                             \
     vir::vecmath_detail::v8d  _ZGVeN8vv_##name (vir::vecmath_detail::v8d,                         \
                                                 vir::vecmath_detail::v8d);                        \
     vir::vecmath_detail::v16f _ZGVeN16vv_##name##f (vir::vecmath_detail::v16f,                    \
                                                     vir::vecmath_detail::v16f);
#else
#  define VIR_VECMATH_DECL_1_512(name)
#  define VIR_VECMATH_DECL_2_512(name)
#endif

#if defined __AVX__
#  define VIR_VECMATH_DECL_1_256(name)                                                             \
     vir::vecmath_detail::v4d VIR_VECMATH_SYM(VIR_VECMATH_ISA256, N4v_##name)                      \
       (vir::vecmath_detail::v4d);                                                                 \
     vir::vecmath_detail::v8f VIR_VECMATH_SYM(VIR_VECMATH_ISA256, N8v_##name##f)                   \
       (vir::vecmath_detail::v8f);
#  define VIR_VECMATH_DECL_2_256(name)                                                             \
     vir::vecmath_detail::v4d VIR_VECMATH_SYM(VIR_VECMATH_ISA256, N4vv_##name)                     \
       (vir::vecmath_detail::v4d, vir::vecmath_detail::v4d);                                       \
     vir::vecmath_detail::v8f VIR_VECMATH_SYM(VIR_VECMATH_ISA256, N8vv_##name##f)                  \
       (vir::vecmath_detail::v8f, vir::vecmath_detail::v8f);
#else
#  define VIR_VECMATH_DECL_1_256(name)
#  define VIR_VECMATH_DECL_2_256(name)
#endif

#define VIR_VECMATH_DECL_1(name)                                                                   \
  extern "C" {                                                                                     \
    vir::vecmath_detail::v2d _ZGVbN2v_##name (vir::vecmath_detail::v2d);                           \
    vir::vecmath_detail::v4f _ZGVbN4v_##name##f (vir::vecmath_detail::v4f);                        \
    VIR_VECMATH_DECL_1_256(name)                                                                   \
    VIR_VECMATH_DECL_1_512(name)                                                                   \
  }

#define VIR_VECMATH_DECL_2(name)                                                                   \
  extern "C" {                                                                                     \
    vir::vecmath_detail::v2d _ZGVbN2vv_##name (vir::vecmath_detail::v2d,                           \
                                               vir::vecmath_detail::v2d);                          \
    vir::vecmath_detail::v4f _ZGVbN4vv_##name##f (vir::vecmath_detail::v4f,                        \
                                                  vir::vecmath_detail::v4f);                       \
    VIR_VECMATH_DECL_2_256(name)                                                                   \
    VIR_VECMATH_DECL_2_512(name)                                                                   \
  }

/* Selecting the entry point for one chunk
 *
 * VIR_VECMATH_CALL_1(sin) defines vir::vecmath_detail::call_sin, overloaded on
 * the raw vector type, so the chunk loop below simply calls it.
 */
#if defined __AVX512F__
#  define VIR_VECMATH_CALL_1_512(name)                                                             \
     VIR_ALWAYS_INLINE v8d call_##name (v8d x) { return _ZGVeN8v_##name(x); }                      \
     VIR_ALWAYS_INLINE v16f call_##name (v16f x) { return _ZGVeN16v_##name##f(x); }
#  define VIR_VECMATH_CALL_2_512(name)                                                             \
     VIR_ALWAYS_INLINE v8d call_##name (v8d x, v8d y) { return _ZGVeN8vv_##name(x, y); }           \
     VIR_ALWAYS_INLINE v16f call_##name (v16f x, v16f y)                                           \
     { return _ZGVeN16vv_##name##f(x, y); }
#else
#  define VIR_VECMATH_CALL_1_512(name)
#  define VIR_VECMATH_CALL_2_512(name)
#endif

#if defined __AVX__
#  define VIR_VECMATH_CALL_1_256(name)                                                             \
     VIR_ALWAYS_INLINE v4d call_##name (v4d x)                                                     \
     { return VIR_VECMATH_SYM(VIR_VECMATH_ISA256, N4v_##name)(x); }                                \
     VIR_ALWAYS_INLINE v8f call_##name (v8f x)                                                     \
     { return VIR_VECMATH_SYM(VIR_VECMATH_ISA256, N8v_##name##f)(x); }
#  define VIR_VECMATH_CALL_2_256(name)                                                             \
     VIR_ALWAYS_INLINE v4d call_##name (v4d x, v4d y)                                              \
     { return VIR_VECMATH_SYM(VIR_VECMATH_ISA256, N4vv_##name)(x, y); }                            \
     VIR_ALWAYS_INLINE v8f call_##name (v8f x, v8f y)                                              \
     { return VIR_VECMATH_SYM(VIR_VECMATH_ISA256, N8vv_##name##f)(x, y); }
#else
#  define VIR_VECMATH_CALL_1_256(name)
#  define VIR_VECMATH_CALL_2_256(name)
#endif

#define VIR_VECMATH_CALL_1(name)                                                                   \
  namespace vir::vecmath_detail {                                                                  \
    VIR_ALWAYS_INLINE v2d call_##name (v2d x) { return _ZGVbN2v_##name(x); }                       \
    VIR_ALWAYS_INLINE v4f call_##name (v4f x) { return _ZGVbN4v_##name##f(x); }                    \
    VIR_VECMATH_CALL_1_256(name)                                                                   \
    VIR_VECMATH_CALL_1_512(name)                                                                   \
  }

#define VIR_VECMATH_CALL_2(name)                                                                   \
  namespace vir::vecmath_detail {                                                                  \
    VIR_ALWAYS_INLINE v2d call_##name (v2d x, v2d y) { return _ZGVbN2vv_##name(x, y); }            \
    VIR_ALWAYS_INLINE v4f call_##name (v4f x, v4f y) { return _ZGVbN4vv_##name##f(x, y); }         \
    VIR_VECMATH_CALL_2_256(name)                                                                   \
    VIR_VECMATH_CALL_2_512(name)                                                                   \
  }

namespace vir::vecmath_detail
{
  //! true if the math functions below hand this simd to the vector math library
  template <typename T, typename Abi>
    inline constexpr bool use_vecmath
      = std::is_floating_point_v<T>
          && is_supported_width<T, int(stdx::simd<T, Abi>::size())>;

  //! true if they leave it to the underlying simd implementation instead
  template <typename T, typename Abi>
    inline constexpr bool use_fallback
      = std::is_floating_point_v<T>
          && !is_supported_width<T, int(stdx::simd<T, Abi>::size())>;

  /* Evaluate call on every chunk of x
   *
   * The round trip through lane[] is what lets this use nothing but the public
   * simd interface. It costs nothing: the buffer is a local of exactly the
   * chunk alignment, so the stores and loads fold into register moves and the
   * generated code is a plain call per chunk.
   */
  template <typename T, typename Abi, typename F>
    VIR_ALWAYS_INLINE stdx::simd<T, Abi>
    apply (const stdx::simd<T, Abi>& x, F&& call)
    {
      using V = stdx::simd<T, Abi>;
      constexpr int width = int(V::size());
      constexpr int chunk = chunk_width<T, width>::value;
      using chunk_type = vec<T, chunk>;

      alignas(stdx::memory_alignment_v<V>) T lane[width];
      x.copy_to(lane, stdx::vector_aligned);

      for (int i = 0; i < width; i += chunk)
        {
          chunk_type v;
          std::memcpy(&v, lane + i, sizeof(chunk_type));
          v = call(v);
          std::memcpy(lane + i, &v, sizeof(chunk_type));
        }

      V r;
      r.copy_from(lane, stdx::vector_aligned);
      return r;
    }

  //! @see apply
  template <typename T, typename Abi, typename F>
    VIR_ALWAYS_INLINE stdx::simd<T, Abi>
    apply (const stdx::simd<T, Abi>& x, const stdx::simd<T, Abi>& y, F&& call)
    {
      using V = stdx::simd<T, Abi>;
      constexpr int width = int(V::size());
      constexpr int chunk = chunk_width<T, width>::value;
      using chunk_type = vec<T, chunk>;

      alignas(stdx::memory_alignment_v<V>) T lane_x[width];
      alignas(stdx::memory_alignment_v<V>) T lane_y[width];
      x.copy_to(lane_x, stdx::vector_aligned);
      y.copy_to(lane_y, stdx::vector_aligned);

      for (int i = 0; i < width; i += chunk)
        {
          chunk_type vx, vy;
          std::memcpy(&vx, lane_x + i, sizeof(chunk_type));
          std::memcpy(&vy, lane_y + i, sizeof(chunk_type));
          vx = call(vx, vy);
          std::memcpy(lane_x + i, &vx, sizeof(chunk_type));
        }

      V r;
      r.copy_from(lane_x, stdx::vector_aligned);
      return r;
    }
} // namespace vir::vecmath_detail

/* Defining the overloads
 *
 * The overloads go into vir::stdx, which already pulls in the underlying
 * implementation with a using-directive. Qualified lookup stops as soon as it
 * finds a declaration in vir::stdx itself, so vir::stdx::sinh means these and
 * never the underlying one, without any ambiguity.
 *
 * That also means the pair below has to cover every simd the underlying
 * implementation covers: the fallback overload is not an optimization, it is
 * what keeps unsupported element types and widths working.
 */
#define VIR_VECMATH_FN_1(name)                                                                     \
  VIR_VECMATH_DECL_1(name)                                                                         \
  VIR_VECMATH_CALL_1(name)                                                                         \
  namespace vir::stdx {                                                                            \
    template <typename T, typename Abi>                                                            \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<vir::vecmath_detail::use_vecmath<T, Abi>, simd<T, Abi>>                     \
      name (const simd<T, Abi>& x)                                                                 \
      {                                                                                            \
        return vir::vecmath_detail::apply(                                                         \
                 x, [](auto v) { return vir::vecmath_detail::call_##name(v); });                   \
      }                                                                                            \
                                                                                                   \
    template <typename T, typename Abi>                                                            \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<vir::vecmath_detail::use_fallback<T, Abi>, simd<T, Abi>>                    \
      name (const simd<T, Abi>& x)                                                                 \
      { return std::experimental::parallelism_v2::name(x); }                                       \
  }

#define VIR_VECMATH_FN_2(name)                                                                     \
  VIR_VECMATH_DECL_2(name)                                                                         \
  VIR_VECMATH_CALL_2(name)                                                                         \
  namespace vir::stdx {                                                                            \
    template <typename T, typename Abi>                                                            \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<vir::vecmath_detail::use_vecmath<T, Abi>, simd<T, Abi>>                     \
      name (const simd<T, Abi>& x, const simd<T, Abi>& y)                                          \
      {                                                                                            \
        return vir::vecmath_detail::apply(                                                         \
                 x, y, [](auto a, auto b) { return vir::vecmath_detail::call_##name(a, b); });     \
      }                                                                                            \
                                                                                                   \
    template <typename T, typename Abi>                                                            \
      VIR_ALWAYS_INLINE                                                                             \
      std::enable_if_t<vir::vecmath_detail::use_fallback<T, Abi>, simd<T, Abi>>                    \
      name (const simd<T, Abi>& x, const simd<T, Abi>& y)                                          \
      { return std::experimental::parallelism_v2::name(x, y); }                                    \
  }

// available since glibc 2.22
VIR_VECMATH_FN_1(sin)
VIR_VECMATH_FN_1(cos)
VIR_VECMATH_FN_1(exp)
VIR_VECMATH_FN_1(log)
VIR_VECMATH_FN_2(pow)

#ifdef VIR_HAVE_SIMD_VECMATH_EXTENDED
VIR_VECMATH_FN_1(tan)
VIR_VECMATH_FN_1(asin)
VIR_VECMATH_FN_1(acos)
VIR_VECMATH_FN_1(atan)
VIR_VECMATH_FN_1(sinh)
VIR_VECMATH_FN_1(cosh)
VIR_VECMATH_FN_1(tanh)
VIR_VECMATH_FN_1(asinh)
VIR_VECMATH_FN_1(acosh)
VIR_VECMATH_FN_1(atanh)
VIR_VECMATH_FN_1(exp2)
VIR_VECMATH_FN_1(expm1)
VIR_VECMATH_FN_1(log2)
VIR_VECMATH_FN_1(log10)
VIR_VECMATH_FN_1(log1p)
VIR_VECMATH_FN_1(cbrt)
VIR_VECMATH_FN_1(erf)
VIR_VECMATH_FN_1(erfc)
VIR_VECMATH_FN_2(atan2)
#endif

/* Deliberately not routed here:
 *
 * hypot, because the underlying implementation already evaluates it with SIMD
 * instructions, including the fixups libmvec's version would not do, and
 * because its overload set (two and three arguments, plus the converting
 * forms) is larger than the pair generated above.
 */

#endif // VIR_HAVE_SIMD_VECMATH
#endif // VIR_SIMD_VECMATH_H_
