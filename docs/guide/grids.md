# Grids

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

An inside's type is a **grid**: the values from a lower limit to an upper
limit in steps of a *notch*. This page shows how to write grids and their
values, and the ready-made formats.

```cpp
using pct    = inside<{0, 100}>;                    // 0, 1, …, 100
using sample = inside<{{-1, 1}, per<16384>}>;       // −1 … 1 in steps of 1/16384
using temp   = inside<{{-40, 60}, 0.5}>;            // −40 … 60 in steps of 0.5
using money  = inside<{{0, 1'000'000}, per<100>}>;  // cents
```

## Writing a grid

A grid is `{{Lower, Upper}, Notch}`. The one rule: the notch divides
Upper − Lower, so the interval holds a whole number of notches. A grid that
breaks it does not compile, and the message gives both numbers.

With only the limits, `{Lower, Upper}`, the notch is derived: the coarsest
step 1/k that keeps every integer and both limits on the grid. That is 1 for
integer limits, 1/2 for `{0.5, 10}`, and 1/80 for
`{just<3, 10>, just<13, 16>}`.

Both limits are values of the grid: `{0, 100}` holds 0 and 100. Lower need not
be a multiple of the notch: `{{0.5, 10.5}, 1}` holds 0.5, 1.5, …, 10.5.

## Writing numbers

Every number in a grid, and every constant in an expression, is exact:

| Spelling | Means |
|---|---|
| `42`, `0.5`, `0x1p-4` | an integer, or a floating-point literal that is exact in binary |
| `per<D>` | the step 1/D |
| `just<N, D>` | the exact ratio N/D, as an inside of one point; `just<N>` is N |
| `0.1_r` | the exact decimal 1/10, as a `rational` |
| `5_ins` | the integer 5, as an inside of one point |

The literals `_ins`, `_r` and `_g` are also in `beman::inside::literals`:
`using namespace beman::inside::literals;` brings only them.

`0.1` is not 1/10 in binary, so a grid or a constant that needs 1/10 says
`0.1_r`, `per<10>` or `just<1, 10>`. A value at run time can be a `double`:
`inside<{{0, 1}, per<10>}, round_nearest>{0.1}` rounds it to the nearest notch,
1/10.

A point such as `just<440>` or `5_ins` holds its value in the type and takes no
storage. Use one as a constant in arithmetic: `x + 1_ins`, `x * just<3, 4>`.

## Half-open ranges

For a range you think of as half-open, such as an index `[0, N)` or an angle
`[0, 360)`, say so with a third argument. The open limit moves in by one notch,
so the grid is still closed and the type is the same:

```cpp
using degrees = inside<{0, 360, bounds::half_open}>;             // inside<{0, 359}>
using pos     = inside<{{0, 64}, per<16>, bounds::half_open}>;   // inside<{{0, 63.9375}, per<16>}>
using inner   = inside<{0, 10, bounds::open}>;                   // inside<{1, 9}>
```

## The default value

`T x;` and `T{}` hold 0, or Lower on a grid without 0:
`inside<{5, 10}> x;` holds 5.

## Continuous grids

A notch of 0 makes a *continuous* grid: any fraction in the interval whose
denominator stays within a bound you name.

```cpp
using thousandths = inside<{{0, 1}, max_denominator{1000}}>;
thousandths t{rational{1, 3}};                   // exactly 1/3
```

You rarely write one yourself. They come from exact division: `a / b` on
integer grids is a continuous grid that holds the exact quotient. They are
exact but slower than notched grids, so keep them out of hot loops.
[Exact arithmetic](../advanced/exact-arithmetic.md#bounded-continuous-grids)
explains the bound.

## Predefined formats

`<beman/inside/inside/formats.hpp>` names grids that fill a native width:

| Types | Range, notch | Stored as | Policy |
|---|---|---|---|
| `uint8`, `uint16`, `uint32`, `uint64` | [0, 2ᴺ − 1], 1 | `uintN_t` | checked |
| `int8`, `int16`, `int32`, `int64` | [−2ᴺ⁻¹, 2ᴺ⁻¹ − 1], 1 | `intN_t` | checked |
| `unorm8`, `unorm16`, `unorm32` | [0, 1], 1/(2ᴺ − 1) | `uintN_t` | `round_nearest` |
| `q4_4`, `q8_8`, `q16_16` | [0, 2ᴹ − 2⁻ᴹ], 2⁻ᴹ: every bit pattern | `uint8_t`, `uint16_t`, `uint32_t` | `round_nearest` |
| `clamp_counter<Max>` | `inside<{0, Max}, clamp>`: `++` stops at Max | the smallest that fits | `clamp` |
| `wrap_counter<Max>` | `inside<{0, Max}, wrap>`: `++` wraps Max → 0 | the smallest that fits | `wrap` |

```cpp
q8_8 gain{1.5};              // stored as 384
wrap_counter<255> seq{255};
++seq;                       // 0
```

`-x` of an `int64` reaches 2⁶³, past `int64_t`: its type stores a wider
integer, as every result type holds every result.

## How a grid is stored

An inside stores one integer, its *raw*, and nothing else. The grid picks it:
the smallest native integer that numbers every point. `inside<{0, 100}>` is a
`uint8_t`, `inside<{-40, 85}>` an `int8_t`, a point takes no storage, and a
grid with more than 2⁶⁴ points gets a wider integer of 64-bit limbs. No flag
chooses the storage; [Storage](../advanced/storage.md) has the rules.

A grid's numbers have no size limit: `inside<{0, 0x1p100}>` and a notch of
10⁻⁴¹ are fine
([grids past 64 bits](../advanced/storage.md#grids-past-64-bits)).

## Asking a type about its grid

| Spelling | Gives |
|---|---|
| `grid_of<T>` | the grid, as a value |
| `lower_of<T>`, `upper_of<T>`, `notch_of<T>` | its numbers, grid numbers for spelling other grids (`inside<{lower_of<T>, 2 * upper_of<T>}>`) |
| `flags_of<T>` | the policy |
| `std::numeric_limits<T>::min()`, `max()` | the limits, as insides |
| `step_count_of<T>` | (Upper − Lower)/Notch, as a point inside |
| `x.index()`, `T::from_index(i)` | a value's notch index as an `index_t<T>` = `inside<{0, step_count}>`, and back |

Grid numbers are compile-time values: their arithmetic is `consteval`, so
`N - 1` with a constant `N` is evaluated while compiling, wherever it appears.
At run time every count and index is an inside, which holds any value of its
grid, past 2⁶⁴ points too
([at run time](../advanced/storage.md#at-run-time)).
