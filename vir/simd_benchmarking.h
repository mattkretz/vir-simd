/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VIR_SIMD_BENCHMARKING_H_
#define VIR_SIMD_BENCHMARKING_H_

#if __cpp_concepts >= 201907
#include "simd.h"

#define VIR_HAVE_SIMD_BENCHMARKING 1

namespace vir
{
  template <typename T>
    [[gnu::always_inline]] inline void
    fake_modify_one(T& x)
    {
      if constexpr (std::is_floating_point_v<T>)
	asm volatile("" : "+x"(x));
      else if constexpr (stdx::is_simd_v<T> || stdx::is_simd_mask_v<T>)
	{
#if defined _GLIBCXX_EXPERIMENTAL_SIMD && __cpp_lib_experimental_parallel_simd >= 201803
	  auto& d = stdx::__data(x);
	  if constexpr (requires {{d._S_tuple_size};})
	    stdx::__for_each(d, [](auto, auto& element) {
	      fake_modify_one(element);
	    });
	  else if constexpr (requires {{d._M_data};})
	    fake_modify_one(d._M_data);
	  else
	    fake_modify_one(d);
#else
	  // assuming vir::stdx::simd implementation
	  if constexpr (sizeof(x) < 16)
	    asm volatile("" : "+g"(x));
	  else
	    asm volatile("" : "+x,g,m"(x));
#endif
	}
      else if constexpr (sizeof(x) >= 16)
	asm volatile("" : "+x"(x));
      else
	asm volatile("" : "+g"(x));
    }

  template <typename... Ts>
    [[gnu::always_inline]] inline void
    fake_modify(Ts&... more)
    { (fake_modify_one(more), ...); }

  template <typename T>
    [[gnu::always_inline]] inline void
    fake_read_one(const T& x)
    {
      if constexpr (std::is_floating_point_v<T>)
	asm volatile("" ::"x"(x));
      else if constexpr (stdx::is_simd_v<T> || stdx::is_simd_mask_v<T>)
	{
#if defined _GLIBCXX_EXPERIMENTAL_SIMD && __cpp_lib_experimental_parallel_simd >= 201803
	  const auto& d = stdx::__data(x);
	  if constexpr (requires {{d._S_tuple_size};})
	    stdx::__for_each(d, [](auto, const auto& element) {
	      fake_read_one(element);
	    });
	  else if constexpr (requires {{d._M_data};})
	    fake_read_one(d._M_data);
	  else
	    fake_read_one(d);
#else
	  // assuming vir::stdx::simd implementation
	  if constexpr (sizeof(x) < 16)
	    asm volatile("" ::"g"(x));
	  else
	    asm volatile("" ::"x,g,m"(x));
#endif
	}
      else if constexpr (sizeof(x) >= 16)
	asm volatile("" ::"x"(x));
      else
	asm volatile("" ::"g"(x));
    }

  template <typename... Ts>
    [[gnu::always_inline]] inline void
    fake_read(const Ts&... more)
    { (fake_read_one(more), ...); }

} // namespace vir
#endif  // __cpp_concepts
#endif  // VIR_SIMD_BENCHMARKING_H_

// vim: noet cc=101 tw=100 sw=2 ts=8
