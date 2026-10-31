# The single header

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

The whole library is also one self-contained file,
[`single_include/beman/inside/inside.hpp`](../single_include/beman/inside/inside.hpp)
(about 22 000 lines). It inlines every public header, units and math
included, so it needs only the C++ standard library. Put the file on the
include path and use it exactly as the full tree:

```cpp
#include <beman/inside/inside.hpp>   // single_include/ on the include path: nothing else needed
```

It behaves exactly like the separate headers. `BEMAN_INSIDE_MATH_NO_FP` and
`BEMAN_INSIDE_MATH_TABLE_SLOTS` work the same way, as compiler flags. Three
guards drop parts of it:

| Define | Drops | For |
|---|---|---|
| `BEMAN_INSIDE_NO_STRING` | inside/io.hpp (`to_string`, `<<`, `>>`, `std::format`, `parse` of a `string_view`) with `<string>`, `<ostream>` and `<format>`; and with it units/io.hpp, the text of quantities | freestanding builds ([freestanding.md](freestanding.md)) |
| `BEMAN_INSIDE_NO_UNITS` | units.hpp, units/io.hpp, units/math.hpp and units/constants.hpp (the physical constants), and their compile time | builds without units |
| (automatic) | inside/random.hpp, when `__STDC_HOSTED__` is 0 or `BEMAN_INSIDE_MATH_NO_FP` is set | freestanding and no-FP builds |

The mathematical constants (`math::pi`, …, math/constants.hpp) come with the
math and stay in every configuration.

Compile with `-std=c++26 -freflection` (GCC 16): the header needs static
reflection.

## On Compiler Explorer

Where there is no include tree to set up, the single header is the easy way
in:

- paste it into a second source pane named `beman/inside/inside.hpp` and
  `#include` it; or
- include it by URL (Compiler Explorer resolves URL includes for
  single-header libraries), for GCC 16 with `-std=c++26 -freflection`. Pin
  a release tag for a link that stays valid, or use `main` for the latest:

  ```cpp
  #include <https://raw.githubusercontent.com/PeterNeiss/inside/v0.6.0/single_include/beman/inside/inside.hpp>
  #include <https://raw.githubusercontent.com/PeterNeiss/inside/main/single_include/beman/inside/inside.hpp>
  ```

A first program with units:

```cpp
#include <https://raw.githubusercontent.com/PeterNeiss/inside/main/single_include/beman/inside/inside.hpp>
#include <iostream>
using namespace beman::inside;
using namespace beman::inside::units;

int main() {
    quantity<si::kilo(si::metre), inside<{{0, 500}, per<1000>}>> leg{12.5};
    auto total = leg + just<800> * si::metre;                     // 13300 m, exact
    std::cout << total << " = " << total.in(si::kilo(si::metre)) << "\n";
}
```

## Regenerating it

The committed single header is **generated**, never edited by hand. A
pure-CMake amalgamator (`cmake/amalgamate.cmake`) produces it from `include/`.
After editing any header under `include/`, format first, then regenerate and
commit:

```bash
pre-commit run --all-files                             # clang-format first: the header copies its text
cmake --build build --target beman.inside.amalgamate   # rewrites single_include/beman/inside/inside.hpp
ctest --test-dir build -L tooling                      # amalgamate_up_to_date: fails if it drifted
ctest --test-dir build -R single_header_smoke          # a TU that sees only single_include/
```

`ctest` runs `amalgamate_up_to_date` in the normal suite, so a stale single
header fails the build until it is regenerated. CI also builds
`single_header_freestanding_smoke` (exceptions off, `BEMAN_INSIDE_NO_STRING`)
and `single_header_nofp_smoke` (a poison `<cmath>` that proves
`BEMAN_INSIDE_MATH_NO_FP` pulls in no floating-point header) against it.
