# The single header

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

The whole library is also one self-contained file,
[`single_include/beman/inside/inside.hpp`](../../single_include/beman/inside/inside.hpp)
(about 22 000 lines). It inlines every public header — the number, the math,
the units, text, the formats and random sampling — so it needs only the C++
standard library. Put the file on the include path and use it exactly as the
full tree:

```cpp
#include <beman/inside/inside.hpp>   // single_include/ on the include path: nothing else needed
```

```bash
g++-16 -std=c++26 -freflection -I single_include app.cpp
```

It behaves exactly like the separate headers. `BEMAN_INSIDE_MATH_NO_FP` and
`BEMAN_INSIDE_MATH_INSTRUMENT` work the same way, as compiler flags, with one
value in every translation unit of a program. Three guards drop parts of it;
they may differ between translation units:

| Define | Drops | For |
|---|---|---|
| `BEMAN_INSIDE_NO_STRING` | `inside/io.hpp` (`to_string`, `<<`, `>>`, `std::format`, `parse` of a `string_view`) with `<string>`, `<ostream>` and `<format>`, and `units/io.hpp`, the text of quantities | [freestanding builds](freestanding.md) |
| `BEMAN_INSIDE_NO_UNITS` | `units.hpp`, `units/io.hpp`, `units/math.hpp`, `units/random.hpp`, `units/mdspan.hpp` and `units/constants.hpp`, and their compile time | builds without units |
| `BEMAN_INSIDE_NO_MDSPAN` | `inside/mdspan.hpp` and `units/mdspan.hpp` with `<mdspan>` and `<ranges>` | builds without `std::mdspan` support |
| (automatic) | `inside/random.hpp` and `units/random.hpp`, when `__STDC_HOSTED__` is 0 or `BEMAN_INSIDE_MATH_NO_FP` is set; the mdspan headers when `__STDC_HOSTED__` is 0 | freestanding and no-FP builds |

The mathematical constants (`math::pi`, …) come with the math and stay in
every configuration.

## On Compiler Explorer

**[This example](https://godbolt.org/z/MafnPMMxh)** uses the number, the math
and the units together. Compiler Explorer resolves an include by URL for a
single-header library; use GCC 16 (`x86-64 gcc 16.2`) with
`-std=c++26 -freflection -O2`:

```cpp
#include <https://raw.githubusercontent.com/PeterNeiss/inside/main/single_include/beman/inside/inside.hpp>
#include <iostream>

using namespace beman::inside;
using namespace beman::inside::units;

int main() {
    // inside: the range and the step are part of the type.
    inside<{0, 100}> a{70}, b{58};
    auto sum = a + b;                           // inside<{0, 200}>: cannot overflow
    inside<{0, 100}, clamp> capped{sum};        // storing back: the policy decides
    std::cout << "70 + 58         = " << sum << ", capped " << capped << '\n';

    // units: a conversion rescales the grid, so mixed units add exactly.
    quantity<si::kilo(si::metre), inside<{{0, 500}, per<1000>}>> leg{12.5};
    auto total = leg + just<800> * si::metre;   // runs in metres
    auto speed = total / quantity<si::minute, inside<{1, 600}>>{20};
    std::cout << "12.5 km + 800 m = " << total << '\n'
              << "in 20 min       = " << speed.in(si::kilo(si::metre) / si::hour) << '\n';

    // math: correctly rounded onto the output grid, the same bits everywhere.
    using amplitude = inside<{{-1, 1}, per<1000>}, round_nearest>;
    auto s = units::sin_into<amplitude>(quantity<si::degree, inside<{0, 360}>>{30});
    auto r = math::sqrt(inside<{{0, 10}, per<1000>}, round_nearest>{2});
    std::cout << "sin 30°         = " << s << '\n'
              << "sqrt 2          = " << r << '\n';
}
```

```text
70 + 58         = 128, capped 100
12.5 km + 800 m = 13300 m
in 20 min       = 39.9 km/h
sin 30°         = 0.500
sqrt 2          = 1.414
```

The URL above follows `main`. For a link that keeps working when the API
changes, pin a release tag instead:
`https://raw.githubusercontent.com/PeterNeiss/inside/v0.7.0/single_include/beman/inside/inside.hpp`.
Where URL includes are not available, paste the file into a second source
pane named `beman/inside/inside.hpp` and include it as usual.

## How it is made

The single header is generated from `include/` by `cmake/amalgamate.cmake` and
committed; a test fails when it is out of date.
[CONTRIBUTING.md](../../CONTRIBUTING.md#the-single-header) has the steps.
