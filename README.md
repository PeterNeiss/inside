# bound

[![CI](https://github.com/NiceAndPeter/bound/actions/workflows/ci.yml/badge.svg)](https://github.com/NiceAndPeter/bound/actions/workflows/ci.yml)

> **Status: alpha.** The library is under active development and the public API
> may change between versions. `bound` was developed with
> [Claude Code](https://claude.com/claude-code), Anthropic's agentic coding tool.

A header-only C++23 library providing safe arithmetic on bounded rational
number grids. A grid is defined by a lower and upper (inclusive) bound, hence
the name, and a notch (step size); all three are exact fractions, and
`(Upper − Lower) / Notch` must be an unsigned integer. You write those
fractions with literals, `notch<N, D>`, and `frac<N, D>` — the exact-fraction
representation itself is an internal detail you never name.

Arithmetic operators (`+`, `-`, `*`, `/`) are type-safe by construction: the
result type's grid is computed at compile time to contain every possible
value. Policy only governs what happens when a value is **assigned** or
**converted** into a narrower type.

## Quick start

```cpp
#include "bound/bound.hpp"
using namespace bnd;

// A percentage: integer values in [0, 100].
using pct = bound<{0, 100}>;
pct x = 42;
pct y = 58;
auto sum = x + y;                          // bound<{0, 200}> — no overflow possible

// Fractional grid: −1 .. 1 in 1/16 384 steps (Q1.14 audio sample).
using sample = bound<{{-1, 1}, notch<1, 16384>}, round_nearest>;
sample s = 0.5;                            // dyadic literal — exact
s.numerator();                             // 1   (denominator() == 2): exact read-out

// Clamped percentage: saturates instead of throwing.
using safe_pct = bound<{0, 100}, clamp>;
safe_pct p = 150;                          // p == 100
```

## Single header

The whole library is also available as one self-contained header at
[`single_include/bound/bound.hpp`](single_include/bound/bound.hpp). It inlines the
entire `bound/` + `slim/` tree, so it needs only the C++ standard library — there
are no `bound/...` or `slim/...` sub-includes left to resolve. Drop the one file
into a project, put `single_include/` on the include path, and use it exactly as
the full tree:

```cpp
#include "bound/bound.hpp"   // single_include/ on the include path — nothing else needed
```

It is behaviourally identical to the multi-header form; the `-DBOUND_MATH_FIXED`
engine switch and the C++20 mode apply the same way, as ordinary compiler flags.

**On Compiler Explorer**, where there is no include tree to set up, the single
header is the easy way in:

- paste it into a second source pane named `bound/bound.hpp` and `#include` it; or
- once the repo is on GitHub, pull it in with a single raw-URL include (Compiler
  Explorer resolves URL includes only for single-header libraries — which is
  exactly what this is):

  ```cpp
  #include <https://raw.githubusercontent.com/NiceAndPeter/bound/main/single_include/bound/bound.hpp>
  ```

The header is generated, not hand-edited — regenerate it after changing anything
under `include/` with `cmake --build build --target amalgamate` (see
[Build & Test](#build--test)).

## Feature highlights

- **Policy-driven assignment** — `clamp`, `wrap`, `sentinel`, `round_nearest`,
  `snap`, plus `on_clamp` / `on_wrap` / `on_overflow` callbacks and
  a `bnd::errc` mode for throw-free error reporting. Representation
  flags (`real`, `exact`, `direct`, `indexed`) select how the raw value is
  stored. See [docs/policies.md](docs/policies.md).
- **Type-safe widening arithmetic** — `+ - * /` widen the result grid at
  compile time; integer fast paths keep the common case at native speed.
  Scalars need a grid — write `a + 1_b`, not `a + 1` (a raw `int`/`double`
  has no grid). Bound-space `dot` / `cross` / `lerp` keep 2-D geometry inside
  the bounded world. See [docs/arithmetic.md](docs/arithmetic.md).
- **Conversions and casts** — typed-error `to<T>()` / direct `as<T>()`
  (member and free forms), exact `numerator()` / `denominator()` read-out,
  conversion predicates (`will_conversion_overflow`, `is_conversion_lossy`),
  implicit `operator imax()` so a bound indexes arrays directly, and the
  `clamp_cast` / `wrap_cast` / `clamp_round` family. See
  [docs/conversions.md](docs/conversions.md).
- **Optimal storage & iteration** — automatic raw-type selection (uint/int
  sizes, or an exact-fraction representation for non-dyadic grids),
  `slim::optional` with a sentinel
  encoding (no size overhead), `bound_range` for compile-time iteration,
  and STL/ranges integration. Plus predefined hardware-width aliases
  (`bnd::byte`, `bnd::unorm16`, `bnd::q8_8`, …) in `bound/formats.hpp`.
  See [docs/storage.md](docs/storage.md).
- **Reproducible math, three engines** — a `<cmath>`-shaped function set over
  bounds (`sin`/`cos`/`tan`, `asin`/`acos`/`atan`/`atan2`, `sinh`/`cosh`/`tanh`,
  `exp`/`log`/`log2`/`log10`/`pow`, `sqrt`/`cbrt`/`hypot`). One API; three engines
  callable side-by-side by namespace (`dbl::` binary64, `flt::` binary32,
  `cordic::` integer/FPU-free `constexpr`), each bit-identical across platforms.
  The unqualified `bnd::math::fn` picks the build default (`-DBOUND_MATH_FIXED=ON`
  → cordic, `-DBOUND_MATH_FLOAT=ON` → float, else double); `-DBND_MATH_NO_FP`
  drops `<cmath>` for bare metal. Operands need only the `snap` bit (`f64`/`f32`
  storage is an optional fast path); angles are radians; output grids auto-deduce.
  See [docs/math.md](docs/math.md).
- **Library internals** — grid invariants, storage decision tree, Q-format
  fast path, policy cascade. See [docs/internals.md](docs/internals.md).

## Documentation

- [Tutorial — the mental model](docs/tutorial.md)
- [Policies, callbacks & error handling](docs/policies.md)
- [Arithmetic, rounding & compound assignment](docs/arithmetic.md)
- [Conversions, casts & predicates](docs/conversions.md)
- [Storage, iteration & STL integration](docs/storage.md)
- [`bnd::math` — constexpr, bit-exact math](docs/math.md)
- [Determinism & reproducibility](docs/determinism.md)
- [Bound for fixed-point users](docs/fixed-point.md)
- [Freestanding & bare-metal](docs/freestanding.md)
- [Reading a `bound<>` in a compiler error](docs/diagnostics.md)
- [Internals (architecture / design notes)](docs/internals.md)
- [Roadmap — features gated on future C++ standards](docs/roadmap.md)
- [Resources — prior art & talks](docs/resources.md)

## Examples

The `examples/` directory contains ~30 self-contained programs. A curated
selection:

| Example | Feature |
|---------|---------|
| [`percentage.cpp`](examples/percentage.cpp)         | Clamped percentage with `+=` and `with_clamp()` |
| [`clock.cpp`](examples/clock.cpp)                   | Cascading wrap with carry (seconds → minutes → hours) |
| [`audio_sample.cpp`](examples/audio_sample.cpp)     | Signed Q1.14 audio samples with mixing and clamp |
| [`money.cpp`](examples/money.cpp)                   | Cents-precision currency arithmetic via fractional notch |
| [`pid_controller.cpp`](examples/pid_controller.cpp) | Fixed-point PID loop with `add_all` and `clamp \| round_nearest` actuator |
| [`audio_mixer.cpp`](examples/audio_mixer.cpp)       | 4-channel Q1.14 mix with `with(on_clamp, on_overflow)` peak metering |
| [`sensor_fusion.cpp`](examples/sensor_fusion.cpp)   | Weighted average across sensors with disparate fixed-point ranges |
| [`torus_map.cpp`](examples/torus_map.cpp)           | 2-D sub-pixel position with `wrap` on both axes and edge-crossing events |
| [`algorithms.cpp`](examples/algorithms.cpp)         | STL and ranges algorithms (sort, find, transform, accumulate, …) |
| [`formats.cpp`](examples/formats.cpp)               | Predefined hardware-width types (`byte` / `sword` / `unorm16` / `q8_8`) and interop |
| [`storage_flags.cpp`](examples/storage_flags.cpp)   | Pin the raw type with the `u8`…`u64` width flags (value vs `indexed`); compile-time fit check |

Build and run any example:

```bash
cmake -B build && cmake --build build
./build/example_clock
./build/example_signed
```

## Performance

Full per-operation tables (nanobench: ns/op, hardware instruction/cycle/branch
counters, and a native-baseline `relative` column per group) live in
[docs/performance.md](docs/performance.md), regenerated by the `perf_report`
build target. Headlines from the current run:

- **Unchecked integer and Q-format arithmetic sits at or near native parity**;
  `transform(b += 1_b)` over uint8-width elements vectorizes at native lane
  count, and `++`/`--` compile to a bare integer add.
- `checked` accumulation pays a per-element range check (scalar, ~4× a
  vectorized native loop); `bnd::sum<checked>` defers one bulk check and
  vectorizes.
- The `math::*` double engine is within a small factor of this libm's
  `std::` calls (own polynomials + a grid store); the CORDIC engine trades
  more instructions for FPU-free bit-exactness — see
  [docs/accuracy.md](docs/accuracy.md) for the error side of that trade.
- Known slow paths (by design, documented in the tables): cross-grid stores
  onto incompatible lattices and mixed direct/index-grid arithmetic take the
  exact rational path.

Absolute nanoseconds in the tables are host-specific; the per-group ratios
are the stable signal. (Mind the sentinel slot: `bound<{0,255}>` stores in
uint16 — see [docs/storage.md](docs/storage.md#choosing-the-representation).)

## Build & Test

Requires CMake 3.24+ and a C++23 compiler (GCC 13+, Clang 16+, MSVC 19.36+) for
the full feature set.

The library also builds against **C++20 on GCC 12**: configure with
`-DBOUND_CXX20=ON`. In that mode the error channel uses the bundled
`slim::expected` backport instead of `<expected>`, and the `std::format`
integration is feature-gated off (`to_string()` / `operator<<` remain available)
— everything else is identical.

```bash
cmake -B build
cmake --build build

# C++20 / GCC 12 build:
cmake -B build20 -DBOUND_CXX20=ON -DCMAKE_CXX_COMPILER=g++-12
cmake --build build20

# Integer/CORDIC math engine (constexpr, FPU-free, unconditionally
# bit-identical — see docs/math.md):
cmake -B build-fixed -DBOUND_CXX20=ON -DBOUND_MATH_FIXED=ON
cmake --build build-fixed

ctest --test-dir build --output-on-failure   # runs unit + algo suites
./build/bound_tests                          # unit tests directly
./build/bench                                # performance benchmarks (native vs bound)
./build/example_algorithms                   # one of the example binaries
```

### Regenerating the single header

The committed [single header](#single-header) is generated from `include/` by a
pure-CMake amalgamator (`cmake/amalgamate.cmake` — no Python or other tooling).
After editing any header under `include/`, regenerate and commit it:

```bash
cmake --build build --target amalgamate            # rewrites single_include/bound/bound.hpp
ctest --test-dir build -L tooling                  # amalgamate_up_to_date: fails if it drifted
cmake --build build --target single_header_smoke   # compiles a TU seeing ONLY single_include/
```

`ctest` runs `amalgamate_up_to_date` as part of the normal suite, so a stale
single header fails the build until it is regenerated.
