# Storage, iteration & standard-library integration

Each `bound` stores a single `Raw` member. The storage type is selected
automatically per grid; this page summarises the user-visible rules and
shows how `bound` integrates with the standard containers and algorithms.
For the full decision tree see [internals.md](internals.md).

## Storage selection

**Unsigned integer storage** — when `Lower ≥ 0` and notch is nonzero, `Raw`
is the smallest `uint8_t`…`uint64_t` that can hold `(Upper − Lower) / Notch`.
The value is recovered as `Raw * Notch + Lower` (offset encoding).

```cpp
using pct  = bound<{0, 100}>;          // Raw: uint8_t  (101 values)
using big  = bound<{0, 100'000}>;      // Raw: uint32_t (100 001 values)
using step = bound<{{0, 5}, 0.5}>;     // Raw: uint8_t  (10 steps)
```

When `Lower == 0` and `Notch == 1`, `Raw` equals the value directly — no
offset arithmetic.

**Signed integer storage** — when `Lower < 0` and `Notch == 1`, `Raw` is the
smallest `int8_t`…`int64_t` that fits the interval. `Raw` stores the value
directly (`Raw == value`) with no offset, matching native `int` performance
exactly.

```cpp
using temp = bound<{-40, 85}>;          // Raw: int8_t  (direct storage)
using pos  = bound<{-100'000, 100'000}>;// Raw: int32_t (direct storage)
using diff = bound<{-255, 255}>;        // Raw: int16_t (direct storage)
```

Grids with `Lower < 0` and a fractional notch still use unsigned offset
encoding:

```cpp
using fstep = bound<{{-5, 5}, 0.5}>;    // Raw: uint8_t (20 steps, offset encoding)
```

**Rational storage** — when `Notch == 0`, `Raw` becomes `rational`, an exact
fraction type. This happens for grids with `Notch == 0`, division results,
and single-value grids.

```cpp
using frac = bound<{{-10, 10}, 0}>;     // Raw: rational
frac f = *(2_r / 3);                    // exact 2/3

using u8 = bound<{1, 255}>;
auto q = u8{7} / u8{3};                 // slim::optional<bound<{rational}>>
                                        // value is exactly 7/3
```

Rational storage is exact (no floating-point rounding) but larger and slower
than integer storage. The library picks the most efficient representation
for each grid.

## `slim::optional<bound>` sentinel

`slim::optional<bound>` uses a sentinel value instead of a separate bool
flag, so `sizeof(slim::optional<bound>) == sizeof(bound)`. The sentinel is
`numeric_limits<raw>::max()` for unsigned types and `numeric_limits<raw>::min()`
for signed types. This costs one value from the representable range (e.g.
`int8_t` gives 255 usable values: −127..127).

## Raw storage access

`bound::Raw` is a public data member, but most code should not touch it
directly. The supported access patterns are:

- **`b.is_sentinel()`** — public probe for "does this bound currently hold
  the sentinel value?". The canonical answer to "is this slot empty?" under
  `sentinel` policy. Returns `true` iff `Raw` matches `sentinel_raw<B>()`.
- **`bnd::sentinel_raw<B>()`** — returns the raw byte pattern reserved for
  the sentinel. Useful for interop with C APIs or for constructing a `bound`
  in sentinel state manually (`b.Raw = sentinel_raw<B>()`).
- **`slim::optional<B>::from_maybe_sentinel(b)`** — non-throwing factory:
  returns `nullopt` if `b` is the sentinel, otherwise wraps `b`. The plain
  `slim::optional<B>{b}` constructor still throws on sentinel input — by
  design, since constructing an "engaged" optional from a sentinel value is
  usually a bug.
- **Direct `b.Raw = ...` writes** are only well-defined under `unsafe`
  policy (which opts out of all runtime checks). Outside `unsafe`, the
  library assumes `Raw` always encodes either a valid grid value or the
  sentinel.

## Iteration: `bound_range`

`bound_range` provides range-based for loop support:

```cpp
// Iterate over all values in the grid [0, 9].
for (auto i : bound_range<{0, 9}>{})
  std::cout << i;  // 0 1 2 3 4 5 6 7 8 9

// Wrapping iteration starting at 5 (visits all values once).
for (auto i : bound_range<{0, 9}>{5})
  std::cout << i;  // 5 6 7 8 9 0 1 2 3 4
```

The yielded values are bounds, not raw integers, so they slot directly into
`vec[i]` via the implicit `operator std::size_t()` (when the grid is
index-shaped — see [conversions.md](conversions.md#implicit-operator-conversions-on-bound)):

```cpp
std::vector<int> bins(10);
for (auto i : bound_range<{0, 9}>{})
  bins[i] = some_value(i);   // no .as<>() needed
```

## Compile-time constants

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

## `std` integration

`bound` specialises `std::hash` (works in `unordered_set` / `unordered_map`)
and `std::numeric_limits`, so `numeric_limits<bound<{0,100}>>::max()`
returns the upper bound, and `is_signed` / `is_integer` / `is_bounded` etc.
all report correctly. Include `bound/numeric_limits.hpp` to pull both in.

```cpp
#include "bound/numeric_limits.hpp"

std::unordered_set<bound<{0, 9}>> s;
s.insert(bound<{0, 9}>{3});

static_assert(std::numeric_limits<bound<{-40, 60}>>::is_signed);
static_assert(std::numeric_limits<bound<{0,  100}>>::max() == 100);
```

## STL algorithms

`bound` types work with standard algorithms out of the box — both
`std::ranges` and classic iterator-based forms:

```cpp
#include <algorithm>
#include <numeric>
#include <vector>
#include "bound/bound.hpp"
using namespace bnd;

using celsius = bound<{{-40, 60}, 0.5}, round_nearest>;
using score   = bound<{0, 1000}>;

std::vector<celsius> temps = {21.5, -5.0, 37.0, 0.0, 15.5};

// Ranges algorithms
std::ranges::sort(temps);
auto it = std::ranges::find(temps, celsius{0.0});
auto hot = std::ranges::count_if(temps, [](celsius c) { return c > 30; });
auto [lo, hi] = std::ranges::minmax_element(temps);

// Classic STL algorithms
std::sort(temps.begin(), temps.end(), std::greater<>{});
std::nth_element(temps.begin(), temps.begin() + 2, temps.end());

// Accumulate into a wider type to avoid overflow.
using wide = bound<{0, 100'000}>;
std::vector<score> scores = {100, 250, 500};
auto total = std::reduce    (scores.begin(), scores.end(), wide{0}, std::plus<>{});
auto sum   = std::accumulate(scores.begin(), scores.end(), wide{0}, std::plus<>{});
```

Comparison-heavy algorithms (sort, find, min/max, lower_bound) use an
optimised comparison path that matches native integer performance — no
runtime overhead versus raw `int16_t` or `uint8_t`. See
[examples/algorithms.cpp](../examples/algorithms.cpp) for a full pass over
common STL / ranges operations.
