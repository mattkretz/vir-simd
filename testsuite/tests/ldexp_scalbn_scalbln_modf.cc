/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

// only: float|double|ldouble * * *
// expensive: * [1-9] * *
#include "bits/main.h"

template <typename V>
  V rot1(const V& x) {
    return V([&](auto i) {
	     if constexpr(i == 0)
	       return x[V::size() - 1];
	     else
	       return x[i - 1];
	     });
  }

template <typename V>
  void
  test()
  {
    vir::test::setFuzzyness<float>(0);
    vir::test::setFuzzyness<double>(0);

    using T = typename V::value_type;

    // See https://sourceware.org/bugzilla/show_bug.cgi?id=18031
    const bool modf_is_broken = [] {
      volatile T x = T(5e20) / 7;
      T tmp;
      return std::fabs(std::modf(x, &tmp)) >= 1;
    }();
    if (modf_is_broken)
      __builtin_fprintf(stderr,
			"NOTE: Skipping modf because std::modf is broken.\n");

    test_values<V>(
      {
#ifdef __STDC_IEC_559__
	vir::quiet_NaN_v<T>,
	vir::infinity_v<T>,
	-vir::infinity_v<T>,
	-0.,
	vir::denorm_min_v<T>,
	vir::denorm_min_v<T> * 3,
	vir::norm_min_v<T> / 3,
	-vir::denorm_min_v<T>,
	-vir::denorm_min_v<T> * 3,
	-vir::norm_min_v<T> / 3,
	vir::quiet_NaN_v<T>,
#endif
	+0.,
	+1.3,
	-1.3,
	2.1,
	-2.1,
	0.99,
	0.9,
	-0.9,
	-0.99,
	vir::norm_min_v<T>,
	vir::norm_min_v<T> * 3,
	vir::finite_max_v<T>,
	vir::finite_max_v<T> / 3,
	-vir::norm_min_v<T>,
	-vir::norm_min_v<T> * 3,
	-vir::finite_max_v<T>,
	-vir::finite_max_v<T> / 3},
      {10000},
      [](V input) {
	using IV = std::experimental::fixed_size_simd<int, V::size()>;
	using LV = std::experimental::fixed_size_simd<long, V::size()>;
	test_values<IV>(
	  {-10000, -1026, -1024, -1023, -1000, -130, -128, -127,
	   -100,   -10,   -1,    0,     1,     10,   100,
	   127,    128,   130,   1000,  1023,  1024, 1026, 10000},
	  [&](IV exp) {
	    for (std::size_t rot = 0; rot < V::size(); ++rot)
	      exp = rot1(exp);

	    { // scalbn
	      FloatExceptCompare fec;
	      const auto totest = scalbn(input, exp);
	      fec.record_first();
	      using R = std::remove_const_t<decltype(totest)>;
	      auto&& expected = [&](const auto& v) -> const R {
		R tmp = {};
		using std::scalbn;
		for (std::size_t i = 0; i < R::size(); ++i)
		  tmp[i] = scalbn(v[i], exp[i]);
		return tmp;
	      };
	      const R expect1 = expected(input);
	      fec.record_second();
	      COMPARE(isnan(totest), isnan(expect1))
		<< "\nscalbn(" << input << ", " << exp << ") = " << totest
		<< " != " << expect1;
	      input = iif(isnan(expect1), 0, input);
	      FUZZY_COMPARE(scalbn(input, exp), expected(input))
		.on_failure("\ninput = ", input, "\nexp = ", exp);
	      fec.verify_equal_state(__FILE__, __LINE__,
				     " After scalbn(", input, ", ", exp, ") = ", totest);
	    }
	    { // ldexp
	      FloatExceptCompare fec;
	      const auto totest = ldexp(input, exp);
	      fec.record_first();
	      using R = std::remove_const_t<decltype(totest)>;
	      auto&& expected = [&](const auto& v) -> const R {
		R tmp = {};
		using std::ldexp;
		for (std::size_t i = 0; i < R::size(); ++i)
		  tmp[i] = ldexp(v[i], exp[i]);
		return tmp;
	      };
	      const R expect1 = expected(input);
	      fec.record_second();
	      COMPARE(isnan(totest), isnan(expect1))
		<< "\nldexp(" << input << ", " << exp << ") = " << totest
		<< " != " << expect1;
	      input = iif(isnan(expect1), 0, input);
	      FUZZY_COMPARE(ldexp(input, exp), expected(input))
		.on_failure("\ninput = ", input, "\nexp = ", exp);
	      fec.verify_equal_state(__FILE__, __LINE__,
				     " After ldexp(", input, ", ", exp, ") = ", totest);
	    }
	    { // scalbln
	      LV expl = std::experimental::simd_cast<LV>(exp);
	      FloatExceptCompare fec;
	      const auto totest = scalbln(input, expl);
	      fec.record_first();
	      using R = std::remove_const_t<decltype(totest)>;
	      auto&& expected = [&](const auto& v) -> const R {
		R tmp = {};
		using std::scalbln;
		for (std::size_t i = 0; i < R::size(); ++i)
		  tmp[i] = scalbln(v[i], expl[i]);
		return tmp;
	      };
	      const R expect1 = expected(input);
	      fec.record_second();
	      COMPARE(isnan(totest), isnan(expect1))
		<< "\nscalbln(" << input << ", " << expl << ") = " << totest
		<< " != " << expect1;
	      input = iif(isnan(expect1), 0, input);
	      FUZZY_COMPARE(scalbln(input, expl), expected(input))
		.on_failure("\ninput = ", input, "\nexp = ", expl);
	      fec.verify_equal_state(__FILE__, __LINE__,
				     " After scalbln(", input, ", ", exp, ") = ", totest);
	    }
	  });
      },
      [modf_is_broken](const V input) {
	if (modf_is_broken)
	  return;
	V integral = {};
	const V totest = modf(input, &integral);
	auto&& expected = [&](const auto& v) -> std::pair<const V, const V> {
	  std::pair<V, V> tmp = {};
	  using std::modf;
	  for (std::size_t i = 0; i < V::size(); ++i)
	    {
	      typename V::value_type tmp2;
	      tmp.first[i] = modf(v[i], &tmp2);
	      tmp.second[i] = tmp2;
	    }
	  return tmp;
	};
	const auto expect1 = expected(input);
#ifdef __STDC_IEC_559__
	COMPARE(isnan(totest), isnan(expect1.first))
	  << "modf(" << input << ", iptr) = " << totest << " != " << expect1;
	COMPARE(isnan(integral), isnan(expect1.second))
	  << "modf(" << input << ", iptr) = " << totest << " != " << expect1;
	COMPARE(isnan(totest), isnan(integral))
	  << "modf(" << input << ", iptr) = " << totest << " != " << expect1;
	const V clean = iif(isnan(totest), V(), input);
#else
	const V clean = iif(isnormal(input), input, V());
#endif
	const auto expect2 = expected(clean);
	COMPARE(modf(clean, &integral), expect2.first) << "\nclean = " << clean;
	COMPARE(integral, expect2.second);
      });
  }
