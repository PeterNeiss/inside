# beman.inside: Numbers That Cannot Go Out of Range

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<!-- markdownlint-disable line-length -->
[![Library Status](https://raw.githubusercontent.com/bemanproject/beman/refs/heads/main/images/badges/beman_badge-beman_library_under_development.svg)](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#the-beman-library-maturity-model)
[![Continuous Integration Tests](https://github.com/NiceAndPeter/inside/actions/workflows/ci_tests.yml/badge.svg)](https://github.com/NiceAndPeter/inside/actions/workflows/ci_tests.yml)
[![Lint Check (pre-commit)](https://github.com/NiceAndPeter/inside/actions/workflows/pre-commit-check.yml/badge.svg)](https://github.com/NiceAndPeter/inside/actions/workflows/pre-commit-check.yml)
[![Coverage](https://coveralls.io/repos/github/NiceAndPeter/inside/badge.svg?branch=main)](https://coveralls.io/github/NiceAndPeter/inside?branch=main)
<!-- markdownlint-restore -->

`beman.inside` is a header-only C++23 library (C++20 fallback) for numbers that
**cannot go out of range** — the range and step size live in the *type*. It
follows the layout and tooling of [The Beman Standard](https://github.com/bemanproject/beman/blob/main/docs/beman_standard.md).

- **Arithmetic cannot overflow.** `+ - * /` widen the result type at compile
  time to hold every possible value — no runtime surprises.
- **You decide what happens at the edges.** Out-of-range is only possible when a
  value is assigned into a narrower type, and a policy you pick — `clamp`
  (saturate), `wrap` (modular), `sentinel`, checked error — decides the outcome.
- **It's fixed-point, done by the compiler.** Think Qm.n with the scale and
  range checked for you; the optimal raw storage (uint8…int64, double, exact
  fraction) is picked automatically.
- **Reproducible math.** `sin`/`cos`/`sqrt`/`exp`/… give bit-identical results
  across platforms — three engines, including an FPU-free `constexpr` CORDIC
  engine for bare metal.
- **Built for:** audio samples, money, percentages, PID controllers, sensor
  fusion, embedded registers — anywhere a plain `int`/`float` silently
  overflows, wraps, or drifts.

**Implements**: not yet proposed for standardization.

**Status**: [Under development and not yet ready for production use.](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#under-development-and-not-yet-ready-for-production-use)
The public API may change between versions. Developed with
[Claude Code](https://claude.com/claude-code).

## License

`beman.inside` is licensed under the Apache License v2.0 with LLVM Exceptions.
The `beman::inside::slim` components (`slim/optional.hpp`, `slim/expected.hpp`)
are MIT-licensed.

## Usage

```cpp
#include <beman/inside/inside.hpp>
using namespace beman::inside;

// A percentage: integer values in [0, 100].
using pct = inside<{0, 100}>;
pct x = 42;
pct y = 58;
auto sum = x + y;          // inside<{0, 200}> — no overflow possible
auto z = x + 1_ins;        // scalars need a grid: 1_ins, not 1

// Fractional grid: −1 .. 1 in 1/16 384 steps (Q1.14 audio sample).
using sample = inside<{{-1, 1}, notch<1, 16384>}, round_nearest>;
sample s = 0.5;            // dyadic literal — exact
s.numerator();             // 1   (denominator() == 2): exact read-out

// Clamped percentage: saturates instead of throwing.
using safe_pct = inside<{0, 100}, clamp>;
safe_pct p = 150;          // p == 100
```

### New to inside? Start with the tutorial

**[docs/tutorial.md](docs/tutorial.md)** is a 10-minute tour of the mental
model — what an `inside` *is*, why arithmetic widens, and how a value flows
through a program. Read it first; everything else builds on it.

### Documentation

**Guides** — [policies & error handling](docs/policies.md) ·
[arithmetic & rounding](docs/arithmetic.md) ·
[conversions & casts](docs/conversions.md) ·
[storage & STL integration](docs/storage.md) ·
[`beman::inside::math` — bit-exact math](docs/math.md)

**Special topics** — [for fixed-point users](docs/fixed-point.md) ·
[determinism & reproducibility](docs/determinism.md) ·
[freestanding & bare-metal](docs/freestanding.md) ·
[reading compiler errors](docs/diagnostics.md) ·
[the single header](docs/single-header.md)

**Reference** — [internals & design](docs/internals.md) ·
[roadmap](docs/roadmap.md) · [prior art & talks](docs/resources.md) ·
[accuracy](docs/accuracy.md) / [performance](docs/performance.md) (generated
reports)

### Examples

[`examples/`](examples/) holds 30+ self-contained programs — e.g.
[`clock.cpp`](examples/clock.cpp) (wrap with carry),
[`money.cpp`](examples/money.cpp) (cents-exact currency),
[`pid_controller.cpp`](examples/pid_controller.cpp) (fixed-point control loop).
Each builds as `beman.inside.examples.<name>` and runs as a ctest test
(`ctest --preset gcc-debug -L example`).

### Single header

The library also ships as one self-contained file,
[`single_include/beman/inside/inside.hpp`](single_include/beman/inside/inside.hpp) —
ideal for Compiler Explorer. See [docs/single-header.md](docs/single-header.md).

## Dependencies

### Build Environment

This project requires at least the following to build:

* A C++ compiler that conforms to the C++20 standard or greater (C++23 is the
  default and enables `std::format` / `<expected>` integration)
* CMake 3.30 or later
* (Test Only) GoogleTest

You can disable building tests by setting CMake option `BEMAN_INSIDE_BUILD_TESTS` to
`OFF` when configuring the project.

You can disable building examples by setting CMake option `BEMAN_INSIDE_BUILD_EXAMPLES` to
`OFF` when configuring the project.

Library options:

| Option | Default | Effect |
|--------|---------|--------|
| `BEMAN_INSIDE_MATH_FIXED` | `OFF` | Use the integer/CORDIC math engine (FPU-free) instead of the double engine |
| `BEMAN_INSIDE_MATH_FLOAT` | `OFF` | Make unqualified `beman::inside::math` use the float (binary32) engine |
| `BEMAN_INSIDE_STRICT_SFINAE` | `OFF` | Drop the assignment diagnostic overloads so `is_constructible` stays honest |
| `BEMAN_INSIDE_BUILD_TOOLS` | `OFF` | Build the benchmarks, property fuzzer, accuracy sweep and perf workload |

C++20/GCC-12 mode, math-engine selection, and bare-metal builds are covered in
[docs/freestanding.md](docs/freestanding.md) and [docs/math.md](docs/math.md).

### Supported Platforms

| Compiler   | Version | C++ Standards | Standard Library  |
|------------|---------|---------------|-------------------|
| GCC        | 16-13   | C++26-C++20   | libstdc++         |
| GCC        | 12      | C++20         | libstdc++         |
| Clang      | 22-19   | C++26-C++20   | libstdc++, libc++ |
| Clang      | 18      | C++26-C++20   | libc++            |
| Clang      | 18      | C++23-C++20   | libstdc++         |
| AppleClang | latest  | C++26-C++20   | libc++            |
| MSVC       | latest  | C++23         | MSVC STL          |

## Development

See the [Contributing Guidelines](CONTRIBUTING.md).

## Integrate beman.inside into your project

### Build

You can build inside using a CMake workflow preset:

```bash
cmake --workflow --preset gcc-release
```

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
