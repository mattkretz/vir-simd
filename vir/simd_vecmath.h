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
 * This header offers the functions glibc's libmvec provides as
 * vir::vecmath::sinh(x) and so on, and leaves everything else to the
 * underlying simd implementation. Call them qualified: an unqualified sinh(x)
 * resolves to the underlying implementation through argument-dependent
 * lookup, and no using-declaration changes that.
 *
 * The calls go to the vector entry points directly, using the x86-64 vector
 * function ABI names, so no compiler flags and no auto-vectorization are
 * involved.
 *
 * What this costs. libmvec is built for -ffast-math callers, and glibc's own
 * test suite exercises it with errno and exception checking switched off, so
 * both become unspecified here:
 *
 *  - errno may be set where the scalar routine leaves it alone, and left alone
 *    where the scalar routine sets it,
 *  - the exception flags may gain FE_INEXACT on exact results, may miss flags
 *    the scalar routine raises, and may raise a different one (atanh(1) reports
 *    FE_INVALID rather than only FE_DIVBYZERO),
 *  - flags and errno are per call, not per lane, so one lane hitting a pole
 *    leaves them set for the whole vector,
 *  - results are accurate to 4 ULP where the scalar routines stay below 1, so
 *    they are not bit-wise identical to a scalar evaluation.
 *
 * Sign of zero, infinities, NaNs and denormals are handled the same as by the
 * scalar routines. Define VIR_DISABLE_SIMD_VECMATH to keep the underlying
 * implementation, which has none of the above caveats.
 *
 * For what the standard intends here, see P1928R15 section 6.1: "The intent is
 * to avoid errno altogether, while still supporting floating-point exceptions
 * (possibly depending on compiler flags)", noted as needing more work and not
 * yet reflected in the wording. Dropping errno is therefore the direction of
 * travel; the exception flags are where a vector math library falls short of
 * it. No accuracy bound is specified either way, so the ULP figures above are
 * glibc's own documentation rather than anything guaranteed.
 */

// __GLIBC__ only exists once a libc header has been seen, so pull it in first
#if __has_include(<features.h>)
#include <features.h>
#endif

// __ILP32__ excludes x32, where __x86_64__ is defined but the ABI is not this one
#if defined __x86_64__ && !defined __ILP32__ && defined __GLIBC__ \
      && defined VIR_HAVE_STD_SIMD && !defined VIR_DISABLE_SIMD_VECMATH
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
 *
 * The c (AVX) entry points are the one class glibc does not resolve through an
 * ifunc: they are wrappers that split the argument and call the SSE2 routine
 * twice. They are still far ahead of one scalar call per lane, but a target
 * without AVX2 should not expect a true 256-bit routine.
 */
#if defined __AVX2__
#  define VIR_VECMATH_ISA256 d
#else
#  define VIR_VECMATH_ISA256 c
#endif

/* The chunk callers below are always_inline, so normally no out-of-line copy
 * exists, but -fkeep-inline-functions or taking an address emits one, and its
 * body differs per instruction set exactly as the public overloads' does. An
 * inline namespace named after the ISA keeps those symbols apart without
 * touching how they are called.
 */
#if defined __AVX512F__
#  define VIR_VECMATH_ISA_NS isa_avx512
#elif defined __AVX2__
#  define VIR_VECMATH_ISA_NS isa_avx2
#elif defined __AVX__
#  define VIR_VECMATH_ISA_NS isa_avx
#else
#  define VIR_VECMATH_ISA_NS isa_sse2
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

/* The chunk callers carry the tag as well: they are always_inline too, but
 * -fkeep-inline-functions or taking an address emits them, and the body
 * differs per ISA exactly as the public overloads' does.
 */
#define VIR_VECMATH_CALL_1(name)                                                                   \
  namespace vir::vecmath_detail { inline namespace VIR_VECMATH_ISA_NS {                            \
    VIR_ALWAYS_INLINE v2d call_##name (v2d x) { return _ZGVbN2v_##name(x); }                       \
    VIR_ALWAYS_INLINE v4f call_##name (v4f x) { return _ZGVbN4v_##name##f(x); }                    \
    VIR_VECMATH_CALL_1_256(name)                                                                   \
    VIR_VECMATH_CALL_1_512(name)                                                                   \
  } }

