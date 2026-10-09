# Storage, iteration & standard-library integration

Each `inside` stores a single private `Raw` member. The storage type is selected
automatically per grid; this page summarises the user-visible rules and
shows how `inside` integrates with the standard containers and algorithms.
For the full decision tree see [internals.md](internals.md); for which grids are
fastest (from a fixed-point perspective) see [fixed-point.md](fixed-point.md).

## Storage selection

**Unsigned integer storage** — when `Lower ≥ 0` and notch is nonzero, `Raw`
is the smallest `uint8_t`…`uint64_t` that can hold `(Upper − Lower) / Notch`.
The value is recovered as `Raw * Notch + Lower` (offset encoding).

```cpp
using pct  = inside<{0, 100}>;          // Raw: uint8_t  (101 values)
using big  = inside<{0, 100'000}>;      // Raw: uint32_t (100 001 values)
using step = inside<{{0, 5}, 0.5}>;     // Raw: uint8_t  (10 steps)
```

**Wide integer storage** — a grid with more than 2⁶⁴ slots stores its index
in a library `wide_int` of as many 64-bit limbs as the slot count needs, so
the value stays exact and arithmetic on it stays total (no `expected`):

```cpp
using fine = inside<{{0, 1ull << 34}, per<(1ull << 32)>}>;
                                        // Raw: 2-limb wide_int (2⁶⁶ + 1 slots)
fine a = 1.5;
auto s = a + a;                         // exact; a 2-limb index again
```

Such an inside compares, adds, subtracts, multiplies, negates, takes `%`,
assigns (under every policy and `on_clamp` / `on_wrap` action), sums, prints,
parses, hashes and samples exactly; `+=`, `-=` and same-notch assignments
are a raw add of the limbs. Its values can outgrow the 64-bit exact
fraction, so it has no implicit conversion to it — compare it, read it
exactly with `numerator()` / `denominator()` (wide integers), or
approximately with `to<T>()`. A quotient `a / b` is an exact fraction and reports
`errc::overflow` when it does not fit one (a same-notch Q-format quotient
under `snap` stays on the Q-format grid, computed in a wide work type when
`raw·N` passes 64 bits). `from_chars` reads values past the
64-bit fraction in decimal (`digits[.digits][e±n]`, or `n/d`).

When `Lower == 0` and `Notch == 1`, `Raw` equals the value directly — no
offset arithmetic.

A grid written with two limits derives its notch (the coarsest `1/k` keeping
integers and both limits on the grid), and storage follows from that:
`inside<{0.5, 10}>` has notch 1/2 and 20 values in a `uint8_t`;
`inside<{frac<3, 10>, frac<13, 16>}>` has notch 1/80 and 42 values.

**Signed integer storage** — when `Lower < 0` and `Notch == 1`, `Raw` is the
smallest `int8_t`…`int64_t` that fits the interval. `Raw` stores the value
directly (`Raw == value`) with no offset, matching native `int` performance
exactly.

```cpp
using temp = inside<{-40, 85}>;          // Raw: int8_t  (direct storage)
using pos  = inside<{-100'000, 100'000}>;// Raw: int32_t (direct storage)
using diff = inside<{-255, 255}>;        // Raw: int16_t (direct storage)
```

Grids with `Lower < 0` and a fractional notch still use unsigned offset
encoding:

```cpp
using fstep = inside<{{-5, 5}, 0.5}>;    // Raw: uint8_t (20 steps, offset encoding)
```

**Exact-fraction storage** — when `Notch == 0`, `Raw` becomes an exact
fraction (`beman::inside::rational`). This happens for grids
with `Notch == 0` and exact division results. Read the value back out with
`numerator()` / `denominator()`.

**No storage for a point** — a single-value grid (`Lower == Upper`: `just<…>`,
`5_ins`, `zero`, `one`, `math::pi`) holds its value in the type, so its raw is
empty: `sizeof(5_ins) == 1` (the C++ minimum for a complete object), and a
point stored as a `[[no_unique_address]]` member of your own struct takes no
space at all. An integer-valued point counts as integer storage, so it takes
the native integer paths (`a % 5_ins`, `a / 5_ins` under `snap`).

```cpp
struct voice {
  [[no_unique_address]] decltype(just<440>) pitch;   // 0 bytes
  std::uint32_t frames;
};
static_assert(sizeof(voice) == 4);
```

