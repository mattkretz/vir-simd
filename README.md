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

* `arithmetic<T>`: What `std::arithmetic<T>` should be: satisfied if `T` is an 
  arithmetic type (as specified by the C++ core language).

* `vectorizable<T>`: Satisfied if `T` is a valid element type for `simd` and 
  `simd_mask`.

* `simd_abi_tag<T>`: Satisfied if `T` is a valid ABI tag for `simd` and 
  `simd_mask`.

* `any_simd<V>`: Satisfied if `V` is a specialization of `simd<T, Abi>` and the 
  types `T` and `Abi` satisfy `vectorizable<T>` and `simd_abi_tag<Abi>`.

* `any_simd_mask<V>`: Analogue to `any_simd<V>` for `simd_mask` instead of 
  `simd`.

* `typed_simd<V, T>`: Satisfied if `any_simd<V>` and `T` is the element type of 
  `V`.

* `sized_simd<V, Width>`: Satisfied if `any_simd<V>` and `Width` is the width 
  of `V`.

* `sized_simd_mask<V, Width>`: Analogue to `sized_simd<V, Width>` for 
  `simd_mask` instead of `simd`.


### simdize type transformation

:warning: consider this interface under :construction:

The header
```c++
#include <vir/simdize.h>
```
defines the following types and constants:

* `simdize<T, N>`: `N` is optional. Type alias for a `simd` or `simd_tuple` 
  type determined from the type `T`.

  - If `vectorizable<T>` is satisfied, then `simd<T, Abi>` is produced. `Abi` 
    is determined from `N` and will be `simd_abi::native<T>` is `N` was 
    omitted.

  - Otherwise, if `T` is a `std::tuple` or aggregate that can be reflected, 
    then a specialization of `simd_tuple` is produced. This specialization will 
    be derived from `std::tuple` and the tuple elements will either be 
    `simd_tuple` or `simd` types. `simdize` is applied recursively to the 
    `tuple`/aggregate data members.
    If `N` was omitted, the resulting width of *all* `simd` types in the 
    resulting type will match the largest `native_simd` width.

  Example: `simdize<std::tuple<double, short>>` produces a tuple with the 
  element types `rebind_simd_t<double, native_simd<short>>` and
  `native_simd<short>`.

* `simd_tuple<reflectable_struct T, size_t N>`: Don't use this class template 
  directly. Let `simdize` instantiate specializations of this class template. 
  `simd_tuple` derives from `std::tuple` and adds the following interface on 
  top of `std::tuple`:

  - `size`

  - `mask_type`

  - `copy_from`

* `simdize_size<T>`, `simdize_size_v<T>`


## Debugging

Compile with `-D _GLIBCXX_DEBUG_UB` to get runtime checks for undefined 
behavior in the `simd` implementation(s). Otherwise, `-fsanitize=undefined` 
without the macro definition will also find the problems, but without 
additional error message.
