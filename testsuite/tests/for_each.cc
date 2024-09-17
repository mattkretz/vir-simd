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

#if VIR_HAVE_SIMD_CVT
template <typename T, typename U = T>
  struct A
  {
    T x;
    U y;

    constexpr void
    operator++()
    { ++x, ++y; }

    friend constexpr auto
    operator==(A const& a, A const& b)
    { return a.x == b.x and vir::cvt(a.y == b.y); }
  };
#endif

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
        if (not std::experimental::all_of(a.x[i] == b.x[i]))
          return false;
      return true;
    }
  };

template <typename V>
  void
  test()
  {
#if VIR_HAVE_SIMD_EXECUTION && VIR_HAVE_SIMD_CVT
    using T = typename V::value_type;

    constexpr auto exec_simd = vir::execution::simd.prefer_size<V::size()>();

    {
      std::vector<T> data;
      data.resize(V::size() * 16 - 1);
      std::iota(data.begin(), data.end(), T());

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
    }
    {
      using U = vir::meta::as_unsigned_t<T>;
      std::array<A<T, U>, V::size() * 4 - 1> data;
      std::iota(data.begin(), data.end(), A<T, U>{T(0), U(data.size())});
      auto ref = data;
      auto op = [](auto& v) {
        v.x += T(1);
        v.y += U(2);
      };
      std::for_each(exec_simd, data.begin(), data.end(), op);
      std::for_each(ref.begin(), ref.end(), op);
      COMPARE(data, ref);
    }
    {
      std::array<B<T, 3>, V::size() * 4 - 1> data;
      std::iota(data.begin(), data.end(), B<T, 3>{T(0), T(data.size()), T(2 * data.size())});
      auto ref = data;
      auto op = [](auto& v) {
        v.x[0] += T(1);
        v.x[1] += T(2);
        v.x[2] += T(3);
      };
      std::for_each(exec_simd, data.begin(), data.end(), op);
      std::for_each(ref.begin(), ref.end(), op);
      COMPARE(data, ref);
    }
    {
      std::array<B<T, 4>, V::size() * 4 - 1> data;
      std::iota(data.begin(), data.end(), B<T, 4>{T(0), T(data.size()), T(2 * data.size())});
      auto ref = data;
      auto op = [](auto& v) {
        v.x[0] += T(1);
        v.x[1] += T(2);
        v.x[2] += T(3);
        v.x[3] += T(4);
      };
      std::for_each(exec_simd, data.begin(), data.end(), op);
      std::for_each(ref.begin(), ref.end(), op);
      COMPARE(data, ref);
    }

#endif // VIR_HAVE_SIMD_EXECUTION
  }
