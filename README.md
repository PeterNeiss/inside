# bound

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

## Feature highlights

- **Policy-driven assignment** — `clamp`, `wrap`, `sentinel`, `round_nearest`,
  `ignore_round`, plus `on_clamp` / `on_wrap` / `on_overflow` callbacks and
  an `std::error_code` mode for throw-free error reporting. Representation
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
  (`bnd::u8`, `bnd::unorm16`, `bnd::q8_8`, …) in `bound/formats.hpp`.
  See [docs/storage.md](docs/storage.md).
- **Reproducible math, two engines** — a `<cmath>`-shaped function set over
  bounds (`sin`/`cos`/`tan`, `asin`/`acos`/`atan`/`atan2`, `sinh`/`cosh`/`tanh`,
  `exp`/`log`/`log2`/`log10`/`pow`, `sqrt`/`cbrt`/`hypot`). One API, two
  build-time engines: a fast `double` engine (default, bit-identical across
  IEEE-754 platforms) and an integer/CORDIC engine
  (`-DBOUND_MATH_FIXED=ON` — `constexpr`, FPU-free, bit-identical
  unconditionally). Math operands carry the `real` policy; angles are
  radians; output grids auto-deduce. See [docs/math.md](docs/math.md).
- **Library internals** — grid invariants, storage decision tree, Q-format
  fast path, policy cascade. See [docs/internals.md](docs/internals.md).

## Documentation

- [Tutorial — the mental model](docs/tutorial.md)
- [Policies, callbacks & error handling](docs/policies.md)
- [Arithmetic, rounding & compound assignment](docs/arithmetic.md)
- [Conversions, casts & predicates](docs/conversions.md)
- [Storage, iteration & STL integration](docs/storage.md)
- [`bnd::math` — constexpr, bit-exact math](docs/math.md)
- [Internals (architecture / design notes)](docs/internals.md)

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
| [`formats.cpp`](examples/formats.cpp)               | Predefined hardware-width types (`u8` / `i16` / `unorm16` / `q8_8`) and interop |

Build and run any example:

```bash
cmake -B build && cmake --build build
./build/example_clock
./build/example_signed
```

## Performance

Measured at `-O3 -DNDEBUG` on x86-64 via `tests/bench.cpp` (5M iterations per
scenario, native baseline paired with each bound case). Lower is better.

| Workload | bound | native | ratio |
|---|---|---|---|
| `bound<{0,200}> ±/×/÷` (integer raw, unsafe) | 13 ns | 13 ns | **1.0×** |
| `bound<{{0,255},1/256}>` construct (Q8.8) | 13 ns | 14 ns | **0.97×** |
| `bound<{{0,65535},1/65536}>` construct (Q16.16) | 14 ns | 14 ns | **0.97×** |
| `accumulate(bound, unsafe)` 1000 elts | 64 ns | 64 ns | 1.0× (vectorized) |
| `accumulate(bound, checked)` 1000 elts | 274 ns | 64 ns | 4.3× (scalar) |
| `bnd::sum<checked>` 1000 elts (one deferred check) | 105 ns | 64 ns | **1.6×** (vectorized) |
| `transform(b += 1)` 10k uint8-width elts (unsafe) | 1.02 µs | 1.02 µs | **1.0×** (SIMD) |
| Q-format store from exact fraction | ~6 ns | n/a | n/a |
| Q-format store from `double` | ~46 ns | n/a | n/a |
| `math::sin` (`real` operand, double engine) | 35 ns | 23 ns (`std::sin`) | 1.5× |
| `math::fmod` (integer grids) | 19 ns | 25 ns (`std::fmod`) | **0.76×** |

Notes:

- Integer-raw bounds (the common case: `bound<{0,N}>`, `bound<{a,b}>` with
  notch 1) are at native parity for arithmetic and assignment. Unchecked
  compound ops run at the raw type's width, so byte-wide loops vectorize at
  native lane count. (Mind the sentinel slot: `bound<{0,255}>` stores in
  uint16 — see [docs/storage.md](docs/storage.md#choosing-the-representation).)
- Q-format grids (integer Lower, unit-numerator Notch) take integer fast
  paths in `assignment::store` and `from_value` — storing an exact fraction
  is one `gcd` plus three integer multiplies; storing a `double` adds only
  its exact decomposition.
- `checked` policy on accumulation pays a 4× penalty: the per-element domain
  check breaks autovectorisation. Use `unsafe` for tight inner loops where
  no-overflow is proven upfront, then convert back to a `checked` bound
  after the loop.
- The `math::*` rows measure the call alone (the bench constructs inputs
  outside the timed blocks); `real`-policy operands are double-backed, so
  input marshalling is free and the gap to `std::` is the output grid-snap.
  `math::fmod` on commensurable integer grids beats `std::fmod` — it is a
  single integer remainder.

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
