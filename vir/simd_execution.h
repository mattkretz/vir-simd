/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_EXECUTION_H_
#define VIR_SIMD_EXECUTION_H_

#include "simd_concepts.h"
#include "simd_cvt.h"
#include "simdize.h"

#if VIR_HAVE_SIMD_CONCEPTS and VIR_HAVE_SIMDIZE and VIR_HAVE_SIMD_CVT
#define VIR_HAVE_SIMD_EXECUTION 1

#include <ranges>
#include <cstdint>
#include <utility>

namespace vir
{
  namespace detail
  {
    [[noreturn]] inline void
    unreachable()
    {
#if __cpp_lib_unreachable >= 202202L
      std::unreachable();
#elif defined __GNUC__
      __builtin_unreachable();
#endif
    }

    template <typename T>
      constexpr auto
      data_or_ptr(T&& r) -> decltype(std::ranges::data(r))
      { return std::ranges::data(r); }

    template <typename T>
      constexpr T*
      data_or_ptr(T* ptr)
      { return ptr; }

    template <typename T>
      constexpr const T*
      data_or_ptr(const T* ptr)
      { return ptr; }

    // Invokes fun(V&) or fun(const V&) with V copied from ptr.
    // If write_back is true, copy it back to ptr.
    template <typename V, bool write_back = false, typename Flags = stdx::element_aligned_tag,
              std::size_t... Is>
      constexpr void
      simd_load_and_invoke(auto&& fun, auto ptr, Flags f, std::index_sequence<Is...>)
      {
        [&](auto... chunks) {
          std::invoke(fun, chunks...);
          if constexpr (write_back)
            (chunks.copy_to(ptr + (V::size() * Is), f), ...);
        }(std::conditional_t<write_back, V, const V>(ptr + (V::size() * Is), f)...);
      }

    template <typename V, typename Flags = stdx::element_aligned_tag, std::size_t... Is>
      constexpr void
      simd_load_and_invoke(auto&& unary_op, auto ptr, auto&& result_op, Flags f, std::index_sequence<Is...>)
      {
        [&](const auto... chunks) {
          (std::invoke(result_op, std::invoke(unary_op, chunks),
                       std::integral_constant<std::size_t, V::size() * Is>()), ...);
        }(V(ptr + (V::size() * Is), f)...);
      }

    template <typename V1, typename V2, typename F1 = stdx::element_aligned_tag,
              typename F2 = stdx::element_aligned_tag, std::size_t... Is>
      constexpr void
      simd_load_and_invoke(auto&& binary_op, auto ptr1, auto ptr2, auto&& result_op, F1 f1, F2 f2,
                           std::index_sequence<Is...>)
      {
        constexpr auto size = V1::size();
        static_assert(size == V2::size());
        [&](const auto&... v1s) {
          [&](const auto&... v2s) {
            (std::invoke(result_op, std::invoke(binary_op, v1s, v2s),
                         std::integral_constant<std::size_t, size * Is>()), ...);
          }(V2(ptr2 + (size * Is), f2)...);
        }(V1(ptr1 + (size * Is), f1)...);
      }

    static constexpr auto no_unroll = std::make_index_sequence<1>();

    template <class V, bool write_back, std::size_t max_elements>
      void
      simd_for_each_jumptable_prologue(auto&& fun, auto ptr, std::size_t to_process)
      {
        switch (to_process)
        {
#define VIR_CASE(N, M)                                                                             \
          case N:                                                                                  \
            static_assert(std::has_single_bit(unsigned(N - M)));                                   \
            static_assert(N - M < M or M == 0);                                                    \
            static_assert(((N - M) & M) == 0);                                                     \
            goto case##N;                                                                          \
case##N:                                                                                           \
            if constexpr (N >= max_elements)                                                       \
              unreachable();                                                                       \
            simd_load_and_invoke<stdx::resize_simd_t<N - M, V>, write_back>(                       \
              fun, ptr, stdx::vector_aligned, no_unroll);                                          \
            ptr += N - M;                                                                          \
            goto case##M

          VIR_CASE(1, 0);
          VIR_CASE(2, 0);
          VIR_CASE(3, 2);
          VIR_CASE(4, 0);
          VIR_CASE(5, 4);
          VIR_CASE(6, 4);
          VIR_CASE(7, 6);
          VIR_CASE(8, 0);
          VIR_CASE(9, 8);
          VIR_CASE(10, 8);
          VIR_CASE(11, 10);
          VIR_CASE(12, 8);
          VIR_CASE(13, 12);
          VIR_CASE(14, 12);
          VIR_CASE(15, 14);
          VIR_CASE(16, 0);
          VIR_CASE(17, 16);
          VIR_CASE(18, 16);
          VIR_CASE(19, 18);
          VIR_CASE(20, 16);
          VIR_CASE(21, 20);
          VIR_CASE(22, 20);
          VIR_CASE(23, 22);
          VIR_CASE(24, 16);
          VIR_CASE(25, 24);
          VIR_CASE(26, 24);
          VIR_CASE(27, 26);
          VIR_CASE(28, 24);
          VIR_CASE(29, 28);
          VIR_CASE(30, 28);
          VIR_CASE(31, 30);
          VIR_CASE(32,  0);
          VIR_CASE(33, 32);
          VIR_CASE(34, 32);
          VIR_CASE(35, 34);
          VIR_CASE(36, 32);
          VIR_CASE(37, 36);
          VIR_CASE(38, 34);
          VIR_CASE(39, 38);
          VIR_CASE(40, 32);
          VIR_CASE(41, 40);
          VIR_CASE(42, 40);
          VIR_CASE(43, 42);
          VIR_CASE(44, 40);
          VIR_CASE(45, 44);
          VIR_CASE(46, 44);
          VIR_CASE(47, 46);
          VIR_CASE(48, 32);
          VIR_CASE(49, 48);
          VIR_CASE(50, 48);
          VIR_CASE(51, 50);
          VIR_CASE(52, 48);
          VIR_CASE(53, 52);
          VIR_CASE(54, 52);
          VIR_CASE(55, 54);
          VIR_CASE(56, 48);
          VIR_CASE(57, 56);
          VIR_CASE(58, 56);
          VIR_CASE(59, 58);
          VIR_CASE(60, 56);
          VIR_CASE(61, 60);
          VIR_CASE(62, 60);
          VIR_CASE(63, 62);
#undef VIR_CASE

          default:
            unreachable();
        }
case0:
        return;
      }

