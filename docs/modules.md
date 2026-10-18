# Modules (experimental)

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`import beman.inside;` gives everything the public headers give, units aside;
`import beman.inside.units;` adds the units and re-exports `beman.inside`.
The modules are wrappers around the headers (modules/), which stay the
source of truth.

```cpp
#include <iostream> // standard headers first (GCC, see below)
import beman.inside;
using namespace beman::inside;

int main() {
    inside<{{0, 10}, per<4>}> a{2.5};
    std::cout << a * just<2> << '\n'; // 5
}
```

## Building them

Configure with `-DBEMAN_INSIDE_BUILD_MODULES=ON` and a generator that supports
C++ modules (Ninja, Visual Studio), CMake 3.28 or later. Link
`beman::inside_module`. With Clang, CMake also needs `clang-scan-deps`
(`-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=clang-scan-deps-20`). The modules are
not installed yet.

The library's macros (`BEMAN_INSIDE_MATH_NO_FP`, `BEMAN_INSIDE_NO_UNITS`, …)
are options of the modules' build, not of the importing file: a module
exports no macros.

## How they are made

Each module includes the headers in its purview inside `extern "C++"`, so the
library's entities belong to the global module as if included: one file of
a program may `#include` the headers and another `import` the module. The
standard headers sit in the global module fragment (modules/std_headers.hpp).
Only the public names are exported, one using-declaration each, generated
from the headers by `modules/gen_exports.py` (clang's AST): rerun it when a
header gains or loses a public name.

With `BEMAN_INSIDE_BUILD_MODULES=ON` every example is also built against the
modules (ctest label `module`): headers named like the library's
(modules/shim) hold only an import, so an example compiles unchanged, and a
name the modules fail to export fails its build.

## Status (2026-10-18)

| | C++23 | C++26 |
|---|---|---|
| Clang 20 | 38/38 examples | 38/38 examples |
| GCC 16 | 37/38 examples | 37/38 without big grids; the units module fails with big grids |

GCC 16's limits, all in its handling of headers next to imports:

- A standard header that is not in the modules' global fragment, included
  *after* the import, breaks (thousands of errors inside libstdc++; `decibels`
  includes `<print>` after its library includes). Include standard headers
  before the import.
- With big grids (C++26, `-freflection`, `<meta>`) the units module's
  fragment conflicts with the imported core ("conflicting imported
  declaration").
- Explicit specializations of an `inline constexpr` variable template were
  not emitted for importers (worked around: `type_name_of` is a function).

## Compile time

`tests/beman/inside/module_compile_time.sh`: best of three, the same flags for
both (`-std=gnu++NN -mfma`, as CMake builds the modules); N = 200 expressions.

| | core, nothing used | 200 inside expressions | 200 quantity expressions | building the modules |
|---|---|---|---|---|
| Clang 20, C++23 | 0.47 s → 0.01 s | 6.85 s → 6.61 s | 8.96 s → 8.47 s | 3.52 s |
| Clang 20, C++26 | 0.52 s → 0.02 s | 9.34 s → 9.19 s | 11.44 s → 10.88 s | 3.72 s |
| GCC 16, C++23 | 0.33 s → 0.01 s | 3.84 s → 3.84 s | 5.29 s → 5.45 s | 3.35 s |
| GCC 16, C++26 (no big grids) | 0.37 s → 0.01 s | 3.95 s → 4.01 s | 6.17 s → 5.90 s | 4.46 s |

An import removes the cost of parsing the headers, about 0.3–0.5 s per file.
It does not touch the cost of instantiating grids and quantities, which is
most of a file that uses the library heavily: there the change is within
±5%.
