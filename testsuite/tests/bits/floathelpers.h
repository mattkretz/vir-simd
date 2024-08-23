/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2021–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef BITS_FLOATHELPERS_H_
#define BITS_FLOATHELPERS_H_
#include <cfenv>
#include "verify.h"

#if (math_errhandling & MATH_ERREXCEPT) && defined FE_INEXACT
class FloatExceptCompare
{
  int first_state;
  int second_state;

public:
  static inline bool ignore = false;
  static inline int ignore_spurious = 0;
  static inline int ignore_missing = 0;

  FloatExceptCompare()
  { reset(); }

  // call after first math function invocation
  [[gnu::noinline]]
  void
  record_first()
  {
    if (!ignore)
      {
	first_state = std::fetestexcept(FE_ALL_EXCEPT);
	std::feclearexcept(FE_ALL_EXCEPT);
      }
  }

  [[gnu::noinline]]
  void
  record_second()
    {
      if (!ignore)
	{
	  second_state = std::fetestexcept(FE_ALL_EXCEPT);
	  std::feclearexcept(FE_ALL_EXCEPT);
	}
    }

  // call after second math function invocation with different implementation but equal arguments
  template <typename... More>
    [[gnu::always_inline]]
    void
    verify_equal_state(const char* file, const int line, More&&... moreinfo) const
    {
      if (!ignore)
	{
	  const int difference = second_state ^ first_state;
	  if (difference != 0)
	    {
	      if((second_state | (first_state & ignore_spurious))
		   ^ (first_state | (second_state & ignore_missing)))
		COMPARE(first_state, second_state)
		.on_failure("\ndifference = ", difference,
			    difference & FE_INEXACT ? " FE_INEXACT" : "",
			    difference & FE_UNDERFLOW ? " FE_UNDERFLOW" : "",
			    difference & FE_OVERFLOW ? " FE_OVERFLOW" : "",
			    difference & FE_DIVBYZERO ? " FE_DIVBYZERO" : "",
			    difference & FE_INVALID ? " FE_INVALID" : "",
			    '\n', file, ':', line, ": called from here.",
			    moreinfo...);
	    }
	}
    }

  [[gnu::noinline]]
  void
  reset()
  {
    first_state = -1;
    second_state = -1;
    std::feclearexcept(FE_ALL_EXCEPT);
  }
};

#else
class FloatExceptCompare
{
public:
  static inline bool ignore = true;
  static inline int ignore_spurious = 0;
  static inline int ignore_missing = 0;

  void record_first() {}

  void record_second() {}

  template <typename... More>
    void verify_equal_state(const char*, const int, More&&... ) const {}

  void reset() {}
};

#endif
#endif // BITS_FLOATHELPERS_H_
