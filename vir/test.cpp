/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#include "vir/simd.h"
#include "vir/simdize.h"
#include "vir/simd_benchmarking.h"
#include "vir/simd_iota.h"
#include "vir/simd_cvt.h"
#include "vir/simd_permute.h"
#include "vir/simd_execution.h"

namespace stdx = vir::stdx;

template <typename T>
  using V = stdx::native_simd<T>;

template <typename T, std::size_t N>
  using DV = stdx::simd<T, stdx::simd_abi::deduce_t<T, N>>;

template <typename T, typename U>
  using RV = stdx::rebind_simd_t<T, V<U>>;

template <typename T, typename... Ts>
  constexpr stdx::resize_simd_t<1 + sizeof...(Ts), stdx::simd<T>>
  make_simd(T v0, Ts... values)
  {
    using V = stdx::resize_simd_t<1 + sizeof...(Ts), stdx::simd<T>>;
    return V([&](auto i) {
	     return (std::array<T, V::size()> {v0, static_cast<T>(values)...})[i];
	   });
  }

// work around all_of(simd_mask) not really being constexpr in libstdc++
template <typename T, typename U>
  constexpr bool
  all_equal(const T a, const U b)
  {
    if constexpr (std::is_convertible_v<T, U> and not std::is_convertible_v<U, T>)
      return all_equal<U, U>(a, b);
    else if constexpr (not std::is_convertible_v<T, U> and std::is_convertible_v<U, T>)
      return all_equal<T, T>(a, b);
    else if constexpr (std::is_same_v<T, U> and stdx::is_simd_v<T>)
      {
	for (std::size_t i = 0; i < T::size(); ++i)
	  {
	    if (a[i] != b[i])
	      return false;
	  }
	return true;
      }
    else
      return a == b;
  }

#if VIR_HAVE_SIMD_IOTA
//arithmetic
static_assert(vir::iota_v<int> == 0);
static_assert(vir::iota_v<float> == 0.f);
// array
static_assert(vir::iota_v<int[4]>[0] == 0);
static_assert(vir::iota_v<int[4]>[1] == 1);
static_assert(vir::iota_v<int[4]>[2] == 2);
static_assert(vir::iota_v<int[4]>[3] == 3);
static_assert(vir::iota_v<std::array<int, 5>> == std::array<int, 5>{0, 1, 2, 3, 4});
// simd
static_assert(vir::iota_v<V<int>>[0] == 0);
static_assert(all_equal(vir::iota_v<V<short>>, V<short>([](short i) { return i; })));
#endif // VIR_HAVE_SIMD_IOTA

#if VIR_HAVE_SIMD_CVT
namespace
{
  using vir::cvt;
  // simd:
  static_assert(all_equal(cvt(RV<int, float>(2)) * V<float>(1), V<float>(2)));
  static_assert(all_equal(cvt(10 * RV<float, int>(cvt(V<int>(2)))), V<int>(20)));
  static_assert(all_equal([](auto x) -> RV<float, int> {
		  auto y = cvt(x);
		  return y;
		}(V<int>(1)), 1.f));
  // simd_mask:
  static_assert(std::same_as<decltype(cvt(RV<int, float>(2) == 2) == (V<float>(1) == 1.f)),
			     typename V<float>::mask_type>);
  // arithmetic:
  static_assert(float(cvt(1)) == 1.f);
}
#endif // VIR_HAVE_SIMD_CVT

#if VIR_HAVE_SIMD_PERMUTE and VIR_HAVE_SIMD_IOTA
static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::duplicate_even),
	    make_simd(0, 0, 2, 2)));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::duplicate_odd),
	    make_simd(1, 1, 3, 3)));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::swap_neighbors<>),
	    make_simd(1, 0, 3, 2)));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::swap_neighbors<2>),
	    make_simd(2, 3, 0, 1)));

#if !defined __cpp_lib_experimental_parallel_simd || _GLIBCXX_RELEASE > 11
static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3, 4, 5), vir::simd_permutations::swap_neighbors<3>),
	    make_simd(3, 4, 5, 0, 1, 2)));
#endif

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::broadcast_first), 0));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::broadcast_last), 3));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::broadcast<2>), 2));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::reverse),
	    make_simd(3, 2, 1, 0)));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::rotate<1>),
	    make_simd(1, 2, 3, 0)));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::rotate<-2>),
	    make_simd(2, 3, 0, 1)));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::rotate<-3>),
	    make_simd(1, 2, 3, 0)));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::shift<1>),
	    make_simd(1, 2, 3, 0)));

static_assert(
  all_equal(vir::simd_permute(make_simd(0, 1, 2, 3), vir::simd_permutations::shift<2>),
	    make_simd(2, 3, 0, 0)));

