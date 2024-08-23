/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef SIMD_TESTS_BITS_MAKE_VEC_H_
#define SIMD_TESTS_BITS_MAKE_VEC_H_
#include <vir/simd.h>

template <class M>
  inline M
  make_mask(const std::initializer_list<bool> &init)
  {
    std::size_t i = 0;
    M r = {};
    for (;;)
      {
	for (bool x : init)
	  {
	    r[i] = x;
	    if (++i == M::size())
	      {
		return r;
	      }
	  }
      }
  }

template <class M>
  M
  make_alternating_mask()
  {
    return make_mask<M>({false, true});
  }

template <class V>
  inline V
  make_vec(const std::initializer_list<typename V::value_type> &init,
	   typename V::value_type inc = 0)
  {
    std::size_t i = 0;
    V r = {};
    typename V::value_type base = 0;
    for (;;)
      {
	for (auto x : init)
	  {
	    r[i] = base + x;
	    if (++i == V::size())
	      {
		return r;
	      }
	  }
	base += inc;
      }
  }
#endif  // SIMD_TESTS_BITS_MAKE_VEC_H_
