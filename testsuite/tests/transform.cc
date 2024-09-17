/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2023–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */
// expensive: * [1-9] * *
#include "bits/main.h"

#include <numeric>
#include <vector>

#include <vir/simd_iota.h>
#include <vir/simdize.h>
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
    constexpr int range_size = sizeof(T) == 1 ? std::min(N * 16 - 1, 123) : N * 16 - 1;
    data0.resize(range_size);
    data1.resize(range_size);
    dataInt.resize(range_size);
    data2.resize(range_size);
    std::iota(data0.begin(), data0.end(), T());

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

    vir::transform(exec_simd.template unroll_by<2>(), data0, data1, [](auto v) {
      return v + 1;
    });
    vir::transform(exec_simd, data0, data1, data2, [](auto v0, auto v1) {
      return Point2D<decltype(v0)>{v0, v1 + 2};
    });

    for (std::size_t i = 0; i < data2.size(); ++i)
      {
        COMPARE(data2[i].x, T(i));
        COMPARE(data2[i].y, T(i + 3));
      }

    dataInt[0] = 3;
    std::transform(exec_simd.prefer_aligned(), data2.begin() + 1, data2.end(), dataInt.begin() + 1,
                   []<typename U>(const U& v) {
                     return vir::stdx::static_simd_cast<vir::simdize<int, U::size()>>(v.y - v.x);
                   });
    for (int x : dataInt)
      COMPARE(x, 3);

#if defined __clang_major__ and __clang_major__ <= 17
  // Clang 17 fails to compile libstdc++ zip_view code
#define BAD_COMPILER 1
#endif
#if __cpp_lib_ranges_zip >= 202110L and not BAD_COMPILER
    vir::transform(exec_simd, std::views::zip(data0, data1), dataInt, [](const auto& v) {
      using IV = vir::simdize<int, std::remove_cvref_t<decltype(v.first)>::size()>;
      const IV a = vir::cvt(v.first);
      const IV b = vir::cvt(v.second);
      return a - b;
    });
    for (int x : dataInt)
      COMPARE(x, -1);
#endif
#endif // VIR_HAVE_SIMD_EXECUTION
  }
