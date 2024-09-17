/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2023–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */
// expensive: * [1-9] * *
#include "bits/main.h"

#include <numeric>
#include <vector>

#include <vir/simd_iota.h>
#include <vir/simd_execution.h>

template <typename T>
  struct Point2D
  {
    T x,y;

    friend constexpr T
    operator*(const Point2D& a, const Point2D& b) noexcept
    { return a.x * b.x + a.y * b.y; }
  };

#if VIR_HAVE_SIMDIZE
static_assert(vir::vectorizable_struct_template<Point2D<float>>);
#endif

template <typename V>
  void
  test()
  {
#if VIR_HAVE_SIMD_EXECUTION
    constexpr int N = V::size();
    using T = typename V::value_type;

    std::vector<T> data0, data1;
    data0.resize(N * 16 - 1);
    std::iota(data0.begin(), data0.end(), T());
    data1 = data0;

    std::vector<Point2D<T>> data2, data3;
    data2.resize(N * 16 - 1);
    {
      unsigned i = 1;
      std::for_each(data2.begin(), data2.end(), [&i](auto& d) {
        d.x = T(i % 4);
        d.y = T((i + 2u) % 5);
        ++i;
      });
    }
    data3 = data2;

    constexpr auto exec_simd = vir::execution::simd.prefer_size<N>();

    // unordered reduction can lead to differences in results
    vir::test::setFuzzyness<float>(5.f);
    vir::test::setFuzzyness<double>(5.);

    T result = std::transform_reduce(exec_simd, data0.begin(), data0.end(), data1.begin(), T());
    T expected = std::transform_reduce(data0.begin(), data0.end(), data1.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd.prefer_aligned(), data0.begin(), data0.end(), data1.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd.template unroll_by<2>(), data0.begin(), data0.end(), data1.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd.template unroll_by<3>(), data0.begin(), data0.end(), data1.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd.template unroll_by<4>(), data0.begin(), data0.end(), data1.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd.template unroll_by<5>(), data0.begin(), data0.end(), data1.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd.prefer_aligned().template unroll_by<5>(), data0.begin(), data0.end(), data1.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd, data2.begin(), data2.end(), data3.begin(), T());
    expected = std::transform_reduce(data2.begin(), data2.end(), data3.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd.template unroll_by<3>(), data2.begin(), data2.end(), data3.begin(), T());
    FUZZY_COMPARE(result, expected);

    result = std::transform_reduce(exec_simd, data2.begin(), data2.end(), T(), std::plus<>(),
                                   [](auto v) { return v * v; });
    FUZZY_COMPARE(result, expected);
#endif // VIR_HAVE_SIMD_EXECUTION
  }
