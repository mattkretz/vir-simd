// expensive: * [1-9] * *
#include "bits/main.h"

#include <numeric>
#include <vector>

#include <vir/simd_iota.h>
#include <vir/simd_execution.h>

template <typename V>
  void
  test()
  {
#if VIR_HAVE_SIMD_EXECUTION
    using T = typename V::value_type;
    std::vector<T> data;
    data.resize(V::size() * 16 - 1);
    std::iota(data.begin(), data.end(), T());

    constexpr auto exec_simd = vir::execution::simd.prefer_size<V::size()>();

    T i = 0;
    vir::for_each(exec_simd, data,
                  [&i](auto v) {
                    COMPARE(v, vir::iota_v<decltype(v)> + i);
                    i += v.size();
                  });
    COMPARE(i, T(data.size()));

    i = 1;
    vir::for_each(exec_simd.prefer_aligned(), data.begin() + 1, data.end(),
                  [&i](auto v) {
                    COMPARE(v, vir::iota_v<decltype(v)> + i);
                    i += v.size();
                  });
    COMPARE(i, T(data.size()));

    i = 0;
    std::for_each(exec_simd, data.begin(), data.end(),
                  [&i](auto v) {
                    COMPARE(v, vir::iota_v<decltype(v)> + i);
                    i += v.size();
                  });

    int count = vir::count_if(exec_simd, data, [](auto v) {
                  if constexpr (std::is_floating_point_v<T>)
                    return fmod(v, T(2)) == T(1);
                  else
                    return (v & T(1)) == T(1);
                });
    COMPARE(count, int(data.size()) / 2);

    count = std::count_if(exec_simd, data.begin(), data.end(), [](auto v) {
              if constexpr (std::is_floating_point_v<T>)
                return fmod(v, T(2)) == T(1);
              else
                return (v & T(1)) == T(1);
            });
    COMPARE(count, int(data.size()) / 2);
#endif // VIR_HAVE_SIMD_EXECUTION
  }
