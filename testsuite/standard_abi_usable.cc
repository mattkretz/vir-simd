/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// { dg-options "-std=c++17 -fno-fast-math" }
// { dg-do compile { target c++17 } }

#include <experimental/simd>

template <typename V>
  void
  is_usable()
  {
    static_assert(std::is_default_constructible_v<V>);
    static_assert(std::is_destructible_v         <V>);
    static_assert(std::is_default_constructible_v<typename V::mask_type>);
    static_assert(std::is_destructible_v         <typename V::mask_type>);
  }

template <typename T>
  void
  test01()
  {
    namespace stdx = std::experimental;
    is_usable<stdx::simd<T>>();
    is_usable<stdx::native_simd<T>>();
    is_usable<stdx::fixed_size_simd<T, 3>>();
    is_usable<stdx::fixed_size_simd<T, stdx::simd_abi::max_fixed_size<T>>>();
  }

int main()
{
  test01<char>();
  test01<wchar_t>();
  test01<char16_t>();
  test01<char32_t>();

  test01<signed char>();
  test01<unsigned char>();
  test01<short>();
  test01<unsigned short>();
  test01<int>();
  test01<unsigned int>();
  test01<long>();
  test01<unsigned long>();
  test01<long long>();
  test01<unsigned long long>();
  test01<float>();
  test01<double>();
  test01<long double>();
}
