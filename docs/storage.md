# Storage, iteration & standard-library integration

Each `bound` stores a single `Raw` member. The storage type is selected
automatically per grid; this page summarises the user-visible rules and
shows how `bound` integrates with the standard containers and algorithms.
For the full decision tree see [internals.md](internals.md); for which grids are
fastest (from a fixed-point perspective) see [fixed-point.md](fixed-point.md).

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

**Exact-fraction storage** — when `Notch == 0`, `Raw` becomes the internal
exact-fraction representation (`bnd::detail::rational`). This happens for grids
with `Notch == 0`, division results, and single-value grids. You never name
that type; you read the value back out with `numerator()` / `denominator()`.

```cpp
using ratio = bound<{{-10, 10}, 0}>;    // Raw: exact-fraction representation
ratio f = bound<{2, 2}>{2} / just<3>;   // exact 2/3
f.numerator();                          // 2  (denominator() == 3)

using lvl = bound<{1, 255}>;
auto q = lvl{7} / lvl{3};               // slim::optional<bound> (exact-fraction raw)
                                        // *q is exactly 7/3
```

Exact-fraction storage is exact (no floating-point rounding) but larger and
slower than integer storage. The library picks the most efficient representation
for each grid.

## Choosing the representation

The rules above are the **default deduction**. Several policy flags override it
(see [policies.md](policies.md#representation-flags) for the full table):

```cpp
using gain   = bound<{{0, 4}, notch<1, 65536>}, round_nearest | real>;
                                       // Raw: double (math operand, double-exact grid)
using ratio  = bound<{{0, 1}, notch<1, 3>}, exact>;
                                       // Raw: exact fraction on a NOTCHED grid
using regval = bound<{5, 100}, direct>; // Raw: uint8_t, raw() == value (5..100)
using slot   = bound<{-5, 5}, indexed>; // Raw: uint8_t, raw() == index (0..10)
using wide   = bound<{0, 100}, u16>;    // Raw: uint16_t (pinned width, raw() == value)
using sidx   = bound<{0, 4, notch<1,16>}, u32 | indexed>; // Raw: uint32_t index
```

`real`/`f32`/`f64` are the math-operand flags ([math.md](math.md)); `exact` lifts the
notch-count limit and removes `double` entirely; `direct` makes the raw equal
the wire/debugger value for interop; `indexed` gives signed grids a dense
unsigned layout for serialization.

The **fixed-width flags** `i8 u8 i16 u16 i32 u32 i64 u64` pin the exact backing
integer type instead of letting deduction pick the smallest fit (e.g. force a
`uint16_t` even where `uint8_t` would do, for a fixed wire layout). A bare width
flag means value storage (`raw() == value`, so `Notch == 1`, like `direct`); add
`indexed` for 0-based index storage on a notched grid. Unlike deduction or the fp
flags there is **no silent widening** — a type too small for the grid is a
compile error (`storage_pick` static_asserts the range fits). Mixed-flag results
from arithmetic resolve widest-wins: `exact > f64 > f32 > {width} > direct >
indexed > deduced` (width flags are dropped on arithmetic results, which deduce
their own width). See [`examples/storage_flags.cpp`](../examples/storage_flags.cpp)
for value/index storage and the compile-time fit check. `real` is selected only
when the grid is **double-exact** (every value fits `double`'s 53-bit significand);
otherwise it is dropped and deduction proceeds — and a result grid finer than the
`uint64` index space deduces `rational`, keeping the result exact.

> **Sentinel slot and SIMD width.** The smallest-type selection reserves one
> raw slot for the `slim::optional` sentinel (next section). That makes
> `bound<{0, 255}>` a **uint16**, not uint8 — raw 255 is the sentinel. In
> SIMD-width-sensitive loops this halves the lanes versus native `uint8_t`;
> a `bound<{0, 254}>` fits uint8 and runs at exactly native speed.

## `slim::optional<bound>` sentinel

`slim::optional<bound>` uses a sentinel value instead of a separate bool
flag, so `sizeof(slim::optional<bound>) == sizeof(bound)`. The sentinel is
`numeric_limits<raw>::max()` for unsigned types and `numeric_limits<raw>::min()`
for signed types. This costs one value from the representable range (e.g.
`int8_t` gives 255 usable values: −127..127). For `real` (double) raw the
sentinel is that same `numeric_limits<raw>::max()` rule applied to `double`,
i.e. the finite, comparable `DBL_MAX` — unreachable as an on-grid value, never a
NaN/Inf.

## Predefined hardware formats

`#include "bound/formats.hpp"` for a curated set of `bnd::` aliases that map to
native byte widths — so you can write `bnd::byte` / `bnd::unorm16` / `bnd::q8_8`
instead of spelling the grid and policy by hand. (The bare `u8`/`i16`/… names are
storage *flags*, so the native-width types use width words instead.)

| Type | Range / notch | Storage |
|---|---|---|
| `byte` `word` `dword` | `[0, 254]` … `[0, 2³²−2]` | uint8 / uint16 / uint32 |
| `sbyte` `sword` `sdword` `sqword` | `[−127, 127]` … | int8 / int16 / int32 / int64 |
| `unorm8` `unorm16` `unorm32` | `[0,1]`, notch 1/254, 1/65534, 1/(2³²−2) | uint8 / uint16 / uint32 |
| `q4_4` `q8_8` `q16_16` | `[0,15]`/`[0,255]`/`[0,65535]`, notch 1/16, 1/256, 1/65536 | uint8 / uint16 / uint32 |

**The reserved-top tradeoff.** Because the top value of each storage type is
the reserved sentinel slot (above), a *full*-width range would promote to the
next-larger type. These aliases instead stop one short of the sentinel — `byte`
is `[0, 254]`, not `[0, 255]`; `unorm8` uses notch 1/254 (still reaching 1.0
exactly). The payoff is native byte size **and** a still-zero-overhead
`slim::optional` (the sentinel slot is retained). Q-formats already have
headroom, so they keep their full natural range and power-of-two notches.

An unsigned `qword` is intentionally absent: the library's internal value path is
`imax` (`int64`), so unsigned values above 2⁶³−1 can't round-trip — use `sqword`
or a hand-rolled grid.

**Performance.** Multiplication is unaffected by the (non-power-of-two) UNORM
notches — it operates on raw notch indices, with the denominator folded into
the compile-time result grid. Power-of-two notches give only a negligible
shift-vs-constant-multiply edge on division/construction. The integer types are
direct storage (native-speed). Behavior is composable: these default to
`checked`; for register-style `wrap`/`clamp` declare your own variant, e.g.
`bound<{0,254}, wrap>`.

## Raw storage access

`bound::Raw` is a public data member, but most code should not touch it
directly. The supported access patterns are:

- **`b.is_sentinel()`** — public probe for "does this bound currently hold
  the sentinel value?". The canonical answer to "is this slot empty?" under
  `sentinel` policy.
- **`B::make_sentinel()`** — static factory returning a bound in the sentinel
  (empty) state. The supported way to construct one; wraps the underlying
  `bnd::sentinel_raw<B>()` raw pattern so callers never touch `Raw`.
- **`B::from_raw(raw)`** — static factory constructing a bound directly from a
  storage-layout raw value, with no validation (same trust contract as
  `unsafe`). The supported entry point for raw-level construction in tests,
  fast paths, and same-grid raw transfer.
- **`bnd::sentinel_raw<B>()`** — the raw byte pattern reserved for the sentinel
  that `make_sentinel()` wraps. Useful for interop with C APIs that need the
  pattern directly.
- **`slim::optional<B>::from_maybe_sentinel(b)`** — non-throwing factory:
  returns `nullopt` if `b` is the sentinel, otherwise wraps `b`. The plain
  `slim::optional<B>{b}` constructor still throws on sentinel input — by
  design, since constructing an "engaged" optional from a sentinel value is
  usually a bug.
- **Direct `b.Raw = ...` writes** are only well-defined under `unsafe`
  policy (which opts out of all runtime checks); prefer `B::from_raw(raw)` for
  trusted raw construction. Outside `unsafe`, the library assumes `Raw` always
  encodes either a valid grid value or the sentinel.

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
`vec[i]` via the implicit `operator imax()` (the standard imax → size_t
conversion does the rest — see
[conversions.md](conversions.md#implicit-operator-conversions-on-bound)):

```cpp
std::vector<int> bins(10);
for (auto i : bound_range<{0, 9}>{})
  bins[i] = some_value(i);   // no .as<>() needed
```

`bound_range` is a random-access, sized range, so the standard view adaptors
work on it directly. It also offers two conveniences:

```cpp
bound_range<{0, 9}> r;
for (auto i : std::views::reverse(r)) { ... }   // 9 8 7 … 0
for (auto i : r.strided(3))          { ... }    // 0 3 6 9  (every 3rd value)
for (auto [idx, v] : r.indexed())    { ... }    // (0,0) (1,1) …  position + value
```

`strided` and `indexed` are portable stand-ins for C++23 `std::views::stride`
/ `std::views::enumerate`, so they compile unchanged on C++20.

## Compile-time constants

`bnd::zero` and `bnd::one` are built-in point-bounds for the two values you reach
for most. They **assign into any grid that can exactly represent the value**
(verified at compile time — out of range, or off a notch, is a compile error)
and otherwise stand in for `0` / `1` in comparison and arithmetic:

```cpp
bound<{0, 200}>          a = zero;     // ok — stored as 0, no runtime check
bound<{{0, 1}, notch<1, 256>}> q = one; // ok — exact (raw 256)
bound<{5, 10}>           b = zero;     // ✗ compile error: 0 is not on this grid

if (a == zero) { ... }                 // comparison
auto c = a + one;                      // arithmetic — stays a bound
```

For any other constant, `just<value>` creates a single-value bound:

```cpp
constexpr auto pi   = just<3>;          // bound<{3, 3}>
constexpr auto step = just<frac<1, 4>>; // exact 1/4 point-bound
```

The `_b` literal is shorthand for `just<N>`:

```cpp
auto five = 5_b;                // bound<{5, 5}>
auto x    = 10_b + my_bound;    // grid widens via just<N> + bound
```

## `std`-vocabulary helpers

`bound/arithmetic.hpp` provides ADL-found `bnd::min`, `bnd::max`, and
`bnd::midpoint` (alongside `lerp` / `dot` / `cross`) so bounds drop into generic
code that calls them unqualified. `min` / `max` return the same bound type;
`midpoint` returns the **exact** average on a refined grid — the true midpoint
of two grid points need not land on the grid, so unlike `std::midpoint` on
integers it neither rounds nor overflows.

```cpp
bound<{0, 100}> a = 30, b = 71;
auto lo  = bnd::min(a, b);        // 30
auto mid = bnd::midpoint(a, b);   // exactly 50.5 (refined grid), never 50
```

There is no `bnd::clamp` free function: the name belongs to the `clamp` policy
flag. To clamp a value into a grid, use the `clamp` policy or
`clamp_cast<Target>` (see [conversions.md](conversions.md)).

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
