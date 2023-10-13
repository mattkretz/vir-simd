/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2018-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                           Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_STRUCT_SIZE_H_
#define VIR_STRUCT_SIZE_H_

#if defined __cpp_structured_bindings && defined __cpp_concepts && __cpp_concepts >= 201907
#define VIR_HAVE_STRUCT_REFLECT 1
#include <utility>
#include <tuple>
#include <climits>

namespace vir
{
  namespace detail
  {
    using std::remove_cvref_t;
    using std::size_t;

    // struct_size implementation
    template <typename Struct>
      struct anything_but_base_of
      {
	anything_but_base_of() = default;

	anything_but_base_of(const anything_but_base_of&) = delete;

	template <typename T>
	  requires (not std::is_base_of_v<T, Struct>)
	  operator T&() const&;
      };

    template <typename Struct>
      struct any_empty_base_of
      {
	template <typename T>
	  requires (std::is_base_of_v<T, Struct> and std::is_empty_v<T>)
	  operator T();
      };

    template <typename T, size_t... Indexes>
      concept brace_constructible_impl
	= requires { T{{((void)Indexes, anything_but_base_of<T>())}...}; }
	    or requires
	  {
	    T{any_empty_base_of<T>(), {((void)Indexes, anything_but_base_of<T>())}...};
	  }
	    or requires
	  {
	    T{any_empty_base_of<T>(), any_empty_base_of<T>(),
	      {((void)Indexes, anything_but_base_of<T>())}...};
	  }
	    or requires
	  {
	    T{any_empty_base_of<T>(), any_empty_base_of<T>(), any_empty_base_of<T>(),
	      {((void)Indexes, anything_but_base_of<T>())}...};
	  };

    template <typename T, size_t N>
      concept aggregate_with_n_members = []<size_t... Is>(std::index_sequence<Is...>) {
	return brace_constructible_impl<T, Is...>;
      }(std::make_index_sequence<N>());

    template <typename T, size_t Lo = 0, size_t Hi = sizeof(T) * CHAR_BIT, size_t N = 8>
      constexpr size_t
      struct_size()
      {
	constexpr size_t left_index = Lo + (N - Lo) / 2;
	constexpr size_t right_index = N + (Hi - N + 1) / 2;
	if constexpr (aggregate_with_n_members<T, N>) // N is a valid size
	  {
	    if constexpr (N == Hi)
	      // we found the best valid size inside [Lo, Hi]
	      return N;
	    else
	      { // there may be a larger size in [N+1, Hi]
		constexpr size_t right = struct_size<T, N + 1, Hi, right_index>();
		if constexpr (right == 0)
		  // no, there isn't
		  return N;
		else
		  // yes, there is
		  return right;
	      }
	  }
	else if constexpr (N == Lo)
	  { // we can only look for a valid size in [N+1, Hi]
	    if constexpr (N == Hi)
	      // but [N+1, Hi] is empty
	      return 0; // no valid size found
	    else
	      // [N+1, Hi] is non-empty
	      return struct_size<T, N + 1, Hi, right_index>();
	  }
	else
	  { // the answer could be in [Lo, N-1] or [N+1, Hi]
	    constexpr size_t left = struct_size<T, Lo, N - 1, left_index>();
	    if constexpr (left > 0)
	      // valid size in [Lo, N-1] => there can't be a valid size in [N+1, Hi] anymore
	      return left;
	    else if constexpr (N == Hi)
	      // [N+1, Hi] is empty => [Lo, Hi] has no valid size
	      return 0;
	    else
	      // there can only be a valid size in [N+1, Hi], if there is none the
	      // recursion returns 0 anyway
	      return struct_size<T, N + 1, Hi, right_index>();
	  }
      }

    // struct_get implementation
    template <size_t Total>
      struct struct_get;

    template <typename T>
      struct remove_ref_in_tuple;

    template <typename... Ts>
      struct remove_ref_in_tuple<std::tuple<Ts...>>
      { using type = std::tuple<std::remove_reference_t<Ts>...>; };

    template <typename T1, typename T2>
      struct remove_ref_in_tuple<std::pair<T1, T2>>
      { using type = std::pair<std::remove_reference_t<T1>, std::remove_reference_t<T2>>; };

