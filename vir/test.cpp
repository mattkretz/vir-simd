/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright © 2023–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#include "simd.h"
#include "simdize.h"
#include "simd_benchmarking.h"
#include "simd_iota.h"
#include "simd_cvt.h"
#include "simd_permute.h"
#include "simd_execution.h"

#include <complex>

// GCC 11.4, 12.4, 13.2, 14.1, ...
#if !defined __cpp_lib_experimental_parallel_simd || __GLIBCXX__ >= 20230528
#define SIMD_IS_CONSTEXPR_ENOUGH 1
#else
#define SIMD_IS_CONSTEXPR_ENOUGH 0
#endif

static_assert(vir::simd_version >= vir::simd_version_t{0,3,100});
static_assert(vir::simd_version != vir::simd_version_t{0,3,0});
static_assert(vir::simd_version >= vir::simd_version_t{0,3,0});
static_assert(vir::simd_version >  vir::simd_version_t{0,3,0});
static_assert(vir::simd_version <= vir::simd_version_t{0,vir::simd_version.minor + 1,0});
static_assert(vir::simd_version <  vir::simd_version_t{0,vir::simd_version.minor + 1,0});

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

#if SIMD_IS_CONSTEXPR_ENOUGH
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
namespace test_struct_reflect
{
  static_assert(not vir::reflectable_struct<int>);
  static_assert(vir::reflectable_struct<std::tuple<>>);
  static_assert(vir::reflectable_struct<std::tuple<int>>);
  static_assert(vir::reflectable_struct<std::tuple<int&>>);
  static_assert(vir::reflectable_struct<std::tuple<const int&>>);
  static_assert(vir::reflectable_struct<std::tuple<const int>>);
  static_assert(vir::reflectable_struct<std::array<int, 0>>);
  static_assert(vir::reflectable_struct<std::array<int, 2>>);
  static_assert(vir::reflectable_struct<std::pair<int, short>>);
  static_assert(not vir::reflectable_struct<std::complex<float>>);

  template <typename T>
    struct A
    {
      T a, b, c;
    };

  template <typename T, int N = 3>
    struct B
    {
      T x[N];
    };

  static_assert(vir::reflectable_struct<A<float>>);
  static_assert(vir::struct_size_v<A<float>> == 3);
  static_assert(vir::struct_size_v<A<std::tuple<float, stdx::simd<int>>>> == 3);
  static_assert(vir::struct_size_v<A<stdx::native_simd<float>>> == 3);

  static_assert(vir::reflectable_struct<B<float>>);
  static_assert(vir::struct_size_v<B<float>> == 1);
  static_assert(std::same_as<vir::struct_element_t<0, B<float>>, float[3]>);

  static_assert(vir::reflectable_struct<float[3]>);
  static_assert(vir::struct_size_v<float[3]> == 3);
  static_assert(std::same_as<vir::struct_element_t<0, float[3]>, float>);
}

#if VIR_HAVE_SIMDIZE

struct Empty
{ constexpr static int ignore_me = 0; };

static_assert(std::same_as<vir::simdize<Empty>, Empty>);

static_assert(std::same_as<vir::simdize<void>, void>);

static_assert(std::same_as<vir::simdize<std::tuple<>>, std::tuple<>>);

static_assert(std::same_as<vir::simdize<std::complex<float>>, std::complex<float>>);

static_assert(std::same_as<vir::simdize<std::tuple<std::complex<float>>>,
			   std::tuple<std::complex<float>>>);

static_assert(std::same_as<vir::simdize<std::tuple<int, double>>,
			   vir::vectorized_struct<std::tuple<int, double>, V<int>::size()>>);

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
	       vir::vectorized_struct<std::tuple<int, double>, V<short>::size()>>);

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
	       vir::vectorized_struct<std::tuple<float>, V<short>::size()>>);

template <typename T, int N>
  concept can_simdize = requires { typename vir::simdize<T, N>; }
			  and not std::is_same_v<vir::simdize<T, N>, T>;

static_assert(can_simdize<int, 4>);

static_assert(can_simdize<int, 1025> == std::is_destructible_v<stdx::fixed_size_simd<int, 1025>>);

static_assert(can_simdize<std::tuple<char, double>, 2>);

