# vir::stdx::simd

[![Makefile CI](https://github.com/mattkretz/vir-simd/actions/workflows/makefile.yml/badge.svg)](https://github.com/mattkretz/vir-simd/actions/workflows/makefile.yml)
[![fair-software.eu](https://img.shields.io/badge/fair--software.eu-%E2%97%8F%20%20%E2%97%8F%20%20%E2%97%8B%20%20%E2%97%8B%20%20%E2%97%8B-orange)](https://fair-software.eu)

This project aims to provide a simple fallback for users of 
std::experimental::simd (Parallelism TS 2). Not every user can rely on GCC 11+ 
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

:warning: consider this interface under :construction:

The header
```c++
#include <vir/simdize.h>
```
defines the following types and constants:

* `vir::simdize<T, N>`: `N` is optional. Type alias for a `simd` or 
  `vir::simd_tuple` type determined from the type `T`.

  - If `vir::vectorizable<T>` is satisfied, then `stdx::simd<T, Abi>` is 
    produced. `Abi` is determined from `N` and will be `simd_abi::native<T>` is 
    `N` was omitted.

  - Otherwise, if `T` is a `std::tuple` or aggregate that can be reflected, 
    then a specialization of `vir::simd_tuple` is produced. This specialization 
    will be derived from `std::tuple` and the tuple elements will either be 
    `vir::simd_tuple` or `stdx::simd` types. `vir::simdize` is applied 
    recursively to the `std::tuple`/aggregate data members.
    If `N` was omitted, the resulting width of *all* `simd` types in the 
    resulting type will match the largest `native_simd` width.

  Example: `vir::simdize<std::tuple<double, short>>` produces a tuple with the 
  element types `stdx::rebind_simd_t<double, stdx::native_simd<short>>` and
  `stdx::native_simd<short>`.

* `vir::simd_tuple<reflectable_struct T, size_t N>`: Don't use this class 
  template directly. Let `vir::simdize` instantiate specializations of this 
  class template. `vir::simd_tuple` mostly behaves like a `std::tuple` and adds 
  the following interface on top of `std::tuple`:

  - `mask_type`

  - `size`

  - tuple-like constructors

  - broadcast constructors

  - `as_tuple()`: Returns the data members as a `std::tuple`.

  - `operator[](size_t)`: Copy of a single `T` stored in the `simd_tuple`. This 
  is not a cheap operation because there are no `T` objects stored in the 
  `simd_tuple`.

  - `copy_from(std::contiguous_iterator)`: :construction: unoptimized load from 
  a contiguous array of struct (e.g. `std::vector<T>`).

  - `copy_to(std::contiguous_iterator)`: :construction: unoptimized store to a 
  contiguous array of struct.

* `vir::get<I>(simd_tuple)`: Access to the `I`-th data member (a `simd`).

* `vir::simdize_size<T>`, `vir::simdize_size_v<T>`


## Debugging

Compile with `-D _GLIBCXX_DEBUG_UB` to get runtime checks for undefined 
behavior in the `simd` implementation(s). Otherwise, `-fsanitize=undefined` 
without the macro definition will also find the problems, but without 
additional error message.
