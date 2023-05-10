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

namespace vir
{
  namespace detail
  {
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

    // Invokes fun(V&) or fun(const V&) with V copied from r at offset i.
    // If write_back is true, copy it back to r.
    template <typename V, bool write_back, typename Flags, std::size_t... Is>
      constexpr void
      simd_load_and_invoke(auto&& fun, auto&& r, std::size_t i, Flags f, std::index_sequence<Is...>)
      {
        [&](auto... chunks) {
          std::invoke(fun, chunks...);
          if constexpr (write_back)
            (chunks.copy_to(data_or_ptr(r) + i + (V::size() * Is), f), ...);
        }(std::conditional_t<write_back, V, const V>(data_or_ptr(r) + i + (V::size() * Is), f)...);
      }

    template <class V, bool write_back, std::size_t max_bytes>
      constexpr void
      simd_for_each_prologue(auto&& fun, auto ptr, std::size_t to_process)
      {
        if (V::size() & to_process)
          {
            simd_load_and_invoke<V, write_back>(fun, ptr, 0, stdx::vector_aligned,
                                                std::make_index_sequence<1>());
            ptr += V::size();
          }
        if constexpr (V::size() * 2 * sizeof(typename V::value_type) < max_bytes)
          {
            simd_for_each_prologue<stdx::resize_simd_t<V::size() * 2, V>, write_back, max_bytes>(
              fun, ptr, to_process);
          }
      }

    template <class V0, bool write_back>
      constexpr void
      simd_for_each_epilogue(auto&& fun, auto&& rng, std::size_t i, auto f)
      {
        using V = stdx::resize_simd_t<V0::size() / 2, V0>;
        if (i + V::size() <= std::ranges::size(rng))
          {
            simd_load_and_invoke<V, write_back>(fun, rng, i, f, std::make_index_sequence<1>());
            i += V::size();
          }
        if constexpr (V::size() > 1)
          simd_for_each_epilogue<V, write_back>(fun, rng, i, f);
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

  template <typename ExecutionPolicy, std::ranges::contiguous_range R, typename F>
    requires (detail::is_simd_policy<ExecutionPolicy>::value
                and requires
             {
               typename vir::simdize<std::ranges::range_value_t<R>>;
             })
    constexpr void
    for_each(ExecutionPolicy, R&& rng, F&& fun)
    {
      using T = std::ranges::range_value_t<R>;
      using V = vir::simdize<T, ExecutionPolicy::_size>;
      constexpr bool write_back = std::ranges::output_range<R, T>
                                    and std::invocable<F, V&> and not std::invocable<F, V&&>;
      std::size_t i = 0;
      constexpr std::conditional_t<ExecutionPolicy::_prefers_aligned, stdx::vector_aligned_tag,
                                   stdx::element_aligned_tag> flags{};
      if constexpr (ExecutionPolicy::_prefers_aligned)
        {
          const auto misaligned_by = reinterpret_cast<std::uintptr_t>(std::ranges::data(rng))
                                       % stdx::memory_alignment_v<V>;
          if (misaligned_by != 0)
            {
              const auto to_process = stdx::memory_alignment_v<V> - misaligned_by;
              detail::simd_for_each_prologue<stdx::resize_simd_t<1, V>, write_back,
                                             stdx::memory_alignment_v<V>>(
                fun, std::ranges::data(rng), to_process);
              i = to_process;
            }
        }

      if constexpr (ExecutionPolicy::_unroll_by > 1)
        {
          for (; i + V::size() * ExecutionPolicy::_unroll_by <= std::ranges::size(rng);
               i += V::size() * ExecutionPolicy::_unroll_by)
            {
              detail::simd_load_and_invoke<V, write_back>(
                fun, rng, i, flags, std::make_index_sequence<ExecutionPolicy::_unroll_by>());
            }
        }

      for (; i + V::size() <= std::ranges::size(rng); i += V::size())
        detail::simd_load_and_invoke<V, write_back>(fun, rng, i, flags,
                                                    std::make_index_sequence<1>());

      if constexpr (V::size() > 1)
        detail::simd_for_each_epilogue<V, write_back>(fun, rng, i, flags);
    }

  template <typename ExecutionPolicy, std::ranges::contiguous_range R, typename F>
    requires detail::is_simd_policy<ExecutionPolicy>::value
    constexpr int
    count_if(ExecutionPolicy pol, R const& v, F&& fun)
    {
      using T = std::ranges::range_value_t<R>;
      using TV = vir::simdize<T, ExecutionPolicy::_size>;
      using IV = detail::deduced_simd<int, TV::size()>;
      int count = 0;
      IV countv = 0;
      vir::for_each(pol, v, [&](auto... x) {
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
}  // namespace vir
#endif // VIR_HAVE_SIMD_CONCEPTS
#endif // VIR_SIMD_EXECUTION_H_

// vim: et cc=101 tw=100 sw=2 ts=8