    template <>
      struct struct_get<0>
      {
	template <typename T>
	  constexpr std::tuple<>
	  to_tuple_ref(T &&)
	  { return {}; }

	template <typename T>
	  constexpr std::tuple<>
	  to_tuple(const T &)
	  { return {}; }
      };

    template <>
      struct struct_get<1>
      {
	template <typename T>
	  constexpr auto
	  to_tuple_ref(T &&obj)
	  {
	    auto && [a] = obj;
	    return std::forward_as_tuple(a);
	  }

	template <typename T>
	  constexpr auto
	  to_tuple(const T &obj)
	  -> typename remove_ref_in_tuple<decltype(to_tuple_ref(std::declval<T>()))>::type
	  {
	    const auto &[a] = obj;
	    return {a};
	  }

	template <size_t N, typename T>
	  constexpr const auto &
	  get(const T &obj)
	  {
	    static_assert(N == 0);
	    auto && [a] = obj;
	    return a;
	  }

	template <size_t N, typename T>
	  constexpr auto &
	  get(T &obj)
	  {
	    static_assert(N == 0);
	    auto && [a] = obj;
	    return a;
	  }
      };

    template <>
      struct struct_get<2>
      {
	template <typename T>
	  constexpr auto
	  to_tuple_ref(T &&obj)
	  {
	    auto &&[a, b] = obj;
	    return std::forward_as_tuple(a, b);
	  }

	template <typename T>
	  constexpr auto
	  to_tuple(const T &obj)
	  -> typename remove_ref_in_tuple<decltype(to_tuple_ref(std::declval<T>()))>::type
	  {
	    const auto &[a, b] = obj;
	    return {a, b};
	  }

	template <typename T>
	  constexpr auto
	  to_pair_ref(T &&obj)
	  {
	    auto &&[a, b] = obj;
	    return std::pair<decltype((a)), decltype((b))>(a, b);
	  }

	template <typename T>
	  constexpr auto
	  to_pair(const T &obj)
	  -> typename remove_ref_in_tuple<decltype(to_pair_ref(std::declval<T>()))>::type
	  {
	    const auto &[a, b] = obj;
	    return {a, b};
	  }

