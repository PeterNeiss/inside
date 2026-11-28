# Grids

An inside's type is a **grid**: the values from a lower limit to an upper
limit in steps of a *notch*. This page shows how to write grids and their
values, and the ready-made formats.

```cpp
using integers   = id<"[0..100]">;                      // 0, 1, …, 100
using q14        = id<"[-1..1] 1/16384">;               // −1 … 1 in steps of 1/16384
using halves     = id<"[-40..60] 0.5">;                 // −40 … 60 in steps of 0.5
using hundredths = id<"[0..1'000'000] 1/100">;          // 0 … 1 000 000 in steps of 1/100
```

A grid holds numbers. When they measure something, give them a unit:
`id<"[-40..60] 0.5 [m]">` is a length on that grid, and money to the cent
is `id<"[0..1'000'000] 1/100 [USD]">` with a currency of your own
([Units](units.md)).

## Writing a grid

A grid is `{{Lower, Upper}, Notch}`. The one rule: the notch divides
Upper − Lower, so the interval holds a whole number of notches. A grid that
breaks it does not compile, and the message gives both numbers.

With only the limits, `{Lower, Upper}`, the notch is derived: the coarsest
step 1/k that keeps every integer and both limits on the grid. That is 1 for
integer limits, 1/2 for `{0.5, 10}`, and 1/80 for
`{0.3_q, 0.8125_q}`.

Both limits are values of the grid: `{0, 100}` holds 0 and 100. Lower need not
be a multiple of the notch: `{{0.5, 10.5}, 1}` holds 0.5, 1.5, …, 10.5.

## Writing numbers

Every number in a grid, and every constant in an expression, is exact:

| Spelling | Means |
|---|---|
| `42`, `0.5`, `0x1p-4` | an integer, or a floating-point literal that is exact in binary |
| `per<D>` | the step 1/D |
| `5_q`, `0.1_q`, `"2/3"_q` | the integer 5, the exact decimal 1/10, the ratio 2/3: an inside of one point |
| `std::cw<V>` | a value the code computes while compiling (`std::cw<N>` in a template), as that point |

The literals `_q` and `_g` are also in `ins::literals`:
`using namespace ins::literals;` brings only them.

`std::cw<V>` is the standard `std::constant_wrapper`. The operators,
comparisons, compound assignments and constructors read it as the point V:
`x * std::cw<N>`, `x <= std::cw<upper_of<T>>`, `T{std::cw<N>}`,
`std::cw<v> * si::metre`. A function such as `max` or `mod` takes an inside,
so pass it `2_q` or a point type. Two wrappers combine by std's own rules
(`std::cw<1> / std::cw<3>` is `std::cw<0>`), and a wrapper of a double is
rejected as the double is.

`0.1` is not 1/10 in binary, so a grid or a constant that needs 1/10 says
`0.1_q`, `per<10>` or `"1/10"_q`. A value at run time can be a `double`:
`id<"[0..1] 1/10 round_half_away">{0.1}` rounds it to the nearest notch,
1/10.

A point such as `440_q` or `5_q` holds its value in the type and takes no
storage. Use one as a constant in arithmetic: `x + 1_q`, `x * 0.75_q`.

## Half-open ranges

For a range you think of as half-open, such as an index `[0, N)` or an angle
`[0, 360)`, say so with a third argument. The open limit moves in by one notch,
so the grid is still closed and the type is the same:

```cpp
using index      = id<"[0..360)">;                                  // id<"[0..359]">
using sixteenths = id<"[0..64) 1/16">;                              // id<"[0..63.9375] 1/16">
using inner      = id<"(0..10)">;                                   // id<"[1..9]">
using heading    = id<"[0..360) [deg]">;                            // an angle: [0°, 359°]
```

## The default value

`T x;` and `T{}` hold 0, or Lower on a grid without 0:
`id<"[5..10]"> x;` holds 5.

## Continuous grids

A notch of 0 makes a *continuous* grid: any fraction in the interval whose
denominator stays within a bound you name.

```cpp
using thousandths = id<"[0..1] <1000>">;
thousandths t{"1/3"_q};                   // exactly 1/3
```

You rarely write one yourself. They come from exact division: `a / b` on
integer grids is a continuous grid that holds the exact quotient. They are
exact but slower than notched grids, so keep them out of hot loops.
[Exact arithmetic](../advanced/exact-arithmetic.md#bounded-continuous-grids)
explains the bound.

## Predefined formats

`<inside/inside/formats.hpp>` names grids that fill a native width, in
`ins::formats` (`using namespace ins::formats;` brings
them in):

| Types | Range, notch | Stored as | Policy |
|---|---|---|---|
| `uint8`, `uint16`, `uint32`, `uint64` | [0, 2ᴺ − 1], 1 | `uintN_t` | checked |
| `int8`, `int16`, `int32`, `int64` | [−2ᴺ⁻¹, 2ᴺ⁻¹ − 1], 1 | `intN_t` | checked |
| `unorm8`, `unorm16`, `unorm32` | [0, 1], 1/(2ᴺ − 1) | `uintN_t` | `round_half_away` |
| `uq4_4`, `uq8_8`, `uq16_16` | [0, 2ᴹ − 2⁻ᴹ], 2⁻ᴹ: every bit pattern | `uint8_t`, `uint16_t`, `uint32_t` | `round_half_away` |
| `clamp_counter<Max>` | `id<key{grid{0, Max}, clamp}>`: `++` stops at Max | the smallest that fits | `clamp` |
| `wrap_counter<Max>` | `id<key{grid{0, Max}, wrap}>`: `++` wraps Max → 0 | the smallest that fits | `wrap` |

```cpp
using namespace ins::formats;

uq8_8 gain{1.5};              // stored as 384
wrap_counter<255> counter{255};
++counter;                   // 0
```

`-x` of an `int64` reaches 2⁶³, past `int64_t`: its type stores a wider
integer, as every result type holds every result.

## How a grid is stored

An inside stores one integer, its *raw*, and nothing else. The grid picks it:
the smallest native integer that numbers every point. `id<"[0..100]">` is a
`uint8_t`, `id<"[-40..85]">` an `int8_t`, a point takes no storage, and a
grid with more than 2⁶⁴ points gets a wider integer of 64-bit limbs. No flag
chooses the storage; [Storage](../advanced/storage.md) has the rules.

A grid's numbers have no size limit: `id<"[0..0x1p100]">` and a notch of
10⁻⁴¹ are fine
([grids past 64 bits](../advanced/storage.md#grids-past-64-bits)).

## Asking a type about its grid

| Spelling | Gives |
|---|---|
| `grid_of<T>` | the grid, as a value |
| `lower_of<T>`, `upper_of<T>`, `notch_of<T>` | its numbers, grid numbers for spelling other grids (`id<key{grid{lower_of<T>, 2 * upper_of<T>}}>`) |
| `policy_of<T>` | the policy |
| `std::numeric_limits<T>::min()`, `max()` | the limits, as insides |
| `step_count_of<T>` | (Upper − Lower)/Notch, as a point inside |
| `x.step()`, `T::from_step(i)` | a value's step, its count of notches above Lower, as a `step_t<T>` = `id<key{grid{0, step_count}}>`, and back |

Grid numbers are compile-time values: their arithmetic is `consteval`, so
`N - 1` with a constant `N` is evaluated while compiling, wherever it appears.
At run time every count and index is an inside, which holds any value of its
grid, past 2⁶⁴ points too
([at run time](../advanced/storage.md#at-run-time)).
