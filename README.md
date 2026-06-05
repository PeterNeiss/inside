# bound

A header-only C++23 library providing safe arithmetic on bounded rational
number grids. A grid is defined by a lower and upper (inclusive) rational
bound, hence the name, and a notch (step size); all three are `rational`,
and `(Upper − Lower) / Notch` must be an unsigned integer.

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
sample s = 0.5_r;                          // _r literal — exact rational

// Clamped percentage: saturates instead of throwing.
using safe_pct = bound<{0, 100}, clamp>;
safe_pct p = 150;                          // p == 100
```

## Feature highlights

- **Policy-driven assignment** — `clamp`, `wrap`, `sentinel`, `round_nearest`,
  `ignore_round`, plus `on_clamp` / `on_wrap` / `on_overflow` callbacks and
  an `std::error_code` mode for throw-free error reporting. See
  [docs/policies.md](docs/policies.md).
- **Type-safe widening arithmetic** — `+ - * /` widen the result grid at
  compile time; integer fast paths keep the common case at native speed.
  Scalars need a grid — write `a + 1_b`, not `a + 1` (a raw `int`/`double`
  has no grid); exact `bound op rational` is also supported. See
  [docs/arithmetic.md](docs/arithmetic.md).
- **Conversions and casts** — typed-error `to<T>()`, conversion predicates
  (`will_conversion_overflow`, `is_conversion_lossy`), implicit
  `operator std::size_t()` for index-shaped bounds, and the
  `clamp_cast` / `wrap_cast` / `clamp_round` family. See
  [docs/conversions.md](docs/conversions.md).
- **Optimal storage & iteration** — automatic raw-type selection (uint/int
  sizes, or `rational` for exact grids), `slim::optional` with a sentinel
  encoding (no size overhead), `bound_range` for compile-time iteration,
  and STL/ranges integration. See [docs/storage.md](docs/storage.md).
- **Library internals** — grid invariants, storage decision tree, Q-format
  fast path, policy cascade. See [docs/internals.md](docs/internals.md).

## Documentation

- [Policies, callbacks & error handling](docs/policies.md)
- [Arithmetic, rounding & compound assignment](docs/arithmetic.md)
- [Conversions, casts & predicates](docs/conversions.md)
- [Storage, iteration & STL integration](docs/storage.md)
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
| `bound<{{-40,60},0.5}> = double` (rational path) | 87–94 ns | n/a | n/a |

Notes:

- Integer-raw bounds (the common case: `bound<{0,N}>`, `bound<{a,b}>` with
  notch 1) are at native parity for arithmetic and assignment.
- Fixed-point grids with integer Lower and unit-numerator Notch take an
  integer fast path in `assignment::store` and `from_value` — no rational
  construction in the hot loop.
- `checked` policy on accumulation pays a 4× penalty: the per-element domain
  check breaks autovectorisation. Use `unsafe` for tight inner loops where
  no-overflow is proven upfront, then convert back to a `checked` bound
  after the loop.
- Assigning a `double` to a fractional-notch grid is the slowest path
  because the value crosses the API boundary into rational arithmetic — by
  design; the library uses rational + integer math internally to preserve
  exactness.

## Build & Test

Requires CMake 3.24+ and a C++23 compiler (GCC 12+, Clang 16+, MSVC 19.36+).

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure   # runs unit + algo suites
./build/bound_tests                          # unit tests directly
./build/bench                                # performance benchmarks (native vs bound)
./build/example_algorithms                   # one of the example binaries
```
