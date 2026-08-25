/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *

/* VIR_DISABLE_SIMD_VECMATH has to switch the header off completely
 *
 * A user who needs errno, the exception flags or sub-ULP accuracy has to be
 * able to opt out even when the header arrives through another one. What that
 * has to mean is not "results close to the underlying implementation" but
 * exactly it, so every check below is an identity against the very function
 * the header would otherwise have replaced. Comparing against the scalar
 * routines instead would measure the underlying implementation's accuracy,
 * which is not what opting out is about.
 */
#define VIR_DISABLE_SIMD_VECMATH 1
#include "bits/main.h"
#include <vir/simd_vecmath.h>

#ifdef VIR_HAVE_SIMD_VECMATH
#error "VIR_DISABLE_SIMD_VECMATH did not disable vir/simd_vecmath.h"
#endif

namespace underlying = std::experimental::parallelism_v2;

#define SAME_AS_UNDERLYING(name_, x_) \
  COMPARE(vir::stdx::name_(x_), underlying::name_(x_)) << "vir::stdx::" #name_

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;

    /* Built from the lane index so that they stay inside each domain whatever
     * the width is: x is positive and grows, unit stays inside (-1, 1), and
     * above_one stays at or above 1.
     */
    const V x = make_value_unknown(V([](auto i) { return T(0.25) + T(i) * T(0.125); }));
    const V unit = make_value_unknown(V([](auto i) { return T(i) / T(V::size()); }));
    const V above_one = make_value_unknown(V([](auto i) { return T(1) + T(i); }));
    const V y = make_value_unknown(V(T(2)));

    SAME_AS_UNDERLYING(sin, x);      SAME_AS_UNDERLYING(cos, x);
    SAME_AS_UNDERLYING(tan, x);      SAME_AS_UNDERLYING(asin, unit);
    SAME_AS_UNDERLYING(acos, unit);  SAME_AS_UNDERLYING(atan, x);
    SAME_AS_UNDERLYING(sinh, x);     SAME_AS_UNDERLYING(cosh, x);
    SAME_AS_UNDERLYING(tanh, x);     SAME_AS_UNDERLYING(asinh, x);
    SAME_AS_UNDERLYING(atanh, unit); SAME_AS_UNDERLYING(exp, x);
    SAME_AS_UNDERLYING(exp2, x);     SAME_AS_UNDERLYING(expm1, x);
    SAME_AS_UNDERLYING(log, x);      SAME_AS_UNDERLYING(log2, x);
    SAME_AS_UNDERLYING(log10, x);    SAME_AS_UNDERLYING(log1p, x);
    SAME_AS_UNDERLYING(cbrt, x);     SAME_AS_UNDERLYING(erf, x);
    SAME_AS_UNDERLYING(erfc, x);
    SAME_AS_UNDERLYING(acosh, above_one);

    COMPARE(vir::stdx::pow(x, y), underlying::pow(x, y));
    COMPARE(vir::stdx::atan2(x, y), underlying::atan2(x, y));

    // and the scalar spellings still resolve, exactly as before
    {
      const T two = make_value_unknown(T(2));
      COMPARE(vir::stdx::pow(x, two), underlying::pow(x, y));
      COMPARE(vir::stdx::pow(two, x), underlying::pow(y, x));
      COMPARE(vir::stdx::atan2(x, two), underlying::atan2(x, y));
      COMPARE(vir::stdx::atan2(two, x), underlying::atan2(y, x));
    }

    // names the header never touches are unaffected either way
    COMPARE(vir::stdx::hypot(V(T(3)), V(T(4))), V(T(5)));
    VERIFY(all_of(vir::stdx::hypot(V(T(3)), V(T(4)), V(T(0))) == V(T(5))));
    COMPARE(vir::stdx::sqrt(V(T(4))), V(T(2)));
    COMPARE(vir::stdx::abs(V(T(-2))), V(T(2)));
  }
