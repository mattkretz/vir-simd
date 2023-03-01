// Copyright (C) 2020-2023 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

// only: float|double|ldouble * * *
// expensive: * [1-9] * *
#include "bits/main.h"

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;

    vir::test::setFuzzyness<float>(1);
    vir::test::setFuzzyness<double>(1);
    vir::test::setFuzzyness<long double>(1);
    FloatExceptCompare::ignore = true; // https://sourceware.org/bugzilla/show_bug.cgi?id=27460
    test_values_2arg<V>(
      {
#ifdef __STDC_IEC_559__
	vir::quiet_NaN_v<T>, vir::infinity_v<T>, -vir::infinity_v<T>, -0.,
	vir::denorm_min_v<T>, vir::norm_min_v<T> / 3,
#endif
	+0., vir::norm_min_v<T>, 1., 2., vir::finite_max_v<T> / 5,
	vir::finite_max_v<T> / 3, vir::finite_max_v<T> / 2,
#ifdef __FAST_MATH__
	// fast-math hypot is imprecise for the max exponent
      },
      {100000, vir::finite_max_v<T> / 2},
#else
	vir::finite_max_v<T>},
      {100000},
#endif
      MAKE_TESTER(hypot));
    FloatExceptCompare::ignore = false;
#if !__FINITE_MATH_ONLY__
    COMPARE(hypot(V(vir::finite_max_v<T>), V(vir::finite_max_v<T>)),
	    V(vir::infinity_v<T>));
#endif
    COMPARE(hypot(V(vir::norm_min_v<T>), V(vir::norm_min_v<T>)),
	    V(vir::norm_min_v<T> * std::sqrt(T(2))));
    VERIFY((sfinae_is_callable<V, V>(
	  [](auto a, auto b) -> decltype(hypot(a, b)) { return {}; })));
    VERIFY((sfinae_is_callable<typename V::value_type, V>(
	  [](auto a, auto b) -> decltype(hypot(a, b)) { return {}; })));
    VERIFY((sfinae_is_callable<V, typename V::value_type>(
	  [](auto a, auto b) -> decltype(hypot(a, b)) { return {}; })));

    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);
    vir::test::setFuzzyness<long double>(0);
    test_values_2arg<V>(
      {
#ifdef __STDC_IEC_559__
	vir::quiet_NaN_v<T>, vir::infinity_v<T>, -vir::infinity_v<T>,
	vir::denorm_min_v<T>, vir::norm_min_v<T> / 3, -0.,
#endif
	+0., vir::norm_min_v<T>, vir::finite_max_v<T>},
      {10000}, MAKE_TESTER(pow), MAKE_TESTER(fmod), MAKE_TESTER(remainder),
      MAKE_TESTER(copysign),
      MAKE_TESTER(nextafter), // MAKE_TESTER(nexttoward),
      MAKE_TESTER(fdim), MAKE_TESTER(fmax), MAKE_TESTER(fmin),
      MAKE_TESTER(isgreater), MAKE_TESTER(isgreaterequal),
      MAKE_TESTER(isless), MAKE_TESTER(islessequal),
      MAKE_TESTER(islessgreater), MAKE_TESTER(isunordered));
  }