	template <size_t N, typename T>
	  constexpr auto &
	  get(T &&obj)
	  {
	    static_assert(N < 2);
	    auto &&[a, b] = obj;
	    return std::get<N>(std::forward_as_tuple(a, b));
	  }
      };

#define VIR_STRUCT_GET_(size_, ...)                                                      \
  template <>                                                                            \
    struct struct_get<size_>                                                             \
    {                                                                                    \
      template <typename T>                                                              \
	constexpr auto                                                                   \
	to_tuple_ref(T &&obj)                                                            \
	{                                                                                \
	  auto &&[__VA_ARGS__] = obj;                                                    \
	  return std::forward_as_tuple(__VA_ARGS__);                                     \
	}                                                                                \
      \
      template <typename T>                                                              \
	constexpr auto                                                                   \
	to_tuple(const T &obj)                                                           \
	-> typename remove_ref_in_tuple<decltype(to_tuple_ref(std::declval<T>()))>::type \
	{                                                                                \
	  const auto &[__VA_ARGS__] = obj;                                               \
	  return {__VA_ARGS__};                                                          \
	}                                                                                \
      \
      template <size_t N, typename T>                                                    \
	constexpr auto &                                                                 \
	get(T &&obj)                                                                     \
	{                                                                                \
	  static_assert(N < size_);                                                      \
	  auto &&[__VA_ARGS__] = obj;                                                    \
	  return std::get<N>(std::forward_as_tuple(__VA_ARGS__));                        \
	}                                                                                \
    }
    VIR_STRUCT_GET_(3, x0, x1, x2);
    VIR_STRUCT_GET_(4, x0, x1, x2, x3);
    VIR_STRUCT_GET_(5, x0, x1, x2, x3, x4);
    VIR_STRUCT_GET_(6, x0, x1, x2, x3, x4, x5);
    VIR_STRUCT_GET_(7, x0, x1, x2, x3, x4, x5, x6);
    VIR_STRUCT_GET_(8, x0, x1, x2, x3, x4, x5, x6, x7);
    VIR_STRUCT_GET_(9, x0, x1, x2, x3, x4, x5, x6, x7, x8);
    VIR_STRUCT_GET_(10, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9);
    VIR_STRUCT_GET_(11, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10);
    VIR_STRUCT_GET_(12, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11);
    VIR_STRUCT_GET_(13, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12);
    VIR_STRUCT_GET_(14, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13);
    VIR_STRUCT_GET_(15, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14);
    VIR_STRUCT_GET_(16, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15);
    VIR_STRUCT_GET_(17, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16);
    VIR_STRUCT_GET_(18, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17);
    VIR_STRUCT_GET_(19, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18);
    VIR_STRUCT_GET_(20, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19);
    VIR_STRUCT_GET_(21, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20);
    VIR_STRUCT_GET_(22, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21);
    VIR_STRUCT_GET_(23, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22);
    VIR_STRUCT_GET_(24, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23);
    VIR_STRUCT_GET_(25, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24);
    VIR_STRUCT_GET_(26, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25);
    VIR_STRUCT_GET_(27, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26);
    VIR_STRUCT_GET_(28, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27);
    VIR_STRUCT_GET_(29, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28);
    VIR_STRUCT_GET_(30, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29);
    VIR_STRUCT_GET_(31, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29,
		    x30);
    VIR_STRUCT_GET_(32, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31);
    VIR_STRUCT_GET_(33, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32);
    VIR_STRUCT_GET_(34, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33);
    VIR_STRUCT_GET_(35, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34);
    VIR_STRUCT_GET_(36, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35);
    VIR_STRUCT_GET_(37, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36);
    VIR_STRUCT_GET_(38, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37);
    VIR_STRUCT_GET_(39, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38);
    VIR_STRUCT_GET_(40, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39);
    VIR_STRUCT_GET_(41, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40);
    VIR_STRUCT_GET_(42, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41);
    VIR_STRUCT_GET_(43, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42);
    VIR_STRUCT_GET_(44, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43);
    VIR_STRUCT_GET_(45, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44);
    VIR_STRUCT_GET_(46, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44,
		    x45);
    VIR_STRUCT_GET_(47, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45,
		    x46);
    VIR_STRUCT_GET_(48, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45,
		    x46, x47);
    VIR_STRUCT_GET_(49, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45,
		    x46, x47, x48);
    VIR_STRUCT_GET_(50, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
		    x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
		    x31, x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45,
		    x46, x47, x48, x49);
#undef VIR_STRUCT_GET_

    // concept definitions
    template <typename T>
      concept has_tuple_size
	= requires(T x, std::tuple_size<T> ts) { {static_cast<int>(ts)} -> std::same_as<int>; };

    template <typename T>
      concept aggregate_without_tuple_size
	= std::is_aggregate_v<T> and not has_tuple_size<T>
	    and requires (const T& x) { detail::struct_size<T>(); };

    // traits
    template <typename From, typename To>
      struct copy_cvref
      {
	using From2 = std::remove_reference_t<From>;
	using with_const = std::conditional_t<std::is_const_v<From2>, const To, To>;
	using with_volatile
	  = std::conditional_t<std::is_volatile_v<From2>, volatile with_const, with_const>;
	using type = std::conditional_t<std::is_lvalue_reference_v<From>, with_volatile &,
					std::conditional_t<std::is_rvalue_reference_v<From>,
							   with_volatile &&, with_volatile>>;
      };
  }  // namespace detail

  /**
   * Satisfied if \p T can be used with the following functions and types.
   */
  template <typename T>
    concept reflectable_struct = detail::has_tuple_size<std::remove_cvref_t<T>>
				   or detail::aggregate_without_tuple_size<std::remove_cvref_t<T>>;

