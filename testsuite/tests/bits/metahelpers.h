/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VC_TESTS_METAHELPERS_H_
#define VC_TESTS_METAHELPERS_H_

#include <functional>
#include <type_traits>
#include <utility>

namespace vir
{
  namespace test
  {
    template <class A, class B, class Op>
      constexpr bool
      operator_is_substitution_failure_impl(float)
      { return true; }

    template <class A, class B, class Op>
      constexpr typename std::conditional<true, bool, decltype(
	  Op()(std::declval<A>(), std::declval<B>()))>::type
      operator_is_substitution_failure_impl(int)
      { return false; }

    template <class... Ts>
      constexpr bool
      operator_is_substitution_failure()
      { return operator_is_substitution_failure_impl<Ts...>(int()); }

    template <class... Args, class F>
      constexpr auto
      sfinae_is_callable_impl(int, F &&f) -> typename std::conditional<
	true, std::true_type,
	decltype(std::forward<F>(f)(std::declval<Args>()...))>::type;

    template <class... Args, class F>
      constexpr std::false_type
      sfinae_is_callable_impl(float, const F &);

    template <class... Args, class F>
      constexpr bool
      sfinae_is_callable(F &&)
      {
	return decltype(
	    sfinae_is_callable_impl<Args...>(int(), std::declval<F>()))::value;
      }

    template <class... Args, class F>
      constexpr auto sfinae_is_callable_t(F &&f)
	-> decltype(sfinae_is_callable_impl<Args...>(int(), std::declval<F>()));

    template <class A, class B>
      constexpr bool
      has_less_bits()
      { return vir::digits_v<A> < vir::digits_v<B>; }

  }  // namespace test
}  // namespace vir

struct assignment
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() = std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) = std::forward<B>(b)))
    { return std::forward<A>(a) = std::forward<B>(b); }
};

struct bit_shift_left
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() << std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) << std::forward<B>(b)))
    { return std::forward<A>(a) << std::forward<B>(b); }
};

struct bit_shift_right
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() >> std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) >> std::forward<B>(b)))
    { return std::forward<A>(a) >> std::forward<B>(b); }
};

struct assign_modulus
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() %= std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) %= std::forward<B>(b)))
    { return std::forward<A>(a) %= std::forward<B>(b); }
};

struct assign_bit_and
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() &= std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) &= std::forward<B>(b)))
    { return std::forward<A>(a) &= std::forward<B>(b); }
};

struct assign_bit_or
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() |= std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) |= std::forward<B>(b)))
    { return std::forward<A>(a) |= std::forward<B>(b); }
};

struct assign_bit_xor
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() ^= std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) ^= std::forward<B>(b)))
    { return std::forward<A>(a) ^= std::forward<B>(b); }
};

struct assign_bit_shift_left
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() <<= std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) <<= std::forward<B>(b)))
    { return std::forward<A>(a) <<= std::forward<B>(b); }
};

struct assign_bit_shift_right
{
  template <class A, class B>
    constexpr decltype(std::declval<A>() >>= std::declval<B>())
    operator()(A &&a, B &&b) const noexcept(noexcept(
	std::forward<A>(a) >>= std::forward<B>(b)))
    { return std::forward<A>(a) >>= std::forward<B>(b); }
};

template <class A, class B, class Op = std::plus<>>
  constexpr bool is_substitution_failure
    = vir::test::operator_is_substitution_failure<A, B, Op>();

using vir::test::sfinae_is_callable;

using vir::test::has_less_bits;

#endif  // VC_TESTS_METAHELPERS_H_
