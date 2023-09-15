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

  template <typename T, std::size_t N>
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

    template <typename>
      struct default_simdize_size;

    template <vectorizable T>
      struct default_simdize_size<T>
      {
	static inline constexpr std::size_t value = stdx::native_simd<T>::size();

	static_assert(value > 0);
      };

    template <reflectable_struct Tup>
      struct default_simdize_size<Tup>
      {
	static inline constexpr std::size_t value
	  = []<std::size_t... Is>(std::index_sequence<Is...>) {
	    return std::max({simdize_impl<vir::struct_element_t<Is, Tup>, 0>::type::size()... });
	  }(std::make_index_sequence<vir::struct_size_v<Tup>>());

	static_assert(value > 0);
      };

    template <typename Tup>
      inline constexpr std::size_t default_simdize_size_v = default_simdize_size<Tup>::value;

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

    template <std::size_t N, template <typename...> class Tpl, typename... Ts>
      Tpl<typename simdize_impl<Ts, N>::type...>
      simdize_template_arguments_impl(const Tpl<Ts...>&);

    template <typename T, std::size_t N = 0>
      requires requires(const T& tt) {
	simdize_template_arguments_impl<default_simdize_size_v<T>>(tt);
      }
      struct simdize_template_arguments
      {
	using type = decltype(simdize_template_arguments_impl<
				N == 0 ? default_simdize_size_v<T> : N>(std::declval<const T&>()));
      };

    template <typename T, std::size_t N = 0>
      using simdize_template_arguments_t = typename simdize_template_arguments<T, N>::type;

    template <template <typename...> class Comp,
	      template <std::size_t, typename> class Element1, typename T1,
	      template <std::size_t, typename> class Element2, typename T2, std::size_t... Is>
      constexpr std::bool_constant<(Comp<typename Element1<Is, T1>::type,
					 typename Element2<Is, T2>::type>::value and ...)>
      test_all_of(std::index_sequence<Is...>)
      { return {}; }

  }

  template <typename T>
    concept vectorizable_struct
      = reflectable_struct<T> and vir::struct_size_v<T> != 0
	  and reflectable_struct<detail::simdize_template_arguments_t<T>>
	  and struct_size_v<T> == struct_size_v<detail::simdize_template_arguments_t<T>>
	  and detail::test_all_of<std::is_same, std::tuple_element,
				  typename detail::make_simd_tuple
				    <T, detail::default_simdize_size_v<T>>::type,
				  struct_element,
				  detail::simdize_template_arguments_t<T>>(
		std::make_index_sequence<struct_size_v<T>>()).value;

  namespace detail
  {
    template <reflectable_struct Tup, std::size_t N>
      requires (vir::struct_size_v<Tup> > 0 and not vectorizable_struct<Tup>)
      struct simdize_impl<Tup, N>
      {
	static_assert(requires { typename simdize_impl<vir::struct_element_t<0, Tup>, N>::type; });

	using type = simd_tuple<Tup, N == 0 ? default_simdize_size_v<Tup> : N>;
      };

    template <vectorizable_struct T, std::size_t N>
      struct simdize_impl<T, N>
      { using type = simd_tuple<T, N == 0 ? default_simdize_size_v<T> : N>; };
  } // namespace detail

  template <reflectable_struct T, std::size_t N>
    class simd_tuple<T, N>
    {
      using tuple_type = typename detail::make_simd_tuple<T, N>::type;

      tuple_type elements;

      static constexpr auto tuple_size_idx_seq = std::make_index_sequence<vir::struct_size_v<T>>();

    public:
      using value_type = T;
      using mask_type = typename std::tuple_element_t<0, tuple_type>::mask_type;

      static constexpr std::integral_constant<std::size_t, N> size{};

      template <typename U = T>
	inline static constexpr std::size_t memory_alignment = alignof(U);

      template <typename... Ts>
	requires (sizeof...(Ts) == std::tuple_size_v<tuple_type>
		    and detail::test_all_of<std::is_constructible, std::tuple_element, tuple_type,
					    std::tuple_element, std::tuple<Ts...>
					   >(tuple_size_idx_seq).value)
	constexpr
	simd_tuple(Ts&&... args)
	: elements{static_cast<Ts&&>(args)...}
	{}

      constexpr
      simd_tuple(const tuple_type& init)
      : elements(init)
      {}

      /**
       * Constructs from a compatible aggregate. Potentially broadcasts all or some elements.
       */
      template <reflectable_struct U>
	requires (struct_size_v<U> == struct_size_v<T>
		    and detail::test_all_of<std::is_constructible, std::tuple_element, tuple_type,
					    struct_element, U>(tuple_size_idx_seq).value)
	constexpr
	explicit(not detail::test_all_of<std::is_convertible, struct_element, U,
					 std::tuple_element, tuple_type>(tuple_size_idx_seq).value)
	simd_tuple(const U& init)
	: elements([&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    return tuple_type {std::tuple_element_t<Is, tuple_type>(vir::struct_get<Is>(init))...};
	  }(tuple_size_idx_seq))
	{}

      template <reflectable_struct U>
	requires (struct_size_v<U> == struct_size_v<T>
		    and detail::test_all_of<std::is_constructible, struct_element, U,
					    std::tuple_element, tuple_type>(tuple_size_idx_seq)
		    .value)
	constexpr
	explicit(not detail::test_all_of<std::is_same, struct_element, U,
					 std::tuple_element, tuple_type>(tuple_size_idx_seq).value)
	operator U() const
	{
	  return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    return U {static_cast<struct_element_t<Is, U>>(std::get<Is>(elements))...};
	  }(tuple_size_idx_seq);
	}

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
	}(tuple_size_idx_seq);
      }

      /**
       * Copies all values from the range starting at `it` into `*this`.
       *
       * Precondition: [it, it + N) is a valid range.
       */
      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::same_as<std::iter_value_t<It>, T>
	constexpr
	simd_tuple(It it, Flags = {})
	: elements([&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    return tuple_type {std::tuple_element_t<Is, tuple_type>([&](auto i) {
				 return struct_get<Is>(it[i]);
			       })...};
	  }(tuple_size_idx_seq))
	{}

      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::same_as<std::iter_value_t<It>, T>
	constexpr void
	copy_from(It it, Flags = {})
	{
	  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    ((std::get<Is>(elements) = std::tuple_element_t<Is, tuple_type>([&](auto i) {
					 return struct_get<Is>(it[i]);
				       })), ...);
	  }(tuple_size_idx_seq);
	}

      /**
       * Copies all values from `*this` to the range starting at `it`.
       *
       * Precondition: [it, it + N) is a valid range.
       */
      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::output_iterator<It, T>
	constexpr void
	copy_to(It it, Flags = {}) const
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

  template <vectorizable_struct T, std::size_t N>
    class simd_tuple<T, N> : public detail::simdize_template_arguments_t<T, N>
    {
      using tuple_type = typename detail::make_simd_tuple<T, N>::type;
      using base_type = detail::simdize_template_arguments_t<T, N>;

      static constexpr auto struct_size = vir::struct_size_v<T>;
      static constexpr auto struct_size_idx_seq = std::make_index_sequence<struct_size>();

    public:
      using value_type = T;
      using mask_type = typename struct_element_t<0, base_type>::mask_type;

      static constexpr std::integral_constant<std::size_t, N> size{};

      template <typename U = T>
	inline static constexpr std::size_t memory_alignment = alignof(U);

      template <typename... Ts>
	requires requires(Ts&&... args) { base_type(static_cast<Ts&&>(args)...); }
	constexpr
	simd_tuple(Ts&&... args)
	: base_type(static_cast<Ts&&>(args)...)
	{}

      constexpr
      simd_tuple(const base_type& init)
      : base_type(init)
      {}

      // broadcast and or vector copy/conversion
      template <reflectable_struct U>
	requires (struct_size_v<U> == struct_size
		    and detail::test_all_of<std::is_constructible, std::tuple_element, tuple_type,
					    struct_element, U>(struct_size_idx_seq).value)
	constexpr
	explicit(not detail::test_all_of<std::is_convertible, struct_element, U,
					 std::tuple_element, tuple_type>(struct_size_idx_seq).value)
	simd_tuple(const U& init)
	: base_type([&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    return base_type {std::tuple_element_t<Is, tuple_type>(vir::struct_get<Is>(init))...};
	  }(struct_size_idx_seq))
	{}

      constexpr T
      operator[](std::size_t i) const
      {
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	  return T{vir::struct_get<Is>(*this)[i]...};
	}(struct_size_idx_seq);
      }

      /**
       * Copies all values from the range starting at `it` into `*this`.
       *
       * Precondition: [it, it + N) is a valid range.
       */
      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::same_as<std::iter_value_t<It>, T>
	constexpr
	simd_tuple(It it, Flags = {})
	: base_type([&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    return base_type {vir::struct_element_t<Is, base_type>([&](auto i) {
				return struct_get<Is>(it[i]);
			      })...};
	  }(struct_size_idx_seq))
	{}

      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::same_as<std::iter_value_t<It>, T>
	constexpr void
	copy_from(It it, Flags = {})
	{
	  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    ((vir::struct_get<Is>(*this) = vir::struct_element_t<Is, base_type>([&](auto i) {
					     return struct_get<Is>(it[i]);
					   })), ...);
	  }(struct_size_idx_seq);
	}

      /**
       * Copies all values from `*this` to the range starting at `it`.
       *
       * Precondition: [it, it + N) is a valid range.
       */
      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::output_iterator<It, T>
	constexpr void
	copy_to(It it, Flags = {}) const
	{
	  for (std::size_t i = 0; i < size(); ++i)
	    it[i] = operator[](i);
	}

      // The following enables implicit conversions added by simd_tuple. E.g.
      // `simdize<Point> + Point` will broadcast the latter to a `simdize<Point>` before applying
      // operator+.