    template <class V, bool write_back, std::size_t max_elements>
      constexpr void
      simd_for_each_prologue(auto&& fun, auto ptr, std::size_t to_process)
      {
        static_assert(max_elements > 1);
        static_assert(std::has_single_bit(max_elements));
        static_assert(std::has_single_bit(V::size()));
        // pre-conditions:
        // * ptr is a valid pointer to a range [ptr, ptr + to_process)
        // * to_process > 0
        // * to_process < max_elements

        if (to_process == 0 or to_process >= max_elements)
          unreachable();

        if constexpr (max_elements == 2) // implies to_process == 1
          {
            simd_load_and_invoke<V, write_back>(fun, ptr, stdx::vector_aligned, no_unroll);
            return;
          }
#if !__OPTIMIZE_SIZE__
        if constexpr (V::size() == 1 and max_elements < 64)
          if (not std::is_constant_evaluated())
            return simd_for_each_jumptable_prologue<V, write_back, max_elements>(
                     fun, ptr, to_process);
#endif

        if (V::size() & to_process)
          {
            simd_load_and_invoke<V, write_back>(fun, ptr, stdx::vector_aligned, no_unroll);
            ptr += V::size();
          }
        if constexpr (V::size() * 2 < max_elements)
          {
            simd_for_each_prologue<stdx::resize_simd_t<V::size() * 2, V>,
                                   write_back, max_elements>(fun, ptr, to_process);
          }
      }

    template <class V0, bool write_back>
      void
      simd_for_each_jumptable_epilogue(auto&& fun, unsigned leftover, auto last, auto f)
      {
        // pre-conditions:
        // * leftover > 0
        // * [last - leftover, last) is a valid range
        auto ptr = std::to_address(last - leftover);
        switch (leftover)
        {
#define VIR_CASE(N, M)                                                                         \
          case N:                                                                              \
            goto case##N;                                                                      \
case##N:                                                                                       \
            if constexpr (N >= V0::size())                                                     \
              unreachable();                                                                   \
            simd_load_and_invoke<stdx::resize_simd_t<N - M, V0>, write_back>(                  \
              fun, ptr, f, no_unroll);                                                         \
            ptr += N - M;                                                                      \
            goto case##M

          VIR_CASE(1, 0);
          VIR_CASE(2, 0);
          VIR_CASE(3, 1);
          VIR_CASE(4, 0);
          VIR_CASE(5, 1);
          VIR_CASE(6, 2);
          VIR_CASE(7, 3);
          VIR_CASE(8, 0);
          VIR_CASE(9, 1);
          VIR_CASE(10, 2);
          VIR_CASE(11, 3);
          VIR_CASE(12, 4);
          VIR_CASE(13, 5);
          VIR_CASE(14, 6);
          VIR_CASE(15, 7);
          VIR_CASE(16, 0);
          VIR_CASE(17, 1);
          VIR_CASE(18, 2);
          VIR_CASE(19, 3);
          VIR_CASE(20, 4);
          VIR_CASE(21, 5);
          VIR_CASE(22, 6);
          VIR_CASE(23, 7);
          VIR_CASE(24, 8);
          VIR_CASE(25, 9);
          VIR_CASE(26, 10);
          VIR_CASE(27, 11);
          VIR_CASE(28, 12);
          VIR_CASE(29, 13);
          VIR_CASE(30, 14);
          VIR_CASE(31, 15);
          VIR_CASE(32,  0);
          VIR_CASE(33,  1);
          VIR_CASE(34,  2);
          VIR_CASE(35,  3);
          VIR_CASE(36,  4);
          VIR_CASE(37,  5);
          VIR_CASE(38,  6);
          VIR_CASE(39,  7);
          VIR_CASE(40,  8);
          VIR_CASE(41,  9);
          VIR_CASE(42, 10);
          VIR_CASE(43, 11);
          VIR_CASE(44, 12);
          VIR_CASE(45, 13);
          VIR_CASE(46, 14);
          VIR_CASE(47, 15);
          VIR_CASE(48, 16);
          VIR_CASE(49, 17);
          VIR_CASE(50, 18);
          VIR_CASE(51, 19);
          VIR_CASE(52, 20);
          VIR_CASE(53, 21);
          VIR_CASE(54, 22);
          VIR_CASE(55, 23);
          VIR_CASE(56, 24);
          VIR_CASE(57, 25);
          VIR_CASE(58, 26);
          VIR_CASE(59, 27);
          VIR_CASE(60, 28);
          VIR_CASE(61, 29);
          VIR_CASE(62, 30);
          VIR_CASE(63, 31);
#undef VIR_CASE

          default:
            unreachable();
          }
case0:
        return;
      }

    template <class V0, bool write_back>
      constexpr void
      simd_for_each_epilogue(auto&& fun, unsigned leftover, auto last, auto f)
      {
        // pre-conditions:
        // * leftover > 0
        // * leftover < V0::size()
        // * [last - leftover, last) is a valid range
        static_assert(V0::size() > 1);
        using V1 = stdx::resize_simd_t<1, V0>;
        if constexpr (V0::size() == 2) // implies leftover == 1
          {
            simd_load_and_invoke<V1, write_back>(fun, std::to_address(last - 1), f, no_unroll);
            return;
          }
        if constexpr (V0::size() <= 4) // implies leftover ∈ [1, 3]
          {
            if (not std::is_constant_evaluated())
              return simd_for_each_jumptable_epilogue<V0, write_back>(fun, leftover, last, f);
          }
#if !__OPTIMIZE_SIZE__
        // The jumptable implementation for larger V0::size() can result in a lot of machine code.
        // If the user wants to optimize for size then this isn't the right solution.
        if constexpr (V0::size() <= 64
                        and V0::size() <= vir::stdx::native_simd<typename V0::value_type>::size())
          {
            if (not std::is_constant_evaluated())
              return simd_for_each_jumptable_epilogue<V0, write_back>(fun, leftover, last, f);
          }
#endif // !__OPTIMIZE_SIZE__
        using V = stdx::resize_simd_t<std::bit_ceil(V0::size()) / 2, V0>;
        if (leftover & V::size())
          {
            static_assert(std::has_single_bit(V::size())); // by construction
            simd_load_and_invoke<V, write_back>(fun, std::to_address(last - leftover), f,
                                                no_unroll);
            leftover -= V::size();
          }
        if constexpr (V::size() > 1)
          if (leftover)
            simd_for_each_epilogue<V, write_back>(fun, leftover, last, f);
      }

