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

  namespace detail
  {
    template <typename V, bool... Values>
      inline constexpr typename V::mask_type mask_constant
	= typename V::mask_type(std::array<bool, sizeof...(Values)>{Values...}.data(),
				stdx::element_aligned);

    template <typename T, int N, unsigned Bytes = sizeof(T) * std::bit_ceil(unsigned(N))>
      using gnu_vector [[gnu::vector_size(Bytes)]] = T;

#ifdef __GNUC__
    template <int... Indexes>
      VIR_ALWAYS_INLINE
      inline auto
      blend_ps(auto a, auto b)
      {
	static_assert(sizeof(a) == sizeof(b));
	static_assert(sizeof...(Indexes) * sizeof(float) <= sizeof(a));
	static_assert(((Indexes <= sizeof(a) / sizeof(float)) && ...));
#ifdef __SSE4_1__
	if constexpr (sizeof(a) == 16)
	  return __builtin_ia32_blendps(a, b, ((1 << Indexes) | ...));
#endif
#ifdef __AVX__
	if constexpr (sizeof(a) == 32)
	  return __builtin_ia32_blendps256(a, b, ((1 << Indexes) | ...));
#endif
#ifdef __AVX512F__
	if constexpr (sizeof(a) == 64)
	  return __builtin_ia32_blendmps_512_mask(a, b, ((1 << Indexes) | ...));
#endif
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

    template <std::size_t N, template <typename...> class Tpl, typename... Ts>
      Tpl<typename simdize_impl<Ts, N>::type...>
      simdize_template_arguments_impl(const Tpl<Ts...>&);

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
    template <reflectable_struct Tup, int N>
      requires (vir::struct_size_v<Tup> > 0 and not vectorizable_struct<Tup>)
      struct simdize_impl<Tup, N>
      {
	static_assert(requires { typename simdize_impl<vir::struct_element_t<0, Tup>, N>::type; });

	using type = simd_tuple<Tup, N == 0 ? default_simdize_size_v<Tup> : N>;
      };

    template <vectorizable_struct T, int N>
      struct simdize_impl<T, N>
      { using type = simd_tuple<T, N == 0 ? default_simdize_size_v<T> : N>; };
  } // namespace detail

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

  template <std::size_t I, reflectable_struct T, int N>
    constexpr decltype(auto)
    get(const simd_tuple<T, N>& tup)
    { return std::get<I>(tup.as_tuple()); }

  template <std::size_t I, reflectable_struct T, int N>
    constexpr decltype(auto)
    get(simd_tuple<T, N>& tup)
    { return std::get<I>(tup.as_tuple()); }

  template <vectorizable_struct T, int N>
    class simd_tuple<T, N> : public detail::simdize_template_arguments_t<T, N>
    {
      using tuple_type = typename detail::make_simd_tuple<T, N>::type;
      using base_type = detail::simdize_template_arguments_t<T, N>;

      static constexpr auto struct_size = vir::struct_size_v<T>;
      static constexpr auto struct_size_idx_seq = std::make_index_sequence<struct_size>();

    public:
      using value_type = T;
      using mask_type = typename struct_element_t<0, base_type>::mask_type;

      static constexpr auto size = vir::cw<N>;

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

      VIR_ALWAYS_INLINE
      static constexpr base_type
      _load_elements_via_permute(const T* addr)
      {
	static_assert(sizeof(T) * N == sizeof(base_type));
#ifdef __GNUC__
	const std::byte* byte_ptr = reinterpret_cast<const std::byte*>(addr);
	if constexpr (std::same_as<vir::as_tuple_t<T>, std::tuple<float, float, float>>)
	  {
#ifdef __AVX__
	    using float8 = detail::deduced_simd<float, 8>;
	    using v8sf [[gnu::vector_size(32)]] = float;
	    if constexpr (std::same_as<tuple_type, std::tuple<float8, float8, float8>>
			    and std::constructible_from<float8, v8sf>)
	      {
		// Implementation notes:
		// 1. Three gather instructions with indexes 0, 3, 6, 9, 12, 15, 18, 21 is super
		//    slow
		// 2. Eight 3/4-element loads -> concat to 8 elements -> unpack also much slower

		//                               abc   abc abc
		// a = [a0 b0 c0 a1 b1 c1 a2 b2] 332 = 211+121
		// b = [c2 a3 b3 c3 a4 b4 c4 a5] 323 = 112+211
		// c = [b5 c5 a6 b6 c6 a7 b7 c7] 233 = 121+112

		if constexpr (true) // allow_unordered
		  {
		    v8sf x0, x1, x2, a0, b0, c0;
		    std::memcpy(&x0, byte_ptr +  0, 32);
		    std::memcpy(&x1, byte_ptr + 32, 32);
		    std::memcpy(&x2, byte_ptr + 64, 32);

		    a0 = detail::blend_ps<1, 4, 7>(x0, x1);
		    a0 = detail::blend_ps<2, 5>(a0, x2);
		    // a0 a3 a6 a1 a4 a7 a2 a5

		    b0 = detail::blend_ps<2, 5>(x0, x1);
		    b0 = detail::blend_ps<0, 3, 6>(b0, x2);
		    // b5 b0 b3 b6 b1 b4 b7 b2

		    c0 = detail::blend_ps<0, 3, 6>(x0, x1);
		    c0 = detail::blend_ps<1, 4, 7>(c0, x2);
		    // c2 c5 c0 c3 c6 c1 c4 c7

		    float8 a(a0);
		    float8 b = simd_permute(float8(b0), simd_permutations::rotate<1>);
		    float8 c = simd_permute(float8(c0), simd_permutations::rotate<2>);
		    return base_type {a, b, c};
		  }
		else
		  {
		    float8 a, b, c;
		    std::memcpy(&a, byte_ptr +  0, 32);
		    std::memcpy(&b, byte_ptr + 32, 32);
		    std::memcpy(&c, byte_ptr + 64, 32);

		    // a0 b0 c0 a1 b5 c5 a6 b6
		    float8 ac0(_mm256_permute2f128_ps(static_cast<__m256>(a),
						      static_cast<__m256>(c), 0 + (2 << 4)));

		    // b1 c1 a2 b2 c6 a7 b7 c7
		    float8 ac1(_mm256_permute2f128_ps(static_cast<__m256>(a),
						      static_cast<__m256>(c), 1 + (3 << 4)));

		    using M = typename float8::mask_type;
		    constexpr M k036 = detail::mask_constant<float8, 1, 0, 0, 1, 0, 0, 1, 0>;
		    constexpr M k147 = detail::mask_constant<float8, 0, 1, 0, 0, 1, 0, 0, 1>;
		    constexpr M k25  = detail::mask_constant<float8, 0, 0, 1, 0, 0, 1, 0, 0>;
		    // a0 a3 a2 a1 a4 a7 a6 a5
		    float8 tmp0 = ac0;
		    where(k147, tmp0) = b;
		    where(k25, tmp0) = ac1;

		    // b1 b0 b3 b2 b5 b4 b7 b6
		    float8 tmp1 = ac0;
		    where(k25, tmp1) = b;
		    where(k036, tmp1) = ac1;

		    // c2 c1 c0 c3 c6 c5 c4 c7
		    float8 tmp2 = ac0;
		    where(k036, tmp2) = b;
		    where(k147, tmp2) = ac1;

		    return {
		      simd_permute(tmp0, [](auto i) { return std::array{0, 3, 2, 1, 4, 7, 6, 5}[i]; }),
		      simd_permute(tmp1, [](auto i) { return std::array{1, 0, 3, 2, 5, 4, 7, 6}[i]; }),
		      simd_permute(tmp2, [](auto i) { return std::array{2, 1, 0, 3, 6, 5, 4, 7}[i]; })
		    };
		  }
	      }
#endif
#ifdef __SSE__
	    using float4 = detail::deduced_simd<float, 4>;
	    if constexpr (std::same_as<tuple_type, std::tuple<float4, float4, float4>>
			    and std::constructible_from<float4, detail::gnu_vector<float, 4>>)
	      {
		// abca = [a0 b0 c0 a1]
		// bcab = [b1 c1 a2 b2]
		// cabc = [c2 a3 b3 c3]

		detail::gnu_vector<float, 4> abca, bcab, cabc, a, b, c;
		std::memcpy(&abca, byte_ptr +  0, 16);
		std::memcpy(&bcab, byte_ptr + 16, 16);
		std::memcpy(&cabc, byte_ptr + 32, 16);

		a = __builtin_ia32_shufps(cabc, bcab, (0 << 0) + (1 << 2) + (2 << 4) + (3 << 6));
		a = __builtin_ia32_shufps(abca,    a, (0 << 0) + (3 << 2) + (2 << 4) + (1 << 6));

		b = __builtin_ia32_shufps(
		      __builtin_shuffle(abca, bcab, detail::gnu_vector<int, 4>{4,1,2,3}), // b1 b0 . .
		      __builtin_ia32_movhlps(bcab, cabc), // b3 . . b2
		      (1 << 0) + (0 << 2) + (3 << 4) + (0 << 6));

		c = __builtin_ia32_shufps(bcab, abca, (0 << 0) + (1 << 2) + (2 << 4) + (3 << 6));
		c = __builtin_ia32_shufps(c,    cabc, (2 << 0) + (1 << 2) + (0 << 4) + (3 << 6));

		return base_type {float4(a), float4(b), float4(c)};
	      }
#endif
	  }
#endif // __GNUC__

	// not optimized fallback
	return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
	  return base_type {std::tuple_element_t<Is, tuple_type>([&](auto i) {
			      return struct_get<Is>(addr[i]);
			    })...};
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
	: base_type(_load_elements_via_permute(std::to_address(it)))
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

  template <std::size_t I, vectorizable_struct T, int N>
    constexpr decltype(auto)
    get(const simd_tuple<T, N>& tup)
    {
      return vir::struct_get<I>(
	       static_cast<const detail::simdize_template_arguments_t<T, N>&>(tup));
    }

  template <std::size_t I, vectorizable_struct T, int N>
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
  template <typename T, int N = 0>
    requires reflectable_struct<T> or vectorizable<T>
    using simdize = typename detail::simdize_impl<T, N>::type;

} // namespace vir

template <vir::reflectable_struct T, int N>
  struct std::tuple_size<vir::simd_tuple<T, N>>
    : std::integral_constant<std::size_t, vir::struct_size_v<T>>
  {};

template <std::size_t I, vir::reflectable_struct T, int N>
  struct std::tuple_element<I, vir::simd_tuple<T, N>>
    : std::tuple_element<I, typename vir::detail::make_simd_tuple<T, N>::type>
  {};

#endif  // VIR_HAVE_STRUCT_REFLECT
#endif  // VIR_SIMD_SIMDIZE_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