  /**
   * The value of struct_size_v is the number of non-static data members of the type \p T.
   * More precisely, struct_size_v is the number of elements in the identifier-list of a
   * structured binding declaration.
   *
   * \tparam T An aggregate type or a type specializing `std::tuple_size` that can be
   *           destructured via a structured binding. This implies that either \p T has only
   *           empty bases (only up to 3 are supported) or \p T has no non-static data
   *           members and a single base class with non-static data members. Using
   *           aggregate types that do not support destructuring is ill-formed. Using
   *           non-aggregate types that do not support destructuring results in a
   *           substitution failure.
   */
  template <typename T>
    requires (detail::aggregate_without_tuple_size<T> or detail::has_tuple_size<T>)
    constexpr inline std::size_t struct_size_v = 0;

  template <detail::aggregate_without_tuple_size T>
    constexpr inline std::size_t struct_size_v<T> = detail::struct_size<T>();

  template <detail::has_tuple_size T>
    constexpr inline std::size_t struct_size_v<T> = std::tuple_size_v<T>;

  /**
   * Returns a cv-qualified reference to the \p N -th non-static data member in \p obj.
   */
  template <std::size_t N, reflectable_struct T>
    requires (N < struct_size_v<std::remove_cvref_t<T>>)
    constexpr decltype(auto)
    struct_get(T &&obj)
    {
      return detail::struct_get<struct_size_v<std::remove_cvref_t<T>>>()
	       .template get<N>(std::forward<T>(obj));
    }

  template <std::size_t N, reflectable_struct T>
    struct struct_element
    {
      using type = std::remove_reference_t<decltype(struct_get<N>(std::declval<T &>()))>;
    };

  /**
   * `struct_element_t` is an alias for the type of the \p N -th non-static data member of
   * \p T.
   */
  template <std::size_t N, reflectable_struct T>
    using struct_element_t
      = std::remove_reference_t<decltype(struct_get<N>(std::declval<T &>()))>;

  /**
   * Returns a std::tuple with a copy of all the non-static data members of \p obj.
   */
  template <reflectable_struct T>
    constexpr auto
    to_tuple(T &&obj)
    {
      return detail::struct_get<struct_size_v<std::remove_cvref_t<T>>>().to_tuple(
	       std::forward<T>(obj));
    }

  /**
   * Returns a std::tuple of lvalue references to all the non-static data members of \p obj.
   */
  template <reflectable_struct T>
    constexpr auto
    to_tuple_ref(T &&obj)
    {
      return detail::struct_get<struct_size_v<std::remove_cvref_t<T>>>().to_tuple_ref(
	       std::forward<T>(obj));
    }

  /**
   * Defines the member type \p type to a std::tuple specialization matching the non-static
   * data members of \p T.
   */
  template <reflectable_struct T>
    struct as_tuple
    : detail::copy_cvref<
	T, decltype(detail::struct_get<struct_size_v<std::remove_cvref_t<T>>>().to_tuple(
		      std::declval<T>()))>
    {};

  /**
   * Alias for a std::tuple specialization matching the non-static data members of \p T.
   */
  template <typename T>
    using as_tuple_t = typename as_tuple<T>::type;

  /**
   * Returns a std::pair with a copy of all the non-static data members of \p obj.
   */
  template <typename T>
    requires(struct_size_v<std::remove_cvref_t<T>> == 2)
    constexpr auto
    to_pair(T &&obj)
    { return detail::struct_get<2>().to_pair(std::forward<T>(obj)); }

  /**
   * Returns a std::pair of lvalue references to all the non-static data members of \p obj.
   */
  template <typename T>
    requires(struct_size_v<std::remove_cvref_t<T>> == 2)
    constexpr auto
    to_pair_ref(T &&obj)
    { return detail::struct_get<2>().to_pair_ref(std::forward<T>(obj)); }

  /**
   * Defines the member type \p type to a std::pair specialization matching the non-static
   * data members of \p T.
   */
  template <typename T>
    requires (struct_size_v<std::remove_cvref_t<T>> == 2)
    struct as_pair
    : detail::copy_cvref<T, decltype(detail::struct_get<2>().to_pair(std::declval<T>()))>
    {};

  /**
   * Alias for a std::pair specialization matching the non-static data members of \p T.
   */
  template <typename T>
    using as_pair_t = typename as_pair<T>::type;

}  // namespace vir

#endif // structured bindings & concepts
#endif // VIR_STRUCT_SIZE_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
