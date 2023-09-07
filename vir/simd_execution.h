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
    [[noreturn]] void
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
    template <typename V, bool write_back, typename Flags, std::size_t... Is>
      constexpr void
      simd_load_and_invoke(auto&& fun, auto ptr, Flags f, std::index_sequence<Is...>)
      {
        [&](auto... chunks) {
          std::invoke(fun, chunks...);
          if constexpr (write_back)
            (chunks.copy_to(ptr + (V::size() * Is), f), ...);
        }(std::conditional_t<write_back, V, const V>(ptr + (V::size() * Is), f)...);
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
        // pre-conditions:
        // * ptr is a valid pointer to a range [ptr, ptr + to_process)
        // * to_process > 0
        // * to_process < max_elements

        if (to_process == 0 or to_process >= max_elements)
          unreachable();

        if constexpr (max_elements == 2)
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
            simd_for_each_prologue<stdx::resize_simd_t<V::size() * 2, V>, write_back, max_elements>(
              fun, ptr, to_process);
          }
      }

    template <class V0, bool write_back>
      void
      simd_for_each_jumptable_epilogue(auto&& fun, unsigned leftover, auto last, auto f)
      {
        auto ptr = std::to_address(last - leftover);
        switch (leftover)
        {
          case 0:
case0:
            return;

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
      }

    template <class V0, bool write_back>
      constexpr void
      simd_for_each_epilogue(auto&& fun, unsigned leftover, auto last, auto f)
      {
        // pre-conditions:
        // * leftover < V0::size()
        // * [last - leftover, last) is a valid range
#if !__OPTIMIZE_SIZE__
        if constexpr (V0::size() <= 64
                        and V0::size() <= vir::stdx::native_simd<typename V0::value_type>::size())
          {
            if (not std::is_constant_evaluated())
              return simd_for_each_jumptable_epilogue<V0, write_back>(fun, leftover, last, f);
          }
#endif // !__OPTIMIZE_SIZE__
        using V = stdx::resize_simd_t<std::bit_ceil(V0::size()) / 2, V0>;
        if (leftover >= V::size())
          {
            simd_load_and_invoke<V, write_back>(fun, std::to_address(last - leftover), f,
                                                no_unroll);
            leftover -= V::size();
          }
        if constexpr (V::size() > 1)
          simd_for_each_epilogue<V, write_back>(fun, leftover, last, f);
      }

    struct simd_policy_prefer_aligned_t {};

    inline constexpr simd_policy_prefer_aligned_t simd_policy_prefer_aligned{};

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

        static constexpr int _unroll_by
          = (0 + ... + detail::simd_policy_unroll_value<decltype(Options)>::value);

        static constexpr int _size
          = (0 + ... + detail::simd_policy_size_value<decltype(Options)>::value);

        static constexpr simd_policy<Options..., detail::simd_policy_prefer_aligned>
        prefer_aligned() requires(not _prefers_aligned)
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
      auto distance = std::distance(first, last);
      if (distance <= 0)
        return;
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
          constexpr auto bytes_per_iteration = size * sizeof(T);
          constexpr bool use_aligned_loadstore
            = ExecutionPolicy::_prefers_aligned and size > 1
                and bytes_per_iteration % stdx::memory_alignment_v<V> == 0;
          constexpr std::conditional_t<use_aligned_loadstore, stdx::vector_aligned_tag,
                                       stdx::element_aligned_tag> flags{};
          if constexpr (use_aligned_loadstore)
            {
              const auto misaligned_by = reinterpret_cast<std::uintptr_t>(std::to_address(first))
                                           % stdx::memory_alignment_v<V>;
              if (misaligned_by != 0)
                {
                  const auto to_process = (stdx::memory_alignment_v<V> - misaligned_by) / sizeof(T);
                  detail::simd_for_each_prologue<stdx::resize_simd_t<1, V>, write_back,
                                                 stdx::memory_alignment_v<V> / sizeof(T)>(
                    fun, std::to_address(first), to_process);
                  first += to_process;
                  distance -= to_process;
                }
            }

          if constexpr (ExecutionPolicy::_unroll_by > 1)
            {
              constexpr auto step = size * ExecutionPolicy::_unroll_by;
              for (; first + step <= last; first += step)
                {
                  detail::simd_load_and_invoke<V, write_back>(
                    fun, std::to_address(first), flags,
                    std::make_index_sequence<ExecutionPolicy::_unroll_by>());
                }
            }

          for (; first + size <= last; first += size)
            detail::simd_load_and_invoke<V, write_back>(fun, std::to_address(first), flags,
                                                        detail::no_unroll);

          if constexpr (size > 1)
            detail::simd_for_each_epilogue<V, write_back>(fun, distance % size, last, flags);
        }
    }

  template <detail::simd_execution_policy ExecutionPolicy, detail::simd_execution_range R,
            typename F>
    constexpr void
    for_each(ExecutionPolicy&& pol, R&& rng, F&& fun)
    { vir::for_each(pol, std::ranges::begin(rng), std::ranges::end(rng), std::forward<F>(fun)); }

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
      vir::for_each(pol, first, last, [&](auto... x) {
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

  template <vir::detail::simd_execution_policy ExecutionPolicy,
            vir::detail::simd_execution_iterator It, typename F>
    constexpr int
    count_if(ExecutionPolicy&& pol, It first, It last, F&& fun)
    { return vir::count_if(pol, first, last, std::forward<F>(fun)); }
}
#endif // VIR_HAVE_SIMD_CONCEPTS
#endif // VIR_SIMD_EXECUTION_H_

// vim: et cc=101 tw=100 sw=2 ts=8
