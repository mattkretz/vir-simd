/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 * Copyright © 2026 Axel Huebl <axelhuebl@lbl.gov>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *

/* VIR_DISABLE_SIMD_VECMATH has to switch the header off completely
 *
 * A user who needs errno, the exception flags or sub-ULP accuracy has to be
 * able to opt out even when the header arrives through another one. The
 * compile-time check below is the substance of it; the rest confirms that
 * vir::stdx is left exactly as it was, which is also what the header claims
 * when it is enabled, since it no longer declares anything there.
 */
#define VIR_DISABLE_SIMD_VECMATH 1
#include "bits/main.h"
#include <vir/simd_vecmath.h>

#ifdef VIR_HAVE_SIMD_VECMATH
#error "VIR_DISABLE_SIMD_VECMATH did not disable vir/simd_vecmath.h"
#endif

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;

    /* Only names the underlying implementation is known to provide: with vir's
     * own simd, exp, exp2, expm1, cbrt and fabs do not exist at all, and this
     * file is compiled in that configuration too.
     */
    const V x = make_value_unknown(V([](auto i) { return T(0.25) + T(i) * T(0.125); }));
    const V unit = make_value_unknown(V([](auto i) { return T(i) / T(V::size()); }));

    // still reachable, and still answering correctly
    COMPARE(vir::stdx::sin(V(T(0))), V(T(0)));
    COMPARE(vir::stdx::cos(V(T(0))), V(T(1)));
    COMPARE(vir::stdx::sinh(V(T(0))), V(T(0)));
    COMPARE(vir::stdx::log(V(T(1))), V(T(0)));
    COMPARE(vir::stdx::asin(V(T(0))), V(T(0)));
    VERIFY(all_of(vir::stdx::sin(x) * vir::stdx::sin(x)
		    + vir::stdx::cos(x) * vir::stdx::cos(x) > V(T(0.99))));
    VERIFY(all_of(vir::stdx::asin(unit) <= V(T(1.5708))));

    // and the full overload set is intact
    COMPARE(vir::stdx::pow(V(T(2)), V(T(3))), V(T(8)));
    COMPARE(vir::stdx::pow(V(T(2)), T(3)), V(T(8)));
    COMPARE(vir::stdx::pow(T(2), V(T(3))), V(T(8)));
    COMPARE(vir::stdx::hypot(V(T(3)), V(T(4))), V(T(5)));
    COMPARE(vir::stdx::sqrt(V(T(4))), V(T(2)));
    COMPARE(vir::stdx::abs(V(T(-2))), V(T(2)));
  }
