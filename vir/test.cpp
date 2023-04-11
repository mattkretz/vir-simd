/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

#include "vir/simd.h"
#include "vir/simdize.h"
#include "vir/simd_benchmarking.h"
#include "vir/simd_iota.h"

namespace stdx = vir::stdx;

template <typename T>
  using V = stdx::native_simd<T>;

template <typename T, std::size_t N>
  using DV = stdx::simd<T, stdx::simd_abi::deduce_t<T, N>>;

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
static_assert(all_of(vir::iota_v<V<short>> == V<short>([](short i) { return i; })));
#endif // VIR_HAVE_SIMD_IOTA

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
			   stdx::rebind_simd_t<double, V<int>>>);

static_assert(
  std::same_as<vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>,
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
    stdx::rebind_simd_t<int, V<short>>>);

static_assert(
  std::same_as<std::tuple_element_t<
		 1, vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>>,
	       V<short>>);

static_assert(
  std::same_as<std::tuple_element_t<
		 2, vir::simdize<std::tuple<std::tuple<int, double>, short, std::tuple<float>>>>,
	       vir::simd_tuple<std::tuple<float>, V<short>::size()>>);

struct Point
{
  float x, y, z;

  friend constexpr bool
  operator==(Point const&, Point const&) = default;
};

static_assert(vir::struct_size_v<Point> == 3);

static_assert(std::same_as<vir::as_tuple_t<Point>,
			   std::tuple<float, float, float>>);

static_assert(std::same_as<vir::simdize<Point>,
			   vir::simd_tuple<Point, V<float>::size()>>);

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

// vim: noet cc=101 tw=100 sw=2 ts=8
