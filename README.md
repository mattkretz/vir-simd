# vir::stdx::simd

[![CI](https://github.com/mattkretz/vir-simd/actions/workflows/CI.yml/badge.svg)](https://github.com/mattkretz/vir-simd/actions/workflows/CI.yml)
[![DOI](https://zenodo.org/badge/536945709.svg)](https://zenodo.org/badge/latestdoi/536945709)
[![OpenSSF Best Practices](https://bestpractices.coreinfrastructure.org/projects/6916/badge)](https://bestpractices.coreinfrastructure.org/projects/6916)
[![fair-software.eu](https://img.shields.io/badge/fair--software.eu-%E2%97%8F%20%20%E2%97%8F%20%20%E2%97%8B%20%20%E2%97%8F%20%20%E2%97%8F-yellow)](https://fair-software.eu)

This project aims to provide a fallback std::experimental::simd (Parallelism TS 2)
implementation with additional features. Not every user can rely on GCC 11+ 
and its standard library to be present on all target systems. Therefore, the 
header `vir/simd.h` provides a fallback implementation of the TS specification 
that only implements the `scalar` and `fixed_size<N>` ABI tags. Thus, your code 
can still compile and run correctly, even if it is missing the performance 
gains a proper implementation provides.


## Installation

This is a header-only library. Installation is a simple copy of the headers to 
wherever you want them. Per default `make install` copies the headers into 
`/usr/local/include/vir/`.

Examples:
```sh
# installs to $HOME/.local/include/vir
make install prefix=~/.local

# installs to $HOME/src/myproject/3rdparty/vir
make install includedir=~/src/myproject/3rdparty
```


## Usage

```c++
#include <vir/simd.h>

namespace stdx = vir::stdx;

using floatv = stdx::native_simd<float>;
// ...
```

The `vir/simd.h` header will include `<experimental/simd>` if it is available, 
so you don't have to add any buildsystem support. It should just work.


## Options

* `VIR_SIMD_TS_DROPIN`: Define the macro `VIR_SIMD_TS_DROPIN` before including 
`<vir/simd.h>` to define everything in the namespace specified in the 
Parallelism TS 2 (namely `std::experimental::parallelism_v2`).

* `VIR_DISABLE_STDX_SIMD`: Do not include `<experimental/simd>` even if it is 
available. This allows compiling your code with the `<vir/simd.h>` 
implementation unconditionally. This is useful for testing.


## Additional Features

The TS curiously forgot to add `simd_cast` and `static_simd_cast` overloads for 
`simd_mask`. With `vir::stdx::(static_)simd_cast`, casts will also work for 
`simd_mask`. This does not require any additional includes.

### Simple iota `simd` constants:

*Requires Concepts (C++20).*

```c++
#include <vir/simd_iota.h>

constexpr auto a = vir::iota_v<stdx::simd<float>> * 3; // 0, 3, 6, 9, ...
```

The variable template `vir::iota_v<T>` can be instantiated with arithmetic 
types, array types (`std::array` and C-arrays), and `simd` types. In all cases, 
the elements of the variable will be initialized to `0, 1, 2, 3, 4, ...`, 
depending on the number of elements in `T`. For arithmetic types 
`vir::iota_v<T>` is always just `0`.

### Making `simd` conversions more convenient:

*Requires Concepts (C++20).*

The TS is way too strict about conversions, requiring verbose 
`std::experimental::static_simd_cast<T>(x)` instead of a concise `T(x)` or 
`static_cast<T>(x)`. (`std::simd` in C++26 will fix this.)

`vir::cvt(x)` provides a tool to make `x` implicitly convertible into whatever 
the expression wants in order to be well-formed. This only works, if there is 
an unambiguous type that is required.

```c++
#include <vir/simd_cvt.h>

using floatv = stdx::native_simd<float>;
using intv = stdx::rebind_simd_t<int, floatv>;

void f(intv x) {
  using vir::cvt;
  // the floatv constructor and intv assignment operator clearly determine the
  // destination type:
  x = cvt(10 * sin(floatv(cvt(x))));

  // without vir::cvt, one would have write:
  x = stdx::static_simd_cast<intv>(10 * sin(stdx::static_simd_cast<floatv>(x)));

  // probably don't do this too often:
  auto y = cvt(x); // y is a const-ref to x, but so much more convertible
                   // y is of type cvt<intv>
}
```

Note that `vir::cvt` also works for `simd_mask` and non-`simd` types. Thus, 
`cvt` becomes an important building block for writing "`simd`-generic" code 
(i.e. well-formed for `T` and `simd<T>`).


### Permutations ([paper](https://wg21.link/P2664)):

*Requires Concepts (C++20).*

```c++
#include <vir/simd_permute.h>

// v = {0, 1, 2, 3} -> {1, 0, 3, 2}
vir::simd_permute(v, vir::simd_permutations::swap_neighbors);

// v = {1, 2, 3, 4} -> {2, 2, 2, 2}
vir::simd_permute(v, [](unsigned) { return 1; });

// v = {1, 2, 3, 4} -> {3, 3, 3, 3}
vir::simd_permute(v, [](unsigned) { return -2; });
```

The following permutations are pre-defined:

* `vir::simd_permutations::duplicate_even`: copy values at even indices to 
  neighboring odd position

* `vir::simd_permutations::duplicate_odd`: copy values at odd indices to 
  neighboring even position

* `vir::simd_permutations::swap_neighbors<N>`: swap `N` consecutive values with 
the following `N` consecutive values

* `vir::simd_permutations::broadcast<Idx>`: copy the value at index `Idx` to 
all other values

* `vir::simd_permutations::broadcast_first`: alias for `broadcast<0>`

* `vir::simd_permutations::broadcast_last`: alias for `broadcast<-1>`

* `vir::simd_permutations::reverse`: reverse the order of all values

* `vir::simd_permutations::rotate<Offset>`: positive `Offset` rotates values to 
  the left, negative `Offset` rotates values to the right (moves values from 
  index `(i + Offset) % size` to `i`)

* `vir::simd_permutations::shift<Offset>`: positive `Offset` shifts values to 
  the left, negative `Offset` shifts values to the right; shifting in zeros.

A `vir::simd_permute(x, idx_perm)` overload, where `x` is of *vectorizable* 
type, is also included, facilitating generic code.

A special permutation `vir::simd_shift_in<N>(x, ...)` shifts by N elements 
shifting in elements from additional `simd` objects passed via the pack. 
Example:
```c++
// v = {1, 2, 3, 4}, w = {5, 6, 7, 8} -> {2, 3, 4, 5}
vir::simd_shift_in<1>(v, w);
```

### SIMD execution policy ([P0350](https://wg21.link/P0350)):

*Requires Concepts (C++20).*

Adds an execution policy `vir::execution::simd`. The execution policy can be 
used with the algorithms implemented in the `vir` namespace. These algorithms 
are additionally overloaded in the `std` namespace.

At this point, the implementation of the execution policy requires contiguous 
ranges / iterators.

#### Usable algorithms

* `std::for_each` / `vir::for_each`
* `std::count_if` / `vir::count_if`
* `std::transform` / `vir::transform`
* `std::transform_reduce` / `vir::transform_reduce`

#### Example

```c++
#include <vir/simd_execution.h>

void increment_all(std::vector<float> data) {
  std::for_each(vir::execution::simd, data.begin(), data.end(),
    [](auto& v) {
      v += 1.f;
    });
}

// or

void increment_all(std::vector<float> data) {
  vir::for_each(vir::execution::simd, data,
    [](auto& v) {
      v += 1.f;
    });
}
```

#### Execution policy modifiers

The `vir::execution::simd` execution policy supports a few settings modifying 
its behavior:

* `vir::execution::simd.prefer_size<N>()`:
  Start with chunking the range into parts of `N` elements, calling the 
  user-supplied function(s) with objects of type `resize_simd_t<N, simd<T>>`. 

* `vir::execution::simd.unroll_by<M>()`:
  Iterate over the range in chunks of `simd::size() * M` instead of just 
  `simd::size()`. The algorithm will execute `M` loads (or stores) together 
  before/after calling the user-supplied function(s). The user-supplied 
  function may be called with `M` `simd` objects instead of one `simd` object. 
  Note that prologue and epilogue will typically still call the user-supplied 
  function with a single `simd` object.
  Algorithms like `std::count_if` require a return value from the user-supplied 
  function and therefore still call the function with a single `simd` (to avoid 
  the need for returning an `array` or `tuple` of `simd_mask`). Such algorithms 
  will still make use of unrolling inside their implementation.

* `vir::execution::simd.prefer_aligned()`:
  Unconditionally iterate using smaller chunks, until the main iteration can 
  load (and store) chunks from/to aligned addresses. This can be more efficient 
  if the range is large, avoiding cache-line splits. (e.g. with AVX-512, 
  unaligned iteration leads to cache-line splits on every iteration; with AVX 
  on every second iteration)

* `vir::execution::simd.auto_prologue()`
  (still testing its viability, may be removed):
  Determine from run-time information (i.e. add a branch) whether a prologue 
  for alignment of the main chunked iteration might be more efficient.

### Bitwise operators for floating-point `simd`:

```c++
#include <vir/simd_float_ops.h>

using namespace vir::simd_float_ops;
```
Then the `&`, `|`, and `^` binary operators can be used with objects of type 
`simd<`floating-point`, A>`.


### Conversion between `std::bitset` and `simd_mask`:

```c++
#include <vir/simd_bitset.h>

vir::stdx::simd_mask<int> k;
std::bitset b = vir::to_bitset(k);
vir::stdx::simd_mask k2 = vir::to_simd_mask<float>;
```

There are two overloads of `vir::to_simd_mask`:
```c++
to_simd_mask<T, A>(bitset<simd_size_v<T, A>>)
```
and
```c++
to_simd_mask<T, N>(bitset<N>)
```


### vir::simd_resize and vir::simd_size_cast

The header
```c++
#include <vir/simd_resize.h>
```
declares the functions

* `vir::simd_resize<N>(simd)`,

* `vir::simd_resize<N>(simd_mask)`,

* `vir::simd_size_cast<V>(simd)`, and

* `vir::simd_size_cast<M>(simd_mask)`.

These functions can resize a given `simd` or `simd_mask` object. If the return 
type requires more elements than the input parameter, the new elements are 
default-initialized and appended at the end. Both functions do not allow a 
change of the `value_type`. However, implicit conversions can happen on 
parameter passing to `simd_size_cast`.


### vir::simd_bit_cast

The header
```c++
#include <vir/simd_bit.h>
```
declares the function `vir::simd_bit_cast<To>(from)`. This function serves the 
same purpose as `std::bit_cast` but additionally works in cases where a `simd` 
type is not trivially copyable.


### Concepts

*Requires Concepts (C++20).*

The header
```c++
#include <vir/simd_concepts.h>
```
defines the following concepts:

* `vir::arithmetic<T>`: What `std::arithmetic<T>` should be: satisfied if `T` 
  is an arithmetic type (as specified by the C++ core language).

* `vir::vectorizable<T>`: Satisfied if `T` is a valid element type for 
  `stdx::simd` and `stdx::simd_mask`.

* `vir::simd_abi_tag<T>`: Satisfied if `T` is a valid ABI tag for `stdx::simd` 
  and `stdx::simd_mask`.

* `vir::any_simd<V>`: Satisfied if `V` is a specialization of `stdx::simd<T, 
  Abi>` and the types `T` and `Abi` satisfy `vir::vectorizable<T>` and 
  `vir::simd_abi_tag<Abi>`.

* `vir::any_simd_mask<V>`: Analogue to `vir::any_simd<V>` for `stdx::simd_mask` 
  instead of `stdx::simd`.

* `vir::typed_simd<V, T>`: Satisfied if `vir::any_simd<V>` and `T` is the 
  element type of `V`.

* `vir::sized_simd<V, Width>`: Satisfied if `vir::any_simd<V>` and `Width` is 
  the width of `V`.

* `vir::sized_simd_mask<V, Width>`: Analogue to `vir::sized_simd<V, Width>` for 
  `stdx::simd_mask` instead of `stdx::simd`.


### simdize type transformation

*Requires Concepts (C++20).*

:warning: consider this interface under :construction:

The header
```c++
#include <vir/simdize.h>
```
defines the following types and constants:

* `vir::simdize<T, N>`: `N` is optional. Type alias for a `simd` or 
  `vir::simd_tuple` type determined from the type `T`.

  - If `vir::vectorizable<T>` is satisfied, then `stdx::simd<T, Abi>` is 
    produced. `Abi` is determined from `N` and will be `simd_abi::native<T>` if 
    `N` was omitted.

  - Otherwise, if `T` is a `std::tuple` or aggregate that can be reflected, 
    then a specialization of `vir::simd_tuple` is produced. If `T` is a 
    template specialization (without NTTPs), the metafunction tries 
    vectorization via applying `simdize` to all template arguments. If this 
    doesn't yield the same data structure layout as member-only vectorization, 
    then the type behaves similar to a `std::tuple` with additional API to make 
    the type similar to `stdx::simd` (see below).
    This specialization will be derived from `std::tuple` and the tuple 
    elements will either be `vir::simd_tuple` or `stdx::simd` types. 
    `vir::simdize` is applied recursively to the `std::tuple`/aggregate data 
    members.
    If `N` was omitted, the resulting width of *all* `simd` types in the 
    resulting type will match the largest `native_simd` width.

  Example: `vir::simdize<std::tuple<double, short>>` produces a tuple with the 
  element types `stdx::rebind_simd_t<double, stdx::native_simd<short>>` and
  `stdx::native_simd<short>`.

* `vir::simd_tuple<reflectable_struct T, size_t N>`: Don't use this class 
  template directly. Let `vir::simdize` instantiate specializations of this 
  class template. `vir::simd_tuple` mostly behaves like a `std::tuple` and adds 
  the following interface on top of `std::tuple`:

  - `value_type`

  - `mask_type`

  - `size`

  - tuple-like constructors

  - broadcast and/or conversion constructors

  - load constructor

  - `as_tuple()`: Returns the data members as a `std::tuple`.

  - `operator[](size_t)`: Copy of a single `T` stored in the `simd_tuple`. This 
  is not a cheap operation because there are no `T` objects stored in the 
  `simd_tuple`.

  - `copy_from(std::contiguous_iterator)`: :construction: unoptimized load from 
  a contiguous array of struct (e.g. `std::vector<T>`).

  - `copy_to(std::contiguous_iterator)`: :construction: unoptimized store to a 
  contiguous array of struct.

* `vir::simd_tuple<vectorizable_struct T, size_t N>`: TODO

* `vir::get<I>(simd_tuple)`: Access to the `I`-th data member (a `simd`).

* `vir::simdize_size<T>`, `vir::simdize_size_v<T>`


### Benchmark support functions

*Requires Concepts (C++20) and GNU compatible inline-asm.*

The header
```c++
#include <vir/simd_benchmarking.h>
```
defines the following functions:

* `vir::fake_modify(...)`: Let the compiler assume that all arguments passed to 
  this functions are modified. This inhibits constant propagation, hoisting of 
  code sections, and dead-code elimination.

* `vir::fake_read(...)`: Let the compiler assume that all arguments passed to 
  this function are read (in the cheapest manner). This inhibits dead-code 
  elimination leading up to the results passed to this function.


## Debugging

Compile with `-D _GLIBCXX_DEBUG_UB` to get runtime checks for undefined 
behavior in the `simd` implementation(s). Otherwise, `-fsanitize=undefined` 
without the macro definition will also find the problems, but without 
additional error message.