static_assert(
  all_equal(vir::simd_permute(make_simd(5, 1, 2, 3), vir::simd_permutations::shift<-1>),
	    make_simd(0, 5, 1, 2)));

static_assert(
  all_equal(vir::simd_permute(make_simd(5, 1, 2, 3), vir::simd_permutations::shift<-2>),
	    make_simd(0, 0, 5, 1)));

static_assert(
  all_equal(vir::simd_permute(make_simd(5, 1, 2, 3), vir::simd_permutations::shift<-3>),
	    make_simd(0, 0, 0, 5)));

static_assert(
  all_equal(vir::simd_permute<8>(make_simd<short>(0, 1, 2, 3), [](unsigned i) { return i % 3; }),
	    make_simd<short>(0, 1, 2, 0, 1, 2, 0, 1)));

static_assert(vir::simd_permute(2, [](unsigned i) { return i; }) == 2);

static_assert(all_equal(vir::simd_permute<8>(short(2), [](unsigned i) {
			  return (i & 1) ? 0 : vir::simd_permute_zero;
			}), make_simd<short>(0, 2, 0, 2, 0, 2, 0, 2)));

static_assert(all_equal(vir::simd_shift_in<1>(make_simd(0, 1, 2, 3), make_simd(4, 5, 6, 7)),
			make_simd(1, 2, 3, 4)));
#endif // VIR_HAVE_SIMD_PERMUTE

#if VIR_HAVE_STRUCT_REFLECT

static_assert(not vir::reflectable_struct<int>);
static_assert(vir::reflectable_struct<std::tuple<>>);
static_assert(vir::reflectable_struct<std::tuple<int>>);
static_assert(vir::reflectable_struct<std::tuple<int&>>);
static_assert(vir::reflectable_struct<std::tuple<const int&>>);
static_assert(vir::reflectable_struct<std::tuple<const int>>);
static_assert(vir::reflectable_struct<std::array<int, 0>>);
static_assert(vir::reflectable_struct<std::array<int, 2>>);
static_assert(vir::reflectable_struct<std::pair<int, short>>);

template <typename T>
  struct A
  {
    T a, b, c;
  };

static_assert(vir::struct_size_v<A<float>> == 3);
static_assert(vir::struct_size_v<A<std::tuple<float, stdx::simd<int>>>> == 3);
static_assert(vir::struct_size_v<A<stdx::native_simd<float>>> == 3);

#if VIR_HAVE_SIMDIZE

static_assert(std::same_as<vir::simdize<std::tuple<int, double>>,
			   vir::simd_tuple<std::tuple<int, double>, V<int>::size()>>);

static_assert(std::same_as<std::tuple_element_t<0, vir::simdize<std::tuple<int, double>>>,
			   V<int>>);

static_assert(std::same_as<std::tuple_element_t<1, vir::simdize<std::tuple<int, double>>>,
			   RV<double, int>>);

static_assert(
  std::same_as<vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>,
	       vir::simd_tuple<std::tuple<std::tuple<int, double>, short, std::tuple<float>>,
			       V<short>::size()>>);

static_assert(
  std::convertible_to<vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>,
		      vir::simd_tuple<std::tuple<std::tuple<int, double>, short, std::tuple<float>>,
				      V<short>::size()>>);

static_assert(
  std::same_as<std::tuple_element_t<
		 0, vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>>,
	       vir::simd_tuple<std::tuple<int, double>, V<short>::size()>>);

static_assert(
  std::same_as<
    std::tuple_element_t<
      0, std::tuple_element_t<
	   0, vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>>>,
    RV<int, short>>);

static_assert(
  std::same_as<std::tuple_element_t<
		 1, vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>>,
	       V<short>>);

static_assert(
  std::same_as<std::tuple_element_t<
		 2, vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>>,
	       vir::simd_tuple<std::tuple<float>, V<short>::size()>>);

template <typename T>
struct PointTpl
{
  T x, y, z;

  friend constexpr bool
  operator==(PointTpl const&, PointTpl const&) = default;
};

using Point = PointTpl<float>;

static_assert(vir::struct_size_v<Point> == 3);

static_assert(std::same_as<vir::as_tuple_t<Point>,
			   std::tuple<float, float, float>>);

static_assert(std::same_as<vir::simdize<Point>,
			   vir::simd_tuple<Point, V<float>::size()>>);

static_assert(std::convertible_to<vir::simdize<Point>, vir::simd_tuple<Point, V<float>::size()>>);

static_assert(std::convertible_to<vir::simd_tuple<Point, V<float>::size()>, vir::simdize<Point>>);

static_assert(std::convertible_to<PointTpl<V<float>>, vir::simdize<Point>>);

