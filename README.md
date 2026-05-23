# bound

## Description

A header-only C++23 library providing safe arithmetic on bounded rational number grids.

A grid is defined by a lower and upper (inclusive) rational bound, hence the name, and a notch (step size). All three are rational numbers, and `(upper - lower) / notch` must be an unsigned integer.

Arithmetic operators (`+`, `-`, `*`, `/`) are type-safe by construction: the result type's grid is computed at compile time to contain all possible values. Policy only governs what happens when a value is assigned or converted into a narrower type.

## Quick Start

```cpp
#include "bound/bound.hpp"
using namespace bnd;

// A percentage: integer values in [0, 100]
using pct = bound<{0, 100}>;
pct x = 42;
pct y = 58;
auto sum = x + y;  // bound<{0, 200}> — type-safe, no overflow possible

// Fractional grid: values from 0.75 to 10.5 in steps of 0.25
using frac = bound<{{0.75, 10.5}, 0.25}>;
frac f = 3.25;
```

## Policies

The second template parameter controls what happens on out-of-range assignment:

```cpp
// Default: checked — runtime domain validation (throws std::system_error)
using safe = bound<{0, 100}>;
safe x = 150;      // throws std::system_error at runtime

// Unsafe: opt out of all runtime checks (compile-time checks always apply)
using fast = bound<{0, 100}, unsafe>;
fast f = 150;      // silently stores out-of-range value; UB on read

// Clamp: saturates to the nearest boundary
using clamped = bound<{0, 100}, clamp>;
clamped x = 150;   // x == 100
clamped y = -5;    // y == 0

// Wrap: modular arithmetic
using angle = bound<{0, 359}, wrap>;
angle a = 370;     // a == 10
angle b = -10;     // b == 350

// Sentinel: out-of-range produces nullopt (via slim::optional)
using index = bound<{0, 9}, sentinel>;
slim::optional<index> i = 10;  // i == nullopt
```

By default, `bound` enables runtime domain checks (`checked`) and throws on
violations. Use `unsafe` to drop all runtime checks for maximum performance
when you've proven correctness elsewhere. `clamp`, `wrap`, and `sentinel` are
mutually exclusive and enforced by `static_assert`. Flags compose with
bitwise `|` (e.g. `bound<{0, 100}, checked | round_nearest>`).

### Policy flags

| Flag | Effect |
|---|---|
| `checked` | runtime domain / round / overflow checks (**default**) |
| `unsafe` | opt out of all runtime checks |
| `clamp` | saturate to boundary on out-of-range (mutually exclusive with `wrap`/`sentinel`) |
| `wrap` | modular arithmetic on out-of-range |
| `sentinel` | out-of-range yields `nullopt` via `slim::optional` |
| `ignore_round` | rounding mismatches truncate toward zero (no error) |
| `round_nearest` | round to nearest notch, half away from zero (implies `ignore_round`) |
| `round_floor` | round toward -inf (implies `ignore_round`) |
| `round_ceil` | round toward +inf (implies `ignore_round`) |
| `round_half_even` | banker's rounding — half to even (implies `ignore_round`) |
| `ignore_zero` | division by zero is silent |
| `ignore_domain` | suppress the runtime domain check |

### Per-operation override

Even without a type-level policy, you can clamp, wrap, or pick a rounding
mode on a per-operation basis:

```cpp
bound<{0, 100}> x(50);
x.with_clamp() = 150;  // x == 100
x.with_wrap()  = 103;  // x == 2

bound<{{0, 10}, 2}> g{0};
g.with_floor()           = 3.0;  // g == 2
g.with_ceil()            = 3.0;  // g == 4
g.with_round_half_even() = 5.0;  // g == 4 (tie → even)
```

### Callbacks: `on_wrap` / `on_clamp` / `on_overflow` / `on_sentinel` / `on_error`

Each policy event can fire a zero-overhead callback. Unused handlers are
eliminated entirely by the compiler (`if constexpr` + `[[no_unique_address]]`).
Each handler receives the bound by mutable reference (so it can override the
stored value) plus an event-specific payload:

| Method | Fires on | Callback signature |
|---|---|---|
| `on_clamp(λ)`    | clamp narrowing             | `λ(bound&, overshoot)` |
| `on_wrap(λ)`     | wrap with carry             | `λ(bound&, carry)` |
| `on_sentinel(λ)` | sentinel write              | `λ(bound&, original_value)` |
| `on_error(λ)`    | domain / round error        | `λ(bound&, errc, std::string_view msg)` |
| `on_overflow(λ)` | rational/imax arithmetic OF | `λ(bound&, errc::overflow)` |

