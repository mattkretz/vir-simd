/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// expensive: * [1-9] * *
#include "bits/main.h"

enum unscoped_enum
{ foo };

enum class scoped_enum
{ bar };

struct convertible
{
  operator int();
  operator float();
};

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;
    VERIFY(std::experimental::is_simd_v<V>);
    VERIFY(std::experimental::is_abi_tag_v<typename V::abi_type>);

    {
      V x;     // not initialized
      x = V{}; // default broadcasts 0
      COMPARE(x, V(0));
      COMPARE(x, V());
      COMPARE(x, V{});
      x = V(); // default broadcasts 0
      COMPARE(x, V(0));
      COMPARE(x, V());
      COMPARE(x, V{});
      x = 0;
      COMPARE(x, V(0));
      COMPARE(x, V());
      COMPARE(x, V{});

      for (std::size_t i = 0; i < V::size(); ++i)
	{
	  COMPARE(T(x[i]), T(0)) << "i = " << i;
	  COMPARE(x[i], T(0)) << "i = " << i;
	}
    }

    V x = 3;
    V y = T(0);
    for (std::size_t i = 0; i < V::size(); ++i)
      {
	COMPARE(x[i], T(3)) << "i = " << i;
	COMPARE(y[i], T(0)) << "i = " << i;
      }
    y = 3;
    COMPARE(x, y);

    VERIFY(!(is_substitution_failure<V&, unscoped_enum, assignment>) );
    VERIFY((is_substitution_failure<V&, scoped_enum, assignment>) );
    COMPARE((is_substitution_failure<V&, convertible, assignment>),
	    (!std::is_convertible<convertible, T>::value));
    COMPARE((is_substitution_failure<V&, long double, assignment>),
	    (sizeof(long double) > sizeof(T) || std::is_integral<T>::value));
    COMPARE((is_substitution_failure<V&, double, assignment>),
	    (sizeof(double) > sizeof(T) || std::is_integral<T>::value));
    COMPARE((is_substitution_failure<V&, float, assignment>),
	    (sizeof(float) > sizeof(T) || std::is_integral<T>::value));
    COMPARE((is_substitution_failure<V&, long long, assignment>),
	    (has_less_bits<T, long long>() || std::is_unsigned<T>::value));
    COMPARE((is_substitution_failure<V&, unsigned long long, assignment>),
	    (has_less_bits<T, unsigned long long>()));
    COMPARE((is_substitution_failure<V&, long, assignment>),
	    (has_less_bits<T, long>() || std::is_unsigned<T>::value));
    COMPARE((is_substitution_failure<V&, unsigned long, assignment>),
	    (has_less_bits<T, unsigned long>()));
    // int broadcast *always* works:
    VERIFY(!(is_substitution_failure<V&, int, assignment>) );
    // uint broadcast works for any unsigned T:
    COMPARE((is_substitution_failure<V&, unsigned int, assignment>),
	    (!std::is_unsigned<T>::value && has_less_bits<T, unsigned int>()));
    COMPARE((is_substitution_failure<V&, short, assignment>),
	    (has_less_bits<T, short>() || std::is_unsigned<T>::value));
    COMPARE((is_substitution_failure<V&, unsigned short, assignment>),
	    (has_less_bits<T, unsigned short>()));
    COMPARE((is_substitution_failure<V&, signed char, assignment>),
	    (has_less_bits<T, signed char>() || std::is_unsigned<T>::value));
    COMPARE((is_substitution_failure<V&, unsigned char, assignment>),
	    (has_less_bits<T, unsigned char>()));
  }
