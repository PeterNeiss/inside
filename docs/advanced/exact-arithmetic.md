# Exact arithmetic

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

How each operation forms its result type, which code a division runs, how
every rounding mode rounds, and how continuous grids keep their fractions
bounded. The everyday view is in [Arithmetic](../guide/arithmetic.md).

## Result grids

Every operation computes its result grid at compile time from the operands'
grids, so that it holds every value the operation can produce:

| Operation | Result interval | Result notch |
|---|---|---|
| `a + b`, `a - b` | the sum or difference of the intervals | the gcd of the two notches |
| `a * b` | the extremes of the four corner products | the product of the notches |
| `-a` | the interval mirrored | the same |
| `min(a, b)`, `max(a, b)` | [min of the lowers, min of the uppers], [max of the lowers, max of the uppers] | the gcd, as for the hull |
| `lerp(a, b, t)` | the hull of `a`'s and `b`'s ranges, when `t`'s grid lies in [0, 1] | that of `a + (b − a)·t` |
| `a * c`, `a / c` (c a nonzero point) | scaled by c | N·\|c\|, N/\|c\|: the raw number is reused |
| `a / b` | see [division](#division) | |
| `a % b` | what the rounding mode allows: under truncation the dividend's sign and \|r\| ≤ min(m, max\|a\|); under `round_floor` the divisor's sign, `round_ceil` the opposite; to nearest \|r\| ≤ min(⌊max\|b\|/2⌋, max\|a\|); m = max\|b\| − 1 | 1 |

On grids whose lower limit is not a multiple of the notch, the result finds
its own lattice: the sum of two copies of `{{0.5, 255.5}, 1}` is on the
integers again, and the product of two half-offset grids has notch 1/2.

Multiplying by a point scales the lattice instead of forming a product grid:
`x * just<3, 2>` on `inside<{0, 100}>` is `{{0, 150}, 1.5}` with the same raw,
about one native multiply. So `c * 9_ins / 5_ins + 32_ins` converts ℃ to ℉
with no `std::expected` and no fraction.

## Result policies

A result keeps the policy its operands share. A point with the default policy
(`just<2>`, `2_ins`) is neutral, and operands with different policies give
the default. So a rounding mode survives an intermediate step: with
`inside<{0, 10}, round_floor> a{7}`, both `a / 2_ins` and `(a + 0_ins) / 2_ins`
are 3. `std::common_type_t<L, R>` (also `common_inside_t<L, R>`) is the hull
of both grids, with the gcd notch, under the same rule.

## Division

`a / b` returns a plain inside when the divisor's grid excludes 0, and a
`std::expected<…, errc>` when it holds 0. Which code runs is chosen at compile
time from the operands' grids and the rounding mode of either operand or of
the call (`div(a, b, round_floor)`):

| Path | When | What it does | Result grid |
|---|---|---|---|
| A: Q-format | a rounding mode, and both operands on one notch 1/N, N ≥ 2 | `(a.raw × N) / b.raw`, rounded by the mode: the textbook fixed-point divide | the same notch; the interval widens to hold `upper(a) / notch(b)` |
| B: integer | a rounding mode, and both grids on whole numbers | a native integer divide, rounded by the mode | `grid(a) / grid(b)`, each limit rounded the same way |
| C: exact | otherwise | the exact fraction | a continuous grid with a denominator bound |
| D: point | the divisor is a nonzero point, the dividend has a notch | nothing: the dividend's lattice scaled by 1/\|c\| | notch N/\|c\|, integer storage |

Both native paths divide in 32 bits when the operands fit, so `/` and `%` on
small grids run at native speed.

```cpp
using fp = inside<{{0, 255}, per<256>}, unsafe>;      // Q8.8; unsafe rounds toward zero
auto q = div(fp{200}, fp{3}, round_toward_zero);      // path A: raw 17066, about 66.664
```

Path A multiplies before it divides: (51200 × 256) / 768 = 17066, that is
⌊66.667 × 256⌋. Dividing first would give 66 × 256 = 16896 and lose the
fraction. It also **widens the interval**: Q8.8 / Q8.8 has the type
`inside<{{0, 65280}, per<256>}>`, because 255 divided by the smallest nonzero
divisor, 1/256, is 65280.

Path C splits a divisor range that straddles 0 into its positive part
[notch, Upper] and its negative part [Lower, −notch], and joins the results.

### When a quotient is plain

A divisor grid that excludes 0 (`Lower > 0` or `Upper < 0`) gives a plain
inside. No path can overflow: for x = p/a and y = r/b, x/y = p·b/(a·r), so
the quotient's denominator is at most the dividend's largest denominator
times the divisor's largest numerator. The result grid records that bound
([below](#bounded-continuous-grids)), and its fraction is sized for it, up to
1024 bits. Past that the quotient has no type: `/` does not compile, and
`div_into` rounds the quotient onto a type you name instead.

```cpp
using fine = inside<{{0, 1}, rational{1, umax{1} << 40}}>;
auto f = fine{0.5} / inside<{{1, 2}, rational{1, umax{1} << 40}}>{1.5};
// 1/3, a plain value: denominators up to 2^81, in a fraction sized for them
```

[`division.cpp`](../../examples/division.cpp) shows paths B and C side by side.

## Rounding

### Compatible notches

Storing a value between two notches is a compile-time error when the source
grid can produce one. Two notches are compatible when the target's notch is a
whole multiple of the source's: notch 2 → 1 is always exact, 1 → 2 may
round. To round, say so for one store or for the type:

```cpp
using coarse = inside<{{0, 10}, 2}>;   // 0, 2, 4, …, 10
using finer  = inside<{{0, 10}, 1}>;   // 0, 1, 2, …, 10
coarse c{0};
finer  f{3};
// c = f;                              // does not compile: 3 is between notches
c.policy<round_toward_zero>() = f;                  // 2: this store rounds toward zero
c.policy<round_nearest>() = f;         // 4: this store rounds to nearest
auto half = [](finer x) -> coarse { return x.policy<round_toward_zero>(); };   // round at the return
```

`.policy<round_toward_zero>()` on a temporary, `(a * a).policy<round_toward_zero>()`, holds the value,
so it is safe to return. The target's own policy still applies: into a
`clamp` type the range clamps while `policy<round_toward_zero>()` handles the notch.

### How each mode rounds

Rounding is in value space, on every grid, also on one whose points are
offset from the multiples of the notch (`{{0.5, 10.5}, 1}`):

| Mode | Rounds |
|---|---|
| `round_toward_zero` | toward zero: down at or above 0, up below 0 |
| `round_nearest` | to the nearest point; a tie away from zero |
| `round_floor`, `round_ceil` | toward −∞, toward +∞ |
| `round_half_even` | to the nearest point; a tie to the even point, counted from the grid's anchor (…, −1.5, 0.5, 2.5, … on `{{-2.5, 2.5}, 1}`) |

On an offset grid 0 itself lies between two points (−0.5 and 0.5 on
`{{-10.5, 10.5}, 1}`): `round_nearest` takes 0.5, and `round_toward_zero`
takes −0.5, since 0 counts as non-negative. A rounding mode rounds before the
range check, so 9.5 into `inside<{0, 9}, round_floor>` stores 9.

### Rounding a quotient

On the native paths, `round_toward_zero` truncates like C++ `/`. Another
rounding mode rounds the quotient instead, and `%` stays consistent with it,
so `(a / b) * b + a % b == a` for every mode: under `round_floor`,
`-8 / 3 == -3` and `-8 % 3 == 1`; under `round_toward_zero`, `-2` and `-2`.
Without a rounding mode, `/` is exact and `%` truncates. `%` and `divmod` need
whole-number grids; on others they are deleted overloads, so a `requires`
probe answers false.

## Compound assignment

`x op= y` computes `x op y` exactly and stores the result back through `x`'s
policy, so it rounds and checks once. The right-hand side is an inside
(`x += 0.5_ins`); a plain `int`, a `double`, a `rational` and a
`std::expected` are rejected. A zero divisor in `/=` or `%=` goes to `on_error`, the `errc&`
or the handler like any failed store; under `unsafe` it is undefined.

Under `unsafe`, `+=`, `-=` and `*=` run at the raw type's width: a loop of
byte-wide `b += 1_ins` vectorizes like native `uint8_t`.

## Bounded continuous grids

A continuous grid carries a **denominator bound**: every value is n/d with
d ≤ `MaxDenominator`. The exact operators compute the bound, so a quotient
knows how large its fractions can get, and so does everything computed from
it:

| Operation | Denominator bound of the result |
|---|---|
| `x + y`, `x − y`, `x × y` | D(x) · D(y) |
| `x / y` | D(x) · max\|y\| · D(y) |
| `−x`, `abs`, `copysign` | D(x) |
| `fmod(x, y)` | D(x) · D(y) |
| `hull`, `common_type` | the larger |

D of a notched grid is its notch's denominator, of a point its own, and of a
continuous grid its bound. The result's fraction is sized for the bound, so
the operation cannot overflow and returns a plain value:

```cpp
auto q = inside<{0, 100}>{50} / inside<{1, 7}>{3};   // 50/3, denominators ≤ 7
auto f = q * 9_ins / 5_ins + 32_ins;                 // 62, denominators ≤ 35
auto s = q * q - q;                                  // 2350/9, denominators ≤ 343
```

A continuous grid you declare names its bound with `max_denominator`;
`inside<{{0, 1}, 0}>` does not compile:

```cpp
using thousandths = inside<{{0, 1}, max_denominator{1000}}>;
thousandths t{rational{1, 1000}};
auto p = t * t;                       // a plain value, denominators ≤ 10^6
// t = rational{1, 1001};             // rounding_error: off the grid
```

Any bound works while a value's numerator and denominator together fit 1024
bits. An operation whose exact result would need more has no result type: the
operator is deleted, and its message names the fix (store an operand into a
coarser type first, or round the result once with `mul_into` or `div_into`).

A stored value whose denominator passes the bound is a `rounding_error`
unless the policy rounds. Then it goes to a neighbouring fraction within the
bound (a Farey neighbour): `round_floor`, `round_ceil` and `round_toward_zero`
pick a side, `round_nearest` the closer one. So
`inside<{{0, 1}, max_denominator{1000}}, round_nearest>{0.1}` holds exactly
1/10, although the `double` 0.1 is not 1/10.

## The free functions

`add`, `sub`, `mul`, `div`, `mod` and `divmod` are the operators as functions,
with an optional last argument:

| Form | Meaning |
|---|---|
| `op(l, r)` | the operator |
| `op(l, r, round_floor)` | a flag: how a division rounds |
| `op(l, r, make_policy<F>())`, `make_policy<F>(ec)` | flags, and an `errc&` |
| `op(l, r, ec)` | an `errc&` that receives a zero divisor |

They take no hooks: the arithmetic cannot fail but for a zero divisor, which
stays in the result. `divmod(a, b)` returns a `divmod_result` with `Quotient`
and `Remainder`, typed as `a / b` under the rounding mode and `a % b`, from one
division; integers past 64 bits take a wide path.

## Where the code is

`inside/detail/addition.hpp`, `multiplication.hpp` and `division.hpp` hold
the result grids; [internals.md](../internals.md) explains the storage
encoding and the policy cascade behind them.
