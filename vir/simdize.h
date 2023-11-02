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
#include "simd_permute.h"
#include "constexpr_wrapper.h"

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
    : vir::constexpr_wrapper<[]() -> int {
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
    inline constexpr int simdize_size_v = simdize_size<T>::value;

  template <typename T, int N>
    class simd_tuple;

  template <typename T, int N>
    class vectorized_struct;

  namespace detail
  {
    template <typename V, bool... Values>
      inline constexpr typename V::mask_type mask_constant
	= typename V::mask_type(std::array<bool, sizeof...(Values)>{Values...}.data(),
				stdx::element_aligned);

    template <typename T, int N, unsigned Bytes = sizeof(T) * std::bit_ceil(unsigned(N))>
      using gnu_vector [[gnu::vector_size(Bytes)]] = T;

#if VIR_HAVE_WORKING_SHUFFLEVECTOR
    /**
     * Return a with a[i] replaced by b[i] for all i in {Indexes...}.
     */
    template <int... Indexes, typename T>
      VIR_ALWAYS_INLINE inline T
      blend(T a, T b)
      {
	constexpr int N = sizeof(a) / sizeof(a[0]);
	static_assert(sizeof...(Indexes) <= N);
	static_assert(((Indexes <= N) && ...));
	constexpr auto selected = [](int i) {
	  return ((i == Indexes) || ...);
	};
	return [&]<int... Is>(std::integer_sequence<int, Is...>) {
	  return __builtin_shufflevector(a, b, (selected(Is) ? Is + N : Is)...);
	}(std::make_integer_sequence<int, N>());
      }
#endif

    template <typename T, int N>
      struct simdize_impl;

    template <vectorizable T, int N>
      struct simdize_impl<T, N>
      { using type = deduced_simd<T, N == 0 ? stdx::native_simd<T>::size() : N>; };

    template <reflectable_struct Tup, int N>
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
	static inline constexpr int value = stdx::native_simd<T>::size();

	static_assert(value > 0);
      };

    template <reflectable_struct Tup>
      struct default_simdize_size<Tup>
      {
	static inline constexpr int value
	  = []<std::size_t... Is>(std::index_sequence<Is...>) {
	    return std::max({int(simdize_impl<vir::struct_element_t<Is, Tup>, 0>::type::size())...});
	  }(std::make_index_sequence<vir::struct_size_v<Tup>>());

	static_assert(value > 0);
      };

    template <typename Tup>
      inline constexpr int default_simdize_size_v = default_simdize_size<Tup>::value;

    template <reflectable_struct Tup, int N>
      requires (vir::struct_size_v<Tup> > 0 and N > 0)
      struct make_simd_tuple
      {
	using type = decltype([]<std::size_t... Is>(std::index_sequence<Is...>)
				-> std::tuple<typename simdize_impl<
						vir::struct_element_t<Is, Tup>, N>::type...> {
		       return {};
		     }(std::make_index_sequence<vir::struct_size_v<Tup>>()));
      };

    /**
     * Recursively simdize all type template arguments.
     */
    template <std::size_t N, template <typename...> class Tpl, typename... Ts>
      Tpl<typename simdize_impl<Ts, N>::type...>
      simdize_template_arguments_impl(const Tpl<Ts...>&);

    template <std::size_t N, template <typename, auto...> class Tpl, typename T, auto... X>
      requires(sizeof...(X) > 0)
      Tpl<typename simdize_impl<T, N>::type, X...>
      simdize_template_arguments_impl(const Tpl<T, X...>&);

    template <std::size_t N, template <typename, typename, auto...> class Tpl,
	      typename... Ts, auto... X>
      requires(sizeof...(X) > 0)
      Tpl<typename simdize_impl<Ts, N>::type..., X...>
      simdize_template_arguments_impl(const Tpl<Ts..., X...>&);

    template <std::size_t N, template <typename, typename, typename, auto...> class Tpl,
	      typename... Ts, auto... X>
      requires(sizeof...(X) > 0)
      Tpl<typename simdize_impl<Ts, N>::type..., X...>
      simdize_template_arguments_impl(const Tpl<Ts..., X...>&);

    template <typename T, int N = 0>
      requires requires(const T& tt) {
	simdize_template_arguments_impl<default_simdize_size_v<T>>(tt);
      }
      struct simdize_template_arguments
      {
	using type = decltype(simdize_template_arguments_impl<
				N == 0 ? default_simdize_size_v<T> : N>(std::declval<const T&>()));
      };

    template <typename T, int N = 0>
      using simdize_template_arguments_t = typename simdize_template_arguments<T, N>::type;

    /**
     * flat_member_count: count data members in T recursively.
     * IOW, count all non-reflectable data members. If a data member can be reflected, count its
     * members instead.
     */
    template <typename T>
      struct flat_member_count;

    template <typename T>
      inline constexpr int flat_member_count_v = flat_member_count<T>::value;

    template <typename T>
      requires (not vir::reflectable_struct<T>)
      struct flat_member_count<T>
      : vir::constexpr_wrapper<1>
      {};

    template <vir::reflectable_struct T>
      struct flat_member_count<T>
      {
	static constexpr int value = []<int... Is>(std::integer_sequence<int, Is...>) {
	  return (flat_member_count_v<vir::struct_element_t<Is, T>> + ...);
	}(std::make_integer_sequence<int, vir::struct_size_v<T>>());
      };

    /**
     * Determine the type of the I-th non-reflectable data member.
     */
    template <int I, typename T>
      struct flat_element;

    template <int I, typename T>
      using flat_element_t = typename flat_element<I, T>::type;

    template <typename T>
      requires (not vir::reflectable_struct<T>)
      struct flat_element<0, T>
      { using type = T; };

    template <int I, vir::reflectable_struct T>
      requires (flat_member_count_v<T> == struct_size_v<T> and I < struct_size_v<T>)
      struct flat_element<I, T>
      { using type = struct_element_t<I, T>; };

    template <int I, vir::reflectable_struct T>
      requires (flat_member_count_v<T> > struct_size_v<T>
		  and I < flat_member_count_v<struct_element_t<0, T>>)
      struct flat_element<I, T>
      { using type = flat_element_t<I, struct_element_t<0, T>>; };

    /**
     * Return the value of the I-th non-reflectable data member.
     */
    template <int I, int Offset = 0, vir::reflectable_struct T>
      constexpr decltype(auto)
      flat_get(T&& s)
      {
	using TT = std::remove_cvref_t<T>;
	if constexpr (flat_member_count_v<TT> == struct_size_v<TT>)
	  {
	    static_assert(I < struct_size_v<TT>);
	    static_assert(Offset == 0);
	    return vir::struct_get<I>(s);
	  }
	else
	  {
	    static_assert(flat_member_count_v<TT> > struct_size_v<TT>);
	    constexpr auto size = flat_member_count_v<struct_element_t<Offset, TT>>;
	    if constexpr (I < size)
	      return flat_get<I>(vir::struct_get<Offset>(s));
	    else
	      return flat_get<I - size, size>(s);
	  }
      }

    template <template <typename...> class Comp,
	      template <std::size_t, typename> class Element1, typename T1,
	      template <std::size_t, typename> class Element2, typename T2, std::size_t... Is>
      constexpr std::bool_constant<(Comp<typename Element1<Is, T1>::type,
					 typename Element2<Is, T2>::type>::value and ...)>
      test_all_of(std::index_sequence<Is...>)
      { return {}; }

    /**
     * Conjunction of Trait::value<I> for I in {Is...}.
     */
    template <typename Trait, std::size_t... Is>
      constexpr std::bool_constant<(Trait::template value<Is> and ...)>
      test_all_of(std::index_sequence<Is...>)
      { return {}; }

    /**
     * Test that make_simd_tuple and simdize_template_arguments produce equivalent results.
     */
    template <typename T, int N = default_simdize_size_v<T>,
	      typename TTup = typename make_simd_tuple<T, N>::type,
	      typename TS = simdize_template_arguments_t<T>>
      struct is_consistent_struct_vectorization
      {
	template <std::size_t I, typename L = flat_element_t<I, TTup>,
		  typename R = flat_element_t<I, TS>>
	  static constexpr bool value = std::same_as<L, R>;
      };
  }

  /**
   * A type T is vectorizable if all of its data-members can be vectorized via template argument
   * substitution.
   */
  template <typename T>
    concept vectorizable_struct
      = reflectable_struct<T> and vir::struct_size_v<T> != 0
	  and reflectable_struct<detail::simdize_template_arguments_t<T>>
	  and struct_size_v<T> == struct_size_v<detail::simdize_template_arguments_t<T>>
	  and bool(detail::test_all_of<detail::is_consistent_struct_vectorization<T>>(
		     std::make_index_sequence<vir::detail::flat_member_count_v<T>>()));

  namespace detail
  {
    template <reflectable_struct Tup, int N>
      requires (vir::struct_size_v<Tup> > 0 and not vectorizable_struct<Tup>)
      struct simdize_impl<Tup, N>
      {
	static_assert(requires { typename simdize_impl<vir::struct_element_t<0, Tup>, N>::type; });

	using type = simd_tuple<Tup, N == 0 ? default_simdize_size_v<Tup> : N>;
      };

    template <vectorizable_struct T, int N>
      struct simdize_impl<T, N>
      { using type = vectorized_struct<T, N == 0 ? default_simdize_size_v<T> : N>; };
  } // namespace detail

  /**
   * stdx::simd-like interface for tuples of vectorized data members of T.
   */
  template <reflectable_struct T, int N>
    class simd_tuple<T, N>
    {
      using tuple_type = typename detail::make_simd_tuple<T, N>::type;

      tuple_type elements;

      static constexpr auto tuple_size_idx_seq = std::make_index_sequence<vir::struct_size_v<T>>();

    public:
      using value_type = T;
      using mask_type = typename std::tuple_element_t<0, tuple_type>::mask_type;

      static constexpr auto size = vir::cw<N>;

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

      constexpr auto
      operator[](std::size_t i) const
      requires (not std::is_array_v<T>)
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

  /**
   * Implements structured bindings interface.
   *
   * \see std::tuple_size, std::tuple_element
   */
  template <std::size_t I, reflectable_struct T, int N>
    requires (not vectorizable_struct<T>)
    constexpr decltype(auto)
    get(const simd_tuple<T, N>& tup)
    { return std::get<I>(tup.as_tuple()); }

  template <std::size_t I, reflectable_struct T, int N>
    requires (not vectorizable_struct<T>)
    constexpr decltype(auto)
    get(simd_tuple<T, N>& tup)
    { return std::get<I>(tup.as_tuple()); }

  /**
   * stdx::simd-like interface on top of vectorized types (template argument substitution).
   */
  template <vectorizable_struct T, int N>
    class vectorized_struct<T, N> : public detail::simdize_template_arguments_t<T, N>
    {
      using tuple_type = typename detail::make_simd_tuple<T, N>::type;
      using base_type = detail::simdize_template_arguments_t<T, N>;

      static constexpr auto _flat_member_count = detail::flat_member_count_v<T>;
      static constexpr auto _flat_member_idx_seq = std::make_index_sequence<_flat_member_count>();
      static constexpr auto _struct_size = struct_size_v<T>;
      static constexpr auto _struct_size_idx_seq = std::make_index_sequence<_struct_size>();

      constexpr base_type&
      _as_base_type()
      { return *this; }

      constexpr base_type const&
      _as_base_type() const
      { return *this; }

    public:
      using value_type = T;
      using mask_type = typename detail::flat_element_t<0, base_type>::mask_type;

      static constexpr auto size = vir::cw<N>;

      template <typename U = T>
	inline static constexpr std::size_t memory_alignment = alignof(U);

      template <typename... Ts>
	requires requires(Ts&&... args) { base_type{static_cast<Ts&&>(args)...}; }
	VIR_ALWAYS_INLINE constexpr
	vectorized_struct(Ts&&... args)
	: base_type{static_cast<Ts&&>(args)...}
	{}

      VIR_ALWAYS_INLINE constexpr
      vectorized_struct(const base_type& init)
      : base_type(init)
      {}

      // broadcast and or vector copy/conversion
      template <reflectable_struct U>
	requires (struct_size_v<U> == _struct_size
		    and detail::test_all_of<std::is_constructible, std::tuple_element, tuple_type,
					    struct_element, U>(_struct_size_idx_seq).value)
	constexpr
	explicit(not detail::test_all_of<std::is_convertible, struct_element, U,
					 std::tuple_element, tuple_type>(_struct_size_idx_seq).value)
	vectorized_struct(const U& init)
	: base_type([&]<std::size_t... Is>(std::index_sequence<Is...>) {
	    return base_type {std::tuple_element_t<Is, tuple_type>(vir::struct_get<Is>(init))...};
	  }(_struct_size_idx_seq))
	{}

      VIR_ALWAYS_INLINE constexpr T
      operator[](std::size_t i) const
      {
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	  return T{detail::flat_get<Is>(*this)[i]...};
	}(_flat_member_idx_seq);
      }

      VIR_ALWAYS_INLINE
      static constexpr base_type
      _load_elements_via_permute(const T* addr)
      {
#if VIR_HAVE_WORKING_SHUFFLEVECTOR
	if (not std::is_constant_evaluated())
	  if constexpr (N > 1 and sizeof(T) * N == sizeof(base_type) and _flat_member_count >= 2
			  and std::has_single_bit(unsigned(N)))
	    {
	      const std::byte* byte_ptr = reinterpret_cast<const std::byte*>(addr);
	      // struct_size_v == 2 doesn't need anything, the fallback works fine, unless
	      // we allow unordered access
	      using V0 = detail::flat_element_t<0, tuple_type>;
	      using V1 = detail::flat_element_t<1, tuple_type>;
	      if constexpr (_flat_member_count == 2 and N > 2)
		{
		  static_assert(N == V0::size());
		  constexpr int N2 = N / 2;
		  using U = typename V0::value_type;
		  if constexpr (sizeof(U) == sizeof(vir::struct_element_t<0, T>)
				  and sizeof(U) == sizeof(vir::struct_element_t<1, T>)
				  and std::has_single_bit(V0::size())
				  and V0::size() <= stdx::native_simd<U>::size())
		    {
		      using V [[gnu::vector_size(sizeof(U) * N)]] = U;

		      V x0, x1;
		      // [a0 b0 a1 b1 a2 b2 a3 b3]
		      std::memcpy(&x0, std::addressof(vir::struct_get<0>(addr[0])), sizeof(V));
		      // [a4 b4 a5 b5 a6 b6 a7 b7]
		      std::memcpy(&x1, std::addressof(vir::struct_get<0>(addr[N2])), sizeof(V));

		      return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
			return base_type {
			  std::bit_cast<V0>(
			    __builtin_shufflevector(x0, x1, (Is * 2)..., (N + Is * 2)...)),
			  std::bit_cast<V1>(
			    __builtin_shufflevector(x0, x1, (1 + Is * 2)..., (1 + N + Is * 2)...))
			};
		      }(std::make_index_sequence<N2>());

		      /*		    if constexpr (sizeof(V) == 32)
					    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
					    constexpr auto N4 = N2 / 2;
					    const auto tmp = x0;
		      // [a0 b0 a1 b1 a4 b4 a5 b5]
		      x0 = __builtin_shufflevector(tmp, x1, Is..., (Is + N)...);
		      // [a2 b2 a3 b3 a6 b6 a7 b7]
		      x1 = __builtin_shufflevector(tmp, x1, (Is + N2) ..., (Is + N + N2)...);
		      const lo0 = __builtin_shufflevector(x0, x1, ((Is & 1 ? N : 0) + Is / 2)...,
		      N2 + ((Is & 1 ? N : 0) + Is / 2)...);
		      const hi0 = __builtin_shufflevector(x0, x1, N4 + ((Is & 1 ? N : 0) + Is / 2)...,
		      N4 + N2 + ((Is & 1 ? N : 0) + Is / 2)...);
		      }(std::make_index_sequence<N2>());

		      V0 x0(std::addressof(vir::struct_get<0>(addr[0])), stdx::element_aligned);
		      V0 x1(std::addressof(vir::struct_get<0>(addr[V0::size()])), stdx::element_aligned);


		      // [b4 a4 b5 a5 b6 a6 b7 a7]
		      x1 = simd_permute(x1, simd_permutations::swap_neighbors<>);
		      // [0 1 0 1 0 1 0 1]
		      constexpr auto mask = []<std::size_t... Is>(std::index_sequence<Is...>) {
		      return detail::mask_constant<V0, (Is & 1)...>;
		      }(std::make_index_sequence<V0::size()>());
		      V0 tmp = x1;
		      // [b4 b0 b5 b1 b6 b2 b7 b3]
		      stdx::where(mask, x1) = x0;
		      // [b0 b4 b1 b5 b2 b6 b3 b7]
		      x1 = simd_permute(x1, simd_permutations::swap_neighbors<>);
		      // [a0 a4 a1 a5 a2 a6 a3 a7]
		      stdx::where(mask, x0) = tmp;
		      return base_type {x0, x1};*/
		    }
		}
	      else if constexpr (std::same_as<vir::as_tuple_t<T>, std::tuple<float, float, float>>)
		{
		  if constexpr (std::same_as<tuple_type, std::tuple<V0, V0, V0>>
				  and V0::size() == 8 and std::is_trivially_copyable_v<V0>)
		    {
		      // Implementation notes:
		      // 1. Three gather instructions with indexes 0, 3, 6, 9, 12, 15, 18, 21 is super
		      //    slow
		      // 2. Eight 3/4-element loads -> concat to 8 elements -> unpack also much slower

		      //                               abc   abc abc
		      // a = [a0 b0 c0 a1 b1 c1 a2 b2] 332 = 211+121
		      // b = [c2 a3 b3 c3 a4 b4 c4 a5] 323 = 112+211
		      // c = [b5 c5 a6 b6 c6 a7 b7 c7] 233 = 121+112

		      using v8sf [[gnu::vector_size(32)]] = float;
		      if constexpr (true) // allow_unordered
			{
			  v8sf x0, x1, x2, a0, b0, c0;
			  std::memcpy(&x0, byte_ptr +  0, 32);
			  std::memcpy(&x1, byte_ptr + 32, 32);
			  std::memcpy(&x2, byte_ptr + 64, 32);

			  a0 = detail::blend<1, 4, 7>(x0, x1);
			  a0 = detail::blend<2, 5>(a0, x2);
			  // a0 a3 a6 a1 a4 a7 a2 a5

			  b0 = detail::blend<2, 5>(x0, x1);
			  b0 = detail::blend<0, 3, 6>(b0, x2);
			  // b5 b0 b3 b6 b1 b4 b7 b2

			  c0 = detail::blend<0, 3, 6>(x0, x1);
			  c0 = detail::blend<1, 4, 7>(c0, x2);
			  // c2 c5 c0 c3 c6 c1 c4 c7

			  V0 a = std::bit_cast<V0>(a0);
			  V0 b = simd_permute(std::bit_cast<V0>(b0), simd_permutations::rotate<1>);
			  V0 c = simd_permute(std::bit_cast<V0>(c0), simd_permutations::rotate<2>);
			  return base_type {a, b, c};
			}
		      else
			{
			  v8sf a, b, c;
			  std::memcpy(&a, byte_ptr +  0, 32);
			  std::memcpy(&b, byte_ptr + 32, 32);
			  std::memcpy(&c, byte_ptr + 64, 32);

			  // a0 b0 c0 a1 b5 c5 a6 b6
			  v8sf ac0 = __builtin_shufflevector(a, c, 0, 1, 2, 3, 8, 9, 10, 11);

			  // b1 c1 a2 b2 c6 a7 b7 c7
			  v8sf ac1 = __builtin_shufflevector(a, c, 4, 5, 6, 7, 12, 13, 14, 15);

			  // a0 a3 a2 a1 a4 a7 a6 a5
			  V0 tmp0 = std::bit_cast<V0>(detail::blend<2, 5>(
							detail::blend<1, 4, 7>(ac0, b), ac1));

			  // b1 b0 b3 b2 b5 b4 b7 b6
			  V0 tmp1 = std::bit_cast<V0>(detail::blend<0, 3, 6>(
							detail::blend<2, 5>(ac0, b), ac1));

			  // c2 c1 c0 c3 c6 c5 c4 c7
			  V0 tmp2 = std::bit_cast<V0>(detail::blend<1, 4, 7>(
							detail::blend<0, 3, 6>(ac0, b), ac1));

			  return {
			    simd_permute(tmp0, [](auto i) { return std::array{0, 3, 2, 1, 4, 7, 6, 5}[i]; }),
			    simd_permute(tmp1, [](auto i) { return std::array{1, 0, 3, 2, 5, 4, 7, 6}[i]; }),
			    simd_permute(tmp2, [](auto i) { return std::array{2, 1, 0, 3, 6, 5, 4, 7}[i]; })
			  };
			}
		    }
		  if constexpr (std::same_as<tuple_type, std::tuple<V0, V0, V0>>
				  and V0::size() == 4 and std::is_trivially_copyable_v<V0>)
		    {
		      // abca = [a0 b0 c0 a1]
		      // bcab = [b1 c1 a2 b2]
		      // cabc = [c2 a3 b3 c3]

		      using v4sf [[gnu::vector_size(16)]] = float;
		      v4sf abca, bcab, cabc, a, b, c;
		      std::memcpy(&abca, byte_ptr +  0, 16);
		      std::memcpy(&bcab, byte_ptr + 16, 16);
		      std::memcpy(&cabc, byte_ptr + 32, 16);

		      // [a0 a1 a2 a3]
		      a = __builtin_shufflevector(abca, detail::blend<2, 3>(cabc, bcab), 0, 3, 6, 5);

		      // [b0 b1 b2 b3]
		      b = __builtin_shufflevector(detail::blend<0>(abca, bcab), // [b1 b0 c0 a1]
						  detail::blend<2>(bcab, cabc), // [b1 c1 b3 b2]
						  1, 0, 7, 6);

		      c = __builtin_shufflevector(detail::blend<2, 3>(bcab, abca), // [b1 c1 c0 a1]
						  cabc, 2, 1, 4, 7);

		      return base_type {std::bit_cast<V0>(a), std::bit_cast<V0>(b),
				       	std::bit_cast<V0>(c)};
		    }
		}
	      if constexpr (_flat_member_count == 3 and N > 2)
		{
		  using V2 = detail::flat_element_t<2, tuple_type>;
		  static_assert(N == V0::size());
		  static_assert(std::has_single_bit(unsigned(N)));
		  using U = typename V0::value_type;
		  if constexpr (sizeof(U) == sizeof(detail::flat_element_t<0, T>)
				  and sizeof(U) == sizeof(detail::flat_element_t<1, T>)
				  and sizeof(U) == sizeof(detail::flat_element_t<2, T>)
				  and V0::size() <= stdx::native_simd<U>::size())
		    {
		      using V [[gnu::vector_size(sizeof(U) * N)]] = U;

		      V x0, x1, x2;
		      // [a0 b0 c0 a1 b1 c1 a2 b2]
		      std::memcpy(&x0, byte_ptr, sizeof(V));
		      // [c2 a3 b3 c3 a4 b4 c4 a5]
		      std::memcpy(&x1, byte_ptr + sizeof(V), sizeof(V));
		      // [b5 c5 a6 b6 c6 a7 b7 c7]
		      std::memcpy(&x2, byte_ptr + 2 * sizeof(V), sizeof(V));

		      return [&]<int... Is>(std::integer_sequence<int, Is...>)
			     VIR_LAMBDA_ALWAYS_INLINE {
			       return base_type {
				 std::bit_cast<V0>(
				   __builtin_shufflevector(
				     __builtin_shufflevector(
				       x0, x1, (Is % 3 == 0 ? Is : (Is + N) % 3 == 0 ? Is + N : -1)...),
				     x2, (Is * 3 < N ? Is * 3 : Is * 3 - N)...)),
				 std::bit_cast<V1>(
				   __builtin_shufflevector(
				     __builtin_shufflevector(
				       x0, x1, (Is % 3 == 1 ? Is : (Is + N) % 3 == 1 ? Is + N : -1)...),
				     x2, (Is * 3 + 1 < N ? Is * 3 + 1 : Is * 3 - N + 1)...)),
				 std::bit_cast<V2>(
				   __builtin_shufflevector(
				     __builtin_shufflevector(
				       x0, x1, (Is % 3 == 2 ? Is : (Is + N) % 3 == 2 ? Is + N : -1)...),
				     x2, (Is * 3 + 2 < N ? Is * 3 + 2 : Is * 3 - N + 2)...))
			       };
			     }(std::make_integer_sequence<int, N>());
		    }
		}
	    }
#endif // VIR_HAVE_WORKING_SHUFFLEVECTOR

	// not optimized fallback
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	  return base_type {detail::flat_element_t<Is, tuple_type>([&](auto i) {
			      return detail::flat_get<Is>(addr[i]);
			    })...};
	}(_flat_member_idx_seq);
      }

      template <int... Is>
	VIR_ALWAYS_INLINE constexpr void
	_store_elements_via_permute(T* addr, std::integer_sequence<int, Is...>) const
	{
#if VIR_HAVE_WORKING_SHUFFLEVECTOR
	  if (not std::is_constant_evaluated())
	    if constexpr (N > 2 and _flat_member_count >= 2
			    and sizeof(T) * N == sizeof(base_type)
			    and std::has_single_bit(unsigned(N)))
	      {
		using V0 = detail::flat_element_t<0, tuple_type>;
		//using V1 = detail::flat_element_t<1, tuple_type>;
		static_assert(N == V0::size());
		using U = typename V0::value_type;
		using V [[gnu::vector_size(sizeof(U) * N)]] = U;
		std::byte* byte_ptr = reinterpret_cast<std::byte*>(addr);

		if constexpr (_flat_member_count == 3
				and V0::size() <= stdx::native_simd<U>::size())
		  {
		    //using V2 = detail::flat_element_t<2, tuple_type>;
		    if constexpr (sizeof(U) == sizeof(detail::flat_element_t<0, T>)
				    and sizeof(U) == sizeof(detail::flat_element_t<1, T>)
				    and sizeof(U) == sizeof(detail::flat_element_t<2, T>))
		      {
			// [a0 a1 a2 a3 a4 a5 a6 ...]
			V a = std::bit_cast<V>(detail::flat_get<0>(_as_base_type()));
			// [b0 b1 b2 b3 b4 b5 b6 ...]
			V b = std::bit_cast<V>(detail::flat_get<1>(_as_base_type()));
			// [c0 c1 c2 c3 c4 c5 c6 ...]
			V c = std::bit_cast<V>(detail::flat_get<2>(_as_base_type()));
			constexpr auto idx = [](int i, int pos, int a, int b, int c) {
			  constexpr int N3 = N / 3;
			  switch(i % 3)
			  {
			    case 0:
			      return i / 3 + a + pos * N3;
			    case 1:
			      return i / 3 + b + pos * N3 + N;
			    case 2:
			      return i / 3 + c + ((pos + N % 3) % 3) * N3;
			    default:
			      __builtin_unreachable();
			  }
			};
			if constexpr (N % 3 == 1)
			  {
			    // [a0 b0 a6 a1|b1 a7 a2 b2|a8 a3 b3 a9|a4 b4 a10 a5]
			    const V aba = __builtin_shufflevector(a, b, idx(Is, 0, 0, 0, 1)...);
			    // [b5 c5 b11 b6|c6 b12 b7 c7|b13 b8 c8 b14|b9 c9 b15 b10]
			    const V bcb = __builtin_shufflevector(b, c, idx(Is, 1, 0, 0, 1)...);
			    // [c10 a11 c0 c11|a12 c1 c12 a13|c2 c13 a14 c3|c14 a15 c4 c15]
			    const V cac = __builtin_shufflevector(c, a, idx(Is, 2, 0, 1, 0)...);
			    // [a0 b0 c0 a1|b1 c1 a2 b2|c2 a3 b3 c3|a4 b4 c4 a5]
			    a = __builtin_shufflevector(aba, cac, (Is % 3 != 2 ? Is : Is + N)...);
			    // [b5 c5 a6 b6|c6 a7 b7 c7|a8 b8 c8 a9|b9 c9 a10 b10]
			    b = __builtin_shufflevector(bcb, aba, (Is % 3 != 2 ? Is : Is + N)...);
			    // [c10 a11 b11 c11|a12 b12 c12 a13|b13 c13 a14 b14|c14 a15 b15 c15]
			    c = __builtin_shufflevector(cac, bcb, (Is % 3 != 2 ? Is : Is + N)...);
			  }
			else
			  {
			    static_assert(N % 3 == 2);
			    // [a0 b0 a6 a1|b1 a7 a2 b2]
			    const V aba = __builtin_shufflevector(a, b, idx(Is, 0, 0, 0, 2)...);
			    // [c2 a3 c0 c3|a4 c1 c4 a5]
			    const V cac = __builtin_shufflevector(c, a, idx(Is, 1, 0, 1, 0)...);
			    // [b5 c5 b3 b6|c6 b4 b7 c7]
			    const V bcb = __builtin_shufflevector(b, c, idx(Is, 2, 1, 1, 1)...);
			    // [a0 b0 c0 a1|b1 c1 a2 b2]
			    a = __builtin_shufflevector(aba, cac, (Is % 3 != 2 ? Is : Is + N)...);
			    // [c2 a3 b3 c3|a4 b4 c4 a5]
			    b = __builtin_shufflevector(cac, bcb, (Is % 3 != 2 ? Is : Is + N)...);
			    // [b5 c5 a6 b6|c6 a7 b7 c7]
			    c = __builtin_shufflevector(bcb, aba, (Is % 3 != 2 ? Is : Is + N)...);
			  }
			std::memcpy(byte_ptr, &a, sizeof(V));
			std::memcpy(byte_ptr + sizeof(V), &b, sizeof(V));
			std::memcpy(byte_ptr + 2 * sizeof(V), &c, sizeof(V));
			return;
		      }
		  }
	      }
#endif
	  for (int i = 0; i < N; ++i)
	    addr[i] = operator[](i);
	}

      /**
       * Copies all values from the range starting at `it` into `*this`.
       *
       * Precondition: [it, it + N) is a valid range.
       */
      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::same_as<std::iter_value_t<It>, T>
	constexpr
	vectorized_struct(It it, Flags = {})
	: base_type(_load_elements_via_permute(std::to_address(it)))
	{}

      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::same_as<std::iter_value_t<It>, T>
	constexpr void
	copy_from(It it, Flags = {})
	{ static_cast<base_type&>(*this) = _load_elements_via_permute(std::to_address(it)); }

      /**
       * Copies all values from `*this` to the range starting at `it`.
       *
       * Precondition: [it, it + N) is a valid range.
       */
      template <std::contiguous_iterator It, typename Flags = stdx::element_aligned_tag>
	requires std::output_iterator<It, T>
	constexpr void
	copy_to(It it, Flags = {}) const
	{ _store_elements_via_permute(std::to_address(it), std::make_integer_sequence<int, N>()); }

      // The following enables implicit conversions added by vectorized_struct. E.g.
      // `simdize<Point> + Point` will broadcast the latter to a `simdize<Point>` before applying
      // operator+.
      template <typename R>
	using _op_return_type = std::conditional_t<std::same_as<R, base_type>, vectorized_struct, R>;

