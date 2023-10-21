/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_CONSTEXPR_WRAPPER_H_
#define VIR_CONSTEXPR_WRAPPER_H_

#if defined __cpp_concepts && __cpp_concepts >= 201907 && __has_include(<concepts>)
#define VIR_HAVE_CONSTEXPR_WRAPPER 1
#include <concepts>
#include <type_traits>
#include <utility>
#include <charconv>

namespace vir
{
  /**
   * Implements P2781 with modifications:
   *
   * - Overload operator-> to return value.operator->() if well-formed, otherwise act like a pointer
   *   dereference to underlying value (allows calling functions on wrapped type).
   *
   * - Overload operator() to act like integral_constant::operator(), unless value() is
   *   well-formed.
   */
  template <auto Xp, typename = std::remove_cvref_t<decltype(Xp)>>
    struct constexpr_wrapper;

  // *** constexpr_value<T, U = void> ***
  // If U is given, T must be convertible to U (`constexpr_value<int> auto` is analogous to `int`).
  // If U is void (default), the type of the value is unconstrained.
  template <typename Tp, typename Up = void>
    concept constexpr_value = (std::same_as<Up, void> or std::convertible_to<Tp, Up>)
#if defined __GNUC__ and __GNUC__ < 13
                                and not std::is_member_pointer_v<decltype(&Tp::value)>
#endif
                                and requires { typename constexpr_wrapper<Tp::value>; };

  namespace detail
  {
    // exposition-only
    // Note: `not _any_constexpr_wrapper` is not equivalent to `not derived_from<T,
    // constexpr_wrapper<T::value>>` because of SFINAE (i.e. SFINAE would be reversed)
    template <typename Tp>
      concept _any_constexpr_wrapper = std::derived_from<Tp, constexpr_wrapper<Tp::value>>;

    // exposition-only
    // Concept that enables declaring a single binary operator overload per operation:
    // 1. LHS is derived from This, then RHS is either also derived from This (and there's only a
    //    single candidate) or RHS is not a constexpr_wrapper (LHS provides the only operator
    //    candidate).
    // 2. LHS is not a constexpr_wrapper, then RHS is derived from This (RHS provides the only
    //    operator candidate).
    template <typename Tp, typename This>
      concept _lhs_constexpr_wrapper
        = constexpr_value<Tp> and (std::derived_from<Tp, This> or not _any_constexpr_wrapper<Tp>);
  }

  // Prefer to use `constexpr_value<type> auto` instead of `template <typename T> void
  // f(constexpr_wrapper<T, type> ...` to constrain the type of the constant.
  template <auto Xp, typename Tp>
    struct constexpr_wrapper
    {
      using value_type = Tp;

      using type = constexpr_wrapper;

      static constexpr value_type value{ Xp };

      constexpr
      operator value_type() const
      { return Xp; }

      // overloads the following:
      //
      // unary:
      // + - ~ ! & * ++ -- ->
      //
      // binary:
      // + - * / % & | ^ && || , << >> == != < <= > >= ->* = += -= *= /= %= &= |= ^= <<= >>=
      //
      // [] and () are overloaded for constexpr_value or not constexpr_value, the latter not
      // wrapping the result in constexpr_wrapper

      // The overload of -> is inconsistent because it cannot wrap its return value in a
      // constexpr_wrapper. The compiler/standard requires a pointer type. However, -> can work if
      // it unwraps. The utility is questionable though, which it why may be interesting to
      // "dereferencing" the wrapper to its value if Tp doesn't implement operator->.
      constexpr const auto*
      operator->() const
      {
        if constexpr (requires{value.operator->();})
          return value.operator->();
        else
          return std::addressof(value);
      }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<+Yp>
        operator+() const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<-Yp>
        operator-() const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<~Yp>
        operator~() const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<!Yp>
        operator!() const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<&Yp>
        operator&() const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<*Yp>
        operator*() const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<++Yp>
        operator++() const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<Yp++>
        operator++(int) const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<--Yp>
        operator--() const
        { return {}; }

      template <auto Yp = Xp>
        constexpr constexpr_wrapper<Yp-->
        operator--(int) const
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value + Bp::value>
        operator+(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value - Bp::value>
        operator-(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value * Bp::value>
        operator*(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value / Bp::value>
        operator/(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value % Bp::value>
        operator%(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value & Bp::value>
        operator&(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value | Bp::value>
        operator|(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value ^ Bp::value>
        operator^(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value && Bp::value>
        operator&&(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<Ap::value || Bp::value>
        operator||(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value , Bp::value)>
        operator,(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value << Bp::value)>
        operator<<(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value >> Bp::value)>
        operator>>(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value == Bp::value)>
        operator==(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value != Bp::value)>
        operator!=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value < Bp::value)>
        operator<(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value <= Bp::value)>
        operator<=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value > Bp::value)>
        operator>(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value >= Bp::value)>
        operator>=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value <=> Bp::value)>
        operator<=>(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value ->* Bp::value)>
        operator->*(Ap, Bp)
        { return {}; }

      template <constexpr_value Ap>
        requires requires { typename constexpr_wrapper<(Xp = Ap::value)>; }
        constexpr auto
        operator=(Ap) const
        { return constexpr_wrapper<(Xp = Ap::value)>{}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value += Bp::value)>
        operator+=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value -= Bp::value)>
        operator-=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value *= Bp::value)>
        operator*=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value /= Bp::value)>
        operator/=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value %= Bp::value)>
        operator%=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value &= Bp::value)>
        operator&=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value |= Bp::value)>
        operator|=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value ^= Bp::value)>
        operator^=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value <<= Bp::value)>
        operator<<=(Ap, Bp)
        { return {}; }

      template <detail::_lhs_constexpr_wrapper<constexpr_wrapper> Ap, constexpr_value Bp>
        friend constexpr constexpr_wrapper<(Ap::value >>= Bp::value)>
        operator>>=(Ap, Bp)
        { return {}; }