    template <typename V, typename R, std::size_t max_elements>
      constexpr void
      simd_transform_prologue(auto&& unary_op, auto ptr, auto dst_ptr, std::size_t to_process)
      {
        // pre-conditions:
        // * ptr is a valid pointer to a range [ptr, ptr + to_process)
        // * to_process > 0
        // * to_process < max_elements

        static_assert(max_elements > 1);
        static_assert(std::has_single_bit(max_elements));
        static_assert(std::has_single_bit(V::size()));

        if (to_process == 0 or to_process >= max_elements)
          unreachable();

        if constexpr (max_elements == 2) // implies to_process == 1
          {
            const V v(ptr, stdx::vector_aligned);
            const R& result = std::invoke(unary_op, v);
            result.copy_to(dst_ptr, stdx::element_aligned);
            return;
          }
        if (V::size() & to_process)
          {
            const V v(ptr, stdx::vector_aligned);
            const R& result = std::invoke(unary_op, v);
            result.copy_to(dst_ptr, stdx::element_aligned);
            ptr += V::size();
            dst_ptr += V::size();
          }
        if constexpr (V::size() * 2 < max_elements)
          {
            simd_transform_prologue<vir::simdize<typename V::value_type, V::size() * 2>,
                                    vir::simdize<typename R::value_type, V::size() * 2>,
                                    max_elements>(unary_op, ptr, dst_ptr, to_process);
          }
      }

    template <typename V1, typename V2, typename R, std::size_t max_elements>
      constexpr void
      simd_transform_prologue(auto&& binary_op, auto ptr1, auto ptr2, auto dst_ptr,
                              std::size_t to_process)
      {
        static_assert(max_elements > 1);
        static_assert(std::has_single_bit(max_elements));
        constexpr auto size = V1::size();
        static_assert(std::has_single_bit(size));
        static_assert(size == V2::size());
        // pre-conditions:
        // * ptr is a valid pointer to a range [ptr, ptr + to_process)
        // * to_process > 0
        // * to_process < max_elements

        if (to_process == 0 or to_process >= max_elements)
          unreachable();

        if constexpr (max_elements == 2) // implies to_process == 1
          {
            const V1 v1(ptr1, stdx::element_aligned);
            const V2 v2(ptr2, stdx::element_aligned);
            const R& result = std::invoke(binary_op, v1, v2);
            result.copy_to(dst_ptr, stdx::vector_aligned);
            return;
          }
        if (size & to_process)
          {
            const V1 v1(ptr1, stdx::element_aligned);
            const V2 v2(ptr2, stdx::element_aligned);
            const R& result = std::invoke(binary_op, v1, v2);
            result.copy_to(dst_ptr, stdx::vector_aligned);
            ptr1 += size;
            ptr2 += size;
            dst_ptr += size;
          }
        if constexpr (size * 2 < max_elements)
          {
            simd_transform_prologue<vir::simdize<typename V1::value_type, size * 2>,
                                    vir::simdize<typename V2::value_type, size * 2>,
                                    vir::simdize<typename R::value_type, size * 2>,
                                    max_elements>(binary_op, ptr1, ptr2, dst_ptr, to_process);
          }
      }

    template <typename V0, typename R0>
      constexpr void
      simd_transform_epilogue(auto&& unary_op, unsigned leftover, auto last, auto d_first, auto f)
      {
        // pre-conditions:
        // * leftover > 0
        // * leftover < V0::size()
        // * [last - leftover, last) is a valid range
        // * [d_first, d_first + leftover) is a valid range
        static_assert(V0::size() > 1);
        static_assert(V0::size() == R0::size());
        if constexpr (V0::size() == 2) // implies leftover == 1
          {
            using V1 = vir::simdize<typename V0::value_type, 1>;
            using R1 = vir::simdize<typename R0::value_type, 1>;
            const V1 v(std::to_address(last - 1), f);
            const R1& result = std::invoke(unary_op, v);
            result.copy_to(std::to_address(d_first), stdx::element_aligned);
            return;
          }
        using V = vir::simdize<typename V0::value_type, std::bit_ceil(V0::size()) / 2>;
        using R = vir::simdize<typename R0::value_type, V::size()>;
        if (leftover & V::size())
          {
            static_assert(std::has_single_bit(V::size())); // by construction
            const V v(std::to_address(last - leftover), f);
            const R& result = std::invoke(unary_op, v);
            result.copy_to(std::to_address(d_first), stdx::element_aligned);
            leftover -= V::size();
            d_first += V::size();
          }
        if constexpr (V::size() > 1)
          if (leftover)
            simd_transform_epilogue<V, R>(unary_op, leftover, last, d_first, f);
      }

    template <typename V0, typename W0, typename R0>
      constexpr void
      simd_transform_epilogue(auto&& binary_op, unsigned leftover, auto last, auto ptr2,
                              auto d_first, auto f)
      {
        // pre-conditions:
        // * leftover > 0
        // * leftover < V0::size()
        // * [last - leftover, last) is a valid range
        // * [ptr2, ptr2 + leftover) is a valid range
        // * [d_first, d_first + leftover) is a valid range
        constexpr auto size0 = V0::size();
        static_assert(size0 > 1);
        static_assert(size0 == W0::size());
        static_assert(size0 == R0::size());
        if constexpr (size0 == 2) // implies leftover == 1
          {
            using V1 = vir::simdize<typename V0::value_type, 1>;
            using W1 = vir::simdize<typename W0::value_type, 1>;
            using R1 = vir::simdize<typename R0::value_type, 1>;
            const V1 v(std::to_address(last - 1), stdx::element_aligned);
            const W1 w(ptr2, stdx::element_aligned);
            const R1& result = std::invoke(binary_op, v, w);
            result.copy_to(std::to_address(d_first), f);
            return;
          }
        using V = vir::simdize<typename V0::value_type, std::bit_ceil(size0) / 2>;
        using W = vir::simdize<typename W0::value_type, V::size()>;
        using R = vir::simdize<typename R0::value_type, V::size()>;
        if (leftover & V::size())
          {
            static_assert(std::has_single_bit(V::size())); // by construction
            const V v(std::to_address(last - leftover), stdx::element_aligned);
            const W w(ptr2, stdx::element_aligned);
            const R& result = std::invoke(binary_op, v, w);
            result.copy_to(std::to_address(d_first), f);
            leftover -= V::size();
            ptr2 += V::size();
            d_first += V::size();
          }
        if constexpr (V::size() > 1)
          if (leftover)
            simd_transform_epilogue<V, W, R>(binary_op, leftover, last, ptr2, d_first, f);
      }