static_assert(can_simdize<std::tuple<char, double>, 1234>
		== (std::is_destructible_v<stdx::fixed_size_simd<char, 1234>>
		      and std::is_destructible_v<stdx::fixed_size_simd<double, 1234>>));

struct char_double
{
  char a;
  double b;
};

static_assert(can_simdize<char_double, 2>);

static_assert(can_simdize<char_double, 1234>
		== (std::is_destructible_v<stdx::fixed_size_simd<char, 1234>>
		      and std::is_destructible_v<stdx::fixed_size_simd<double, 1234>>));

template <typename T>
struct PointTpl
{
  T x, y, z;

  friend constexpr decltype(auto)
  operator==(PointTpl const& a, PointTpl const& b)
  { return a.x == b.x and a.y == b.y and a.z == b.z; }

  friend constexpr decltype(auto)
  operator!=(PointTpl const& a, PointTpl const& b)
  { return a.x != b.x or a.y != b.y or a.z != b.z; }

  friend constexpr PointTpl
  operator+(PointTpl const& a, PointTpl const& b)
  { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
};

template <typename T, std::size_t N>
struct PointArr
{
  T x[N];
};

using Point = PointTpl<float>;

static_assert(vir::struct_size_v<Point> == 3);

static_assert(std::same_as<vir::as_tuple_t<Point>,
			   std::tuple<float, float, float>>);

static_assert(std::same_as<vir::as_tuple_t<PointArr<float, 4>>,
			   std::tuple<float[4]>>);

static_assert(std::same_as<vir::simdize<Point>,
			   vir::vectorized_struct<Point, V<float>::size()>>);

static_assert(std::same_as<vir::simdize<PointArr<float, 4>>,
			   vir::vectorized_struct<PointArr<float, 4>, V<float>::size()>>);

static_assert(std::convertible_to<vir::simdize<Point>, vir::vectorized_struct<Point, V<float>::size()>>);

static_assert(std::convertible_to<vir::vectorized_struct<Point, V<float>::size()>, vir::simdize<Point>>);

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

static_assert(not vir::vectorizable_struct<std::complex<float>>);
static_assert(not vir::vectorizable_struct<std::tuple<std::complex<float>>>);
static_assert(not vir::vectorizable_struct<std::tuple<std::complex<float>,
							       std::complex<double>>>);
static_assert(not vir::vectorizable_struct<std::tuple<int, float, double,
							       std::complex<double>>>);

void f(typename vir::detail::simdize_template_arguments<Point, 0>::type);
static_assert(vir::vectorizable_struct_template<Point>);
static_assert(vir::vectorizable_struct<Point>);

namespace test_simdize
{
  using T = PointArr<float, 4>;
  using TS = vir::detail::simdize_template_arguments_t<T>;
  using TS0 = vir::struct_element_t<0, TS>;
  using TTup = typename vir::detail::make_simd_tuple<T, V<float>::size()>::type;
  using TTup0 = std::tuple_element_t<0, TTup>;

  static_assert(vir::reflectable_struct<T>);
  static_assert(vir::reflectable_struct<TS>);
  static_assert(vir::reflectable_struct<TTup>);

  static_assert(vir::struct_size_v<T> == 1);
  static_assert(vir::struct_size_v<TS> == 1);
  static_assert(vir::struct_size_v<TTup> == 1);
  static_assert(vir::detail::flat_member_count_v<T> == 4);

  template <typename T, int N>
    struct B
    {
      T x[N];

      constexpr void
      operator++()
      { for (T& y : x) ++y; }

      friend constexpr auto
      operator==(B const& a, B const& b)
      {
	for (int i = 0; i < N; ++i)
	  if (not stdx::all_of(a.x[i] == b.x[i]))
	    return false;
	return true;
      }
    };

  struct C
  : B<vir::simdize<float, 2>, 4>
  {};

  static_assert(vir::vectorizable_struct_template<B<float, 4>>);
  static_assert(vir::vectorizable_struct<B<float, 4>>);
  static_assert(std::same_as<vir::simdize<B<float, 4>, 1>, vir::vectorized_struct<B<float, 4>, 1>>);
  static_assert(vir::reflectable_struct<vir::simdize<B<float, 4>, 1>>);
  static_assert(std::same_as<vir::struct_element_t<0, B<float, 4>>, float[4]>);
  static_assert(std::same_as<vir::struct_element_t<0, C>, vir::simdize<float, 2>[4]>);
  static_assert(std::same_as<vir::struct_element_t<0, vir::simdize<B<float, 4>, 2>>,
			     vir::simdize<float, 2>[4]
			    >);

