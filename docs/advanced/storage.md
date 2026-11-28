# Storage

An inside stores one private member, its **raw**. The raw's type follows from
the grid alone; no flag chooses it. This page gives the rules, grids past 64
bits, and raw access. [Grids](../guide/grids.md) has the short version;
[internals.md](../internals.md#2-storage-encoding) the decision tree.

## Which raw a grid gets

The raw is the smallest native integer that numbers every point of the grid,
except in the cases at the end of this table.

| Grid | Raw | Example |
|---|---|---|
| Lower ≥ 0, a notch | the smallest `uint8_t` … `uint64_t` holding the index (Upper − Lower) / Notch; the value is Lower + raw · Notch | `id<"[0..100]">`: `uint8_t` (101 values); `id<"[0..5] 0.5">`: `uint8_t` |
| whole numbers, notch 1 | the value itself, when its type is no wider than the index's | `id<"[5..100]">`: `uint8_t`, raw == value; `id<"[200..300]">`: a `uint8_t` index (300 would need 16 bits) |
| Lower < 0, notch 1 | the smallest `int8_t` … `int64_t`, holding the value: native `int` speed | `id<"[-40..85]">`: `int8_t`; `id<"[-255..255]">`: `int16_t` |
| Lower < 0, a fractional notch | an unsigned offset index | `id<"[-5..5] 0.5">`: `uint8_t` |
| more than 2⁶⁴ points | a library `wide_int` of as many 64-bit limbs as needed | `id<key{grid{{0, 1ull << 34}, per<(1ull << 32)>}}>`: 2 limbs |
| a single point (`5_q`, `"2/3"_q`) | nothing: the value is in the type | `sizeof(5_q) == 1`, 0 bytes as a `[[no_unique_address]]` member |
| notch 0 (continuous) | an exact fraction of two 64-bit integers, or past 64 bits a fraction of fixed-width integers sized from the bound | `id<"[-10..10] <1000>">`, and exact quotients |

The selection uses each type's full range: `id<"[0..255]">` is a `uint8_t`
and `id<"[-128..127]">` an `int8_t`, so a loop over them vectorizes with the
same lane count as native bytes.

**Grids off the notch's multiples.** Lower need not be a multiple of the
notch: `{{0.5, 255.5}, 1}` holds 0.5, 1.5, …, 255.5 (bin centres), and
`{{-1.1_q, 0.9_q}, 0.2_q}` holds −1.1, −0.9, …, 0.9. These always use an
offset index, because their values are not integers.

```cpp
using centres = id<"[0.5..255.5] 1">;
centres c = 3.5;               // 3 or 4 would be a rounding_error
auto    s = c + c;             // id<"[1..511] 1">: on the integers
```

**A point** costs no storage, and an integer point takes the native integer
paths (`a % 5_q`, `a / 5_q` under a rounding mode):

```cpp
struct voice {
    [[no_unique_address]] decltype(440_q * si::hertz) pitch;   // 440 Hz, 0 bytes
    std::uint32_t frames;
};
static_assert(sizeof(voice) == 4);
```

**A continuous grid** holds any fraction in its range up to its denominator
bound, exactly, but is larger and slower than an integer raw: a gcd per
operation. It comes from a notch of 0 or from an exact quotient (except a
division by a point, which scales the dividend's lattice).

**Wide raws** behave like any other: compare, `+ - * / %`, negate, every
policy and hook, `sum`, print, parse, hash and sample, all exact. `+=` and a
same-notch store are a raw add of the limbs. Read
one with `numerator()`/`denominator()` (insides on wide raws) or `to<T>()`.
[`wei.cpp`](../../examples/wei.cpp) (10²⁷ points of 10⁻¹⁸ ETH) and
[`solar_system.cpp`](../../examples/solar_system.cpp) (nanometres across
±10¹³ m) use wide raws, as quantities.

## Grids past 64 bits

A grid's limits and notch have **no size limit**: a limit past 2⁶⁴ or a notch
finer than 2⁻⁶³ is fine. The library interns large grid numbers in static
storage (`std::define_static_array`), so equal grids are the same type however
they are spelled.

```cpp
using huge = id<"[0..0x1p100]">;          // {0, 2¹⁰⁰}: a 101-bit index
using fine = id<"[0..1] 0x1p-80">;        // notch 2⁻⁸⁰: 2⁸⁰ + 1 points
huge h = 1e30;                            // 1000000000000000019884624838656: the double, exactly
auto p = h * h;                           // a grid up to 2²⁰⁰; exact, no expected
```

A grid number is a `grid_number`, an exact rational of any size: the type of a
grid's limits and notch and of `lower_of<T>`. The `_g` literal spells one
exactly in any form of the literal grammar (decimal, `0x`, `0b`, a point, an
`e` or `p` exponent):
`id<"[0..1267650600228229401496703205376]">` is `id<"[0..0x1p100]">`,
and `id<"[0..1] 1e-30">` has an exact decimal notch of 10⁻³⁰.
`per<D>`, `_g` and `_q` take grid numbers of any size too
(`per<0x1p70_g>`, `18446744073709551616_q`).

Grid numbers past 64 bits exist only at compile time; the raw of such an
inside is a fixed-width integer sized from them. A grid can hold huge values
in few points (`{2¹⁰⁰, 2¹⁰⁰ + 10}` is a `uint8_t`). So grid numbers are
formed during compilation only: their operators, and the grid queries that
return one (`grid_of<B>.step_count()`, `value_unit()`), are `consteval`.
`residue r = N - 1;` in run-time code works, since `N - 1` is evaluated while
compiling; `N - n` with a run-time `n` does not compile. An inside takes a
grid number at run time, checked like any store; `B::try_make(N - 1)` reports
instead of throwing.

### At run time

Run-time values are insides. The type fixes the grid, and the raw holds any of
its values, past 2⁶⁴ points too, so everything a program computes while it
runs stays exact:

| Spelling | Gives |
|---|---|
| `x + y`, `x * y`, `x % m`, `divmod(x, m)`, … | exact results on wide raws; the result types are sized at compile time |
| `x.step()` | the step (x − Lower)/Notch as a `step_t<T>` = `id<key{grid{0, step_count}}>`, with T's raw type |
| `T::from_step(i)` | the value Lower + i·Notch: plain when `i`'s grid lies within the indices, else checked by T's policy (or `from_step(i, clamp)`, `from_step(i)`) |
| `step_count_of<T>` | (Upper − Lower)/Notch as a point inside: 0 bytes, printable |
| `x.numerator()`, `x.denominator()` | the value in lowest terms, as insides on the integers |
| `std::numeric_limits<T>::min()`, `max()`, `std::cw<lower_of<T>>` | the limits as insides |

```cpp
using huge = id<"[0..0x1p100]">;
huge h      = huge::from_step(step_count_of<huge>);         // 2¹⁰⁰
auto half   = div(h.step(), 2_q, round_floor);                // 2⁹⁹, at run time
huge middle = huge::from_step(half);                          // a raw copy
auto steps  = h.step() - middle.step();                      // id<key{grid{-2¹⁰⁰, 2¹⁰⁰}}>
```

`cursor<T>` steps through a big grid one notch at a time. `values<T>` is a
sized random-access range up to 2⁶³ points and a bidirectional one past
that; jump with `step()` and `from_step`.

A continuous grid past 64 bits stores an exact fraction of fixed-width
integers, sized from its denominator bound
([exact arithmetic](exact-arithmetic.md#bounded-continuous-grids)), so its
quotients are plain values:

```cpp
using length_number = id<"[1e-41..1e27] 1e-41 round_half_away">;
using metres        = id<"[1e-41..1e27] 1e-41 [m] round_half_away">;
const metres planck   = 1.616255e-35_q * si::metre;
const metres universe = 8.8e26_q * si::metre;
auto ratio = universe / planck;       // exactly 1760…0/323251, a plain value
math::log10<per<(1 << 20)>>(ratio);   // 61.73597…, rounded once onto 2^-20
```

[`planck_to_cosmos.cpp`](../../examples/planck_to_cosmos.cpp) runs this and
takes the geometric mean. Integer grids past 64 bits compute exactly in fixed-width
integers: [`rsa.cpp`](../../examples/rsa.cpp) runs textbook RSA-1024 on
residues `id<key{grid{0, n - 1}}>`, where a product of two residues is a 2048-bit
grid and `% id<key{grid{n, n}}>{}` brings it back as a plain residue.

Big grid numbers cost compile time; grids within 64 bits take the 64-bit fast
paths.

## Raw access

The raw is private. The supported ways in and out:

- **`b.raw()`**: a const reference to the raw, under every policy (for C
  interop, `&std::as_const(b).raw()`).
- **`B::from_raw(raw, unsafe)`**: build an inside from a raw, without validation (the
  `unsafe` contract). The entry point for trusted raw transfer.
- **Writing through `b.raw()`**: only under `unsafe`. Elsewhere it does not
  compile, because the library relies on the raw always being a valid point.

A quantity stores its number and nothing else, so `sizeof(q)` is the size of
its inside; read its raw in its own unit `u` with
`q.numerical_value_in(u).raw()`. A unit conversion keeps the raw: km → mm on
`id<"[0..10]">` is the same `uint8_t`, read on a rescaled grid.

## Why no representation flags

Storage is not a policy: one grid always has one raw, so two insides on a
grid share their encoding, and every result type finds its own. For a fixed
wire layout, write the inside into the field (`b.to<std::uint16_t>()` where its
grid may not fit, checked) and read
it back with `B{field}` or `B::try_make(field)`
([`storage.cpp`](../../examples/storage.cpp)).
