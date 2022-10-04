/*
    Copyright © 2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
                     Matthias Kretz <m.kretz@gsi.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef VIR_DETAILS_H
#define VIR_DETAILS_H

#include "simd.h"
#include <type_traits>
#include <bit>

namespace vir::detail
{
  template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
    using FloatingPoint = T;

  using namespace vir::stdx;

  template <typename T, int N>
    using deduced_simd = stdx::simd<T, stdx::simd_abi::deduce_t<T, N>>;

  template <typename T, int N>
    using deduced_simd_mask = stdx::simd_mask<T, stdx::simd_abi::deduce_t<T, N>>;
}

#endif // VIR_DETAILS_H
