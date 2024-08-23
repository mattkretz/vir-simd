/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// expensive: * [1-9] * *
#include "bits/main.h"

template <class V>
  struct call_generator
  {
    template <class F>
      auto
      operator()(const F& f) -> decltype(V(f));
  };

using schar = signed char;
using uchar = unsigned char;
using ullong = unsigned long long;

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;
    V x([](int) { return T(1); });
    COMPARE(x, V(1));
    // unconditionally returns int from generator lambda
    x = V([](int) { return 1; });
    COMPARE(x, V(1));
    x = V([](auto i) { return T(i); });
    COMPARE(x, V([](T i) { return i; }));

    VERIFY((// that int always works
	sfinae_is_callable<int (&)(int)>(call_generator<V>())));
    COMPARE(sfinae_is_callable<schar (&)(int)>(call_generator<V>()),
	    std::is_signed<T>::value);
    COMPARE(sfinae_is_callable<uchar (&)(int)>(call_generator<V>()),
	    !(std::is_signed_v<T> && sizeof(T) <= sizeof(uchar)));
    COMPARE(sfinae_is_callable<float (&)(int)>(call_generator<V>()),
	    (std::is_floating_point<T>::value));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-const-int-float-conversion"
    COMPARE(sfinae_is_callable<ullong (&)(int)>(call_generator<V>()),
      vir::finite_max_v<T> >= vir::finite_max_v<ullong>
      && vir::digits_v<T> >= vir::digits_v<ullong>);
#pragma clang diagnostic pop
  }
