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

    T i = 0;

    vir::for_each(vir::execution::simd, data, [&i](auto v) {
      COMPARE(v, vir::iota_v<decltype(v)> + i);
      i += v.size();
    });

    const int count = vir::count_if(vir::execution::simd, data, [](auto v) {
                        if constexpr (std::is_floating_point_v<T>)
                          return fmod(v, T(2)) == T(1);
                        else
                          return (v & T(1)) == T(1);
                      });
    COMPARE(count, int(V::size()) * 8 - 1);
#endif // VIR_HAVE_SIMD_EXECUTION
  }
