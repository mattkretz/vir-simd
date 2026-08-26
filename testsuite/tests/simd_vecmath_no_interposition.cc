/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                  Matthias Kretz <m.kretz@gsi.de>
 * Copyright © 2026 Axel Huebl <axelhuebl@lbl.gov>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *

/* Including vir/simd_vecmath.h must leave vir::stdx exactly as it was
 *
 * The functions could have been declared in vir::stdx, which would have made
 * vir::stdx::sinh resolve to them. They are not, because a declaration there
 * hides the underlying overloads of that name, and everything below is what
 * that would have cost. Each check fails to *compile* rather than to run if
 * the header ever starts interposing again, which is the point: every one of
 * these was silent when it broke.
 *
 * This needs its own translation unit for the using-directive at namespace
 * scope, which is the check that would otherwise leak into the other tests.
 */
#include "bits/main.h"
#include <vir/simd_vecmath.h>

// the directive that turns an interposing overload set into an ambiguity
using namespace vir::stdx;

template <typename V>
  void
  test()
  {
    using T = typename V::value_type;

    const V x = make_value_unknown(V([](auto i) { return T(1) + T(i) * T(0.125); }));
    const V two = make_value_unknown(V(T(2)));
    const T two_scalar = make_value_unknown(T(2));

    /* Unqualified, through the using-directive above. If vir::vecmath's
     * overloads were visible here too, every one of these would be ambiguous.
     */
    /* Only names both backends provide: vir's own simd implementation, used
     * where <experimental/simd> is absent, has no exp, exp2, expm1, cbrt or
     * fabs, and this file is compiled in that configuration too.
     */
    VERIFY(all_of(sinh(x) == vir::stdx::sinh(x))) << "unqualified sinh";
    VERIFY(all_of(cosh(x) == vir::stdx::cosh(x))) << "unqualified cosh";
    VERIFY(all_of(tanh(x) == vir::stdx::tanh(x))) << "unqualified tanh";
    VERIFY(all_of(sin(x) == vir::stdx::sin(x))) << "unqualified sin";
    VERIFY(all_of(cos(x) == vir::stdx::cos(x))) << "unqualified cos";
    VERIFY(all_of(tan(x) == vir::stdx::tan(x))) << "unqualified tan";
    VERIFY(all_of(log(x) == vir::stdx::log(x))) << "unqualified log";
    VERIFY(all_of(log2(x) == vir::stdx::log2(x))) << "unqualified log2";
    VERIFY(all_of(atan(x) == vir::stdx::atan(x))) << "unqualified atan";
    VERIFY(all_of(erf(x) == vir::stdx::erf(x))) << "unqualified erf";

    /* The two-argument overload set. libstdc++ generates two overloads per
     * function, the first with a second parameter excluded from deduction,
     * which is what lets a scalar be broadcast. Reproducing that set wrongly
     * is how pow(x, 2.0) stopped compiling once before.
     */
    VERIFY(all_of(pow(x, two) == vir::stdx::pow(x, two))) << "pow(simd, simd)";
    VERIFY(all_of(pow(x, two_scalar) == vir::stdx::pow(x, two))) << "pow(simd, scalar)";
    VERIFY(all_of(pow(two_scalar, x) == vir::stdx::pow(two, x))) << "pow(scalar, simd)";
    VERIFY(all_of(atan2(x, two_scalar) == vir::stdx::atan2(x, two))) << "atan2(simd, scalar)";
    VERIFY(all_of(atan2(two_scalar, x) == vir::stdx::atan2(two, x))) << "atan2(scalar, simd)";

    // an argument that merely converts to the element type
    VERIFY(all_of(pow(x, make_value_unknown(2)) == vir::stdx::pow(x, two))) << "pow(simd, int)";

    /* Names the header deliberately leaves alone. hypot in particular carries
     * a three-argument form and converting overloads that a declaration in
     * vir::stdx would have taken away.
     */
    COMPARE(hypot(V(T(3)), V(T(4))), V(T(5)));
    VERIFY(all_of(hypot(V(T(3)), V(T(4)), V(T(0))) == V(T(5)))) << "hypot, three arguments";
    COMPARE(sqrt(V(T(4))), V(T(2)));
    COMPARE(abs(V(T(-2))), V(T(2)));

    /* And the values are the underlying implementation's, not a vector math
     * library's: identical, not merely close. Anything routed through
     * vir::vecmath would differ here by the few ULP that costs.
     */
    for (std::size_t i = 0; i < V::size(); ++i)
      {
        const T xi = T(x[i]);
        COMPARE(T(vir::stdx::sinh(x)[i]), std::sinh(xi)) << "lane " << i;
        COMPARE(T(vir::stdx::tanh(x)[i]), std::tanh(xi)) << "lane " << i;
      }
  }