Each `on_*()` returns a temporary handle whose `=`/`+=`/`-=`/etc. apply the
operation with the callback wired in. Calling `on_*` automatically OR-merges
the policy bit it implies (e.g. `on_clamp` adds `clamp`).

```cpp
using sec = bound<{0, 59}, wrap>;
using min = bound<{0, 59}>;

sec seconds(0);
min minutes(0);

// When seconds overflows, carry into minutes
seconds.on_wrap([&](auto& self, auto carry) {
    (void)self;
    minutes += carry;
}) = 125;
// seconds == 5, minutes == 2
```

The free arithmetic functions accept the same factories — useful for catching
divide-by-zero or rational overflow without throwing:

```cpp
auto q = div(d, z, on_overflow([&](auto& res, errc c) {
    // res is the result type; assign a fallback
    res = std::remove_cvref_t<decltype(res)>{0};
    log(c);
}));
```

#### Combining actions: `with(...)`

`bound::with(actions...)` packs multiple `on_*` callbacks into a single
operation. Mutually exclusive combinations (e.g. `on_clamp` + `on_wrap`) are
rejected at compile time by `static_assert`.

```cpp
using c100 = bound<{0, 100}>;
c100 acc{50};

acc.with(
    on_overflow([&](auto& self, errc) { self = 0; /* imax saturated */ }),
    on_clamp   ([&](auto&, auto over)  { log_overshoot(over);          })
) += big_value;
```

#### Legacy callable form

`b.policy<wrap>(λ)` still accepts a single bare callable receiving just the
carry/overshoot — kept for backward compatibility. New code should prefer the
typed `on_*()` variants, which give the handler access to the bound itself.

### Error code mode

Instead of throwing, errors can be reported via `std::error_code`. This works with construction, direct assignment, and per-operation policies:

```cpp
std::error_code ec;

// Construction with error code
bound<{0, 100}> x(150, ec);
// ec is set to EDOM, x is unchanged (default-constructed)

// Per-operation with error code
bound<{0, 100}> y(50);
y.policy(ec) = 200;
// ec is set to EDOM, y remains 50

// Free arithmetic with error code
auto sum = add(x, y, ec);
// overflow / range errors captured in ec

// Combining flags with error code
bound<{{0, 10}, 2}> coarse(0);
bound<{{0, 10}, 1}> fine(3);
coarse.policy<ignore_round>(ec) = fine;
// ignore_round suppresses the rounding error, ec captures domain errors only
```

