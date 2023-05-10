/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_SIMDIZE_H_
#define VIR_SIMD_SIMDIZE_H_

#include "struct_reflect.h"

#if VIR_HAVE_STRUCT_REFLECT
#define VIR_HAVE_SIMDIZE 1

#include <tuple>
#include <iterator>
#include "simd.h"
#include "detail.h"
#include "simd_concepts.h"

namespace vir
{
  /**
   * Determines the SIMD width of the given structure. This can either be a stdx::simd object or a
   * tuple-like of stdx::simd (recursively). The latter requires that the SIMD width is homogeneous.
   * If T is not a simd (or tuple thereof), value is 0.
   */
  // Implementation note: partial specialization via concepts is broken on clang < 16
  template <typename T>
    struct simdize_size
    : std::integral_constant<std::size_t, []() {
	if constexpr (stdx::is_simd_v<T>)
	  return T::size();
	else if constexpr (reflectable_struct<T> and []<std::size_t... Is>(
			     std::index_sequence<Is...>) {
			     return ((simdize_size<vir::struct_element_t<0, T>>::value
					== simdize_size<vir::struct_element_t<Is, T>>::value) and ...);
			   }(std::make_index_sequence<vir::struct_size_v<T>>()))
	  return simdize_size<vir::struct_element_t<0, T>>::value;
	else
	  return 0;
      }()>
    {};

  template <typename T>
    inline constexpr std::size_t simdize_size_v = simdize_size<T>::value;

  template <reflectable_struct T, std::size_t N>
    class simd_tuple;

  namespace detail
  {
    template <typename T, std::size_t N>
      struct simdize_impl;

    template <vectorizable T, std::size_t N>
      struct simdize_impl<T, N>
      { using type = deduced_simd<T, N == 0 ? stdx::native_simd<T>::size() : N>; };

    template <reflectable_struct Tup, std::size_t N>
      requires (vir::struct_size_v<Tup> == 0)
      struct simdize_impl<Tup, N>
      {
	static_assert (vir::struct_size_v<Tup> > 0,
		       "simdize<T> requires T and its data members to have at least one member");
      };

    template <reflectable_struct Tup, std::size_t N>
      requires (vir::struct_size_v<Tup> > 0)
      struct simdize_impl<Tup, N>
      {
	static_assert(requires { typename simdize_impl<vir::struct_element_t<0, Tup>, N>::type; });

	static inline constexpr std::size_t size
	  = N > 0 ? N : []<std::size_t... Is>(std::index_sequence<Is...>) constexpr
	{
	  return std::max({simdize_impl<vir::struct_element_t<Is, Tup>, 0>::type::size()... });
	}(std::make_index_sequence<vir::struct_size_v<Tup>>());

	static_assert(size > 0);

	using type = simd_tuple<Tup, size>;
      };

    template <reflectable_struct Tup, std::size_t N>
      requires (vir::struct_size_v<Tup> > 0 and N > 0)
      struct make_simd_tuple
      {
	using type = decltype([]<std::size_t... Is>(std::index_sequence<Is...>)
				-> std::tuple<typename simdize_impl<
						vir::struct_element_t<Is, Tup>, N>::type...> {
		       return {};
		     }(std::make_index_sequence<vir::struct_size_v<Tup>>()));
      };
  } // namespace detail

  template <reflectable_struct T, std::size_t N>
    class simd_tuple
    {
      using tuple_type = typename detail::make_simd_tuple<T, N>::type;

      tuple_type elements;

    public:
      using mask_type = typename std::tuple_element_t<0, tuple_type>::mask_type;

      static constexpr std::integral_constant<std::size_t, N> size{};

      template <typename... Ts>
	requires (sizeof...(Ts) == std::tuple_size_v<tuple_type>)
	constexpr
	simd_tuple(Ts&&... args)
	: elements{static_cast<Ts&&>(args)...}
	{}

      constexpr
      simd_tuple(const T& init)
      : elements([&]<std::size_t... Is>(std::index_sequence<Is...>) {
	  return tuple_type {std::tuple_element_t<Is, tuple_type>(vir::struct_get<Is>(init))...};
	}(std::make_index_sequence<vir::struct_size_v<T>>()))
      {}

      constexpr tuple_type&
      as_tuple()
      { return elements; }

      constexpr tuple_type const&
      as_tuple() const
      { return elements; }

      constexpr T
      operator[](std::size_t i) const
      {
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	  return T{std::get<Is>(elements)[i]...};
	}(std::make_index_sequence<std::tuple_size_v<tuple_type>>());
      }

      template <std::contiguous_iterator It>
	requires std::same_as<std::iter_value_t<It>, T>
	constexpr void
	copy_from(std::contiguous_iterator auto it)
	{
	  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    ((std::get<Is>(elements) = T([&](auto i) {
				      return struct_get<Is>(it[i]);
				    })), ...);
	  }(std::make_index_sequence<size()>());
	}

      template <std::contiguous_iterator It>
	requires std::output_iterator<It, T>
	constexpr void
	copy_to(It it) const
	{
	  for (std::size_t i = 0; i < size(); ++i)
	    it[i] = operator[](i);
	}

    };

  template <std::size_t I, reflectable_struct T, std::size_t N>
    constexpr decltype(auto)
    get(const simd_tuple<T, N>& tup)
    { return std::get<I>(tup.as_tuple()); }

  template <std::size_t I, reflectable_struct T, std::size_t N>
    constexpr decltype(auto)
    get(simd_tuple<T, N>& tup)
    { return std::get<I>(tup.as_tuple()); }

  /**
   * Meta-function that turns a vectorizable type or a tuple-like (recursively) of vectorizable
   * types into a stdx::simd or std::tuple (recursively) of stdx::simd. If N is non-zero, N
   * determines the resulting SIMD width. Otherwise, of all vectorizable types U the maximum
   * stdx::native_simd<U>::size() determines the resulting SIMD width.
   */
  template <typename T, std::size_t N = 0>
    requires reflectable_struct<T> or vectorizable<T>
    using simdize = typename detail::simdize_impl<T, N>::type;

} // namespace vir

template <vir::reflectable_struct T, std::size_t N>
  struct std::tuple_size<vir::simd_tuple<T, N>>
    : std::integral_constant<std::size_t, vir::struct_size_v<T>>
  {};

template <std::size_t I, vir::reflectable_struct T, std::size_t N>
  struct std::tuple_element<I, vir::simd_tuple<T, N>>
    : std::tuple_element<I, typename vir::detail::make_simd_tuple<T, N>::type>
  {};

#endif  // VIR_HAVE_STRUCT_REFLECT
#endif  // VIR_SIMD_SIMDIZE_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