```cpp
using ratio = inside<{{-10, 10}, 0}>;    // Raw: exact-fraction representation
ratio f = inside<{2, 2}>{2} / just<3>;   // exact 2/3
f.numerator();                          // 2  (denominator() == 3)

using lvl = inside<{1, 255}>;
auto q = lvl{7} / lvl{3};               // inside with an exact-fraction raw, exactly 7/3
                                        // (a plain value: the divisor cannot be zero)
```

Exact-fraction storage is exact (no floating-point rounding) but larger and
slower than integer storage. The library picks the most efficient representation
for each grid.

### Grids past 64 bits (C++26)

Under C++23 a grid's limits and notch are 64-bit fractions: a limit past
2⁶⁴, or a notch finer than about 2⁻⁶³, does not compile. Under C++26 with
static reflection they have **no size limit**: the library interns large grid
numbers in static storage (`std::define_static_array`), so equal grids stay the
same type however they were spelled.

```cpp
using huge = inside<{0, 0x1p100}>;        // {0, 2¹⁰⁰}: a 101-bit index
using fine = inside<{{0, 1}, 0x1p-80}>;    // a 2⁻⁸⁰ notch: 2⁸⁰ + 1 slots
using sq   = inside<(grid_of<huge> * grid_of<huge>).value()>;   // {0, 2²⁰⁰}

huge h = 1e30;          // 1000000000000000019884624838656: a double is exact
auto p = h * h;         // the result grid reaches 2²⁰⁰; exact, no expected
auto q = qword{~0ull} + qword{~0ull};     // 2⁶⁵ − 2 needs a big upper bound
```

The `_g` literal spells a grid number exactly, from decimal digits, a point
and an `e±n` exponent: `inside<{0, 1267650600228229401496703205376_g}>` is the
same type as `inside<{0, 0x1p100}>`, and `inside<{{0, 1}, 1e-30_g}>` has an
exact decimal notch of 10⁻³⁰. A floating-point limit or notch is taken as its
exact binary value, and grid arithmetic builds the rest. (`per<D>`,
`frac<N, D>` and the `_r` / `_ins` literals stay 64-bit; under C++23 `_g` is
64-bit too and rejects larger values at compile time.)

Grid helpers such as `grid_of<B>.slot_count()` form big values; call them in a
constant expression (`constexpr auto n = grid_of<B>.slot_count();`) — at
runtime a result past 64 bits reports `errc::overflow`.

Such an inside supports everything a wide-index inside does (above) — values
past 64 bits may even sit on a grid with few slots, like
`{2¹⁰⁰, 2¹⁰⁰ + 10}` in a `uint8_t`. The math functions (`sin`, `sqrt`, …)
take and return such insides too, correctly rounded: `log` of a value near
2³⁰⁰, or `sin` onto a 2⁻¹⁰⁰ grid ([math.md](math.md)). The big grid numbers
themselves (limits and notch) exist only at compile time; runtime arithmetic
on such an inside uses fixed-width integers sized from them.

A continuous grid (notch 0) whose limits pass 64 bits stores its value as an
exact fraction of fixed-width integers sized from the limits, so a quotient
of such values keeps its exact value and prints it:

```cpp
using length = inside<{{1e-41_g, 1e27_g}, 1e-41_g}, round_nearest>;
auto ratio = universe / planck;   // expected<…>: exactly 17600…000/323251
math::log10_into<decade>(*ratio); // 61.7359724…, rounded once
```

A sum, product or quotient whose fraction outgrows those integers reports
`errc::overflow`, as one past the 64-bit fraction does on a 64-bit grid.

Requirements and switches:

- C++26 with static reflection: GCC 16 with `-freflection`. The CMake option
  `BEMAN_INSIDE_REFLECTION` (default `ON`) adds the flag under C++26; the
  `gcc16-debug` / `gcc16-release` presets build that way.
- The headers detect reflection (`BEMAN_INSIDE_BIG_GRIDS`); define it to `0`
  to keep 64-bit grids on a C++26 compiler.
- The two modes give `grid` a different layout, so they live in different
  inline namespaces: mixing C++23 and C++26 translation units that pass
  insides between them fails at link time instead of misbehaving.
