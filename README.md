# beman.inside: numbers that cannot go out of range

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<!-- markdownlint-disable line-length -->
[![Continuous Integration Tests](https://github.com/PeterNeiss/inside/actions/workflows/ci_tests.yml/badge.svg)](https://github.com/PeterNeiss/inside/actions/workflows/ci_tests.yml)
[![Lint Check (pre-commit)](https://github.com/PeterNeiss/inside/actions/workflows/pre-commit-check.yml/badge.svg)](https://github.com/PeterNeiss/inside/actions/workflows/pre-commit-check.yml)
[![Coverage](https://coveralls.io/repos/github/PeterNeiss/inside/badge.svg?branch=main)](https://coveralls.io/github/PeterNeiss/inside?branch=main)
[![Try it on Compiler Explorer](https://img.shields.io/badge/Try%20it%20on%20Compiler%20Explorer-grey?logo=compilerexplorer&logoColor=67c52a)](https://godbolt.org/z/MafnPMMxh)
<!-- markdownlint-restore -->

`beman.inside` is a header-only C++26 library for numbers whose **range and
step size are part of the type**. Arithmetic widens its result type, so it
cannot overflow; when a value is stored into a narrower type, a policy you
pick decides what happens. On top of that number sit physical units, where a
conversion is an exact rescale of the grid, and math functions that return
the correctly rounded point of any output grid.

```cpp
#include <beman/inside/inside.hpp>       // the number
#include <beman/inside/math.hpp>         // sin, sqrt, exp, …
#include <beman/inside/units.hpp>        // quantities
#include <beman/inside/units/math.hpp>   // math on quantities
using namespace beman::inside;
using namespace beman::inside::units;

inside<{0, 100}> a{70}, b{58};
auto sum = a + b;                           // inside<{0, 200}>: 128, cannot overflow
inside<{0, 100}, clamp> capped{sum};        // 100: the type's policy decides

quantity<si::kilo(si::metre), inside<{{0, 500}, per<1000>}>> leg{12.5, si::kilo(si::metre)};
auto total = leg + just<800> * si::metre;   // 13300 m, exact
auto speed = total / quantity<si::minute, inside<{1, 600}>>{20, si::minute};
speed.in(si::kilo(si::metre) / si::hour);   // 39.9 km/h

using amplitude = inside<{{-1, 1}, per<1000>}, round_nearest>;
math::sin<amplitude>(just<30> * si::degree);                       // exactly 0.5
math::sqrt(inside<{{0, 10}, per<1000>}, round_nearest>{2});              // 1.414, correctly rounded
```

**[Try it on Compiler Explorer](https://godbolt.org/z/MafnPMMxh).**

## What you get

- **Arithmetic cannot overflow.** `+ - * /` compute a result type that holds
  every possible result, at compile time.
- **You decide what happens at the edges.** Only a store into a narrower type
  can fail, and the type's policy decides: report an error (the default),
  saturate (`clamp`), wrap around (`wrap`), or round.
- **Exact by default.** Values are integers or exact fractions. A step of
  1/100 holds cents exactly, and a division is exact unless you ask it to round.
- **Physical units, and units of your own.** SI and ISQ quantities, ℃/℉
  temperatures, and mixed-unit sums that are exact. A dimension of your own
  (a currency, hit points, ADC counts) never mixes with anything else.
- **Correctly rounded math.** `sin`, `sqrt`, `exp`, … return the correctly
  rounded point of the output grid, with the same bits on every platform, at
  compile time and without an FPU.
- **Fixed point without the bookkeeping.** A grid is a Qm.n format whose scale
  and range the compiler tracks; the storage (`uint8_t` … `int64_t`, or wider)
  follows from the grid.

## Getting it

The library needs **GCC 16** in C++26 mode with static reflection
(`-std=c++26 -freflection`). It has no other dependencies.

**One header.** Copy
[`single_include/beman/inside/inside.hpp`](single_include/beman/inside/inside.hpp),
which holds the whole library, and compile with
`g++-16 -std=c++26 -freflection -I single_include app.cpp`. On Compiler
Explorer, include it by URL; see
[advanced/single-header.md](docs/advanced/single-header.md).

**CMake.** Install the library and link the `beman::inside` target, which
adds `-freflection` for you:

```bash
cmake --workflow --preset gcc-release           # gcc16-release names g++-16
cmake --install build/gcc-release --prefix /opt/inside
```

```cmake
find_package(beman.inside REQUIRED)             # with -DCMAKE_PREFIX_PATH=/opt/inside
target_link_libraries(app PRIVATE beman::inside)
```

`add_subdirectory` or `FetchContent` on this repository gives the same
target, without the tests and examples.

| Header | Holds |
|---|---|
| `<beman/inside/inside.hpp>` | the number `inside`, its arithmetic, policies, ranges |
| `<beman/inside/math.hpp>` | `beman::inside::math`: correctly rounded functions and constants |
| `<beman/inside/units.hpp>` | `beman::inside::units`: quantities, the SI and ISQ catalog |
| `<beman/inside/units/math.hpp>` | the math functions on quantities: degrees, roots of units, … |
| `…/inside/io.hpp`, `…/units/io.hpp` | text: `to_string`, `<<`, `std::format`, `parse` (opt-in) |

The other opt-in headers are listed in
[docs/README.md](docs/README.md#headers).

## Documentation

Start with **[Getting started](docs/getting-started.md)**, a short tour of
the ideas. Then:

- **Guides**, for everyday use:
  [grids](docs/guide/grids.md) ·
  [policies](docs/guide/policies.md) ·
  [arithmetic](docs/guide/arithmetic.md) ·
  [conversions and text](docs/guide/conversions.md) ·
  [ranges and the STL](docs/guide/ranges.md) ·
  [units](docs/guide/units.md) ·
  [math](docs/guide/math.md)
- **Advanced topics**, for the details: exact arithmetic, storage, hooks,
  the units model, the math engine, determinism, fixed point, bare metal,
  compiler errors and more.

[docs/README.md](docs/README.md) is the map of all pages.

## Examples

[`examples/`](examples/) holds 44 programs; each checks its own output and
runs as a test (`ctest --preset gcc-debug -L example`). A few to start with:

- [`basics.cpp`](examples/basics.cpp): clamp, wrap and bounded loops.
- [`units.cpp`](examples/units.cpp): conversions, kinds, temperatures.
- [`money.cpp`](examples/money.cpp): a currency of your own, a tax rounded once
  to the cent.
- [`clock.cpp`](examples/clock.cpp): a wrapping carry through s, min, h and months.
- [`navigation.cpp`](examples/navigation.cpp): range and bearing with the math functions.
- [`fixed_point.cpp`](examples/fixed_point.cpp): Q formats as grids.
- [`solar_system.cpp`](examples/solar_system.cpp): nanometres across the solar
  system, on a grid past 64 bits.

[docs/README.md](docs/README.md#examples) lists all of them by theme.

## Status

Under development: the API may change between versions, and each release
lists what changed in the [changelog](CHANGELOG.md). Not yet proposed for
standardization; three draft papers are in [docs/proposal](docs/proposal):
the number ([inside.md](docs/proposal/inside.md)), quantities
([quantity.md](docs/proposal/quantity.md)) and correctly rounded math
([math.md](docs/proposal/math.md)). Developed with
[Claude Code](https://claude.com/claude-code).

Contributions: see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Apache License v2.0 with LLVM Exceptions.
