# Storage, iteration and the standard library

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

An inside stores one private member, its **raw**. The raw's type follows from
the grid alone; no flag chooses it. This page gives the rules, the predefined
native formats, raw access, iteration, random sampling and the standard-library
hooks. The full decision tree is in [internals.md](internals.md); which grids
are fastest is in [fixed-point.md](fixed-point.md).

## Storage selection

The raw is the smallest native integer that numbers every point of the grid,
except in the cases at the end of this list.

| Grid | Raw | Example |
|---|---|---|
| Lower ≥ 0, a notch | the smallest `uint8_t` … `uint64_t` holding the index (Upper − Lower) / Notch; the value is Lower + raw·Notch | `inside<{0, 100}>`: `uint8_t` (101 values); `inside<{{0, 5}, 0.5}>`: `uint8_t` |
| whole numbers, notch 1 | the value itself, when its type is no wider than the index's | `inside<{5, 100}>`: `uint8_t`, raw == value; `inside<{200, 300}>`: a `uint8_t` index (300 would need 16 bits) |
| Lower < 0, notch 1 | the smallest `int8_t` … `int64_t`, holding the value: native `int` speed | `inside<{-40, 85}>`: `int8_t`; `inside<{-255, 255}>`: `int16_t` |
| Lower < 0, a fractional notch | an unsigned offset index | `inside<{{-5, 5}, 0.5}>`: `uint8_t` |
| more than 2⁶⁴ points | a library `wide_int` of as many 64-bit limbs as needed | `inside<{{0, 1ull << 34}, per<(1ull << 32)>}>`: 2 limbs |
| a single point (`just<…>`, `5_ins`) | nothing: the value is in the type | `sizeof(5_ins) == 1`, 0 bytes as a `[[no_unique_address]]` member |
| notch 0 (continuous) | an exact fraction (`beman::inside::rational`) | `inside<{{-10, 10}, 0}>`, and exact quotients |

The selection uses each type's full range: `inside<{0, 255}>` is a `uint8_t`
and `inside<{-128, 127}>` an `int8_t`, so a loop over them vectorizes with the
same lane count as native bytes.

