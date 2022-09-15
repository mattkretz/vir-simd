# vir::stdx::simd

This project aims to provide a simple fallback for users of 
std::experimental::simd (Parallelism TS 2). Not every user can rely on GCC 11+ 
and its standard library to be present on all target systems. Therefore, the 
header `vir/simd.h` provides a fallback implementation of the TS specification 
that only implements the `scalar` ABI tag. Thus, your code can still compile 
and run correctly, even if it is missing the performance gains a proper 
implementation provides.

## Usage

```c++
#include <vir/simd.h>

namespace stdx = vir::stdx;

using floatv = stdx::native_simd<float>;
// ...
```

The `vir/simd.h` header will include `<experimental/simd>` if it is available, 
so you don't have to add any buildsystem support. It should just work.

## Debugging

Compile with `-D _GLIBCXX_DEBUG_UB` to get runtime checks for undefined 
behavior in the `simd` implementation(s). Otherwise, `-fsanitize=undefined` 
without the macro definition will also find the problems, but without 
additional error message.
