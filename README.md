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
// Default: throws std::system_error on out-of-range
using strict = bound<{0, 100}>;

// Clamp: saturates to the nearest boundary
using clamped = bound<{0, 100}, clamp>;
clamped x = 150;   // x == 100
clamped y = -5;    // y == 0

// Wrap: modular arithmetic
using angle = bound<{0, 359}, wrap>;
angle a = 370;     // a == 10
angle b = -10;     // b == 350
```

`clamp` and `wrap` are mutually exclusive and enforced by `static_assert`.

### Per-operation override

Even without a type-level policy, you can clamp or wrap on a per-operation basis:

```cpp
bound<{0, 100}> x(50);
x.with_clamp() = 150;  // x == 100
x.with_wrap()  = 103;  // x == 2
```

### Overflow action

An optional zero-overhead callback can be provided to react to overflow. When not used, the compiler eliminates it entirely (`if constexpr` + `[[no_unique_address]]`).

```cpp
using sec = bound<{0, 59}, wrap>;
using min = bound<{0, 59}>;

sec seconds(0);
min minutes(0);

// When seconds overflows, carry into minutes
seconds.policy<wrap>([&](auto carry) {
    minutes += carry;
}) = 125;
// seconds == 5, minutes == 2
```

### Error code mode

Instead of throwing, errors can be reported via `std::error_code`. This works with construction, direct assignment, and per-operation policies:

```cpp
std::error_code ec;

// Construction with error code
bound<{0, 100}> x(150, make_policy(ec));
// ec is set to EDOM, x is unchanged (default-constructed)

// Per-operation with error code
bound<{0, 100}> y(50);
y.policy(ec) = 200;
// ec is set to EDOM, y remains 50

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

### When `slim::optional` is returned

Operations return `slim::optional<bound>` in two cases:

1. **Division** — the divisor could be zero, so the result is always optional.

2. **Rational raw storage** — when the result grid cannot be represented with an unsigned integer raw type (see below), the result uses `rational` as its raw storage. Addition and multiplication on such types return `slim::optional` because rational arithmetic can overflow (denominator overflow).

All `optional`-returning operators propagate nullopt: if either operand is nullopt, the result is nullopt.

```cpp
using u8 = bound<{1, 255}>;
slim::optional<u8> a{u8(100)};
slim::optional<u8> none{slim::nullopt};

auto r1 = a + u8(10);     // optional<bound<{2, 510}>>, has value 110
auto r2 = none + u8(10);  // optional<bound<{2, 510}>>, nullopt
```

## Storage and Rational Fallback

Each `bound` stores a single `Raw` member. The storage type depends on the grid:

**Unsigned integer storage** (the common case): when the grid has a nonzero notch and the number of notch steps fits in an unsigned integer, `Raw` is the smallest `uint8_t`/`uint16_t`/`uint32_t`/`uint64_t` that can hold `(upper - lower) / notch`. The actual value is recovered as `Raw * notch + lower`. This gives compact, cache-friendly storage and fast integer arithmetic.

```cpp
using pct = bound<{0, 100}>;           // Raw: uint8_t  (101 values)
using big = bound<{0, 100000}>;        // Raw: uint32_t (100001 values)
using step = bound<{{-5, 5}, 0.5}>;    // Raw: uint8_t  (20 steps)
```

**Rational storage** (fallback): when the notch is zero, the grid allows all rationals within the interval. The raw type becomes `rational`, an exact fraction type. This happens for:

- Grids explicitly declared with notch 0: `bound<{{-10, 10}, 0}>` — any rational in [-10, 10].
- Division results: `bound<{1,255}> / bound<{1,255}>` produces a grid covering all rationals in the result interval, since the quotient of two integers is generally not on any fixed notch.
- Single-value grids: `bound<{42}>` — notch is 0, raw is rational (but only one value is valid).

```cpp
using frac = bound<{{-10, 10}, 0}>;    // Raw: rational
frac f = *(2_r/3);                     // exact 2/3

using u8 = bound<{1, 255}>;
auto q = u8(7) / u8(3);               // slim::optional<bound<{rational}>>
                                       // value is exactly 7/3
```

Rational storage is exact (no floating-point rounding) but larger (`sizeof(rational)` > `sizeof(uint64_t)`) and slower than integer storage. The library automatically selects the most efficient representation for each grid.

## Build & Test

Requires CMake 3.24+ and a C++23 compiler (GCC 12+, Clang 16+, MSVC 19.36+).

```bash
cmake -B build
cmake --build build
./build/test
```