    template <int size, int max_size, typename A1, typename It1, typename It2>
      constexpr A1
      simd_transform_reduce_prologue(A1 acc1, It1 first1, It2 first2, std::size_t to_process,
                                     auto reduce_op, auto transform_op)
      {
        // preconditions:
        // - to_process > 0
        // - to_process < max_size
        // - [first1, first1 + to_process) is a valid range
        // - [first2, first2 + to_process) is a valid range
        static_assert(std::has_single_bit(unsigned(size)));
        static_assert(std::has_single_bit(unsigned(max_size)));
        if (to_process & size)
          {
            using T1 = std::iter_value_t<It1>;
            using T2 = std::iter_value_t<It2>;
            const std::size_t offset = to_process & (size - 1);
            const vir::simdize<T1, size> v1(std::to_address(first1 + offset), stdx::vector_aligned);
            const vir::simdize<T2, size> v2(std::to_address(first2 + offset), stdx::element_aligned);
            const A1 x = reduce(std::invoke(transform_op, v1, v2), reduce_op);
            acc1 = std::invoke(reduce_op, acc1, x);
          }
        if constexpr (size < max_size)
          return simd_transform_reduce_prologue<size * 2, max_size>(
                   acc1, first1, first2, to_process, reduce_op, transform_op);
        else
          return acc1;
      }

    template <typename A1, typename A, typename It1, typename It2>
      VIR_ALWAYS_INLINE
      constexpr typename A1::value_type
      simd_transform_reduce_epilogue(A1 acc1, A acc, It1 first1, It2 first2, std::size_t leftover,
                                     auto reduce_op, auto transform_op, auto flags1)
      {
        // preconditions:
        // leftover > 0
        // leftover & (A::size() * 2 - 1) != 0 (i.e. at least one more load+transform is necessary)
        static_assert(A1::size() == 1);
        static_assert(std::has_single_bit(A::size()));
        if (leftover & A::size())
          {
            using T1 = std::iter_value_t<It1>;
            using T2 = std::iter_value_t<It2>;
            using V1 = vir::simdize<T1, A::size()>;
            using V2 = vir::simdize<T2, A::size()>;
            const V1 v1(std::to_address(first1), flags1);
            const V2 v2(std::to_address(first2), stdx::element_aligned);
            acc = std::invoke(reduce_op, acc, std::invoke(transform_op, v1, v2));
            if constexpr (A::size() == 1)
              // definetely no more work
              return std::invoke(reduce_op, acc1, acc)[0];
            else if ((leftover & (A::size() - 1)) == 0)
              // no more work
              return std::invoke(reduce_op, acc1, A1(reduce(acc, reduce_op)))[0];
            else
              {
                constexpr std::size_t size2 = A::size() / 2;
                using A2 = vir::simdize<typename A::value_type, size2>;
                auto [lo, hi] = stdx::split<size2, size2>(acc);
                A2 acc2 = std::invoke(reduce_op, lo, hi);
                return simd_transform_reduce_epilogue(acc1, acc2, first1 + A2::size(),
                                                      first2 + A2::size(), leftover, reduce_op,
                                                      transform_op, flags1);
              }
          }
        else if ((leftover & (A::size() - 1)) == 0)
          unreachable(); // because we're only called if there is more work to be done
        else if constexpr (A::size() > 1)
          {
            constexpr std::size_t size2 = A::size() / 2;
            using A2 = vir::simdize<typename A::value_type, size2>;
            auto [lo, hi] = stdx::split<size2, size2>(acc);
            A2 acc2 = std::invoke(reduce_op, lo, hi);
            return simd_transform_reduce_epilogue(acc1, acc2, first1, first2, leftover, reduce_op,
                                                  transform_op, flags1);
          }
        else
          unreachable();
      }

    template <std::size_t size, typename A1, typename It1, typename It2>
      VIR_ALWAYS_INLINE
      constexpr typename A1::value_type
      simd_transform_reduce_epilogue(A1 acc1, It1 first1, It2 first2, std::size_t leftover,
                                     auto reduce_op, auto transform_op, auto flags1)
      {
        // preconditions:
        // leftover > 0
        // leftover & (size * 2 - 1) != 0 (i.e. at least one more load+transform is necessary)
        static_assert(A1::size() == 1);
        static_assert(std::has_single_bit(size));
        using A = vir::simdize<typename A1::value_type, size>;
        if (leftover & size)
          {
            using T1 = std::iter_value_t<It1>;
            using T2 = std::iter_value_t<It2>;
            using V1 = vir::simdize<T1, size>;
            using V2 = vir::simdize<T2, size>;
            const V1 v1(std::to_address(first1), flags1);
            const V2 v2(std::to_address(first2), stdx::element_aligned);
            A acc = std::invoke(transform_op, v1, v2);
            if constexpr (size == 1)
              // definetely no more work
              return std::invoke(reduce_op, acc1, acc)[0];
            else if ((leftover & (size - 1)) == 0)
              // no more work
              return std::invoke(reduce_op, acc1, A1(reduce(acc, reduce_op)))[0];
            else
              {
                constexpr std::size_t size2 = size / 2;
                using A2 = vir::simdize<typename A::value_type, size2>;
                auto [lo, hi] = stdx::split<size2, size2>(acc);
                A2 acc2 = std::invoke(reduce_op, lo, hi);
                return simd_transform_reduce_epilogue(acc1, acc2, first1 + A2::size(),
                                                      first2 + A2::size(), leftover, reduce_op,
                                                      transform_op, flags1);
              }
          }
        else if ((leftover & (A::size() - 1)) == 0)
          unreachable(); // because we're only called if there is more work to be done
        else if constexpr (size > 1)
          {
            constexpr std::size_t size2 = A::size() / 2;
            return simd_transform_reduce_epilogue<size2>(acc1, first1, first2, leftover, reduce_op,
                                                         transform_op, flags1);
          }
        else
          unreachable();
      }

    struct simd_policy_prefer_aligned_t {};

    inline constexpr simd_policy_prefer_aligned_t simd_policy_prefer_aligned{};

    struct simd_policy_auto_prologue_t {};

    inline constexpr simd_policy_auto_prologue_t simd_policy_auto_prologue{};

    template <int N>
      struct simd_policy_unroll_by_t
      {};

    template <int N>
      inline constexpr simd_policy_unroll_by_t<N> simd_policy_unroll_by{};

    template <typename T>
      struct simd_policy_unroll_value
      : std::integral_constant<int, 0>
      {};

    template <int N>
      struct simd_policy_unroll_value<const simd_policy_unroll_by_t<N>>
      : std::integral_constant<int, N>
      {};

    template <int N>
      struct simd_policy_size_t
      {};

    template <int N>
      inline constexpr simd_policy_size_t<N> simd_policy_size{};

    template <typename T>
      struct simd_policy_size_value
      : std::integral_constant<int, 0>
      {};

    template <int N>
      struct simd_policy_size_value<const simd_policy_size_t<N>>
      : std::integral_constant<int, N>
      {};

    template <typename T>
      struct is_simd_policy
      : std::false_type
      {};

    template <typename T>
      concept simd_execution_policy = is_simd_policy<std::remove_cvref_t<T>>::value;