  static_assert(std::same_as<TTup0, vir::simdize<float[4]>>);
  static_assert(std::same_as<std::tuple_element_t<0, vir::simdize<float[4]>>, V<float>>);
  static_assert(std::same_as<std::tuple_element_t<1, vir::simdize<float[4]>>, V<float>>);
  static_assert(std::same_as<std::tuple_element_t<2, vir::simdize<float[4]>>, V<float>>);
  static_assert(std::same_as<std::tuple_element_t<3, vir::simdize<float[4]>>, V<float>>);

  static_assert(std::same_as<TS0, vir::simdize<float>[4]>);
  static_assert(std::tuple_size_v<TTup0> == std::extent_v<TS0>);

  static_assert(vir::vectorizable_struct_template<T>);
  static_assert(vir::vectorizable_struct<T>);
}

static_assert([] {
  vir::simdize<Point> p1 = Point{1.f, 1.f, 1.f};
  vir::simdize<Point> p2 {V<float>([](short i) { return i; }),
			  V<float>([](short i) { return i; }),
			  V<float>([](short i) { return i; })};
  vir::simdize<Point> p3 {V<float>([](short i) { return i + 1; }),
			  V<float>([](short i) { return i + 1; }),
			  V<float>([](short i) { return i + 1; })};
  {
    [[maybe_unused]] auto p = p1 + p2;
#if SIMD_IS_CONSTEXPR_ENOUGH
    if (any_of(p != p3))
      return false;
#endif
  }
  {
    [[maybe_unused]] auto p = Point{1.f, 1.f, 1.f} + p2;
#if SIMD_IS_CONSTEXPR_ENOUGH
    if (any_of(p != p3))
      return false;
#endif
  }
  {
    [[maybe_unused]] PointTpl<V<float>> p = p1 + p2;
#if SIMD_IS_CONSTEXPR_ENOUGH
    if (any_of(p != p3))
      return false;
#endif
  }
  return true;
}());

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

static_assert([] {
  std::array<Point, 5> data = {};
  vir::simdize<Point, 4> v(data.begin());
  PointTpl<DV<float, 4>> w = v;
#if SIMD_IS_CONSTEXPR_ENOUGH
  if (not all_of(w.x == 0.f and w.y == 0.f and w.z == 0.f))
    return false;
#endif
  v.copy_from(data.begin() + 1);
  w = v;
#if SIMD_IS_CONSTEXPR_ENOUGH
  if (not all_of(w.x == 0.f and w.y == 0.f and w.z == 0.f))
    return false;
#endif
  w.x = 1.f;
  w.y = DV<float, 4>([](float i) { return i; });
  v = w;
  v.copy_to(data.begin());
  if (data != std::array<Point, 5> {Point{1, 0, 0}, {1, 1, 0}, {1, 2, 0}, {1, 3, 0}, {0, 0, 0}})
    return false;
  v.copy_from(data.begin() + 1);
  v.copy_to(data.begin());
  return data == std::array<Point, 5> {Point{1, 1, 0}, {1, 2, 0}, {1, 3, 0}, {0, 0, 0}, {0, 0, 0}};
}());

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

  static_assert([] {
    std::array a1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    std::array a2 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    std::array a3 = a2;
    std::transform(vir::execution::simd, a1.begin(), a1.end(), a3.begin(), [](auto v) {
      return v + 2;
    });
    if (a3 != std::array{3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21})
      return false;
    std::transform(vir::execution::simd, a1.begin(), a1.end(), a2.begin(), a3.begin(),
		   [](auto v1, auto v2) {
		     return v1 - v2;
		   });
    if (a3 != std::array<int, 19> {})
      return false;
    return true;
  }());

  static_assert([] {
    std::array a1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    std::array a2 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    int r = std::transform_reduce(vir::execution::simd, a1.begin(), a1.end(), a2.begin(), 0);
    return r == 2470;
  }());
}
#endif  // VIR_HAVE_SIMD_EXECUTION

// vim: noet cc=101 tw=100 sw=2 ts=8