- Big grids cost compile time (about 4–10% in the test suite); small grids
  compile and run exactly as under C++23.

## Choosing the representation

The rules above are the **default deduction**. Several policy flags override it
(table below):

```cpp
using gain   = inside<{{0, 4}, per<65536>}, round_nearest | f64>;
                                       // Raw: double (double-exact grid)
using ratio  = inside<{{0, 1}, per<3>}, exact>;
                                       // Raw: exact fraction on a NOTCHED grid
using regval = inside<{5, 100}, direct>; // Raw: uint8_t, raw() == value (5..100)
using slot   = inside<{-5, 5}, indexed>; // Raw: uint8_t, raw() == index (0..10)
using wide   = inside<{0, 100}, u16>;    // Raw: uint16_t (pinned width, raw() == value)
using sidx   = inside<{0, 4, per<16>}, u32 | indexed>; // Raw: uint32_t index
```

| Flag | Forces | Grid requirement | Notes |
|---|---|---|---|
| `f64` | IEEE-754 `double` raw (the value itself) | dyadic **and** double-exact (every value fits `double`'s 53-bit significand); a continuous grid is a compile error | **storage only**: every result — value, rounding, error, return type, printing — is the one the type gives without it; rounding is stated separately (`round_nearest \| f64`). Arithmetic and deduced math results keep it when their grid is double-exact, else drop it. Under `BEMAN_INSIDE_MATH_NO_FP` it falls back to integer storage. |
| `f32` | IEEE-754 `float` raw (the value itself) | dyadic **and** float-exact (every value fits `float`'s 24-bit significand) | the binary32 sibling of `f64`, for single-precision FPUs, storage only as well. Results **demote `f32`→`f64`** when their grid outgrows `float` (and drop it when the grid outgrows `double`). Under `BEMAN_INSIDE_MATH_NO_FP` it falls back to integer storage. |
| `exact` | exact-fraction raw on **any** grid | none | no notch-count limit, no `double` anywhere; arithmetic is exact — on notched grids overflow is usually provably impossible and `+ − ×` return plain bounds (no `std::expected`) |
| `i8 u8 i16 u16 i32 u32 i64 u64` | the named fixed-width integer raw | value storage needs `Notch == 1` and the value range to fit (add `indexed` for a notched grid) | **pins the exact backing type** (e.g. a `uint16_t` where deduction would pick `uint8_t`) for a fixed wire layout. Bare = value storage (`raw() == value`, like `direct`); `+ indexed` = 0-based index storage. **No silent widening** — a type too small for the grid is a compile error. One width flag at a time; dropped on arithmetic results. |
| `direct` | raw == value as a plain integer | `Notch == 1` | e.g. `inside<{5, 100}, direct>` stores 5..100, not index 0..95 — the raw equals the wire/debugger value |
| `indexed` | raw == 0-based notch index | `Notch != 0` | e.g. `inside<{-5, 5}, indexed>` stores 0..10 unsigned — dense layout for serialization |

Arithmetic ORs the operands' policies; storage resolves several representation
flags **widest-wins**: `exact > f64 > f32 > {width} > direct > indexed >
deduced` (width flags are dropped on results, which deduce their own width),
so an `exact + f64` sum is exact and an `f64` math chain stays double-backed
where its grids allow.
A result grid finer than the `uint64` index space deduces a wide integer
index, keeping the result exact. See
[`examples/storage.cpp`](../examples/storage.cpp); [`examples/wei.cpp`](../examples/wei.cpp)
and [`examples/solar_system.cpp`](../examples/solar_system.cpp) use wide raws.

> **Full range and SIMD width.** The smallest-type selection uses each raw
> type's full range: `inside<{0, 255}>` is a **uint8** and `inside<{-128, 127}>`
> an **int8**, so SIMD-width-sensitive loops run at the same lane count as
> native `uint8_t` / `int8_t`.

## Predefined hardware formats

`#include <beman/inside/formats.hpp>` for a curated set of `beman::inside::` aliases that map to
native byte widths — so you can write `beman::inside::byte` / `beman::inside::unorm16` / `beman::inside::q8_8`
instead of spelling the grid and policy by hand. (The bare `u8`/`i16`/… names are
storage *flags*, so the native-width types use width words instead.)

| Type | Range / notch | Storage |
|---|---|---|
| `byte` `word` `dword` `qword` | `[0, 255]` … `[0, 2⁶⁴−1]` | uint8 / uint16 / uint32 / uint64 |
| `sbyte` `sword` `sdword` | `[−128, 127]` … `[−2³¹, 2³¹−1]` | int8 / int16 / int32 |
| `sqword` | `[−(2⁶³−1), 2⁶³−1]` | int64 |
| `unorm8` `unorm16` `unorm32` | `[0,1]`, notch 1/255, 1/65535, 1/(2³²−1) | uint8 / uint16 / uint32 |
| `q4_4` `q8_8` `q16_16` | `[0,15]`/`[0,255]`/`[0,65535]`, notch 1/16, 1/256, 1/65536 | uint8 / uint16 / uint32 |

Every alias fills its native type's range: `byte` is `[0, 255]` and `unorm8`
uses notch 1/255 (reaching 1.0 exactly). `sqword` stays symmetric because the
internal value path is `imax` and −2⁶³ has no negation in int64. Q-formats keep
their full natural range and power-of-two notches.

`qword` reaches past int64, so it has no implicit `operator imax`: read it with
`to<std::uint64_t>()`. A difference of two qwords spans 2⁶⁵ values and gets a
wide integer index; a sum's upper bound passes the 64-bit grid numbers, so it
needs [C++26 big grids](#grids-past-64-bits-c26).

**Performance.** Multiplication is unaffected by the (non-power-of-two) UNORM
notches — it operates on raw notch indices, with the denominator folded into
the compile-time result grid. Power-of-two notches give only a negligible
shift-vs-constant-multiply edge on division/construction. The integer types are
direct storage (native-speed). Behavior is composable: these default to
`checked`; for register-style `wrap`/`clamp` declare your own variant, e.g.
`inside<{0,255}, wrap>`.

### Counters

`formats.hpp` also names the two counter shapes, where the policy says what
`++` does at the ceiling:

| Type | Definition | At the ceiling |
|---|---|---|
| `counter<Max>` | `inside<{0, Max}, clamp>` | `++` stays at `Max`, `--` stays at 0 (a saturating tally) |
| `ring_counter<Max>` | `inside<{0, Max}, wrap>` | `++` wraps `Max` → 0 (sequence numbers, epochs) |

```cpp
beman::inside::ring_counter<255> seq{255};
++seq;                                  // seq == 0
```

## Raw storage access

`Raw` is private. The supported access patterns are:

- **`b.raw()`** — a const reference to the storage-layout raw value, under
  every policy (read-only C interop: `&std::as_const(b).raw()`).
- **`B::from_raw(raw)`** — static factory constructing an inside directly from a
  storage-layout raw value, with no validation (same trust contract as
  `unsafe`). The supported entry point for raw-level construction in tests,
  fast paths, and same-grid raw transfer.
- **Writes through `b.raw()`** — the mutable overload exists only under the
  `unsafe` policy (which opts out of all runtime checks); elsewhere it is a
  compile error, because the library assumes the raw always encodes a valid
  grid value. Prefer `B::from_raw(raw)` for trusted raw construction.

## Iteration: `inside_range`

`inside_range` provides range-based for loop support:

```cpp
// Iterate over all values in the grid [0, 9].
for (auto i : inside_range<{0, 9}>{})
  std::cout << i;  // 0 1 2 3 4 5 6 7 8 9

// Wrapping iteration starting at 5 (visits all values once).
for (auto i : inside_range<{0, 9}>{5})
  std::cout << i;  // 5 6 7 8 9 0 1 2 3 4
```

The yielded values are insides, not raw integers, so they slot directly into
`vec[i]` via the implicit `operator imax()` (the standard imax → size_t
conversion does the rest — see
[conversions.md](conversions.md#implicit-operator-conversions-on-inside)):

```cpp
std::vector<int> bins(10);
for (auto i : inside_range<{0, 9}>{})
  bins[i] = some_value(i);   // no .as<>() needed
```

`inside_range` is a random-access, sized range, so the standard view adaptors
work on it directly. It also offers two conveniences:

```cpp
inside_range<{0, 9}> r;
for (auto i : std::views::reverse(r)) { ... }   // 9 8 7 … 0
for (auto i : r.strided(3))          { ... }    // 0 3 6 9  (every 3rd value)
for (auto [idx, v] : r.indexed())    { ... }    // (0,0) (1,1) …  position + value
```

`strided` and `indexed` are portable stand-ins for C++23 `std::views::stride`
/ `std::views::enumerate`, so they also work with standard libraries that do not
ship those views yet.

## Random sampling: `uniform<B>(rng)`

`#include <beman/inside/random.hpp>` (opt-in: it pulls `<random>`) for
`uniform<B>(rng)`, which draws a `B` uniformly from the grid's slots — `Lower`,
`Lower + Notch`, …, `Upper` — for any storage and any standard random engine:

```cpp
std::mt19937 rng{42};
auto die  = uniform<inside<{1, 6}>>(rng);                   // 1..6
auto gain = uniform<inside<{{0, 1}, per<256>}>>(rng);       // one of 257 slots in [0, 1]
```

A continuous grid (notch 0) has no slots and is a compile error. The single header
drops this part in freestanding and `BEMAN_INSIDE_MATH_NO_FP` builds.

## Compile-time constants

`zero`, `one`, `just<V>` and the `_ins` literal are point insides: their value
lives in the type and their raw is empty ([above](#storage-selection)). When to
use which: [conversions.md](conversions.md#idiom-writing-literal-values-into-insides).

## `std`-vocabulary helpers

`beman/inside/arithmetic.hpp` provides ADL-found `beman::inside::min`, `beman::inside::max`, and
`beman::inside::midpoint` (alongside `lerp` / `dot` / `cross`) so insides drop into generic
code that calls them unqualified. `min` / `max` on two values of the same type
return that type. On two different grids they return `common_inside_t<L, R>`:
the interval hull with the gcd notch, which holds every value of both exactly
(the same type `std::common_type_t<L, R>` names). `midpoint` takes mixed grids
too.
`midpoint` returns the **exact** average on a refined grid — the true midpoint
of two grid points need not land on the grid, so unlike `std::midpoint` on
integers it neither rounds nor overflows. The refined grid has half the notch
and stays integer-backed, so `midpoint` costs about the same as `(a + b) / 2`.

```cpp
inside<{0, 100}> a = 30, b = 71;
auto lo  = beman::inside::min(a, b);        // 30
auto mid = beman::inside::midpoint(a, b);   // exactly 50.5 (notch ½ grid), never 50
```

There is no `beman::inside::clamp` free function: the name belongs to the `clamp` policy
flag. To clamp a value into a grid, use the `clamp` policy or
`clamp_cast<Target>` (see [conversions.md](conversions.md)).

## `std` integration

`inside` specialises `std::hash` (works in `unordered_set` / `unordered_map`)
and `std::numeric_limits`, so `numeric_limits<inside<{0,100}>>::max()`
returns the upper bound, and `is_signed` / `is_integer` / `is_bounded` etc.
all report correctly. Include `beman/inside/numeric_limits.hpp` to pull both in.

```cpp
#include <beman/inside/numeric_limits.hpp>

std::unordered_set<inside<{0, 9}>> s;
s.insert(inside<{0, 9}>{3});

static_assert(std::numeric_limits<inside<{-40, 60}>>::is_signed);
static_assert(std::numeric_limits<inside<{0,  100}>>::max() == 100);
```

## STL algorithms

`inside` types work with standard algorithms out of the box — both
`std::ranges` and classic iterator-based forms:

```cpp
#include <algorithm>
#include <numeric>
#include <vector>
#include <beman/inside/inside.hpp>
using namespace beman::inside;

using celsius = inside<{{-40, 60}, 0.5}, round_nearest>;
using score   = inside<{0, 1000}>;

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
using wide = inside<{0, 100'000}>;
std::vector<score> scores = {100, 250, 500};
auto total = std::reduce    (scores.begin(), scores.end(), wide{0}, std::plus<>{});
auto sum   = std::accumulate(scores.begin(), scores.end(), wide{0}, std::plus<>{});
```

Comparison-heavy algorithms (sort, find, min/max, lower_bound) use an
optimised comparison path that matches native integer performance — no
runtime overhead versus raw `int16_t` or `uint8_t`. See
[examples/algorithms.cpp](../examples/algorithms.cpp) for a full pass over
common STL / ranges operations.