#define VIR_VECMATH_CALL_2(name)                                                                   \
  namespace vir::vecmath_detail { inline namespace VIR_VECMATH_ISA_NS {                            \
    VIR_ALWAYS_INLINE v2d call_##name (v2d x, v2d y) { return _ZGVbN2vv_##name(x, y); }            \
    VIR_ALWAYS_INLINE v4f call_##name (v4f x, v4f y) { return _ZGVbN4vv_##name##f(x, y); }         \
    VIR_VECMATH_CALL_2_256(name)                                                                   \
    VIR_VECMATH_CALL_2_512(name)                                                                   \
  } }

namespace vir::vecmath_detail
{
  /* Discriminator for the emitted symbol
   *
   * These functions are always_inline, so at -O1 and above no out-of-line copy
   * is emitted at all. At -O0, or when the address of one is taken, a weak
   * symbol appears, and two translation units built for different instruction
   * sets would otherwise agree on its name while disagreeing on its body: the
   * linker keeps one, and the other TU ends up calling a libmvec entry point
   * its target may not be able to execute. Naming the ISA in the signature
   * keeps those symbols apart. libstdc++ solves the same problem the same way,
   * see __odr_helper in <experimental/bits/simd.h>.
   */
  template <int Isa>
    struct isa_tag {};

#if defined __AVX512F__
  using odr_tag = isa_tag<3>;
#elif defined __AVX2__
  using odr_tag = isa_tag<2>;
#elif defined __AVX__
  using odr_tag = isa_tag<1>;
#else
  using odr_tag = isa_tag<0>;
#endif

  /* Keeps a parameter out of template argument deduction
   *
   * The second argument of a two-argument function must not take part in
   * deduction, so that pow(x, 2.0) converts the scalar to a simd instead of
   * failing to deduce. This mirrors _Extra_argument_type in libstdc++.
   */
  template <typename T>
    struct nondeduced { using type = T; };

  template <typename T>
    using nondeduced_t = typename nondeduced<T>::type;

