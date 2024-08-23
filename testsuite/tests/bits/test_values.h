/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef SIMD_TESTS_BITS_TEST_VALUES_H_
#define SIMD_TESTS_BITS_TEST_VALUES_H_

#include "verify.h"

#include <vir/simd.h>
#include <vir/simd_bit.h>
#include <vir/simd_cast.h>
#include <initializer_list>
#include <random>
#include "floathelpers.h"
#include "numeric_traits.h"

template <class T, class A>
  std::experimental::simd<T, A>
  iif(std::experimental::simd_mask<T, A> k,
      const typename std::experimental::simd_mask<T, A>::simd_type& t,
      const std::experimental::simd<T, A>& f)
  {
    auto r = f;
    where(k, r) = t;
    return r;
  }

template <class V>
  V
  epilogue_load(const typename V::value_type* mem, const std::size_t size)
  {
    const int rem = size % V::size();
    return where(V([](int i) { return i; }) < rem, V(0))
	     .copy_from(mem + size / V::size() * V::size(),
			std::experimental::element_aligned);
  }

template <class V, class... F>
  void
  test_values(const std::initializer_list<typename V::value_type>& inputs,
	      F&&... fun_pack)
  {
    for (auto it = inputs.begin(); it + V::size() <= inputs.end();
	 it += V::size())
      {
	[](auto...) {
	}((fun_pack(V(&it[0], std::experimental::element_aligned)), 0)...);
      }
    [](auto...) {
    }((fun_pack(epilogue_load<V>(inputs.begin(), inputs.size())), 0)...);
  }

template <typename T, typename U = long long, bool = (sizeof(T) == sizeof(U))>
  struct int_for_dist;

template <typename T, typename U>
  struct int_for_dist<T, U, true>
  {
    using type = typename std::conditional_t<std::is_unsigned_v<T>,
                                             std::make_unsigned<U>, std::make_signed<U>>::type;
  };

template <typename T>
  struct int_for_dist<T, long long, false>
  : int_for_dist<T, long> {};

template <typename T>
  struct int_for_dist<T, long, false>
  : int_for_dist<T, int> {};

template <typename T>
  struct int_for_dist<T, int, false>
  : int_for_dist<T, short> {};

template <typename T>
  struct int_for_dist<T, short, false>
  {
    using type = std::conditional_t<sizeof(T) == sizeof(char),
                                    typename int_for_dist<T, short, true>::type, T>;
  };

template <typename T>
using int_for_dist_t = typename int_for_dist<T>::type;

template <class V>
  struct RandomValues
  {
    using T = typename V::value_type;
    static constexpr bool isfp = std::is_floating_point_v<T>;
    const std::size_t count;

    std::conditional_t<std::is_floating_point_v<T>,
		       std::uniform_real_distribution<T>,
                       std::uniform_int_distribution<int_for_dist_t<T>>>
      dist;

    const bool uniform;

    const T abs_max = vir::finite_max_v<T>;

    RandomValues(std::size_t count_, T min, T max)
    : count(count_), dist(min, max), uniform(true)
    {
      if constexpr (std::is_floating_point_v<T>)
	VERIFY(max - min <= vir::finite_max_v<T>);
    }

    RandomValues(std::size_t count_)
    : count(count_), dist(isfp ? 1 : vir::finite_min_v<T>,
			  isfp ? 2 : vir::finite_max_v<T>),
      uniform(!isfp)
    {}

    RandomValues(std::size_t count_, T abs_max_)
    : count(count_), dist(isfp ? 1 : -abs_max_, isfp ? 2 : abs_max_),
      uniform(!isfp), abs_max(abs_max_)
    {}

    template <typename URBG>
      V
      operator()(URBG& gen)
      {
	if constexpr (!isfp)
          return V([&](int) { return T(dist(gen)); });
	else if (uniform)
          return V([&](int) { return T(dist(gen)); });
	else
	  {
	    auto exp_dist
	      = std::normal_distribution<float>(0.f,
						vir::max_exponent_v<T> * .5f);
	    return V([&](int) {
                     const T mant = T(dist(gen));
		     T fp = 0;
		     do {
		       const int exp = exp_dist(gen);
		       fp = std::ldexp(mant, exp);
		     } while (fp >= abs_max || fp <= vir::denorm_min_v<T>);
		     fp = gen() & 0x4 ? fp : -fp;
		     return fp;
		   });
	  }
      }
  };

static std::mt19937 g_mt_gen{0};

template <class V, class... F>
  void
  test_values(const std::initializer_list<typename V::value_type>& inputs,
	      RandomValues<V> random, F&&... fun_pack)
  {
    test_values<V>(inputs, fun_pack...);
    for (size_t i = 0; i < (random.count + V::size() - 1) / V::size(); ++i)
      {
	[](auto...) {}((fun_pack(random(g_mt_gen)), 0)...);
      }
  }