The error code is only set on the first error (subsequent errors don't overwrite it). Domain violations produce `EDOM`, rounding violations produce `ENOSPC`.

### Optional construction

`try_make` returns `slim::optional<bound>` instead of throwing:

```cpp
auto maybe = bound<{0, 100}>::try_make(150);
if (!maybe) { /* out of range */ }
```

For types with a `clamp` or `wrap` policy, `try_make` applies the policy before checking, so it will always succeed:

```cpp
auto clamped = bound<{0, 100}, clamp>::try_make(150);
// clamped has value 100 — clamp always succeeds
```

### Free-function casts

Seven named casts complement the constructors — the intent (clamp, wrap,
throw, trust, or compose with a rounding mode) is visible at the call site
(e.g. inside `std::transform` callbacks).

| Cast | Behaviour |
|---|---|
| `saturated_cast<B>(v)`  | clamp to `[Lower, Upper]`, never throw |
| `wrap_cast<B>(v)`       | modular reduction into the target interval |
| `checked_cast<B>(v)`    | throw `std::system_error` on overflow or off-notch |
| `unchecked_cast<B>(v)`  | trust the caller — UB if out of range |
| `clamp_floor<B>(v)`     | clamp + round toward −∞ |
| `clamp_ceil<B>(v)`      | clamp + round toward +∞ |
| `clamp_round<B>(v)`     | clamp + round to nearest |

```cpp
using pct = bound<{0, 100}>;

saturated_cast<pct>(150);   // 100  (clamps regardless of B's declared policy)
wrap_cast    <pct>(105);    // 4    (modular reduction into [0, 100])
checked_cast <pct>(42);     // 42   (throws on out-of-range or off-notch)
unchecked_cast<pct>(42);    // 42   (skips runtime checks — caller's contract)
```

For the `double → bounded integer` pipeline (audio/graphics/DSP), the
`clamp_*` family composes clamping with a rounding mode:

```cpp
using coarse = bound<{{0, 10}, 2}>;
clamp_floor<coarse>(3.0);   // 2  (clamp + floor)
clamp_ceil <coarse>(3.0);   // 4  (clamp + ceil)
clamp_round<coarse>(3.0);   // 4  (clamp + round to nearest)
clamp_floor<coarse>(15.0);  // 10 (out-of-range clamps to upper)
```

### Conversion predicates

Inspect a value *before* attempting an unsafe construction:

```cpp
will_conversion_overflow<pct>(150);    // true  — out of [0, 100]
will_conversion_truncate<pct>(3.5);    // true  — doesn't land on notch 1
is_conversion_lossy<pct>(150);         // true  — overflow OR truncation
```

All three are pure inspection — none performs the conversion or has side
effects.

## Arithmetic

Binary operations compute the result grid at compile time:

```cpp
using u8 = bound<{0, 255}>;
u8 a = 100, b = 200;

auto sum  = a + b;   // bound<{0, 510}>
auto diff = a - b;   // bound<{-255, 255}>
auto prod = a * b;   // bound<{0, 65025}>
auto quot = a / b;   // slim::optional<bound<{rational}>>
```

Division always returns `slim::optional` (division by zero yields `nullopt`).

By default, division produces exact rational results. With the `ignore_round` policy, division uses native integer division (truncation towards zero) for zero overhead:

```cpp
using val = bound<{0, 100}>;
val a = 7, b = 3;

// Exact: result is rational (7/3)
auto exact = a / b;

// Per-call integer division: result is integer (2)
auto quotient = div(a, b, truncated);

// Type-level: operator/ uses native division automatically
using fast = bound<{0, 100}, ignore_round>;
auto q = fast(7) / fast(3);  // 2
```

The free functions `add`, `sub`, `mul`, `div`, and `mod` each accept a named
convenience policy (`bnd::truncated`, `bnd::round_to_nearest`, `bnd::clamped`,
`bnd::wrapped`), one or more `on_*` action factories, or an `std::error_code&`
for direct error-code reporting — in any order. See the [Callbacks section](#callbacks-on_wrap--on_clamp--on_overflow--on_sentinel--on_error)
for the action API; for example, recovering from divide-by-zero:

```cpp
auto q = div(x, y, on_overflow([&](auto& res, errc) {
    res = std::remove_cvref_t<decltype(res)>{0};
}));
```

### Rounding

Assigning between bounds with incompatible notches is a compile-time error:

```cpp
using coarse = bound<{{0, 10}, 2}>;   // notch 2: values 0, 2, 4, 6, 8, 10
using fine   = bound<{{0, 10}, 1}>;   // notch 1: values 0, 1, 2, ..., 10

coarse c(0);
fine f(3);
c = f;  // ERROR: incompatible notches (3 would round to 2)
```

Notches are "compatible" when the target notch is an integer multiple of the source notch. For example, notch 2 to notch 1 is always exact (every even number is an integer), but notch 1 to notch 2 may round.

To opt in to rounding:

```cpp
// Per-operation (truncates towards zero)
c.with_round() = f;

// Round to nearest notch
c.with_round_nearest() = f;

// Per-call with explicit policy
c.policy<ignore_round>() = f;
c.policy<round_nearest>() = f;  // nearest notch

// Type-level: all assignments allow rounding
using coarse_r = bound<{{0, 10}, 2}, ignore_round>;
coarse_r cr = f;  // OK, truncates to nearest notch
```

### Modulo

The `%` operator requires integer-valued grids and the `ignore_round` policy:

```cpp
using val = bound<{0, 100}, ignore_round>;
val a = 17, b = 5;
auto r = a % b;  // slim::optional<bound<{0, 99}>>, value 2
```

Like division, modulo returns `slim::optional` (division by zero yields `nullopt`).

### Compound Assignment and Increment

Compound assignment operators work with arithmetic scalars:

```cpp
using pct = bound<{0, 100}, clamp>;
pct x = 50;
x += 30;   // x == 80
x -= 10;   // x == 70
x *= 2;    // x == 100 (clamped)
x /= 3;    // x == 33
x %= 10;   // x == 3

++x;       // x == 4
x--;       // x == 3
```

`operator+=` and `-=` also accept other bound types when grids are compatible.

### When `slim::optional` is returned

Operations return `slim::optional<bound>` in two cases:

1. **Division** — the divisor could be zero, so the result is always optional.

2. **Rational raw storage** — when the result grid cannot be represented with an integer raw type (see Storage below), the result uses `rational` as its raw storage. Addition and multiplication on such types return `slim::optional` because rational arithmetic can overflow (denominator overflow).

All `optional`-returning operators propagate nullopt: if either operand is nullopt, the result is nullopt.

```cpp
using u8 = bound<{1, 255}>;
slim::optional<u8> a{u8(100)};
slim::optional<u8> none{slim::nullopt};

auto r1 = a + u8(10);     // optional<bound<{2, 510}>>, has value 110
auto r2 = none + u8(10);  // optional<bound<{2, 510}>>, nullopt
```

## Storage

Each `bound` stores a single `Raw` member. The storage type is selected automatically:

**Unsigned integer storage**: when `lower >= 0` and the notch is nonzero, `Raw` is the smallest `uint8_t`..`uint64_t` that can hold `(upper - lower) / notch`. The value is recovered as `Raw * notch + lower` (offset encoding).

```cpp
using pct = bound<{0, 100}>;           // Raw: uint8_t  (101 values)
using big = bound<{0, 100000}>;        // Raw: uint32_t (100001 values)
using step = bound<{{0, 5}, 0.5}>;     // Raw: uint8_t  (10 steps)
```

When `lower == 0` and `notch == 1`, `Raw` equals the value directly — no offset arithmetic.

**Signed integer storage**: when `lower < 0` and `notch == 1`, `Raw` is the smallest `int8_t`..`int64_t` that fits the interval. `Raw` stores the value directly (`Raw == value`) with no offset arithmetic, matching native `int` performance exactly.

```cpp
using temp = bound<{-40, 85}>;         // Raw: int8_t   (direct storage)
using pos  = bound<{-100000, 100000}>; // Raw: int32_t  (direct storage)
using diff = bound<{-255, 255}>;       // Raw: int16_t  (direct storage)
```

Grids with `lower < 0` and a fractional notch still use unsigned offset encoding:

```cpp
using fstep = bound<{{-5, 5}, 0.5}>;   // Raw: uint8_t  (20 steps, offset encoding)
```

**Rational storage**: when the notch is zero, `Raw` becomes `rational`, an exact fraction type. This happens for grids with notch 0, division results, and single-value grids.

```cpp
using frac = bound<{{-10, 10}, 0}>;    // Raw: rational
frac f = *(2_r/3);                     // exact 2/3

using u8 = bound<{1, 255}>;
auto q = u8(7) / u8(3);               // slim::optional<bound<{rational}>>
                                       // value is exactly 7/3
```

Rational storage is exact (no floating-point rounding) but larger and slower than integer storage. The library automatically selects the most efficient representation for each grid.

### `slim::optional` sentinel

`slim::optional<bound>` uses a sentinel value instead of a bool flag, so `sizeof(slim::optional<bound>) == sizeof(bound)`. The sentinel is `numeric_limits<raw>::max()` for unsigned types and `numeric_limits<raw>::min()` for signed types. This costs one value from the representable range (e.g., `int8_t` gives 255 usable values: -127..127).

### Raw storage access

`bound::Raw` is a public data member, but most code should not touch it directly. The supported access patterns are:

- **`b.is_sentinel()`** — public probe for "does this bound currently hold the sentinel value?". The canonical answer to "is this slot empty?" under `sentinel` policy. Returns `true` iff `Raw` matches `sentinel_raw<B>()`.
- **`bnd::sentinel_raw<B>()`** — returns the raw byte pattern reserved for the sentinel. Useful for interop with C APIs or for constructing a `bound` in sentinel state manually (`b.Raw = sentinel_raw<B>()`).
- **`slim::optional<B>::from_maybe_sentinel(b)`** — non-throwing factory: returns `nullopt` if `b` is the sentinel, otherwise wraps `b`. The plain `slim::optional<B>{b}` ctor still throws on sentinel input — by design, since constructing an "engaged" optional from a sentinel value is usually a bug.
- **Direct `b.Raw = ...` writes** are only well-defined under `unsafe` policy (which opts out of all runtime checks). Outside `unsafe`, the library assumes `Raw` always encodes either a valid grid value or the sentinel.

## Comparing and Extracting Values

`bound` compares directly with arithmetic types and other bounds — no `static_cast` needed:

```cpp
bound<{0, 100}> b{42};
if (b == 42) { ... }      // works
if (b < 50)  { ... }      // works
REQUIRE(a + b == 100);    // works in tests too
```

To extract an integer value, use the implicit conversion (available when the grid is integer-aligned, i.e. notch is `±1`):

```cpp
imax v = b;               // implicit
int  arr_index = b;       // for array subscripts
sum += b;                 // accumulators
```

Or call `b.value()`, which always works and returns the most natural type for the storage (raw integer for direct storage, `rational` for fractional grids).

For `rational`, prefer the named reductions over `static_cast<int>(r)`:

```cpp
rational r{7u, 2};        // 3.5
r.trunc();                //  3   (toward zero)
r.floor();                //  3   (toward -inf)
r.round();                //  4   (half away from zero)
```

`bound`'s conversion to `double` is `explicit`; use `double(b)` for floating-point arithmetic.

`rational` also exposes a few standalone helpers:

```cpp
rational::inv(rational{3u, 4});         // slim::optional<rational> -> 4/3
rational::inv(rational{0u});            // nullopt (zero numerator)

divides_evenly(rational{6u}, rational{2u});   // true
divides_evenly(rational{7u}, rational{2u});   // false (7/2 is non-integer)
```

`inv` returns `nullopt` when the result would put the original numerator into
the denominator slot but it does not fit in `imax`. `divides_evenly` treats
division by zero as `true` (convention: everything divides 0 evenly).

## Iteration

`bound_range` provides range-based for loop support:

```cpp
// Iterate over all values in the grid [0, 9]
for (auto i : bound_range<{0, 9}>{})
  std::cout << i;  // 0 1 2 3 4 5 6 7 8 9

// Wrapping iteration starting at 5 (visits all values once)
for (auto i : bound_range<{0, 9}>{5})
  std::cout << i;  // 5 6 7 8 9 0 1 2 3 4
```

## Compile-time Constants

`just<value>` creates a single-value bound:

```cpp
constexpr auto one = just<1>;   // bound<{1, 1}>
constexpr auto pi  = just<3>;
```

The `_b` literal is shorthand for `just<N>`:

```cpp
auto five = 5_b;                // bound<{5, 5}>
auto x    = 10_b + my_bound;    // grid widens via just<N> + bound
```

## Variadic folds

`add_all` and `mul_all` are variadic equivalents of `+` and `*` over bounds:

```cpp
using v = bound<{0, 100}>;
v a{10}, b{20}, c{30};
auto sum  = add_all(a, b, c);   // bound<{0, 300}>, value 60
auto prod = mul_all(a, b);      // bound<{0, 10000}>, value 200
```

## `std` integration

`bound` specialises `std::hash` (works in `unordered_set`/`unordered_map`)
and `std::numeric_limits` (so `numeric_limits<bound<{0,100}>>::max()` returns
the upper bound, `is_signed`, `is_integer`, `is_bounded` etc. all report
correctly). Include `bound/numeric_limits.hpp` to pull both in.

```cpp
#include "bound/numeric_limits.hpp"

std::unordered_set<bound<{0, 9}>> s;
s.insert(bound<{0, 9}>{3});

static_assert(std::numeric_limits<bound<{-40, 60}>>::is_signed);
static_assert(std::numeric_limits<bound<{0,  100}>>::max() == 100);
```

## STL Algorithms

`bound` types work with standard algorithms out of the box — both `std::ranges` and classic iterator-based versions:

```cpp
#include <algorithm>
#include <numeric>
#include <vector>
#include "bound/bound.hpp"
using namespace bnd;

using celsius = bound<{{-40, 60}, 0.5}, round_nearest>;
using score  = bound<{0, 1000}>;

std::vector<celsius> temps = {21.5, -5.0, 37.0, 0.0, 15.5};

// Ranges algorithms
std::ranges::sort(temps);
auto it = std::ranges::find(temps, celsius{0.0});
auto hot = std::ranges::count_if(temps, [](celsius c) { return c > 30; });
auto [lo, hi] = std::ranges::minmax_element(temps);

// Classic STL algorithms
std::sort(temps.begin(), temps.end(), std::greater<>{});
std::nth_element(temps.begin(), temps.begin() + 2, temps.end());

// Accumulate into a wider type to avoid overflow
using wide = bound<{0, 100'000}>;
std::vector<score> scores = {100, 250, 500};
auto total = std::reduce(scores.begin(), scores.end(), wide{0}, std::plus<>{});
auto sum   = std::accumulate(scores.begin(), scores.end(), wide{0}, std::plus<>{});
```

Comparison-heavy algorithms (sort, find, min/max, lower_bound) use an optimized comparison path that matches native integer performance — no runtime overhead versus raw `int16_t` or `uint8_t`.

## Examples

The `examples/` directory contains self-contained programs demonstrating key features:

| Example | Feature |
|---------|---------|
| `percentage.cpp` | Clamped percentage with `+=` and `with_clamp()` |
| `color.cpp` | RGB channels with clamp saturation |
| `angles.cpp` | Wrapping heading arithmetic |
| `clock.cpp` | Cascading wrap with carry (seconds -> minutes -> hours) |
| `temperature.cpp` | Fractional notch grid (0.5 degree steps) |
| `division.cpp` | Exact rational division and integer division |
| `integer_division.cpp` | `ignore_round` policy for native integer division |
| `fixed_point.cpp` | Fixed-point arithmetic with fractional notch grids |
| `audio_sample.cpp` | Signed Q1.14 audio samples with mixing and clamp |
| `money.cpp` | Cents-precision currency arithmetic via fractional notch |
| `q_formats.cpp` | Q-format storage reference (Q4.4, Q1.7, Q8.8, Q16.16) |
| `signed.cpp` | Signed integer bounds with negative ranges |
| `errors.cpp` | Error handling: throw, error_code, optional |
| `array_index.cpp` | Bounded array indexing with sentinel, range-based for |
| `algorithms.cpp` | STL and ranges algorithms (sort, find, transform, accumulate, ...) |
| `pid_controller.cpp` | Fixed-point PID loop with `add_all` and `clamp_round` for saturating output |
| `audio_mixer.cpp` | 4-channel Q1.14 mix with `with(on_clamp, on_overflow)` peak metering and dB gain |
| `sequence_number.cpp` | TCP-style wrap SEQ with `on_wrap` epoch counter and `%` ring index |
| `histogram.cpp` | Latency bucketing with `bound_range`, `will_conversion_overflow`, `is_conversion_lossy` |
| `game_hp.cpp` | HP/ammo with `mul_all` damage chain, `saturated_cast`, modulo for magazine |
| `calendar.cpp` | Day → month → year cascade via nested `on_wrap` callbacks |
| `jitter_buffer.cpp` | Reorder buffer with `sentinel` policy + `on_sentinel` drop detection |
| `id_pool.cpp` | Bounded ID allocator using `std::hash<bound>`, `numeric_limits`, `try_make` |
| `sensor_fusion.cpp` | Weighted average across sensors with disparate fixed-point ranges |
| `torus_map.cpp` | 2-D sub-pixel position with `wrap` on both axes and edge-crossing events |

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
| `bound<{0,200}> ±/×/÷` (integer raw, unsafe) | 13 ns | 13 ns | **1.0x** |
| `bound<{{0,255},1/256}>` construct (Q8.8) | 13 ns | 14 ns | **0.97x** |
| `bound<{{0,65535},1/65536}>` construct (Q16.16) | 14 ns | 14 ns | **0.97x** |
| `accumulate(bound, unsafe)` 1000 elts | 64 ns | 64 ns | 1.0x (vectorized) |
| `accumulate(bound, checked)` 1000 elts | 274 ns | 64 ns | 4.3x (scalar) |
| `bound<{{-40,60},0.5}> = double` (rational path) | 87-94 ns | n/a | n/a |

Notes:

- Integer-raw bounds (the common case: `bound<{0,N}>`, `bound<{a,b}>` with
  notch 1) are at native parity for arithmetic and assignment.
- Fixed-point grids with integer Lower and unit-numerator Notch take an
  integer fast path in `assignment::store` and `from_value` — no rational
  construction in the hot loop.
- `checked` policy on accumulation pays a 4x penalty: the per-element domain
  check breaks autovectorization. Use the default `unsafe` for tight inner
  loops where you can prove no overflow upfront, then convert back to a
  `checked` bound after the loop.
- Assigning a `double` to a fractional-notch grid (`bound<{{-40,60},0.5}>`) is
  the slowest path because the value crosses the API boundary into
  rational arithmetic. By design — the library uses rational + integer math
  internally to preserve exactness.

## Build & Test

Requires CMake 3.24+ and a C++23 compiler (GCC 12+, Clang 16+, MSVC 19.36+).

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure   # runs unit + algo suites
./build/test                                 # unit tests directly
./build/algo                                 # STL algorithm integration tests
./build/bench                                # performance benchmarks (native vs bound)
./build/example_algorithms                   # algorithms example
```
