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
  };

template <typename V>
  void
  test()
  {
#if VIR_HAVE_SIMD_EXECUTION
    constexpr int N = V::size();
    using T = typename V::value_type;
    std::vector<T> data0, data1;
    std::vector<int> dataInt;
    std::vector<Point2D<T>> data2;
    data0.resize(N * 16 - 1);
    data1.resize(N * 16 - 1);
    dataInt.resize(N * 16 - 1);
    data2.resize(N * 16 - 1);
    std::iota(data0.begin(), data0.end(), T());
/*    std::for_each(data2.begin(), data2.end(), [](auto& d) {
      d.x = T();
      d.y = T();
    });*/

    VERIFY(data0 != data1);

    constexpr auto exec_simd = vir::execution::simd.prefer_size<N>();

    T i = 0;
    vir::transform(exec_simd, data0, data1, [&i](auto v) {
      COMPARE(v, vir::iota_v<decltype(v)> + i);
      i += v.size();
      return v;
    });
    COMPARE(i, T(data0.size()));
    COMPARE(data0, data1);

    vir::transform(exec_simd.template unroll_by<2>(), data0, data1, [&i](auto v) {
      return v + 1;
    });
    vir::transform(exec_simd, data0, data1, data2, [&i](auto v0, auto v1) {
      return Point2D<decltype(v0)>{v0, v1 + 2};
    });

    for (std::size_t i = 0; i < data2.size(); ++i)
      {
        COMPARE(data2[i].x, T(i));
        COMPARE(data2[i].y, T(i + 3));
      }

    std::transform(exec_simd.prefer_aligned(), data2.begin(), data2.end(), dataInt.begin(),
                   []<typename U>(const U& v) {
                     return vir::stdx::static_simd_cast<vir::simdize<int, U::size()>>(
                              std::get<1>(v.as_tuple()) - std::get<0>(v.as_tuple()));
                   });
    for (int x : dataInt)
      COMPARE(x, 3);
#endif // VIR_HAVE_SIMD_EXECUTION
  }