template <class V, class... F>
  void
  test_values_2arg(const std::initializer_list<typename V::value_type>& inputs,
		   F&&... fun_pack)
  {
    using T = typename V::value_type;
    for (auto scalar_it = inputs.begin(); scalar_it != inputs.end();
	 ++scalar_it)
      {
	for (auto it = inputs.begin(); it + V::size() <= inputs.end();
	     it += V::size())
	  {
	    (fun_pack(V(&it[0], std::experimental::element_aligned), V(*scalar_it)), ...);
	    (fun_pack(V(&it[0], std::experimental::element_aligned), T(*scalar_it)), ...);
	    (fun_pack(T(it[0]), V(*scalar_it)), ...);
	  }
	(fun_pack(epilogue_load<V>(inputs.begin(), inputs.size()), V(*scalar_it)), ...);
      }
  }

template <class V, class... F>
  void
  test_values_2arg(const std::initializer_list<typename V::value_type>& inputs,
		   RandomValues<V> random, F&&... fun_pack)
  {
    test_values_2arg<V>(inputs, fun_pack...);
    for (size_t i = 0; i < (random.count + V::size() - 1) / V::size(); ++i)
      {
	[](auto...) {}((fun_pack(random(g_mt_gen), random(g_mt_gen)), 0)...);
      }
  }

template <class V, class... F>
  void
  test_values_3arg(const std::initializer_list<typename V::value_type>& inputs,
		   F&&... fun_pack)
  {
    for (auto scalar_it1 = inputs.begin(); scalar_it1 != inputs.end();
	 ++scalar_it1)
      {
	for (auto scalar_it2 = inputs.begin(); scalar_it2 != inputs.end();
	     ++scalar_it2)
	  {
	    for (auto it = inputs.begin(); it + V::size() <= inputs.end();
		 it += V::size())
	      {
		[](auto...) {
		}((fun_pack(V(&it[0], std::experimental::element_aligned),
			    V(*scalar_it1), V(*scalar_it2)),
		   0)...);
	      }
	    [](auto...) {
	    }((fun_pack(epilogue_load<V>(inputs.begin(), inputs.size()),
			V(*scalar_it1), V(*scalar_it2)),
	       0)...);
	  }
      }
  }

template <class V, class... F>
  void
  test_values_3arg(const std::initializer_list<typename V::value_type>& inputs,
		   RandomValues<V> random, F&&... fun_pack)
  {
    test_values_3arg<V>(inputs, fun_pack...);
    for (size_t i = 0; i < (random.count + V::size() - 1) / V::size(); ++i)
      {
	[](auto...) {
	}((fun_pack(random(g_mt_gen), random(g_mt_gen), random(g_mt_gen)),
	   0)...);
      }
  }

#if defined(__GCC_IEC_559) && __GCC_IEC_559 < 2
#define COMPLETE_IEC559_SUPPORT (__GCC_IEC_559 >= 2)
#else
#define COMPLETE_IEC559_SUPPORT (__STDC_IEC_559__ == 1)
#endif
#if !COMPLETE_IEC559_SUPPORT
// Without IEC559 we consider -0, subnormals, +/-inf, and all NaNs to be
// invalid (potential UB when used or "produced"). This can't use isnormal (or
// any other classification function), since they know about the UB.
template <class V>
  typename V::mask_type
  isvalid(V x)
  {
    using namespace std::experimental::parallelism_v2;
    using T = typename V::value_type;
    if constexpr (sizeof(T) <= sizeof(double))
      {
	using I = rebind_simd_t<vir::meta::as_int_t<T>, V>;
	const I abs_x = vir::simd_bit_cast<I>(abs(x));
	const I min = vir::simd_bit_cast<I>(V(vir::norm_min_v<T>));
	const I max = vir::simd_bit_cast<I>(V(vir::finite_max_v<T>));
	return static_simd_cast<typename V::mask_type>(
		 vir::simd_bit_cast<I>(x) == 0 || (abs_x >= min && abs_x <= max));
      }
    else
      {
	const V abs_x = abs(x);
	const V min = vir::norm_min_v<T>;
	// Make max non-const static to inhibit constprop. Otherwise the
	// compiler might decide `abs_x <= max` is constexpr true, by definition
	// (-ffinite-math-only)
	static V max = vir::finite_max_v<T>;
	return (x == 0 && copysign(V(1), x) == V(1))
		 || (abs_x >= min && abs_x <= max);
      }
  }
#endif