    template <typename Rng>
      concept simd_execution_range = std::ranges::contiguous_range<Rng> and requires {
        typename vir::simdize<std::ranges::range_value_t<Rng>>;
      };

    template <typename It>
      concept simd_execution_iterator = std::contiguous_iterator<It> and requires {
        typename vir::simdize<std::iter_value_t<It>>;
      };

  } // namespace detail

  namespace execution
  {
    template <auto... Options>
      struct simd_policy
      {
        static constexpr bool _prefers_aligned
          = (false or ... or std::same_as<decltype(Options),
                                          const detail::simd_policy_prefer_aligned_t>);

        static constexpr bool _auto_prologue
          = (false or ... or std::same_as<decltype(Options),
                                          const detail::simd_policy_auto_prologue_t>);

        static constexpr int _unroll_by
          = (0 + ... + detail::simd_policy_unroll_value<decltype(Options)>::value);

        static constexpr int _size
          = (0 + ... + detail::simd_policy_size_value<decltype(Options)>::value);

        static constexpr simd_policy<Options..., detail::simd_policy_prefer_aligned>
        prefer_aligned() requires(not _prefers_aligned and not _auto_prologue)
        { return {}; }

        static constexpr simd_policy<Options..., detail::simd_policy_auto_prologue>
        auto_prologue() requires(not _prefers_aligned and not _auto_prologue)
        { return {}; }

        template <int N>
          static constexpr simd_policy<Options..., detail::simd_policy_unroll_by<N>>
          unroll_by() requires(_unroll_by == 0)
          {
            static_assert(N > 1);
            return {};
          }

        template <int N>
          static constexpr simd_policy<Options..., detail::simd_policy_size<N>>
          prefer_size() requires(_size == 0)
          {
            static_assert(N > 0);
            return {};
          };
      };

    inline constexpr simd_policy<> simd {};
  } // namespace execution

  namespace detail
  {
    template <auto... Options>
      struct is_simd_policy<execution::simd_policy<Options...>>
      : std::true_type
      {};

    template <typename V, typename T = typename V::value_type>
      inline constexpr std::size_t memory_alignment_v = [] {
        if constexpr (stdx::is_simd_v<V>)
          return stdx::memory_alignment_v<V, T>;
        else if constexpr (requires {
          {V::template memory_alignment<T>} -> std::same_as<std::size_t>;
        })
          return V::template memory_alignment<T>;
        else
          return alignof(T);
      }();

    template <typename V, simd_execution_policy ExecutionPolicy>
      struct prologue
      {
        using T = typename V::value_type;

        static constexpr std::size_t size = V::size();

        static constexpr auto bytes_per_iteration = size * sizeof(T);

        static constexpr bool prologue_is_possible
          = size > 1 and bytes_per_iteration % memory_alignment_v<V> == 0
              and memory_alignment_v<V> > sizeof(T);

        static constexpr bool use_aligned_loadstore
          = ExecutionPolicy::_prefers_aligned and prologue_is_possible;

        static constexpr bool maybe_execute_prologue_anyway
          = ExecutionPolicy::_auto_prologue and prologue_is_possible;

        static constexpr std::conditional_t<use_aligned_loadstore, stdx::vector_aligned_tag,
                                            stdx::element_aligned_tag> flags{};

