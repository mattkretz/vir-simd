/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

// Implements non-members of P2664

#ifndef VIR_SIMD_PERMUTE_H_
#define VIR_SIMD_PERMUTE_H_

#include "simd_concepts.h"

#if VIR_HAVE_SIMD_CONCEPTS
#define VIR_HAVE_SIMD_PERMUTE 1

#include "simd.h"
#include "constexpr_wrapper.h"
#include <bit>

namespace vir
{
  namespace detail
  {
    template <typename F>
      concept index_permutation_function_nosize = requires(F const& f)
      {
	{ f(vir::cw<0>) } -> std::integral;
      };

    template <typename F, std::size_t Size>
      concept index_permutation_function_size = requires(F const& f)
      {
	{ f(vir::cw<0>, vir::cw<Size>) } -> std::integral;
      };

    template <typename F, std::size_t Size>
      concept index_permutation_function
	= index_permutation_function_size<F, Size> or index_permutation_function_nosize<F>;
  }

  constexpr int simd_permute_zero = std::numeric_limits<int>::max();

  constexpr int simd_permute_uninit = simd_permute_zero - 1;

  namespace simd_permutations
  {
    struct DuplicateEven
    {
      consteval unsigned
      operator()(unsigned i) const
      { return i & ~1u; }
    };

    inline constexpr DuplicateEven duplicate_even {};

    struct DuplicateOdd
    {
      consteval unsigned
      operator()(unsigned i) const
      { return i | 1u; }
    };

    inline constexpr DuplicateOdd duplicate_odd {};

    template <unsigned N>
      struct SwapNeighbors
      {
	consteval unsigned
	operator()(unsigned i, auto size) const
	{
	  static_assert(size % (2 * N) == 0,
			"swap_neighbors<N> permutation requires a multiple of 2N elements");
	  if (std::has_single_bit(N))
	    return i ^ N;
	  else if (i % (2 * N) >= N)
	    return i - N;
	  else
	    return i + N;
	}
      };

    template <unsigned N = 1u>
      inline constexpr SwapNeighbors<N> swap_neighbors {};

    template <int Position>
      struct Broadcast
      {
	consteval int
	operator()(int) const
	{ return Position; }
      };

    template <int Position>
      inline constexpr Broadcast<Position> broadcast {};

    inline constexpr Broadcast<0> broadcast_first {};

    inline constexpr Broadcast<-1> broadcast_last {};

    struct Reverse
    {
      consteval int
      operator()(int i) const
      { return -1 - i; }
    };

    inline constexpr Reverse reverse {};

    template <int O>
      struct Rotate
      {
	static constexpr int Offset = O;
	static constexpr bool is_even_rotation = Offset % 2 == 0;

	consteval int
	operator()(int i, auto size) const
	{ return (i + Offset) % size.value; }
      };

    template <int Offset>
      inline constexpr Rotate<Offset> rotate {};

    template <int Offset>
      struct Shift
      {
	consteval int
	operator()(int i, int size) const
	{
	  const int j = i + Offset;
	  if constexpr (Offset >= 0)
	    return j >= size ? simd_permute_zero : j;
	  else
	    return j < 0 ? simd_permute_zero : j;
	}
      };

    template <int Offset>
      inline constexpr Shift<Offset> shift {};
  }

