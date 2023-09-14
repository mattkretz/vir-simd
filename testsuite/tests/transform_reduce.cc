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

static_assert(vir::vectorizable_struct<Point2D<float>>);

template <typename V>
  void
  test()
  {
#if VIR_HAVE_SIMD_EXECUTION
    constexpr int N = V::size();
    using T = typename V::value_type;
    std::vector<T> data0, data1;
    std::vector<Point2D<T>> data2, data3;
    data0.resize(N * 16 - 1);
    data1.resize(N * 16 - 1);
    data2.resize(N * 16 - 1);
    data3.resize(N * 16 - 1);
    std::iota(data0.begin(), data0.end(), T());
    data1 = data0;
    std::for_each(data2.begin(), data2.end(), [](auto& d) {
      d.x = T(1);
      d.y = T(2);
    });
    data3 = data2;

    constexpr auto exec_simd = vir::execution::simd.prefer_size<N>();

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
#endif // VIR_HAVE_SIMD_EXECUTION
  }
