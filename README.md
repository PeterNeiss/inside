# beman.inside: Numbers That Cannot Go Out of Range

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<!-- markdownlint-disable line-length -->
[![Library Status](https://raw.githubusercontent.com/bemanproject/beman/refs/heads/main/images/badges/beman_badge-beman_library_under_development.svg)](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#the-beman-library-maturity-model)
[![Continuous Integration Tests](https://github.com/PeterNeiss/inside/actions/workflows/ci_tests.yml/badge.svg)](https://github.com/PeterNeiss/inside/actions/workflows/ci_tests.yml)
[![Lint Check (pre-commit)](https://github.com/PeterNeiss/inside/actions/workflows/pre-commit-check.yml/badge.svg)](https://github.com/PeterNeiss/inside/actions/workflows/pre-commit-check.yml)
[![Coverage](https://coveralls.io/repos/github/PeterNeiss/inside/badge.svg?branch=main)](https://coveralls.io/github/PeterNeiss/inside?branch=main)
![Standard Target](https://github.com/bemanproject/beman/blob/main/images/badges/cpp29.svg)
[![Compiler Explorer Example](https://img.shields.io/badge/Try%20it%20on%20Compiler%20Explorer-grey?logo=compilerexplorer&logoColor=67c52a)](https://godbolt.org/z/Poxjj4ffG)
<!-- markdownlint-restore -->

`beman.inside` is a header-only library for numbers that **cannot go out of
range**: the range and the step size live in the *type*. With
`<beman/inside/units.hpp>` the unit, the dimension and the kind live there too,
so a conversion is a compile-time rescale of the grid — exact and free. It needs
C++26 with static reflection (GCC 16) and follows
[The Beman Standard](https://github.com/bemanproject/beman/blob/main/docs/beman_standard.md).

```cpp
inside<{0, 100}> a{42}, b{58};
auto sum = a + b;                       // inside<{0, 200}>: 100, cannot overflow
inside<{0, 50}, clamp> capped = sum;    // 50: the type's policy decides
auto d = just<5> * si::kilo(si::metre) + just<250> * si::metre;   // 5250 m, exact
```

- **Arithmetic cannot overflow.** `+ - * /` widen the result type at compile
  time to hold every possible value.
- **You decide what happens at the edges.** Out of range is only possible when
  a value is stored into a narrower type, and a policy you pick decides:
  `clamp` (saturate), `wrap` (modular, with a carry), or a checked error.
  Fallible results such as division by a grid that holds 0 come back as
  `std::expected<inside, errc>`.
- **Fixed point, done by the compiler.** Think Qm.n with the scale and range
  checked for you; the raw storage (uint8 … int64, an exact fraction, or a
  multi-limb integer past 2⁶⁴ steps) follows from the grid alone.
- **Physical units, and units of your own.** SI and ISQ quantities with kinds,
  ℃/℉ points, and mixed-unit sums that are exact. Add a base dimension for
  your domain — a currency, hit points, pixels, ADC counts — and it never mixes
  with anything else.
- **Correctly rounded math.** `sin`/`cos`/`sqrt`/`exp`/… return the correctly
  rounded point of any output grid wherever the engine decides it, and the
  same bits on every platform, at compile time, and on bare metal without an
  FPU. `sin` of 30° is exactly ½. What is guaranteed, and how to certify a
  type pair your program uses, is in [docs/math.md](docs/math.md#what-is-guaranteed)
  and [docs/certify.md](docs/certify.md).
- **Any size of grid.** A grid's limits and notch have no size limit
  (`inside<{0, 0x1p100}>`); see
  [docs/storage.md](docs/storage.md#grids-past-64-bits).
- **Built for:** audio samples, money, sensor fusion, PID controllers, clocks
  and calendars, game state, embedded registers — anywhere a plain
  `int`/`float` silently overflows, wraps, or drifts.

**Implements**: not yet proposed for standardization. Three draft proposals
are in [docs/proposal](docs/proposal): the number
([inside.md](docs/proposal/inside.md)), quantities
([quantity.md](docs/proposal/quantity.md)) and correctly rounded math
([math.md](docs/proposal/math.md)); build them with `make -C docs/proposal`.

**Status**: [Under development and not yet ready for production use.](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#under-development-and-not-yet-ready-for-production-use)
The public API may change between versions; see the [changelog](CHANGELOG.md).
Developed with [Claude Code](https://claude.com/claude-code).

## License

`beman.inside` is licensed under the Apache License v2.0 with LLVM Exceptions.

## Usage

### Numbers in a range

```cpp
#include <beman/inside/inside.hpp>
using namespace beman::inside;

using pct = inside<{0, 100}>;           // integers 0 … 100
pct x{42}, y{58};
auto sum = x + y;                       // inside<{0, 200}>: no overflow possible
auto z   = x + 1_ins;                   // a scalar needs a grid: 1_ins, not 1

// −1 … 1 in steps of 1/16384: a Q1.14 audio sample.
using sample = inside<{{-1, 1}, per<16384>}, round_nearest>;
sample s{0.5};                          // a dyadic literal: exact

using safe_pct = inside<{0, 100}, clamp>;
safe_pct p{150};                        // 100: saturates instead of failing

auto q = x / inside<{1, 10}>{4};        // 10.5: exact, and a plain value
auto r = x / inside<{0, 10}>{0};        // std::expected: this divisor's grid holds 0
```

### Units

```cpp
#include <beman/inside/units.hpp>
using namespace beman::inside::units;

constexpr auto km = si::kilo(si::metre);
quantity<km, inside<{{0, 500}, per<1000>}>> leg{12.5};    // 12.5 km, to the metre
auto total = leg + just<800> * si::metre;                 // 13300 m: the sum runs in metres, exactly
auto speed = total / quantity<si::minute, inside<{1, 600}>>{20};
speed.in(km / si::hour);                                  // 39.9 km/h
// total + just<1> * si::second;                          // does not compile: another dimension

// Temperatures are points: 350 ℉ is exactly 530/3 ℃.
auto oven = point<international::degree_Fahrenheit>(inside<{0, 600}>{350});
oven.point_in(si::degree_Celsius);                        // 530/3 ℃

// A currency of your own: a base dimension, its quantity, a unit.
inline constexpr base_dimension dollar_dim{"dollar"};
inline constexpr quantity_spec  dollars_def{"dollars", dollar_dim};
inline constexpr named_unit     usd_def{"USD", spec_c<dollars_def>};
inline constexpr auto           USD = unit_c<usd_def>;

using usd = quantity<USD, inside<{{0, 1'000'000}, per<100>}, round_nearest>>;
const usd subtotal{45.07};
const usd tax = mul_into<usd>(subtotal, just<8> * percent);  // 3.61 USD: rounded once, to the cent
```

The policies, `on_wrap`/`on_clamp` hooks, `sum`, `mul_into`, `parse` and the
math functions all work on quantities. See [docs/units.md](docs/units.md) and
[`own_units.cpp`](examples/own_units.cpp).

### New to inside? Start with the tutorial

**[docs/tutorial.md](docs/tutorial.md)** is a 10-minute tour of the mental
model — what an `inside` *is*, why arithmetic widens, and how a value flows
through a program. Read it first; everything else builds on it.

### Documentation

**Guides** — [physical units](docs/units.md) ·
[policies & error handling](docs/policies.md) ·
[arithmetic & rounding](docs/arithmetic.md) ·
[conversions, casts & text](docs/conversions.md) ([printing](docs/conversions.md#writing-text-to_string-operator-and-stdformat)) ·
[storage & STL integration](docs/storage.md) ·
[`beman::inside::math` — bit-exact math](docs/math.md) ·
[mathematical & physical constants](docs/constants.md)

**Special topics** — [adopting safe numbers](docs/adoption.md) ·
[for fixed-point users](docs/fixed-point.md) ·
[determinism & reproducibility](docs/determinism.md) ·
[certifying your math grids](docs/certify.md) ·
[freestanding & bare-metal](docs/freestanding.md) ·
[reading compiler errors](docs/diagnostics.md) ·
[the single header](docs/single-header.md)

**Reference** — [internals & design](docs/internals.md) ·
[roadmap](docs/roadmap.md) · [prior art & talks](docs/resources.md) ·
[accuracy](docs/accuracy.md) / [performance](docs/performance.md) (generated
reports)

### Examples

[`examples/`](examples/) holds 38 self-contained programs; each checks its own
output.

- **Start:** [`basics.cpp`](examples/basics.cpp) (clamp, wrap, bounded loops).
- **Units:** [`units.cpp`](examples/units.cpp) (conversions, kinds, temperatures),
  [`moon_rocket.cpp`](examples/moon_rocket.cpp) (a flight to the Moon),
  [`kitchen.cpp`](examples/kitchen.cpp) (mixed kitchen units, oven points),
  [`circuit.cpp`](examples/circuit.cpp) (Ohm's law, an RC filter),
  [`navigation.cpp`](examples/navigation.cpp) (range and bearing).
- **Units of your own:** [`own_units.cpp`](examples/own_units.cpp) (fathoms, °R, EUR, bytes),
  [`money.cpp`](examples/money.cpp) (USD and ¢, a tax in %),
  [`wei.cpp`](examples/wei.cpp) (ETH, gwei/gas and USD/ETH on 10^27-point grids),
  [`clock.cpp`](examples/clock.cpp) (wrap carries through s, min, h, months),
  [`decibels.cpp`](examples/decibels.cpp) (dB as a level),
  [`adc_calibration.cpp`](examples/adc_calibration.cpp) (ADC counts, a two-point calibration),
  [`game_hp.cpp`](examples/game_hp.cpp), [`rifle.cpp`](examples/rifle.cpp) and
  [`torus_map.cpp`](examples/torus_map.cpp) (HP, rounds, pixels).
- **Exact and big:** [`json_io.cpp`](examples/json_io.cpp) (exact JSON numbers),
  [`expected_pipeline.cpp`](examples/expected_pipeline.cpp) (errors with their cause),
  [`solar_system.cpp`](examples/solar_system.cpp) (nanometres across the solar system),
  [`planck_to_cosmos.cpp`](examples/planck_to_cosmos.cpp) (grids past 64 bits),
  [`rsa.cpp`](examples/rsa.cpp) (textbook RSA-1024 on residue grids).
- **Signal and control:** [`audio_mixer.cpp`](examples/audio_mixer.cpp),
  [`oscillator.cpp`](examples/oscillator.cpp) (a phase counter in slots of 2π/65536 rad),
  [`pid_controller.cpp`](examples/pid_controller.cpp) (K in, % out),
  [`sensor_fusion.cpp`](examples/sensor_fusion.cpp) (a weighted mean of ℃ points).

Each builds as `beman.inside.examples.<name>` and runs as a ctest test
(`ctest --preset gcc-debug -L example`).

### Single header

The library also ships as one self-contained file,
[`single_include/beman/inside/inside.hpp`](single_include/beman/inside/inside.hpp) —
ideal for Compiler Explorer. See [docs/single-header.md](docs/single-header.md).

## Dependencies

### Build Environment

This project requires at least the following to build:

* A C++26 compiler with static reflection: GCC 16 with `-freflection`, which
  the `beman::inside` target adds (configuring fails without it). Grid numbers
  of any size are interned with `std::define_static_array`.
* CMake 3.30 or later
* (Test Only) GoogleTest

You can disable building tests by setting CMake option `BEMAN_INSIDE_BUILD_TESTS` to
`OFF` when configuring the project.

You can disable building examples by setting CMake option `BEMAN_INSIDE_BUILD_EXAMPLES` to
`OFF` when configuring the project.

Library options:

| Option | Default | Effect |
|--------|---------|--------|
| `BEMAN_INSIDE_MATH_NO_FP` | `OFF` | Build without hardware floating point: no `<cmath>`, the math engine's integer path computes every result; results do not change |
| `BEMAN_INSIDE_STRICT_SFINAE` | `OFF` | Drop the assignment diagnostic overloads so `is_constructible` stays honest |
| `BEMAN_INSIDE_FMA` | `ON` | Add `-mfma` on x86-64 so the math engine's `std::fma` is one instruction (see below) |
| `BEMAN_INSIDE_BUILD_TOOLS` | `OFF` | Build the benchmarks, property fuzzer, accuracy sweep and perf workload |

The math engine and bare-metal builds are covered in
[docs/freestanding.md](docs/freestanding.md) and [docs/math.md](docs/math.md).

**FMA on x86-64.** The math engine's kernels use `std::fma`; baseline x86-64
emulates it in software, several times slower (results don't change).
`BEMAN_INSIDE_FMA` (default `ON`) adds `-mfma` to targets linking
`beman::inside` on x86-64, which needs a Haswell/Piledriver (2013) or
later CPU; configure with `-DBEMAN_INSIDE_FMA=OFF` for older ones. Without
CMake, add `-mfma` (or `-march=x86-64-v3`) yourself.

### Supported Platforms

| Compiler | Version | C++ Standard | Standard Library |
|----------|---------|--------------|------------------|
| GCC      | 16      | C++26        | libstdc++        |

The library needs static reflection; GCC 16 is the first compiler with it
(`-freflection`). CI tests every build option on GCC 16, and a weekly run adds
Debug, TSan and the full certify check.

## Development

See the [Contributing Guidelines](CONTRIBUTING.md).

## Integrate beman.inside into your project

### Build

You can build inside using a CMake workflow preset:

```bash
cmake --workflow --preset gcc-release
```

The `gcc-*` presets use the `g++` on the path, which must be GCC 16; the
`gcc16-*` presets name `g++-16` (the build directory is then `build/gcc16-release`).

To list available workflow presets, you can invoke:

```bash
cmake --list-presets=workflow
```

For details on building beman.inside without using a CMake preset, refer to the
[Contributing Guidelines](CONTRIBUTING.md).

### Installation

To install beman.inside globally after building with the `gcc-release` preset, you can
run:

```bash
sudo cmake --install build/gcc-release
```

Alternatively, to install to a prefix, for example `/opt/beman`, you can run:

```bash
sudo cmake --install build/gcc-release --prefix /opt/beman
```

This will generate the following directory structure:

```txt
/opt/beman
├── include
│   └── beman
│       └── inside
│           ├── inside.hpp
│           └── ...
├── lib
│   └── cmake
│       └── beman.inside
│           ├── beman.inside-config-version.cmake
│           ├── beman.inside-config.cmake
│           └── beman.inside-targets.cmake
└── share
    └── beman.inside
        └── single_include
            └── beman
                └── inside
                    └── inside.hpp
```

### CMake Configuration

If you installed beman.inside to a prefix, you can specify that prefix to your CMake
project using `CMAKE_PREFIX_PATH`; for example, `-DCMAKE_PREFIX_PATH=/opt/beman`.

You need to bring in the `beman.inside` package to define the `beman::inside` CMake
target:

```cmake
find_package(beman.inside REQUIRED)
```

You will then need to add `beman::inside` to the link libraries of any libraries or
executables that include `beman.inside` headers.

```cmake
target_link_libraries(yourlib PUBLIC beman::inside)
```

### Using beman.inside

To use `beman.inside` in your C++ project,
include an appropriate `beman.inside` header from your source code.

```c++
#include <beman/inside/inside.hpp>
```

Units are in `<beman/inside/units.hpp>`, and their math functions in
`<beman/inside/units_math.hpp>`; the single header holds both.
