/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef VC_TESTS_SIMD_VIEW_H_
#define VC_TESTS_SIMD_VIEW_H_

#include <vir/simd.h>

namespace vir
{
  namespace imported_begin_end
  {
    using std::begin;
    using std::end;

    template <class T>
      using begin_type = decltype(begin(std::declval<T>()));

    template <class T>
      using end_type = decltype(end(std::declval<T>()));
  }  // namespace imported_begin_end

  template <class V, class It, class End>
    class viewer
    {
      It it;
      const End end;

      template <class F>
	void
	for_each_impl(F &&fun, std::index_sequence<0, 1, 2>)
	{
	  for (; it + V::size() <= end; it += V::size())
	    {
	      fun(V([&](auto i) { return std::get<0>(it[i].as_tuple()); }),
		  V([&](auto i) { return std::get<1>(it[i].as_tuple()); }),
		  V([&](auto i) { return std::get<2>(it[i].as_tuple()); }));
	    }
	  if (it != end)
	    {
	      fun(V([&](auto i)
	      {
		auto ii = it + i < end ? i + 0 : 0;
		return std::get<0>(it[ii].as_tuple());
	      }),
		  V([&](auto i) {
		    auto ii = it + i < end ? i + 0 : 0;
		    return std::get<1>(it[ii].as_tuple());
		  }),
		  V([&](auto i) {
		    auto ii = it + i < end ? i + 0 : 0;
		    return std::get<2>(it[ii].as_tuple());
		  }));
	    }
	}

      template <class F>
	void
	for_each_impl(F &&fun, std::index_sequence<0, 1>)
	{
	  for (; it + V::size() <= end; it += V::size())
	    {
	      fun(V([&](auto i) { return std::get<0>(it[i].as_tuple()); }),
		  V([&](auto i) { return std::get<1>(it[i].as_tuple()); }));
	    }
	  if (it != end)
	    {
	      fun(V([&](auto i) {
		auto ii = it + i < end ? i + 0 : 0;
		return std::get<0>(it[ii].as_tuple());
	      }),
		  V([&](auto i) {
		    auto ii = it + i < end ? i + 0 : 0;
		    return std::get<1>(it[ii].as_tuple());
		  }));
	    }
	}

    public:
      viewer(It _it, End _end)
      : it(_it), end(_end) {}

      template <class F>
	void
	for_each(F &&fun)
	{
	  constexpr size_t N
	    = std::tuple_size<std::decay_t<decltype(it->as_tuple())>>::value;
	  for_each_impl(std::forward<F>(fun), std::make_index_sequence<N>());
	}
    };

  template <class V, class Cont>
    viewer<V, imported_begin_end::begin_type<const Cont &>,
	   imported_begin_end::end_type<const Cont &>>
    simd_view(const Cont &data)
    {
      using std::begin;
      using std::end;
      return {begin(data), end(data)};
    }
}  // namespace vir

#endif  // VC_TESTS_SIMD_VIEW_H_