static_assert(std::convertible_to<vir::simdize<Point>, PointTpl<V<float>>>);

static_assert(not std::convertible_to<vir::simdize<Point>, PointTpl<V<double>>>);

/* Only with std::simd / C++26:
static_assert(std::constructible_from<vir::simdize<Point>, PointTpl<RV<double, float>>>);
static_assert(std::constructible_from<PointTpl<RV<double, float>>, vir::simdize<Point>>);
 */

static_assert(std::same_as<std::tuple_element_t<0, vir::simdize<Point>>, V<float>>);
static_assert(std::same_as<std::tuple_element_t<1, vir::simdize<Point>>, V<float>>);
static_assert(std::same_as<std::tuple_element_t<2, vir::simdize<Point>>, V<float>>);

static_assert(vir::struct_size_v<vir::simdize<Point>> == 3);

static_assert(vir::reflectable_struct<vir::simdize<Point>>);

static_assert(vir::simdize_size_v<vir::simdize<Point>> == stdx::native_simd<float>::size());

static_assert(std::same_as<typename vir::simdize<Point>::mask_type,
			   typename V<float>::mask_type>);

struct Line
{
  Point a, b;
};

static_assert(vir::struct_size_v<Line> == 2);

static_assert(std::same_as<vir::as_tuple_t<Line>,
			   std::tuple<Point, Point>>);

static_assert(std::same_as<vir::simdize<Line>,
			   vir::simd_tuple<Line, V<float>::size()>>);

#if VIR_HAVE_SIMD_IOTA
constexpr vir::simdize<Point> point{vir::iota_v<V<float>>, 2.f, 3.f};
static_assert(point[0].x == 0.f);
static_assert(point[0].y == 2.f);
static_assert(point[0].z == 3.f);
static_assert([]() {
  constexpr vir::simdize<Point> point{vir::iota_v<V<float>>, 2.f, 3.f};
  for (std::size_t i = 0; i < V<float>::size(); ++i)
    if (point[i] != Point {float(i), 2.f, 3.f})
      return false;
  return true;
}());
#endif  // VIR_HAVE_SIMD_IOTA

static_assert(vir::simdize<Point>(Point{2.f, 1.f, 0.f})[0] == Point{2.f, 1.f, 0.f});

#endif  // VIR_HAVE_SIMDIZE
#endif  // VIR_HAVE_STRUCT_REFLECT

#if VIR_HAVE_SIMD_BENCHMARKING
void
f()
{
  V<float> x {};
  vir::fake_modify(x);
  x += 1;
  vir::fake_read(x);
}
#endif  // VIR_HAVE_SIMD_BENCHMARKING

#if VIR_HAVE_SIMD_EXECUTION && _GLIBCXX_RELEASE >= 13
// needs recent libstdc++ for better constexpr support
namespace algorithms_tests
{
  static_assert(vir::execution::simd._size == 0);
  static_assert(vir::execution::simd.prefer_size<4>()._size == 4);
  static_assert(vir::execution::simd.prefer_aligned().prefer_size<4>()._size == 4);
  static_assert(vir::execution::simd._unroll_by == 0);
  static_assert(vir::execution::simd.unroll_by<3>()._unroll_by == 3);
  static_assert(vir::execution::simd.prefer_aligned().unroll_by<5>()._unroll_by == 5);
  static_assert(vir::execution::simd._prefers_aligned == false);
  static_assert(vir::execution::simd.prefer_aligned()._prefers_aligned == true);
  static_assert(vir::execution::simd.prefer_size<4>().prefer_aligned()._prefers_aligned == true);
  static_assert(vir::execution::simd._auto_prologue == false);
  static_assert(vir::execution::simd.auto_prologue()._auto_prologue == true);
  static_assert(vir::execution::simd.prefer_size<4>().auto_prologue()._auto_prologue == true);
  static_assert([] {
    std::array input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    vir::for_each(vir::execution::simd, input, [](auto& v) {
      using T = std::remove_reference_t<decltype(v)>;
      if (v[0] == 1)
	 {
	   if (std::same_as<T, V<int>>)
	     v += 2;
	 }
      else
	v += 2;
    });
    if (input != std::array{3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21})
      return false;
    vir::for_each(vir::execution::simd.prefer_aligned(), input, [](auto v) {
      v += 2;
    });
    if (input != std::array{3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21})
      return false;

    const int c = vir::count_if(vir::execution::simd, input, [](auto v) {
      return (v & 1) == 1;
    });
    return c == 10;
  }());
}
#endif  // VIR_HAVE_SIMD_EXECUTION

// vim: noet cc=101 tw=100 sw=2 ts=8
