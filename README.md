# bound

[![CI](https://github.com/NiceAndPeter/bound/actions/workflows/ci.yml/badge.svg)](https://github.com/NiceAndPeter/bound/actions/workflows/ci.yml)

> **Status: alpha** — the public API may change between versions. Developed with
> [Claude Code](https://claude.com/claude-code).

A header-only C++23 library (C++20 fallback) for numbers that **cannot go out of
range** — the range and step size live in the *type*.

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

## Quick start

```cpp
#include "bound/bound.hpp"
using namespace bnd;

// A percentage: integer values in [0, 100].
using pct = bound<{0, 100}>;
pct x = 42;
pct y = 58;
auto sum = x + y;                          // bound<{0, 200}> — no overflow possible
auto z = x + 1_b;                          // scalars need a grid: 1_b, not 1

// Fractional grid: −1 .. 1 in 1/16 384 steps (Q1.14 audio sample).
using sample = bound<{{-1, 1}, notch<1, 16384>}, round_nearest>;
sample s = 0.5;                            // dyadic literal — exact
s.numerator();                             // 1   (denominator() == 2): exact read-out

// Clamped percentage: saturates instead of throwing.
using safe_pct = bound<{0, 100}, clamp>;
safe_pct p = 150;                          // p == 100
```

## New to bound? Start with the tutorial

**[docs/tutorial.md](docs/tutorial.md)** is a 10-minute tour of the mental
model — what a bound *is*, why arithmetic widens, and how a value flows through
a program. Read it first; everything else builds on it.

## Documentation

**Guides** — [policies & error handling](docs/policies.md) ·
[arithmetic & rounding](docs/arithmetic.md) ·
[conversions & casts](docs/conversions.md) ·
[storage & STL integration](docs/storage.md) ·
[`bnd::math` — bit-exact math](docs/math.md)

**Special topics** — [for fixed-point users](docs/fixed-point.md) ·
[determinism & reproducibility](docs/determinism.md) ·
[freestanding & bare-metal](docs/freestanding.md) ·
[reading compiler errors](docs/diagnostics.md) ·
[the single header](docs/single-header.md)

**Reference** — [internals & design](docs/internals.md) ·
[roadmap](docs/roadmap.md) · [prior art & talks](docs/resources.md) ·
[accuracy](docs/accuracy.md) / [performance](docs/performance.md) (generated
reports)

## Examples

[`examples/`](examples/) holds 30+ self-contained programs — e.g.
[`clock.cpp`](examples/clock.cpp) (wrap with carry),
[`money.cpp`](examples/money.cpp) (cents-exact currency),
[`pid_controller.cpp`](examples/pid_controller.cpp) (fixed-point control loop).
The normal build compiles them all; run `./build/example_clock` or
`ctest --test-dir build -L example`.

## Build & test

Requires CMake 3.24+ and a C++23 compiler (GCC 13+, Clang 16+, MSVC 19.36+).

```bash
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

C++20/GCC-12 mode, math-engine selection, and bare-metal builds are covered in
[docs/freestanding.md](docs/freestanding.md) and [docs/math.md](docs/math.md).

## Single header

The library also ships as one self-contained file,
[`single_include/bound/bound.hpp`](single_include/bound/bound.hpp) — ideal for
Compiler Explorer. See [docs/single-header.md](docs/single-header.md).