**Grids off the notch's multiples.** Lower need not be a multiple of the notch:
`{{0.5, 255.5}, 1}` holds 0.5, 1.5, …, 255.5 (bin centres), and
`{{-1.1_r, 0.9_r}, 0.2_r}` holds −1.1, −0.9, …, 0.9. These always use an offset
index, because their values are not integers. Arithmetic finds the lattice of
the result: `c + c` on the bin centres is on the integers again, and the
product of two half-offset grids has notch 1/2. Rounding on them is in value
space ([arithmetic.md](arithmetic.md#rounding)).

```cpp
using centres = inside<{{0.5, 255.5}, 1}>;
centres c = 3.5;               // 3 or 4 would be a rounding_error
auto    s = c + c;             // inside<{{1, 511}, 1}>: on the integers
```

**Wide raws** behave like any other: compare, `+ - * %`, negate, every
policy and hook, `sum`, print, parse, hash and sample, all exact. `+=` and a
same-notch store are a raw add of the limbs. Their values can outgrow the
64-bit fraction, so a wide inside has no implicit `rational` conversion: read
it with `numerator()`/`denominator()` (wide integers) or `to<T>()`. A quotient
`a / b` is an exact fraction and reports `errc::overflow` if it does not fit
one. [`wei.cpp`](../examples/wei.cpp) (10²⁷ points of 10⁻¹⁸ ETH) and
[`solar_system.cpp`](../examples/solar_system.cpp) (nanometres across ±10¹³ m)
use wide raws, as quantities.

**A point** costs no storage, and an integer point takes the native integer
paths (`a % 5_ins`, `a / 5_ins` under `snap`):

```cpp
struct voice {
    [[no_unique_address]] decltype(just<440>) pitch;   // 0 bytes
    std::uint32_t frames;
};
static_assert(sizeof(voice) == 4);
```

**A continuous grid** holds any fraction in its range, exactly, but is larger
and slower than an integer raw. It comes from a notch of 0 or from an exact
quotient (except division by a point, which scales the dividend's lattice):

```cpp
using lvl = inside<{1, 255}>;
auto q = lvl{7} / lvl{3};       // exactly 7/3, and plain: the divisor cannot be 0
q.numerator();                  // 7 (denominator() == 3)
```

### Grids past 64 bits (C++26)

Under C++23 a grid's limits and notch are 64-bit fractions: a limit past 2⁶⁴,
or a notch finer than about 2⁻⁶³, does not compile. Under C++26 with static
reflection they have **no size limit**. The library interns large grid numbers
in static storage (`std::define_static_array`), so equal grids are the same
type however they are spelled.

```cpp
using huge = inside<{0, 0x1p100}>;        // {0, 2¹⁰⁰}: a 101-bit index
using fine = inside<{{0, 1}, 0x1p-80}>;   // notch 2⁻⁸⁰: 2⁸⁰ + 1 points
huge h = 1e30;                            // 1000000000000000019884624838656: the double, exactly
auto p = h * h;                           // a grid up to 2²⁰⁰; exact, no expected
auto q = uint64{~0ull} + uint64{~0ull};   // 2⁶⁵ − 2 needs a big upper limit
```

The `_g` literal spells a grid number exactly from decimal digits, a point and
an exponent: `inside<{0, 1267650600228229401496703205376_g}>` is
`inside<{0, 0x1p100}>`, and `inside<{{0, 1}, 1e-30_g}>` has an exact decimal
notch of 10⁻³⁰. Under C++23 `_g` is 64-bit and rejects larger values.
`per<D>`, `just<N, D>`, `_r` and `_ins` stay 64-bit in both modes.

Grid numbers past 64 bits exist only at compile time; the raw of such an
inside is a fixed-width integer sized from them. A grid can hold huge values
in few points (`{2¹⁰⁰, 2¹⁰⁰ + 10}` is a `uint8_t`). The math functions take and
return such insides, correctly rounded. Call grid helpers that form big
values, like `grid_of<B>.slot_count()`, in a constant expression; at run time a
result past 64 bits reports `errc::overflow`.

An inside takes a grid number at run time, checked like any store, once the
number exists: name a big one as a constant first (`constexpr auto top = N - 1;`
then `residue r = top;`), since `N - 1` evaluated at run time would have to form
it. `B::try_make(top)` reports instead of throwing.

A continuous grid past 64 bits stores an exact fraction of fixed-width
integers. A bounded one ([arithmetic.md](arithmetic.md#bounded-continuous-grids))
sizes them from its denominator bound, so its quotients are plain values:

```cpp
using length = inside<{{1e-41_g, 1e27_g}, 1e-41_g}, round_nearest>;
using decade = inside<{{-100, 100}, per<1 << 20>}, round_nearest>;
const length planck   = just<1.616255e-35_g>;
const length universe = just<8.8e26_g>;
auto ratio = universe / planck;           // exactly 1760…0/323251, a plain value
math::log10_into<decade>(ratio);          // 61.73597…, rounded once
```

An unbounded one sizes them from its limits, and reports `errc::overflow`
when a result outgrows them. [`planck_to_cosmos.cpp`](../examples/planck_to_cosmos.cpp)
runs this example with lengths in metres.

Integer grids past 64 bits compute exactly in fixed-width integers.
[`rsa.cpp`](../examples/rsa.cpp) runs textbook RSA-1024 on residues
`inside<{0, n - 1}>`: a product of two residues is a 2048-bit grid, and
`% inside<{n, n}>{}` brings it back as a plain residue.

Switches:

- C++26 with static reflection means GCC 16 with `-freflection`. The CMake
  option `BEMAN_INSIDE_REFLECTION` (default `ON`) adds the flag; the
  `gcc16-debug` and `gcc16-release` presets build that way.
- The headers detect reflection as `BEMAN_INSIDE_BIG_GRIDS`. Define it to `0`
  to keep 64-bit grids on a C++26 compiler.
- The two modes give `grid` different layouts, in different inline namespaces,
  so mixing C++23 and C++26 translation units that pass insides fails at link
  time instead of misbehaving.
- Big grids cost compile time; small grids compile and run as under C++23.

## Why no representation flags

Storage is not a policy: one grid always has one raw, so two insides on a
grid share their encoding, and every result type finds its own. For a fixed
wire layout, write `b.to<std::uint16_t>()` into the field (checked) and read
it back with `B{field}` or `B::try_make(field)`
([`storage.cpp`](../examples/storage.cpp)).

## Predefined native formats

`<beman/inside/formats.hpp>` names grids that fill a native width, so you can
write `uint8`, `unorm16` or `q8_8` instead of the grid:

| Types | Range, notch | Raw | Policy |
|---|---|---|---|
| `uint8`, `uint16`, `uint32`, `uint64` | [0, 2ᴺ − 1], 1 | `uintN_t` | checked |
| `int8`, `int16`, `int32` | [−2ᴺ⁻¹, 2ᴺ⁻¹ − 1], 1 | `intN_t` | checked |
| `int64` | [−(2⁶³ − 1), 2⁶³ − 1], 1 | `int64_t` | checked |
| `unorm8`, `unorm16`, `unorm32` | [0, 1], 1/(2ᴺ − 1) | `uintN_t` | `round_nearest` |
| `q4_4`, `q8_8`, `q16_16` | [0, 2ᴹ − 2⁻ᴹ], 2⁻ᴹ: every bit pattern | `uint8_t`, `uint16_t`, `uint32_t` | `round_nearest` |

- `unorm8`'s notch 1/255 reaches 1.0 exactly. A product works on the raw
  indices, with the odd denominator folded into the result grid at compile
  time, so non-power-of-two notches cost nothing there.
- `int64` is symmetric: the internal value path is `imax`, and −2⁶³ has no
  negation in `int64`.
- `uint64` passes `int64`, so it has no implicit `imax`; read it with
  `to<std::uint64_t>()`. A difference of two `uint64`s spans 2⁶⁵ values and
  gets a wide raw; a sum needs [C++26 big grids](#grids-past-64-bits-c26).
- For register-style wrapping, declare your own: `inside<{0, 255}, wrap>`.

### Counters

| Type | Is | At the ceiling |
|---|---|---|
| `clamp_counter<Max>` | `inside<{0, Max}, clamp>` | `++` stays at Max, `--` stays at 0: a saturating tally |
| `wrap_counter<Max>` | `inside<{0, Max}, wrap>` | `++` wraps Max → 0: sequence numbers, epochs |

```cpp
wrap_counter<255> seq{255};
++seq;                       // 0
```

## Raw access

The raw is private. The supported ways in and out:

- **`b.raw()`**: a const reference to the raw, under every policy (for C
  interop, `&std::as_const(b).raw()`).
- **`B::from_raw(raw)`**: build an inside from a raw, without validation (the
  `unsafe` contract). The entry point for trusted raw transfer.
- **Writing through `b.raw()`**: only under `unsafe`. Elsewhere it does not
  compile, because the library relies on the raw always being a valid point.

A quantity stores its number and nothing else, so `sizeof(q)` is the size of
its inside. It has no `raw()` of its own: go through `q.numerical_value().raw()`.
A unit conversion keeps the raw: km → mm on `inside<{0, 10}>` is the same
`uint8_t`, read on a rescaled grid.

## Iteration

### `inside_range` and `values<T>`

`inside_range<G>` walks every point of a grid, from Lower or from a start
point, wrapping once past Upper. `values<T>` is the same range named by a
type. Both yield insides, so `vec[i]` works without a cast:

```cpp
for (auto i : inside_range<{0, 9}>{})  std::cout << i;    // 0 1 2 … 9
for (auto i : inside_range<{0, 9}>{5}) std::cout << i;    // 5 6 7 8 9 0 1 2 3 4: a ring in age order

using bin = inside<{0, 9}>;
std::vector<int> bins(10);
for (bin b : values<bin>) bins[b] = 0;
auto it = std::ranges::find_if(values<bin>, is_full);
```

Both are random-access sized ranges, so views and algorithms compose on them,
and `[i]`, `size()` and `back()` cost nothing:

```cpp
inside_range<{0, 9}> r;
for (auto i : std::views::reverse(r)) { /* … */ }              // 9 8 … 0
for (auto i : r | std::views::stride(3)) { /* … */ }           // 0 3 6 9
auto first = std::ranges::partition_point(values<bin>, below_threshold);   // bisects: O(log n)
```

`std::views::stride` and `std::views::enumerate` are C++23 library features
that some libc++ versions still lack; `cursor` (below) steps by more than one
notch with any standard library.

### Stepping loops: `cursor<T, Step>`

`for (T t = lo; t <= hi; t += step)` needs a value one step past `hi`, which
`T` cannot hold, so the last increment reports out of range. A cursor can hold
it: `cursor<T, Step>` steps through `T`'s range by `Step`, one step past
Upper. It starts at `T`'s Lower, and `end(t)` is the value past the last:

```cpp
using time_t = inside<{{-4, 4}, per<1024>}, round_nearest>;
using amp_t  = inside<{{0, 100}, per<16384>}, round_nearest>;
for (cursor<time_t, per<4>> t; t != end(t); ++t)          // −4, −3.75, …, 4
    std::cout << t << " " << math::exp_into<amp_t>(t) << "\n";
for (cursor<time_t, 0.25_r> t = -2_ins; t <= 2_ins; ++t)  // any start, the classic test
    /* … */;
```

- A cursor is an inside on `{{Lower, Upper + Step}, Step}`: math, printing,
  comparison and assignment back to `T` work on it. It keeps `T`'s checks and
  rounding, so `--` below Lower and `++` past the end report like any store.
- `Step` defaults to `T`'s notch. It must be positive, divide `T`'s range and
  be a whole number of `T`'s notches, all checked at compile time.
- A default-constructed cursor is `T`'s Lower (a plain inside is left
  uninitialized). `t != end(t)` is one integer compare.
- A cursor is its own bidirectional iterator, and `end(t)` its sentinel. `*t`
  is the value as a `T`, so range algorithms take a cursor directly:

  ```cpp
  cursor<time_t, per<4>> t;
  for (time_t x : std::ranges::subrange(t, end(t))) { /* … */ }
  auto hit = std::ranges::find(t, end(t), 1.5_ins);
  ```

  It is not random access: `t - u` is arithmetic, not a distance.

### Iterating quantities

`values` and `cursor` take insides only. For quantities, loop over the
number's grid and attach the unit:

```cpp
using db_t = quantity<dB, inside<{{-24, 12}, per<2>}, round_nearest>>;   // dB: a unit of your own
for (cursor<db_t::rep, 3> n; n != end(n); ++n) {
    const db_t level{*n};                                                 // −24 dB, −21 dB, …, 12 dB
    /* … */
}
```

[`decibels.cpp`](../examples/decibels.cpp) and
[`audio_math.cpp`](../examples/audio_math.cpp) use this form.

## Random sampling: `uniform<B>(rng)`

`<beman/inside/random.hpp>` (opt-in: it includes `<random>`) adds
`uniform<B>(rng)`, which draws one of B's points uniformly, for any raw and any
standard engine:

```cpp
std::mt19937 rng{42};
auto die  = uniform<inside<{1, 6}>>(rng);                // 1 … 6
auto gain = uniform<inside<{{0, 1}, per<256>}>>(rng);    // one of the 257 points of [0, 1]
```

The draw uses only the generator's outputs, by the library's own recipe
(`std::uniform_int_distribution` differs between standard libraries), so a
seed gives the same values on libstdc++, libc++ and the MSVC STL
([determinism.md](determinism.md#other-surfaces)).

A continuous grid has no points to draw from: a compile error. The single
header leaves this header out of freestanding and `BEMAN_INSIDE_MATH_NO_FP`
builds.

## `min`, `max`, `midpoint`

`arithmetic.hpp` has ADL-found `min`, `max` and `midpoint`, so generic code
that calls them unqualified works on insides. On one type they return that
type; on two grids, `common_inside_t<L, R>` (also `std::common_type_t<L, R>`):
the hull of both ranges with the gcd notch, which holds every value of both.

`midpoint` returns the **exact** average, on a grid with half the notch. Unlike
`std::midpoint` on integers it neither rounds nor overflows, and it costs
about the same as `(a + b) / 2`:

```cpp
inside<{0, 100}> a = 30, b = 71;
auto lo  = min(a, b);           // 30
auto mid = midpoint(a, b);      // exactly 50.5
```

There is no `clamp` function: the name is the policy flag. To clamp a value,
use the policy or `clamp_cast` ([conversions.md](conversions.md#free-function-casts)).

## `std::hash` and `std::numeric_limits`

`<beman/inside/numeric_limits.hpp>` specialises `std::hash` (for
`unordered_set` and `unordered_map`) and `std::numeric_limits`, whose `min()`,
`max()`, `is_signed`, `is_integer` and `is_bounded` describe the grid:

```cpp
#include <beman/inside/numeric_limits.hpp>

std::unordered_set<inside<{0, 9}>> s;
s.insert(inside<{0, 9}>{3});
static_assert(std::numeric_limits<inside<{-40, 60}>>::is_signed);
static_assert(std::numeric_limits<inside<{0, 100}>>::max() == 100);
```

A quantity has neither: hash or bound its `numerical_value()`.

## STL algorithms

Insides and quantities work with the standard algorithms, both `std::ranges`
and the iterator forms. Comparisons take a path as fast as native integer
compares:

```cpp
using celsius = inside<{{-40, 60}, 0.5}, round_nearest>;
std::vector<celsius> temps = {21.5, -5.0, 37.0, 0.0, 15.5};
std::ranges::sort(temps);
auto hot      = std::ranges::count_if(temps, [](celsius c) { return c > 30; });
auto [lo, hi] = std::ranges::minmax_element(temps);
std::nth_element(temps.begin(), temps.begin() + 2, temps.end());
```

To add a range, use [`sum<Target>`](arithmetic.md#folds-and-sums), which checks
the total once, rather than `std::reduce` or `std::accumulate` into a wider
type. [`algorithms.cpp`](../examples/algorithms.cpp) runs the common STL and
ranges operations on insides.
