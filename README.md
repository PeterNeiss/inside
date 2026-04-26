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
// Default: no runtime checks (compile-time checks always apply)
using fast = bound<{0, 100}>;

// Checked: opt-in runtime domain validation (throws std::system_error)
using safe = bound<{0, 100}, checked>;
safe x = 150;      // throws std::system_error at runtime

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

By default, `bound` skips runtime domain checks for maximum performance (compile-time checks always apply). The `checked` flag enables runtime validation. `clamp`, `wrap`, and `sentinel` are mutually exclusive and enforced by `static_assert`.

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

By default, division produces exact rational results. With the `ignore_round` policy, division uses native integer division (truncation towards zero) for zero overhead:

```cpp
using val = bound<{0, 100}>;
val a = 7, b = 3;

// Exact: result is rational (7/3)
auto exact = a / b;

// Per-call integer division: result is integer (2)
auto trunc = div(a, b, make_policy<ignore_round>());

// Type-level: operator/ uses native division automatically
using fast = bound<{0, 100}, ignore_round>;
auto q = fast(7) / fast(3);  // 2
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
| `signed.cpp` | Signed integer bounds with negative ranges |
| `errors.cpp` | Error handling: throw, error_code, optional |
| `array_index.cpp` | Bounded array indexing with sentinel, range-based for |

Build and run any example:

```bash
cmake -B build && cmake --build build
./build/example_clock
./build/example_signed
```

## Build & Test

Requires CMake 3.24+ and a C++23 compiler (GCC 12+, Clang 16+, MSVC 19.36+).

```bash
cmake -B build
cmake --build build
./build/test
```