        constexpr void
        operator()(std::size_t& remaining, auto iterator_to_align, auto&& do_prologue,
                   auto&... iterators_to_advance) const
        {
          static_assert(std::same_as<T, std::iter_value_t<decltype(iterator_to_align)>>);

          if constexpr (use_aligned_loadstore or maybe_execute_prologue_anyway)
            {
              const auto misaligned_by
                = reinterpret_cast<std::uintptr_t>(std::to_address(iterator_to_align))
                    % memory_alignment_v<V>;
              if (misaligned_by == 0)
                return;
              const auto to_process = (memory_alignment_v<V> - misaligned_by) / sizeof(T);
              if constexpr (use_aligned_loadstore)
                do_prologue(to_process);
              else if (remaining * sizeof(T) > 4000
                         or (to_process & remaining) == to_process) // prologue replaces epilogue
                do_prologue(to_process);
              else
                return;
              ((iterators_to_advance += to_process), ...);
              remaining -= to_process;
            }
        }
      };
  }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_iterator It,
            typename F>
    constexpr void
    for_each(ExecutionPolicy, It first, It last, F&& fun)
    {
      using T = std::iter_value_t<It>;
      using V = vir::simdize<T, ExecutionPolicy::_size>;
      constexpr int size = V::size();
      constexpr bool write_back = std::indirectly_writable<It, T>
                                    and std::invocable<F, V&> and not std::invocable<F, V&&>;
      if (first == last)
        return;
      std::size_t distance = std::distance(first, last);
      if (std::is_constant_evaluated())
        {
          // needs element_aligned because of GCC PR111302
          for (; first + size <= last; first += size)
            detail::simd_load_and_invoke<V, write_back>(fun, std::to_address(first),
                                                        stdx::element_aligned, detail::no_unroll);

          if constexpr (size > 1)
            detail::simd_for_each_epilogue<V, write_back>(fun, distance % size, last,
                                                          stdx::element_aligned);
        }
      else
        {
          constexpr detail::prologue<V, ExecutionPolicy> prologue;
          constexpr auto flags = prologue.flags;
          prologue(distance, first, [&] (auto to_process) {
            detail::simd_for_each_prologue<stdx::resize_simd_t<1, V>, write_back,
                                           detail::memory_alignment_v<V> / sizeof(T)>(
              fun, std::to_address(first), to_process);
          }, first);
          const auto leftover = distance % size;

          if constexpr (ExecutionPolicy::_unroll_by > 1)
            {
              constexpr auto step = size * ExecutionPolicy::_unroll_by;
              const auto unrolled_last = last - step;
              for (; first <= unrolled_last; first += step)
                {
                  detail::simd_load_and_invoke<V, write_back>(
                    fun, std::to_address(first), flags,
                    std::make_index_sequence<ExecutionPolicy::_unroll_by>());
                }
            }

          const auto simd_last = last - size;
          for (; first <= simd_last; first += size)
            detail::simd_load_and_invoke<V, write_back>(fun, std::to_address(first), flags,
                                                        detail::no_unroll);

          if constexpr (size > 1)
            if (leftover)
              detail::simd_for_each_epilogue<V, write_back>(fun, leftover, last, flags);
        }
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_range R,
            typename F>
    constexpr void
    for_each(ExecutionPolicy&& pol, R&& rng, F&& fun)
    { vir::for_each(pol, std::ranges::begin(rng), std::ranges::end(rng), std::forward<F>(fun)); }

  template<detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_iterator It1,
           detail::simd_execution_iterator OutIt, typename UnaryOperation>
    constexpr OutIt
    transform(ExecutionPolicy, It1 first1, It1 last1, OutIt d_first, UnaryOperation unary_op)
    {
      using T = std::iter_value_t<It1>;
      using V = vir::simdize<T, ExecutionPolicy::_size>;
      using R = vir::simdize<std::iter_value_t<OutIt>, V::size()>;
      using V1 = vir::simdize<T, 1>;
      using R1 = vir::simdize<std::iter_value_t<OutIt>, 1>;
      constexpr int size = V::size();
      if (first1 == last1)
        return d_first;
      std::size_t distance = std::distance(first1, last1);
      if (std::is_constant_evaluated())
        {
          // needs element_aligned because of GCC PR111302
          for (; first1 + size <= last1; first1 += size, d_first += size)
            {
              const V v(std::to_address(first1), stdx::element_aligned);
              const R& result = std::invoke(unary_op, v);
              result.copy_to(std::to_address(d_first), stdx::element_aligned);
            }

          for (; first1 < last1; first1 += 1, d_first += 1)
            {
              const V1 v(std::to_address(first1), stdx::element_aligned);
              const R1& result = std::invoke(unary_op, v);
              result.copy_to(std::to_address(d_first), stdx::element_aligned);
            }
        }
      else
        {
          constexpr detail::prologue<V, ExecutionPolicy> prologue;
          constexpr auto flags = prologue.flags;
          prologue(distance, first1, [&](auto to_process) {
            detail::simd_transform_prologue<V1, R1,
                                            detail::memory_alignment_v<V> / sizeof(T)>(
              unary_op, std::to_address(first1), std::to_address(d_first), to_process);
          }, first1, d_first);
          const auto leftover = distance % size;

          if constexpr (ExecutionPolicy::_unroll_by > 1)
            {
              constexpr auto step = size * ExecutionPolicy::_unroll_by;
              const auto unrolled_last = last1 - step;
              for (; first1 <= unrolled_last; first1 += step, d_first += step)
                {
                  detail::simd_load_and_invoke<V>(
                    unary_op, std::to_address(first1), [&](const R& result, auto offset) {
                      result.copy_to(std::to_address(d_first + offset.value),
                                     stdx::element_aligned);
                    }, flags, std::make_index_sequence<ExecutionPolicy::_unroll_by>());
                }
            }

          const auto simd_last = last1 - size;
          for (; first1 <= simd_last; first1 += size, d_first += size)
            {
              const V v(std::to_address(first1), flags);
              const R& result = std::invoke(unary_op, v);
              result.copy_to(std::to_address(d_first), stdx::element_aligned);
            }

          if constexpr (size > 1)
            if (leftover)
              {
                detail::simd_transform_epilogue<V, R>(unary_op, leftover, last1, d_first, flags);
                d_first += leftover;
              }
        }
      return d_first;
    }

  template<detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_iterator It1,
           detail::simd_execution_iterator It2, detail::simd_execution_iterator OutIt,
           typename BinaryOperation>
    constexpr OutIt
    transform(ExecutionPolicy, It1 first1, It1 last1, It2 first2, OutIt d_first,
              BinaryOperation binary_op)
    {
      using T1 = std::iter_value_t<It1>;
      using T2 = std::iter_value_t<It2>;
      using T3 = std::iter_value_t<OutIt>;
      constexpr int size = [] {
        if constexpr (ExecutionPolicy::_size > 0)
          return ExecutionPolicy::_size;
        else
          {
            constexpr int size1 = vir::simdize<T1>::size();
            constexpr int size2 = vir::simdize<T2>::size();
            return std::max(size1, size2);
          }
      }();
      using V1 = vir::simdize<T1, size>;
      using V2 = vir::simdize<T2, size>;
      using V3 = vir::simdize<T3, size>;

      if (first1 == last1)
        return d_first;

      std::size_t distance = std::distance(first1, last1);
      if (std::is_constant_evaluated())
        {
          // needs element_aligned because of GCC PR111302
          for (; first1 + size <= last1; first1 += size, first2 += size, d_first += size)
            {
              const V1 v1(std::to_address(first1), stdx::element_aligned);
              const V2 v2(std::to_address(first2), stdx::element_aligned);
              const V3& result = std::invoke(binary_op, v1, v2);
              result.copy_to(std::to_address(d_first), stdx::element_aligned);
            }

          for (; first1 < last1; first1 += 1, first2 += 1, d_first += 1)
            {
              const vir::simdize<T1, 1> v1(std::to_address(first1), stdx::element_aligned);
              const vir::simdize<T2, 1> v2(std::to_address(first2), stdx::element_aligned);
              const vir::simdize<T3, 1>& result = std::invoke(binary_op, v1, v2);
              result.copy_to(std::to_address(d_first), stdx::element_aligned);
            }
        }
      else
        {
          // There are three addresses and we can only ensure alignment on one of them. In
          // principle, we could check at runtime whether two of three can be aligned. But the
          // branching necessary to make this work - with relatively little gain (presumably) -
          // renders this approach not interesting.
          // Instead, let's simply focus on aligning the store address, avoiding pressure on the
          // store execution ports (a factor of 2 throughput difference with AVX-512 vectors).
          constexpr detail::prologue<V3, ExecutionPolicy> prologue;
          constexpr auto flags = prologue.flags;
          prologue(distance, d_first, [&] (auto to_process) {
            detail::simd_transform_prologue<
              V1, V2, V3, detail::memory_alignment_v<V3> / sizeof(T3)>(
              binary_op, std::to_address(first1), std::to_address(first2), std::to_address(d_first),
              to_process);
          }, first1, first2, d_first);
          const auto leftover = distance % size;

          if constexpr (ExecutionPolicy::_unroll_by > 1)
            {
              constexpr std::make_index_sequence<ExecutionPolicy::_unroll_by> unroll_idx_seq;
              constexpr auto step = size * ExecutionPolicy::_unroll_by;
              const auto unrolled_last = last1 - step;
              for (; first1 <= unrolled_last; first1 += step, first2 += step, d_first += step)
                {
                  detail::simd_load_and_invoke<V1, V2>(
                    binary_op, std::to_address(first1), std::to_address(first2),
                    [&](const V3& result, auto offset) {
                      result.copy_to(std::to_address(d_first + offset.value), flags);
                    }, stdx::element_aligned, unroll_idx_seq);
                }
            }

          const auto simd_last = last1 - size;
          for (; first1 <= simd_last; first1 += size, first2 += size, d_first += size)
            {
              const V1 v1(std::to_address(first1), stdx::element_aligned);
              const V2 v2(std::to_address(first2), stdx::element_aligned);
              const V3& result = std::invoke(binary_op, v1, v2);
              result.copy_to(std::to_address(d_first), flags);
            }

          if constexpr (size > 1)
            if (leftover)
              {
                detail::simd_transform_epilogue<V1, V2, V3>(
                  binary_op, leftover, last1, std::to_address(first2), d_first, flags);
                d_first += leftover;
              }
        }
      return d_first;
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_range R1,
            detail::simd_execution_range R2, typename UnaryOperation>
    constexpr void
    transform(ExecutionPolicy pol, R1&& rng1, R2& d_rng, UnaryOperation unary_op)
    {
      vir::transform(pol, std::ranges::begin(rng1), std::ranges::end(rng1),
                     std::ranges::begin(d_rng), unary_op);
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_range R1,
            detail::simd_execution_range R2, detail::simd_execution_range R3,
            typename BinaryOperation>
    constexpr void
    transform(ExecutionPolicy pol, R1&& rng1, R2&& rng2, R3& d_rng, BinaryOperation binary_op)
    {
      vir::transform(pol, std::ranges::begin(rng1), std::ranges::end(rng1),
                     std::ranges::begin(rng2), std::ranges::begin(d_rng), binary_op);
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_iterator It1,
            detail::simd_execution_iterator It2, typename T>
    constexpr T
    transform_reduce(ExecutionPolicy&& policy, It1 first1, It1 last1, It2 first2, T init)
    {
      return transform_reduce(policy, first1, last1, first2, init, std::plus<>(),
                              std::multiplies<>());
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_iterator It1,
            detail::simd_execution_iterator It2, typename T, typename BinaryReductionOp,
            typename BinaryTransformOp>
    constexpr T
    transform_reduce(ExecutionPolicy, It1 first1, It1 last1, It2 first2, T init,
                 BinaryReductionOp reduce_op, BinaryTransformOp transform_op)
    {
      using T1 = std::iter_value_t<It1>;
      using T2 = std::iter_value_t<It2>;
      constexpr int size = [] {
        if constexpr (ExecutionPolicy::_size > 0)
          return ExecutionPolicy::_size;
        else
          {
            constexpr int size1 = vir::simdize<T1>::size();
            constexpr int size2 = vir::simdize<T2>::size();
            return std::max(size1, size2);
          }
      }();
      using A1 = vir::simdize<T, 1>;
      using A = vir::simdize<T, size>;
      using V1 = vir::simdize<T1, size>;
      using V2 = vir::simdize<T2, size>;

      if (first1 == last1)
        return init;

      A1 acc1 = init;

      std::size_t distance = std::distance(first1, last1);
      if (std::is_constant_evaluated())
        {
          // needs element_aligned because of GCC PR111302
          for (; first1 < last1; first1 += 1, first2 += 1)
            {
              const vir::simdize<T1, 1> v1(std::to_address(first1), stdx::element_aligned);
              const vir::simdize<T2, 1> v2(std::to_address(first2), stdx::element_aligned);
              const A1 r = std::invoke(transform_op, v1, v2);
              acc1 = std::invoke(reduce_op, acc1, r);
            }
          return acc1[0];
        }
      else
        {
          constexpr detail::prologue<V1, ExecutionPolicy> prologue;
          constexpr auto flags = prologue.flags;
          prologue(distance, first1, [&] (auto to_process) VIR_LAMBDA_ALWAYS_INLINE {
            acc1 = detail::simd_transform_reduce_prologue<1, detail::memory_alignment_v<V1>
                                                            / sizeof(T1)>(
                     acc1, first1, first2, to_process, reduce_op, transform_op);
          }, first1, first2);

          const auto leftover = distance % size;

          constexpr auto lo_size = std::bit_ceil(size / 2u);
          constexpr auto hi_size = size - lo_size;
          using A2 = vir::simdize<T, lo_size>;

          const auto simd_last = last1 - size;
          if (first1 <= simd_last)
            {
              A acc = [&] VIR_LAMBDA_ALWAYS_INLINE {
                if constexpr (ExecutionPolicy::_unroll_by > 1)
                  {
                    constexpr std::make_index_sequence<ExecutionPolicy::_unroll_by> unroll_idx_seq;
                    constexpr auto step = size * ExecutionPolicy::_unroll_by;
                    const auto unrolled_last = last1 - step;
                    if (first1 + step <= unrolled_last)
                      {
                        auto acc = [&]<std::size_t... Is>(std::index_sequence<Is...>)
                                     -> std::array<A, ExecutionPolicy::_unroll_by>
                                   VIR_LAMBDA_ALWAYS_INLINE
                        {
                          return {std::invoke(transform_op,
                                              V1(std::to_address(first1 + Is * size), flags),
                                              V2(std::to_address(first2 + Is * size),
                                                 stdx::element_aligned))...};
                        }(unroll_idx_seq);
                        first1 += step;
                        first2 += step;
                        do {
                          [&]<std::size_t... Is>(std::index_sequence<Is...>)
                            VIR_LAMBDA_ALWAYS_INLINE
                          {
                            acc = {std::invoke(reduce_op, acc[Is],
                                               std::invoke(transform_op,
                                                           V1(std::to_address(first1 + Is * size),
                                                              flags),
                                                           V2(std::to_address(first2 + Is * size),
                                                              stdx::element_aligned)))...};
                          }(unroll_idx_seq);
                          first1 += step;
                          first2 += step;
                        } while (first1 <= unrolled_last);
                        // tree reduction of acc into acc[0]
                        detail::unroll<std::bit_width(unsigned(ExecutionPolicy::_unroll_by - 1))>(
                          [&](auto outer) {
                            constexpr int j = 1 << outer.value; // j: 1 2 4 8 16 32 ...
                            detail::unroll<ExecutionPolicy::_unroll_by / 2>(
                              [&](auto ii) {
                                constexpr int i = ii * 2 * j;
                                if constexpr (i + j < ExecutionPolicy::_unroll_by)
                                  acc[i] = std::invoke(reduce_op, acc[i], acc[i+j]);
                              });
                          });
                        return acc[0];
                      }
                  }
                const V1 v1(std::to_address(first1), flags);
                const V2 v2(std::to_address(first2), stdx::element_aligned);
                first1 += size;
                first2 += size;
                return std::invoke(transform_op, v1, v2);
              }();
              for (; first1 <= simd_last; first1 += size, first2 += size)
                {
                  const V1 v1(std::to_address(first1), flags);
                  const V2 v2(std::to_address(first2), stdx::element_aligned);
                  acc = std::invoke(reduce_op, acc, std::invoke(transform_op, v1, v2));
                }
              if constexpr (size == 1)
                return std::invoke(reduce_op, acc1, acc)[0];
              else if (leftover == 0)
                return std::invoke(reduce_op, acc1, A1(reduce(acc, reduce_op)))[0];
              else
                {
                  auto [lo, hi] = stdx::split<lo_size, hi_size>(acc);
                  A2 acc2 = lo;
                  if constexpr (lo_size == hi_size)
                    acc2 = std::invoke(reduce_op, lo, hi);
                  else
                    acc1 = std::invoke(reduce_op, acc1, A1(reduce(hi, reduce_op)));
                  return detail::simd_transform_reduce_epilogue(acc1, acc2, first1, first2,
                                                                leftover, reduce_op, transform_op,
                                                                flags);
                }
            }
          else
            {
              // leftover > 0 by construction
              return detail::simd_transform_reduce_epilogue<lo_size>(
                       acc1, first1, first2, leftover, reduce_op, transform_op, flags);
            }
        }
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_iterator It,
            typename T, typename BinaryReductionOp, typename UnaryTransformOp>
    constexpr T
    transform_reduce(ExecutionPolicy policy, It first, It last, T init,
                     BinaryReductionOp reduce_op, UnaryTransformOp transform_op)
    { return init; } // TODO

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_range Rng1,
            detail::simd_execution_range Rng2, typename T>
    constexpr T
    transform_reduce(ExecutionPolicy&& policy, Rng1&& r1, Rng2&& r2, T init)
    {
      return transform_reduce(policy, std::ranges::begin(r1), std::ranges::end(r1),
                              std::ranges::begin(r2), init);
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_range Rng1,
            detail::simd_execution_range Rng2, typename T, typename BinaryReductionOp,
            typename BinaryTransformOp>
    constexpr T
    transform_reduce(ExecutionPolicy&& policy, Rng1&& r1, Rng2&& r2, T init,
                 BinaryReductionOp reduce, BinaryTransformOp transform)
    {
      return transform_reduce(policy, std::ranges::begin(r1), std::ranges::end(r1),
                              std::ranges::begin(r2), init, reduce, transform);
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_range Rng,
            typename T, typename BinaryReductionOp, typename UnaryTransformOp>
    constexpr T
    transform_reduce(ExecutionPolicy&& policy, Rng r, T init, BinaryReductionOp reduce,
                     UnaryTransformOp transform)
    {
      return transform_reduce(policy, std::ranges::begin(r), std::ranges::end(r), init, reduce,
                              transform);
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_iterator It,
            typename F>
    constexpr int
    count_if(ExecutionPolicy pol, It first, It last, F&& fun)
    {
      using T = std::iter_value_t<It>;
      using TV = vir::simdize<T, ExecutionPolicy::_size>;
      using IV = detail::deduced_simd<int, TV::size()>;
      int count = 0;
      IV countv = 0;
      vir::for_each(pol, first, last, [&](auto... x) VIR_LAMBDA_ALWAYS_INLINE {
#if __cpp_lib_experimental_parallel_simd >= 201803
        if (std::is_constant_evaluated())
          count += (popcount(fun(x)) + ...);
        else
#endif
          if constexpr (sizeof...(x) == 1)
          {
            if constexpr ((x.size(), ...) == countv.size())
              ++where(vir::cvt(fun(x...)), countv);
            else
              count += popcount(fun(x...));
          }
        else
          {
            ((++where(vir::cvt(fun(x)), countv)), ...);
          }
      });
      return count + reduce(countv);
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_range R,
            typename F>
    constexpr int
    count_if(ExecutionPolicy&& pol, R&& v, F&& fun)
    { return vir::count_if(pol, std::ranges::begin(v), std::ranges::end(v), std::forward<F>(fun)); }
}  // namespace vir

namespace std
{
  template <vir::detail::simd_execution_policy ExecutionPolicy,
            vir::detail::simd_execution_iterator It, typename F>
    constexpr void
    for_each(ExecutionPolicy&& pol, It first, It last, F&& fun)
    { vir::for_each(pol, first, last, std::forward<F>(fun)); }

  template<vir::detail::simd_execution_policy ExecutionPolicy,
           vir::detail::simd_execution_iterator It1, vir::detail::simd_execution_iterator OutIt,
           typename UnaryOperation>
    constexpr OutIt
    transform(ExecutionPolicy pol, It1 first1, It1 last1, OutIt d_first,
              UnaryOperation unary_op)
    { return vir::transform(pol, first1, last1, d_first, unary_op); }

  template<vir::detail::simd_execution_policy ExecutionPolicy,
           vir::detail::simd_execution_iterator It1, vir::detail::simd_execution_iterator It2,
           vir::detail::simd_execution_iterator OutIt, typename BinaryOperation>
    constexpr OutIt
    transform(ExecutionPolicy pol, It1 first1, It1 last1, It2 first2, OutIt d_first,
              BinaryOperation binary_op)
    { return vir::transform(pol, first1, last1, first2, d_first, binary_op); }

  template <vir::detail::simd_execution_policy ExecutionPolicy,
           vir::detail::simd_execution_iterator It1, vir::detail::simd_execution_iterator It2,
           typename T>
    constexpr T
    transform_reduce(ExecutionPolicy&& policy, It1 first1, It1 last1, It2 first2, T init)
    { return vir::transform_reduce(policy, first1, last1, first2, init); }

  template <vir::detail::simd_execution_policy ExecutionPolicy,
            vir::detail::simd_execution_iterator It1, vir::detail::simd_execution_iterator It2,
            typename T, typename BinaryReductionOp, typename BinaryTransformOp>
    constexpr T
    transform_reduce(ExecutionPolicy&& policy, It1 first1, It1 last1, It2 first2, T init,
                 BinaryReductionOp reduce, BinaryTransformOp transform)
    { return vir::transform_reduce(policy, first1, last1, first2, init, reduce, transform); }

  template <vir::detail::simd_execution_policy ExecutionPolicy,
            vir::detail::simd_execution_iterator It, typename T, typename BinaryReductionOp,
            typename UnaryTransformOp>
    constexpr T
    transform_reduce(ExecutionPolicy&& policy, It first, It last, T init, BinaryReductionOp reduce,
                     UnaryTransformOp transform)
    { return vir::transform_reduce(policy, first, last, init, reduce, transform); }

  template <vir::detail::simd_execution_policy ExecutionPolicy,
            vir::detail::simd_execution_iterator It, typename F>
    constexpr int
    count_if(ExecutionPolicy&& pol, It first, It last, F&& fun)
    { return vir::count_if(pol, first, last, std::forward<F>(fun)); }
}
#endif // VIR_HAVE_SIMD_CONCEPTS
#endif // VIR_SIMD_EXECUTION_H_

// vim: et cc=101 tw=100 sw=2 ts=8