#define VIR_OPERATOR_FWD(op)                                                                       \
      VIR_ALWAYS_INLINE                                                                            \
      friend constexpr decltype(auto)                                                              \
      operator op(simd_tuple const& a, simd_tuple const& b)                                        \
      requires requires(base_type const& aa, base_type const& bb)                                  \
      { {aa op bb}; }                                                                              \
      { return static_cast<base_type const&>(a) op static_cast<base_type const&>(b); }

      VIR_OPERATOR_FWD(+)
      VIR_OPERATOR_FWD(-)
      VIR_OPERATOR_FWD(*)
      VIR_OPERATOR_FWD(/)
      VIR_OPERATOR_FWD(%)
      VIR_OPERATOR_FWD(&)
      VIR_OPERATOR_FWD(|)
      VIR_OPERATOR_FWD(^)
      VIR_OPERATOR_FWD(<<)
      VIR_OPERATOR_FWD(>>)
      VIR_OPERATOR_FWD(==)
      VIR_OPERATOR_FWD(!=)
      VIR_OPERATOR_FWD(>=)
      VIR_OPERATOR_FWD(>)
      VIR_OPERATOR_FWD(<=)
      VIR_OPERATOR_FWD(<)
#undef VIR_OPERATOR_FWD
    };

  template <std::size_t I, vectorizable_struct T, std::size_t N>
    constexpr decltype(auto)
    get(const simd_tuple<T, N>& tup)
    {
      return vir::struct_get<I>(
	       static_cast<const detail::simdize_template_arguments_t<T, N>&>(tup));
    }

  template <std::size_t I, vectorizable_struct T, std::size_t N>
    constexpr decltype(auto)
    get(simd_tuple<T, N>& tup)
    {
      return vir::struct_get<I>(
	       static_cast<detail::simdize_template_arguments_t<T, N>&>(tup));
    }

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