  template <std::size_t N = 0, vir::any_simd_or_mask V,
	    detail::index_permutation_function<V::size()> F>
    VIR_ALWAYS_INLINE constexpr stdx::resize_simd_t<N == 0 ? V::size() : N, V>
    simd_permute(V const& v, F const idx_perm) noexcept
    {
      using T = typename V::value_type;
      using R = stdx::resize_simd_t<N == 0 ? V::size() : N, V>;

#if defined __GNUC__
      if (not std::is_constant_evaluated())
	if constexpr (std::has_single_bit(sizeof(V)) and V::size() <= stdx::native_simd<T>::size())
	  {
#if defined __AVX2__
	    using v4df [[gnu::vector_size(32)]] = double;
	    if constexpr (std::same_as<T, float> and std::is_trivially_copyable_v<V>
			    and sizeof(v4df) == sizeof(V)
			    and requires {
			      F::is_even_rotation;
			      F::Offset;
			      { std::bool_constant<F::is_even_rotation>() }
				-> std::same_as<std::true_type>;
			    })
	      {
		const v4df intrin = std::bit_cast<v4df>(v);
		constexpr int control = ((F::Offset / 2) << 0)
					  | (((F::Offset / 2 + 1) % 4) << 2)
					  | (((F::Offset / 2 + 2) % 4) << 4)
					  | (((F::Offset / 2 + 3) % 4) << 6);
		return std::bit_cast<R>(__builtin_ia32_permdf256(intrin, control));
	      }
#endif
#if VIR_HAVE_WORKING_SHUFFLEVECTOR
	    using VecType [[gnu::vector_size(sizeof(V))]] = T;
	    if constexpr (std::is_trivially_copyable_v<V> and std::is_constructible_v<R, VecType>)
	      {
		const VecType vec = std::bit_cast<VecType>(v);
		const auto idx_perm2 = [&](constexpr_value auto i) -> int {
		  constexpr int j = [&]() {
		    if constexpr (detail::index_permutation_function_nosize<F>)
		      return idx_perm(i);
		    else
		      return idx_perm(i, vir::cw<V::size()>);
		  }();
		  if constexpr (j == simd_permute_zero)
		    return V::size();
		  else if constexpr (j == simd_permute_uninit)
		    return -1;
		  else if constexpr (j < 0)
		    {
		      static_assert (-j <= int(V::size()));
		      return int(V::size()) + j;
		    }
		  else
		    {
		      static_assert (j < int(V::size()));
		      return j;
		    }
		};
		return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
		  return R(__builtin_shufflevector(vec, VecType{}, (idx_perm2(vir::cw<Is>))...));
		}(std::make_index_sequence<V::size()>());
	      }
#endif
	  }
#endif // __GNUC__

      return R([&](auto i) -> T {
	       constexpr int j = [&] {
		 if constexpr (detail::index_permutation_function_nosize<F>)
		   return idx_perm(i);
		 else
		   return idx_perm(i, vir::cw<V::size()>);
	       }();
	       if constexpr (j == simd_permute_zero)
		 return 0;
	       else if constexpr (j == simd_permute_uninit)
		 {
		   T uninit;
		   return uninit;
		 }
	       else if constexpr (j < 0)
		 {
		   static_assert(-j <= int(V::size()));
		   return v[v.size() + j];
		 }
	       else
		 {
		   static_assert(j < int(V::size()));
		   return v[j];
		 }
	     });
    }

  template <std::size_t N = 0, vir::vectorizable T, detail::index_permutation_function<1> F>
    VIR_ALWAYS_INLINE constexpr
    std::conditional_t<N <= 1, T, stdx::resize_simd_t<N == 0 ? 1 : N, stdx::simd<T>>>
    simd_permute(T const& v, F const idx_perm) noexcept
    {
      if constexpr (N <= 1)
	{
	  constexpr auto i = vir::cw<0>;
	  constexpr int j = [&] {
	    if constexpr (detail::index_permutation_function_nosize<F>)
	      return idx_perm(i);
	    else
	      return idx_perm(i, vir::cw<std::size_t(1)>);
	  }();
	  if constexpr (j == simd_permute_zero)
	    return 0;
	  else if constexpr (j == simd_permute_uninit)
	    {
	      T uninit;
	      return uninit;
	    }
	  else
	    {
	      static_assert(j == 0 or j == -1);
	      return v;
	    }
	}
      else
	return simd_permute<N>(stdx::simd<T, stdx::simd_abi::scalar>(v), idx_perm);
    }

  template <int Offset, vir::any_simd_or_mask V>
    VIR_ALWAYS_INLINE constexpr V
    simd_shift_in(V const& a, std::convertible_to<V> auto const&... more) noexcept
    {
      return V([&](auto i) -> typename V::value_type {
	       constexpr int ninputs = 1 + sizeof...(more);
	       constexpr int w = V::size();
	       constexpr int j = Offset + int(i);
	       if constexpr (j >= w * ninputs)
		 return 0;
	       else if constexpr (j >= 0)
		 {
		   const V tmp[] = {a, more...};
		   return tmp[j / w][j % w];
		 }
	       else if constexpr (j < -w)
		 return 0;
	       else
		 return a[w + j];
      });
    }
}

#endif // has concepts
#endif // VIR_SIMD_PERMUTE_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
