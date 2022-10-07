// Definition of numeric_limits replacement traits P1841R1 -*- C++ -*-

// Copyright (C) 2020-2022 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

#ifndef VIR_NUMERIC_TRAITS_H_
#define VIR_NUMERIC_TRAITS_H_

#include <type_traits>

namespace vir {

template <template <typename> class Trait, typename T, typename = void>
  struct value_exists_impl : std::false_type {};

template <template <typename> class Trait, typename T>
  struct value_exists_impl<Trait, T, std::void_t<decltype(Trait<T>::value)>>
  : std::true_type {};

template <typename T, bool = std::is_arithmetic_v<T>>
  struct digits_impl {};

template <typename T>
  struct digits_impl<T, true>
  {
    static inline constexpr int value
      = sizeof(T) * __CHAR_BIT__ - std::is_signed_v<T>;
  };

template <>
  struct digits_impl<float, true>
  { static inline constexpr int value = __FLT_MANT_DIG__; };

template <>
  struct digits_impl<double, true>
  { static inline constexpr int value = __DBL_MANT_DIG__; };

template <>
  struct digits_impl<long double, true>
  { static inline constexpr int value = __LDBL_MANT_DIG__; };

template <typename T, bool = std::is_arithmetic_v<T>>
  struct digits10_impl {};

template <typename T>
  struct digits10_impl<T, true>
  {
    // The fraction 643/2136 approximates log10(2) to 7 significant digits.
    static inline constexpr int value = digits_impl<T>::value * 643L / 2136;
  };

template <>
  struct digits10_impl<float, true>
  { static inline constexpr int value = __FLT_DIG__; };

template <>
  struct digits10_impl<double, true>
  { static inline constexpr int value = __DBL_DIG__; };

template <>
  struct digits10_impl<long double, true>
  { static inline constexpr int value = __LDBL_DIG__; };

template <typename T, bool = std::is_arithmetic_v<T>>
  struct max_digits10_impl {};

template <typename T>
  struct max_digits10_impl<T, true>
  {
    static inline constexpr int value
      = std::is_floating_point_v<T> ? 2 + digits_impl<T>::value * 643L / 2136
				 : digits10_impl<T>::value + 1;
  };

template <typename T>
  struct max_exponent_impl {};

template <>
  struct max_exponent_impl<float>
  { static inline constexpr int value = __FLT_MAX_EXP__; };

template <>
  struct max_exponent_impl<double>
  { static inline constexpr int value = __DBL_MAX_EXP__; };

template <>
  struct max_exponent_impl<long double>
  { static inline constexpr int value = __LDBL_MAX_EXP__; };

template <typename T>
  struct max_exponent10_impl {};

template <>
  struct max_exponent10_impl<float>
  { static inline constexpr int value = __FLT_MAX_10_EXP__; };

template <>
  struct max_exponent10_impl<double>
  { static inline constexpr int value = __DBL_MAX_10_EXP__; };

template <>
  struct max_exponent10_impl<long double>
  { static inline constexpr int value = __LDBL_MAX_10_EXP__; };

template <typename T>
  struct min_exponent_impl {};

template <>
  struct min_exponent_impl<float>
  { static inline constexpr int value = __FLT_MIN_EXP__; };

template <>
  struct min_exponent_impl<double>
  { static inline constexpr int value = __DBL_MIN_EXP__; };

template <>
  struct min_exponent_impl<long double>
  { static inline constexpr int value = __LDBL_MIN_EXP__; };

template <typename T>
  struct min_exponent10_impl {};

template <>
  struct min_exponent10_impl<float>
  { static inline constexpr int value = __FLT_MIN_10_EXP__; };

template <>
  struct min_exponent10_impl<double>
  { static inline constexpr int value = __DBL_MIN_10_EXP__; };

template <>
  struct min_exponent10_impl<long double>
  { static inline constexpr int value = __LDBL_MIN_10_EXP__; };

template <typename T, bool = std::is_arithmetic_v<T>>
  struct radix_impl {};

template <typename T>
  struct radix_impl<T, true>
  {
    static inline constexpr int value
      = std::is_floating_point_v<T> ? __FLT_RADIX__ : 2;
  };

// [num.traits.util], numeric utility traits
template <template <typename> class Trait, typename T>
  struct value_exists : value_exists_impl<Trait, T> {};

template <template <typename> class Trait, typename T>
  inline constexpr bool value_exists_v = value_exists<Trait, T>::value;

template <template <typename> class Trait, typename T, typename U = T>
  inline constexpr U
  value_or(U def = U()) noexcept
  {
    if constexpr (value_exists_v<Trait, T>)
      return static_cast<U>(Trait<T>::value);
    else
      return def;
  }

template <typename T, bool = std::is_arithmetic_v<T>>
  struct norm_min_impl {};

template <typename T>
  struct norm_min_impl<T, true>
  { static inline constexpr T value = 1; };

template <>
  struct norm_min_impl<float, true>
  { static inline constexpr float value = __FLT_MIN__; };

template <>
  struct norm_min_impl<double, true>
  { static inline constexpr double value = __DBL_MIN__; };

template <>
  struct norm_min_impl<long double, true>
  { static inline constexpr long double value = __LDBL_MIN__; };

template <typename T>
  struct denorm_min_impl : norm_min_impl<T> {};

#if __FLT_HAS_DENORM__
template <>
  struct denorm_min_impl<float>
  { static inline constexpr float value = __FLT_DENORM_MIN__; };
#endif

#if __DBL_HAS_DENORM__
template <>
  struct denorm_min_impl<double>
  { static inline constexpr double value = __DBL_DENORM_MIN__; };
#endif

#if __LDBL_HAS_DENORM__
template <>
  struct denorm_min_impl<long double>
  { static inline constexpr long double value = __LDBL_DENORM_MIN__; };
#endif

template <typename T>
  struct epsilon_impl {};

template <>
  struct epsilon_impl<float>
  { static inline constexpr float value = __FLT_EPSILON__; };

template <>
  struct epsilon_impl<double>
  { static inline constexpr double value = __DBL_EPSILON__; };

template <>
  struct epsilon_impl<long double>
  { static inline constexpr long double value = __LDBL_EPSILON__; };

template <typename T, bool = std::is_arithmetic_v<T>>
  struct finite_min_impl {};

template <typename T>
  struct finite_min_impl<T, true>
  {
    static inline constexpr T value
      = std::is_unsigned_v<T> ? T()
			   : -2 * (T(1) << digits_impl<T>::value - 1);
  };

template <>
  struct finite_min_impl<float, true>
  { static inline constexpr float value = -__FLT_MAX__; };

template <>
  struct finite_min_impl<double, true>
  { static inline constexpr double value = -__DBL_MAX__; };

template <>
  struct finite_min_impl<long double, true>
  { static inline constexpr long double value = -__LDBL_MAX__; };

template <typename T, bool = std::is_arithmetic_v<T>>
  struct finite_max_impl {};

template <typename T>
  struct finite_max_impl<T, true>
  { static inline constexpr T value = ~finite_min_impl<T>::value; };

template <>
  struct finite_max_impl<float, true>
  { static inline constexpr float value = __FLT_MAX__; };

template <>
  struct finite_max_impl<double, true>
  { static inline constexpr double value = __DBL_MAX__; };

template <>
  struct finite_max_impl<long double, true>
  { static inline constexpr long double value = __LDBL_MAX__; };

template <typename T>
  struct infinity_impl {};

#if __FLT_HAS_INFINITY__
template <>
  struct infinity_impl<float>
  { static inline constexpr float value = __builtin_inff(); };
#endif

#if __DBL_HAS_INFINITY__
template <>
  struct infinity_impl<double>
  { static inline constexpr double value = __builtin_inf(); };
#endif

#if __LDBL_HAS_INFINITY__
template <>
  struct infinity_impl<long double>
  { static inline constexpr long double value = __builtin_infl(); };
#endif

template <typename T>
  struct quiet_NaN_impl {};

#if __FLT_HAS_QUIET_NAN__
template <>
  struct quiet_NaN_impl<float>
  { static inline constexpr float value = __builtin_nanf(""); };
#endif

#if __DBL_HAS_QUIET_NAN__
template <>
  struct quiet_NaN_impl<double>
  { static inline constexpr double value = __builtin_nan(""); };
#endif

#if __LDBL_HAS_QUIET_NAN__
template <>
  struct quiet_NaN_impl<long double>
  { static inline constexpr long double value = __builtin_nanl(""); };
#endif

template <typename T, bool = std::is_floating_point_v<T>>
  struct reciprocal_overflow_threshold_impl {};

template <typename T>
  struct reciprocal_overflow_threshold_impl<T, true>
  {
    // This typically yields a subnormal value. Is this incorrect for
    // flush-to-zero configurations?
    static constexpr T search(T ok, T overflows)
    {
      const T mid = (ok + overflows) / 2;
      // 1/mid without -ffast-math is not a constant expression if it
      // overflows. Therefore divide 1 by the radix before division.
      // Consequently finite_max (the threshold) must be scaled by the
      // same value.
      if (mid == ok || mid == overflows)
	return ok;
      else if (T(1) / (radix_impl<T>::value * mid)
	       <= finite_max_impl<T>::value / radix_impl<T>::value)
	return search(mid, overflows);
      else
	return search(ok, mid);
    }