#define VIR_OPERATOR_FWD(op)                                                                       \
      VIR_ALWAYS_INLINE friend constexpr auto                                                      \
      operator op(vectorized_struct const& a, vectorized_struct const& b)                          \
      requires requires(base_type const& x) { {x op x}; }                                          \
      {                                                                                            \
	return static_cast<_op_return_type<decltype(a._as_base_type() op b._as_base_type())>>(     \
		 a._as_base_type() op b._as_base_type());                                          \
      }                                                                                            \
                                                                                                   \
      VIR_ALWAYS_INLINE friend constexpr auto                                                      \
      operator op(base_type const& a, vectorized_struct const& b)                                  \
      requires requires(base_type const& x) { {x op x}; }                                          \
      {                                                                                            \
	return static_cast<_op_return_type<decltype(a op b._as_base_type())>>(                     \
		 a op b._as_base_type());                                                          \
      }                                                                                            \
                                                                                                   \
      VIR_ALWAYS_INLINE friend constexpr auto                                                      \
      operator op(vectorized_struct const& a, base_type const& b)                                  \
      requires requires(base_type const& x) { {x op x}; }                                          \
      {                                                                                            \
	return static_cast<_op_return_type<decltype(a._as_base_type() op b)>>(                     \
		 a._as_base_type() op b);                                                          \
      }

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

  /**
   * Implements structured bindings interface.
   *
   * \see std::tuple_size, std::tuple_element
   */
  template <std::size_t I, vectorizable_struct T, int N>
    constexpr decltype(auto)
    get(const vectorized_struct<T, N>& tup)
    {
      return vir::struct_get<I>(
	       static_cast<const detail::simdize_template_arguments_t<T, N>&>(tup));
    }

  template <std::size_t I, vectorizable_struct T, int N>
    constexpr decltype(auto)
    get(vectorized_struct<T, N>& tup)
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
  template <typename T, int N = 0>
    requires reflectable_struct<T> or vectorizable<T>
    using simdize = typename detail::simdize_impl<T, N>::type;

  template <int N, typename V>
    requires requires {
      V::size();
      typename V::value_type;
    } and (reflectable_struct<typename V::value_type> or vectorizable<typename V::value_type>)
    using resize_simdize_t = simdize<typename V::value_type, N>;
} // namespace vir

/**
 * Implements structured bindings interface for simd_tuple.
 */
template <vir::reflectable_struct T, int N>
  struct std::tuple_size<vir::simd_tuple<T, N>>
    : std::integral_constant<std::size_t, vir::struct_size_v<T>>
  {};

/**
 * Implements structured bindings interface for simd_tuple (std::tuple based).
 */
template <std::size_t I, vir::reflectable_struct T, int N>
  struct std::tuple_element<I, vir::simd_tuple<T, N>>
    : std::tuple_element<I, typename vir::detail::make_simd_tuple<T, N>::type>
  {};

/**
 * Implements structured bindings interface for simd_tuple (template argument substitution based).
 */
template <vir::vectorizable_struct T, int N>
  struct std::tuple_size<vir::vectorized_struct<T, N>>
    : std::integral_constant<std::size_t, vir::struct_size_v<T>>
  {};

template <std::size_t I, vir::vectorizable_struct T, int N>
  struct std::tuple_element<I, vir::vectorized_struct<T, N>>
    : vir::struct_element<I, vir::detail::simdize_template_arguments_t<T, N>>
  {};

#endif  // VIR_HAVE_STRUCT_REFLECT
#endif  // VIR_SIMD_SIMDIZE_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