template <typename TestF, typename RefF, typename ExcF>
  auto
  make_tester(const char* fun_name, const TestF& testfun, const RefF& reffun,
	      const ExcF& exceptfun, const char* file = __FILE__, const int line = __LINE__)
  {
    return [=](auto... inputs) {
      using namespace std::experimental;
      static constexpr auto as_scalar = []<typename T>(const T& x, [[maybe_unused]] std::size_t i) {
        if constexpr (std::is_arithmetic_v<T>)
          return x;
        else
          return x[i];
      };
      using V = decltype(testfun(inputs...));
      using RT = decltype(reffun(as_scalar(inputs, 0)...));
      using RV = typename std::conditional_t<std::is_same_v<RT, typename V::value_type>,
					     vir::meta::type_identity<V>, rebind_simd<RT, V>>::type;
      auto&& expected = [](const auto& fun, const auto&... vs) {
	RV r{};
	for (std::size_t i = 0; i < RV::size(); ++i)
          r[i] = fun(as_scalar(vs, i)...);
	return r;
      };
#if !COMPLETE_IEC559_SUPPORT
      auto&& replace_invalid = [&]<typename T>(T& x, const RV& e) {
	if constexpr (is_simd_v<T>)
	  {
	    where(!isvalid(x), x) = 1;
	    if constexpr (std::is_floating_point_v<RT>)
	      where(!isvalid(e), x) = 1;
	  }
        else
	  {
	    using TS = simd<T, simd_abi::scalar>;
	    if (!isvalid(TS(x))[0])
	      x = 1;
	    else if constexpr (std::is_floating_point_v<RT>)
	      if (!isvalid(e)[0])
		x = 1;
	  }
      };
      (replace_invalid(inputs, expected(reffun, inputs...)), ...);
#endif
      FloatExceptCompare fec;
      const auto totest = testfun(inputs...);
      fec.record_first();
      // use make_value_unknown to avoid reuse of the result from testfun and thus no fp exceptions
      RV expect = expected(exceptfun, make_value_unknown(inputs)...);
      fec.record_second();
      if constexpr (!std::is_same_v<RefF, ExcF>)
	{
	  asm(""::"m"(expect));
	  expect = expected(reffun, inputs...);
	}
      if constexpr (std::is_floating_point_v<RT>)
	{
#if COMPLETE_IEC559_SUPPORT
	  const auto nan_expect = vir::static_simd_cast<typename V::mask_type>(isnan(expect));
	  COMPARE(isnan(totest), nan_expect)
	    .on_failure('\n', file, ':', line, ": ", fun_name, '(', inputs..., ") =\ntotest = ",
			totest, " !=\nexpect = ", expect);
	  [&](auto... inputs) {
            ([&]<typename T>(T& input) {
              if constexpr (std::experimental::is_simd_v<T>)
                where(nan_expect, input) = 0;
            }(inputs), ...);
	    FUZZY_COMPARE(testfun(inputs...), expected(reffun, inputs...))
	      .on_failure('\n', file, ':', line, ": ", fun_name, '(', inputs..., ')');
	  }(inputs...);
	  fec.verify_equal_state(file, line, '\n', fun_name, '(', inputs...,
				 ")\nfirst  = ", totest, "\nsecond = ", expect);
#else
	  FUZZY_COMPARE(totest, expect)
	    .on_failure('\n', file, ':', line, ": ", fun_name, '(', inputs..., ')');
	  fec.verify_equal_state(file, line, '\n', fun_name, '(', inputs...,
				 ")\nfirst  = ", totest, "\nsecond = ", expect);
#endif
	}
      else
	{
	  COMPARE(totest, expect)
	    .on_failure('\n', file, ':', line, ": ", fun_name, '(', inputs..., ')');
	  fec.verify_equal_state(file, line, '\n', fun_name, '(', inputs...,
				 ")\nfirst  = ", totest, "\nsecond = ", expect);
	}
    };
  }

template <typename TestF, typename RefF>
  auto
  make_tester(const char* fun_name, const TestF& testfun, const RefF& reffun,
	      const char* file = __FILE__, const int line = __LINE__)
  { return make_tester(fun_name, testfun, reffun, reffun, file, line); }

template <typename TestF>
  auto
  make_tester(const char* fun_name, const TestF& testfun,
	      const char* file = __FILE__, const int line = __LINE__)
  { return make_tester(fun_name, testfun, testfun, testfun, file, line); }

#define MAKE_TESTER_2(name_, reference_) \
  make_tester(#name_, [](auto... xs) { return name_(xs...); }, \
	      [](auto... xs) { return reference_(xs...); }, __FILE__, __LINE__)

#define MAKE_TESTER(name_) \
  make_tester(#name_, [](auto... xs) { return name_(xs...); }, \
	      [](auto... xs) { return std::name_(xs...); }, __FILE__, __LINE__)
#endif  // SIMD_TESTS_BITS_TEST_VALUES_H_