#if defined __cpp_static_call_operator && __cplusplus >= 202302L
#define _static static
#define _const
#else
#define _static
#define _const const
#endif

      // overload operator() for constexpr_value and non-constexpr_value
      template <constexpr_value... Args>
        requires requires { typename constexpr_wrapper<value(Args::value...)>; }
        _static constexpr auto
        operator()(Args...) _const
        { return constexpr_wrapper<value(Args::value...)>{}; }

      template <typename... Args>
        requires (not constexpr_value<std::remove_cvref_t<Args>> || ...)
        _static constexpr decltype(value(std::declval<Args>()...))
        operator()(Args&&... _args) _const
        { return value(std::forward<Args>(_args)...); }

      _static constexpr Tp
      operator()() _const
      requires (not requires { value(); })
        { return value; }

      // overload operator[] for constexpr_value and non-constexpr_value
#if __cpp_multidimensional_subscript
      template <constexpr_value... Args>
        _static constexpr constexpr_wrapper<value[Args::value...]>
        operator[](Args...) _const
        { return {}; }

      template <typename... Args>
        requires (not constexpr_value<std::remove_cvref_t<Args>> || ...)
        _static constexpr decltype(value[std::declval<Args>()...])
        operator[](Args&&... _args) _const
        { return value[std::forward<Args>(_args)...]; }
#else
      template <constexpr_value Arg>
        _static constexpr constexpr_wrapper<value[Arg::value]>
        operator[](Arg) _const
        { return {}; }

      template <typename Arg>
        requires (not constexpr_value<std::remove_cvref_t<Arg>>)
        _static constexpr decltype(value[std::declval<Arg>()])
        operator[](Arg&& _arg) _const
        { return value[std::forward<Arg>(_arg)]; }
#endif

#undef _static
#undef _const
    };

  // constexpr_wrapper constant (cw)
  template <auto Xp>
    inline constexpr constexpr_wrapper<Xp> cw{};

  namespace detail
  {
    struct cw_base_and_offset_result
    {
      int base;
      int offset;
    };

    enum class cw_status
    {
      okay,
      try_next_type,
      error
    };

    template <typename T>
      struct cw_result
      {
        T value;
        cw_status status;
      };

    template <typename T, auto size>
      consteval cw_result<T>
      do_parse(const char* arr, cw_base_and_offset_result bao)
      {
        T x = {};
        const auto result = std::from_chars(arr + bao.offset, arr + size, x, bao.base);
        if (result.ec == std::errc::result_out_of_range)
          return cw_result<T>{x, cw_status::try_next_type};
        else if (result.ec != std::errc {} or result.ptr != arr + size)
          return cw_result<T>{x, cw_status::error};
        else
          return cw_result<T>{x, cw_status::okay};
      }

    template <char... Chars>
      consteval auto
      cw_parse()
      {
        constexpr auto size = sizeof...(Chars);
        constexpr char arr[size] = {Chars...};
        constexpr auto bao = [&]() -> cw_base_and_offset_result {
          if constexpr (arr[0] == '0' and 2 < size)
            {
              constexpr bool is_hex = arr[1] == 'x' or arr[1] == 'X';
              constexpr bool is_binary = arr[1] == 'b';

              if constexpr (is_hex)
                return {16, 2};
              else if constexpr (is_binary)
                return {2, 2};
              else
                return {8, 1};
            }
          else
            return {10, 0};
        }();
        constexpr cw_result result0 = do_parse<signed char, size>(arr, bao);
        if constexpr (result0.status == cw_status::try_next_type)
          {
            constexpr cw_result result1 = do_parse<signed short, size>(arr, bao);
            if constexpr (result1.status == cw_status::try_next_type)
              {
                constexpr cw_result result2 = do_parse<signed int, size>(arr, bao);
                if constexpr (result2.status == cw_status::try_next_type)
                  {
                    constexpr cw_result result3 = do_parse<signed long, size>(arr, bao);
                    if constexpr (result3.status == cw_status::try_next_type)
                      {
                        constexpr cw_result result4 = do_parse<signed long long, size>(arr, bao);
                        if constexpr (result4.status == cw_status::try_next_type)
                          {
                            constexpr cw_result result5 = do_parse<unsigned long long, size>(arr, bao);
                            if constexpr (result5.status == cw_status::try_next_type)
                              {
                                double x = {};
                                const auto result = std::from_chars(arr, arr + size, x);
                                if (result.ec == std::errc {} and result.ptr == arr + size)
                                  return x;
                              }
                            else if constexpr (result5.status != cw_status::error)
                              return result5.value;
                          }
                        else if constexpr (result4.status != cw_status::error)
                              return result4.value;
                      }
                    else if constexpr (result3.status != cw_status::error)
                      return result3.value;
                  }
                else if constexpr (result2.status != cw_status::error)
                  return result2.value;
              }
            else if constexpr (result1.status != cw_status::error)
              return result1.value;
          }
        else if constexpr (result0.status != cw_status::error)
          return result0.value;
        throw;
      }
  }

  namespace literals
  {
    template <char... Chars>
      constexpr auto operator"" _cw()
      { return vir::cw<vir::detail::cw_parse<Chars...>()>; }
  }

} // namespace vir

#endif // has concepts
#endif // VIR_CONSTEXPR_WRAPPER_H_
// vim: et tw=100 ts=8 sw=2 cc=101