  //! true for a first argument that is not a simd but converts to one
  template <typename U, typename T, typename Abi>
    inline constexpr bool is_convertible_first
      = std::is_floating_point_v<T>
          && !std::is_same_v<std::decay_t<U>, stdx::simd<T, Abi>>
          && std::is_convertible_v<U, stdx::simd<T, Abi>>;

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
  namespace vir::vecmath {                                                                         \
    template <typename T, typename Abi, typename = vecmath_detail::odr_tag>                        \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<vecmath_detail::use_vecmath<T, Abi>, stdx::simd<T, Abi>>                    \
      name (const stdx::simd<T, Abi>& x)                                                           \
      {                                                                                            \
        return vecmath_detail::apply(                                                              \
                 x, [](auto v) { return vecmath_detail::call_##name(v); });                        \
      }                                                                                            \
                                                                                                   \
    template <typename T, typename Abi, typename = vecmath_detail::odr_tag>                        \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<vecmath_detail::use_fallback<T, Abi>, stdx::simd<T, Abi>>                   \
      name (const stdx::simd<T, Abi>& x)                                                           \
      { return std::experimental::parallelism_v2::name(x); }                                       \
  }

/* Two-argument functions
 *
 * The shape follows libstdc++'s _GLIBCXX_SIMD_MATH_CALL2_ exactly, because
 * these overloads hide it: a first form whose second parameter is excluded
 * from deduction, so that pow(x, 2.0) broadcasts the scalar, and a reversed
 * form for pow(2.0, x). Deducing both parameters instead would compile, and
 * would silently take those two spellings away from every caller.
 */
#define VIR_VECMATH_FN_2(name)                                                                     \
  VIR_VECMATH_DECL_2(name)                                                                         \
  VIR_VECMATH_CALL_2(name)                                                                         \
  namespace vir::vecmath {                                                                         \
    template <typename T, typename Abi, typename = vecmath_detail::odr_tag>                        \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<vecmath_detail::use_vecmath<T, Abi>, stdx::simd<T, Abi>>                    \
      name (const stdx::simd<T, Abi>& x,                                                           \
            const vecmath_detail::nondeduced_t<stdx::simd<T, Abi>>& y)                             \
      {                                                                                            \
        return vecmath_detail::apply(                                                              \
                 x, y, [](auto a, auto b) { return vecmath_detail::call_##name(a, b); });          \
      }                                                                                            \
                                                                                                   \
    template <typename T, typename Abi, typename = vecmath_detail::odr_tag>                        \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<vecmath_detail::use_fallback<T, Abi>, stdx::simd<T, Abi>>                   \
      name (const stdx::simd<T, Abi>& x,                                                           \
            const vecmath_detail::nondeduced_t<stdx::simd<T, Abi>>& y)                             \
      { return std::experimental::parallelism_v2::name(x, y); }                                    \
                                                                                                   \
    template <typename U, typename T, typename Abi,                                                \
              typename = std::enable_if_t<                                                         \
                           vecmath_detail::is_convertible_first<U, T, Abi>>>                       \
      VIR_ALWAYS_INLINE stdx::simd<T, Abi>                                                         \
      name (U&& x, const stdx::simd<T, Abi>& y)                                                    \
      {                                                                                            \
        /* Qualified, so that argument-dependent lookup cannot add the                             \
         * underlying overload, which deduces both parameters and would                            \
         * therefore win partial ordering. */                                                      \
        return vir::vecmath::name(stdx::simd<T, Abi>(static_cast<U&&>(x)), y);                     \
      }                                                                                            \
  }

/* Functions the vector math library on this system does not have
 *
 * They still get a vir::vecmath overload, forwarding to the underlying
 * implementation. Whether a function is routed is this header's business, not
 * the caller's: code calling vir::vecmath::sinh should compile against any
 * glibc and simply be faster on the ones that can.
 */
#define VIR_VECMATH_FN_1_FORWARD(name)                                                             \
  namespace vir::vecmath {                                                                         \
    template <typename T, typename Abi, typename = vecmath_detail::odr_tag>                        \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<std::is_floating_point_v<T>, stdx::simd<T, Abi>>                            \
      name (const stdx::simd<T, Abi>& x)                                                           \
      { return std::experimental::parallelism_v2::name(x); }                                       \
  }

#define VIR_VECMATH_FN_2_FORWARD(name)                                                             \
  namespace vir::vecmath {                                                                         \
    template <typename T, typename Abi, typename = vecmath_detail::odr_tag>                        \
      VIR_ALWAYS_INLINE                                                                            \
      std::enable_if_t<std::is_floating_point_v<T>, stdx::simd<T, Abi>>                            \
      name (const stdx::simd<T, Abi>& x,                                                           \
            const vecmath_detail::nondeduced_t<stdx::simd<T, Abi>>& y)                             \
      { return std::experimental::parallelism_v2::name(x, y); }                                    \
                                                                                                   \
    template <typename U, typename T, typename Abi,                                                \
              typename = std::enable_if_t<                                                         \
                           vecmath_detail::is_convertible_first<U, T, Abi>>>                       \
      VIR_ALWAYS_INLINE stdx::simd<T, Abi>                                                         \
      name (U&& x, const stdx::simd<T, Abi>& y)                                                    \
      { return vir::vecmath::name(stdx::simd<T, Abi>(static_cast<U&&>(x)), y); }                   \
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
#else
VIR_VECMATH_FN_1_FORWARD(tan)
VIR_VECMATH_FN_1_FORWARD(asin)
VIR_VECMATH_FN_1_FORWARD(acos)
VIR_VECMATH_FN_1_FORWARD(atan)
VIR_VECMATH_FN_1_FORWARD(sinh)
VIR_VECMATH_FN_1_FORWARD(cosh)
VIR_VECMATH_FN_1_FORWARD(tanh)
VIR_VECMATH_FN_1_FORWARD(asinh)
VIR_VECMATH_FN_1_FORWARD(acosh)
VIR_VECMATH_FN_1_FORWARD(atanh)
VIR_VECMATH_FN_1_FORWARD(exp2)
VIR_VECMATH_FN_1_FORWARD(expm1)
VIR_VECMATH_FN_1_FORWARD(log2)
VIR_VECMATH_FN_1_FORWARD(log10)
VIR_VECMATH_FN_1_FORWARD(log1p)
VIR_VECMATH_FN_1_FORWARD(cbrt)
VIR_VECMATH_FN_1_FORWARD(erf)
VIR_VECMATH_FN_1_FORWARD(erfc)
VIR_VECMATH_FN_2_FORWARD(atan2)
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