    static inline constexpr T value
      = search(T(1.01) / finite_max_impl<T>::value,
               T(0.99) / finite_max_impl<T>::value);
  };

template <typename T, bool = std::is_floating_point_v<T>>
  struct round_error_impl {};

template <typename T>
  struct round_error_impl<T, true>
  { static inline constexpr T value = 0.5; };

template <typename T>
  struct signaling_NaN_impl {};

#if __FLT_HAS_QUIET_NAN__
template <>
  struct signaling_NaN_impl<float>
  { static inline constexpr float value = __builtin_nansf(""); };
#endif

#if __DBL_HAS_QUIET_NAN__
template <>
  struct signaling_NaN_impl<double>
  { static inline constexpr double value = __builtin_nans(""); };
#endif

#if __LDBL_HAS_QUIET_NAN__
template <>
  struct signaling_NaN_impl<long double>
  { static inline constexpr long double value = __builtin_nansl(""); };
#endif

// [num.traits.val], numeric distinguished value traits
template <typename T>
  struct denorm_min : denorm_min_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct epsilon : epsilon_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct finite_max : finite_max_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct finite_min : finite_min_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct infinity : infinity_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct norm_min : norm_min_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct quiet_NaN : quiet_NaN_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct reciprocal_overflow_threshold
  : reciprocal_overflow_threshold_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct round_error : round_error_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct signaling_NaN : signaling_NaN_impl<std::remove_cv_t<T>> {};

template <typename T>
  inline constexpr auto denorm_min_v = denorm_min<T>::value;

template <typename T>
  inline constexpr auto epsilon_v = epsilon<T>::value;

template <typename T>
  inline constexpr auto finite_max_v = finite_max<T>::value;

template <typename T>
  inline constexpr auto finite_min_v = finite_min<T>::value;

template <typename T>
  inline constexpr auto infinity_v = infinity<T>::value;

template <typename T>
  inline constexpr auto norm_min_v = norm_min<T>::value;

template <typename T>
  inline constexpr auto quiet_NaN_v = quiet_NaN<T>::value;

template <typename T>
  inline constexpr auto reciprocal_overflow_threshold_v
    = reciprocal_overflow_threshold<T>::value;

template <typename T>
  inline constexpr auto round_error_v = round_error<T>::value;

template <typename T>
  inline constexpr auto signaling_NaN_v = signaling_NaN<T>::value;

// [num.traits.char], numeric characteristics traits
template <typename T>
  struct digits : digits_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct digits10 : digits10_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct max_digits10 : max_digits10_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct max_exponent : max_exponent_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct max_exponent10 : max_exponent10_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct min_exponent : min_exponent_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct min_exponent10 : min_exponent10_impl<std::remove_cv_t<T>> {};

template <typename T>
  struct radix : radix_impl<std::remove_cv_t<T>> {};

template <typename T>
  inline constexpr auto digits_v = digits<T>::value;

template <typename T>
  inline constexpr auto digits10_v = digits10<T>::value;

template <typename T>
  inline constexpr auto max_digits10_v = max_digits10<T>::value;

template <typename T>
  inline constexpr auto max_exponent_v = max_exponent<T>::value;

template <typename T>
  inline constexpr auto max_exponent10_v = max_exponent10<T>::value;

template <typename T>
  inline constexpr auto min_exponent_v = min_exponent<T>::value;

template <typename T>
  inline constexpr auto min_exponent10_v = min_exponent10<T>::value;

template <typename T>
  inline constexpr auto radix_v = radix<T>::value;

// mkretz's extensions
// TODO: does GCC tell me? __GCC_IEC_559 >= 2 is not the right answer
template <typename T>
  struct has_iec559_storage_format : std::true_type {};

template <typename T>
  inline constexpr bool has_iec559_storage_format_v
    = has_iec559_storage_format<T>::value;

/* To propose:
   If has_iec559_behavior<quiet_NaN, T> is true the following holds:
     - nan == nan is false
     - isnan(nan) is true
     - isnan(nan + x) is true
     - isnan(inf/inf) is true
     - isnan(0/0) is true
     - isunordered(nan, x) is true

   If has_iec559_behavior<infinity, T> is true the following holds (x is
   neither nan nor inf):
     - isinf(inf) is true
     - isinf(inf + x) is true
     - isinf(1/0) is true
 */
template <template <typename> class Trait, typename T>
  struct has_iec559_behavior : std::false_type {};

template <template <typename> class Trait, typename T>
  inline constexpr bool has_iec559_behavior_v
    = has_iec559_behavior<Trait, T>::value;

#if !__FINITE_MATH_ONLY__
#if __FLT_HAS_QUIET_NAN__
template <>
  struct has_iec559_behavior<quiet_NaN, float> : std::true_type {};
#endif

#if __DBL_HAS_QUIET_NAN__
template <>
  struct has_iec559_behavior<quiet_NaN, double> : std::true_type {};
#endif

#if __LDBL_HAS_QUIET_NAN__
template <>
  struct has_iec559_behavior<quiet_NaN, long double> : std::true_type {};
#endif

#if __FLT_HAS_INFINITY__
template <>
  struct has_iec559_behavior<infinity, float> : std::true_type {};
#endif

#if __DBL_HAS_INFINITY__
template <>
  struct has_iec559_behavior<infinity, double> : std::true_type {};
#endif

#if __LDBL_HAS_INFINITY__
template <>
  struct has_iec559_behavior<infinity, long double> : std::true_type {};
#endif

#ifdef __SUPPORT_SNAN__
#if __FLT_HAS_QUIET_NAN__
template <>
  struct has_iec559_behavior<signaling_NaN, float> : std::true_type {};
#endif

#if __DBL_HAS_QUIET_NAN__
template <>
  struct has_iec559_behavior<signaling_NaN, double> : std::true_type {};
#endif

#if __LDBL_HAS_QUIET_NAN__
template <>
  struct has_iec559_behavior<signaling_NaN, long double> : std::true_type {};
#endif

#endif
#endif // __FINITE_MATH_ONLY__

} // namespace vir

#endif // VIR_NUMERIC_TRAITS_H_
