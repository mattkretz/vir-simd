/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2020–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *                       Matthias Kretz <m.kretz@gsi.de>
 */

#ifndef SIMD_TESTS_BITS_MATHREFERENCE_H_
#define SIMD_TESTS_BITS_MATHREFERENCE_H_
#include <tuple>
#include <utility>
#include <cstdio>

template <typename T>
  struct SincosReference
  {
    T x, s, c;

    std::tuple<const T &, const T &, const T &>
    as_tuple() const
    { return std::tie(x, s, c); }
  };

template <typename T>
  struct Reference {
    T x, ref;

    std::tuple<const T &, const T &>
    as_tuple() const
    { return std::tie(x, ref); }
  };

template <typename T>
  struct Array
  {
    std::size_t size_;
    const T *data_;

    Array()
    : size_(0), data_(nullptr) {}

    Array(size_t s, const T *p)
    : size_(s), data_(p) {}

    const T*
    begin() const
    { return data_; }

    const T*
    end() const
    { return data_ + size_; }

    std::size_t
    size() const
    { return size_; }
  };

namespace function {
  struct sincos{ static constexpr const char *const str = "sincos"; };
  struct atan  { static constexpr const char *const str = "atan"; };
  struct asin  { static constexpr const char *const str = "asin"; };
  struct acos  { static constexpr const char *const str = "acos"; };
  struct log   { static constexpr const char *const str = "ln"; };
  struct log2  { static constexpr const char *const str = "log2"; };
  struct log10 { static constexpr const char *const str = "log10"; };
}

template <class F>
  struct testdatatype_for_function
  {
    template <class T>
      using type = Reference<T>;
  };

template <>
  struct testdatatype_for_function<function::sincos>
  {
    template <class T>
      using type = SincosReference<T>;
  };

template <class F, class T>
  using testdatatype_for_function_t
    = typename testdatatype_for_function<F>::template type<T>;

template<typename T>
  struct StaticDeleter
  {
    const T *ptr;

    StaticDeleter(const T *p)
    : ptr(p) {}

    ~StaticDeleter()
    { delete[] ptr; }
  };

template <class F, class T>
  inline std::string filename()
  {
    static_assert(std::is_floating_point<T>::value, "");
    static const auto cache
      = std::string("reference-") + std::string(F::str)
      + std::string(sizeof(T) == 4 && vir::digits_v<T> == 24
	 && vir::max_exponent_v<T> == 128
	 ? "-sp.dat"
	 : (sizeof(T) == 8
	    && vir::digits_v<T> == 53
	    && vir::max_exponent_v<T> == 1024
	    ? "-dp.dat"
	    : (sizeof(T) == 16 && vir::digits_v<T> == 64
	       && vir::max_exponent_v<T> == 16384
	       ? "-ep.dat"
	       : (sizeof(T) == 16 && vir::digits_v<T> == 113
		  && vir::max_exponent_v<T> == 16384
		  ? "-qp.dat"
		  : "-unknown.dat"))));
    return cache;
  }

template <class Fun, class T, class Ref = testdatatype_for_function_t<Fun, T>>
  Array<Ref>
  referenceData()
  {
    static Array<Ref> data;
    if (data.data_ == nullptr)
      {
	FILE* file = std::fopen(filename<Fun, T>().c_str(), "rb");
	if (file)
	  {
	    std::fseek(file, 0, SEEK_END);
	    const size_t size = std::ftell(file) / sizeof(Ref);
	    std::rewind(file);
	    auto                      mem = new Ref[size];
	    static StaticDeleter<Ref> _cleanup(data.data_);
	    data.size_ = std::fread(mem, sizeof(Ref), size, file);
	    data.data_ = mem;
	    std::fclose(file);
	  }
	else
	  {
	    __builtin_fprintf(
		stderr,
		"%s:%d: the reference data %s does not exist in the current "
		"working directory.\n",
		__FILE__, __LINE__, filename<Fun, T>().c_str());
	    __builtin_abort();
	  }
      }
    return data;
  }
#endif  // SIMD_TESTS_BITS_MATHREFERENCE_H_
