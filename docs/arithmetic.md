# Arithmetic

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

Every operation computes its **result grid** at compile time, wide enough for
every value it can produce. The result is a new inside type, so arithmetic
itself never goes out of range. A value can only fail to fit when it is stored
back into a narrower type, and there the type's policy decides
([policies.md](policies.md)).

```cpp
using lvl = inside<{0, 255}>;
lvl a{100}, b{200};

auto sum  = a + b;   // inside<{0, 510}>: 300
auto diff = a - b;   // inside<{-255, 255}>: -100
auto prod = a * b;   // inside<{0, 65025}>: 20000
auto quot = a / b;   // std::expected (b's grid holds 0): the exact 0.5
```

On this page: [scalars](#scalars-need-a-grid) · [division](#division) ·
[rounding](#rounding) · [rounding straight into a type](#rounding-straight-into-a-type-mul_into-and-div_into) ·
[modulo and divmod](#modulo-and-divmod) · [folds and sums](#folds-and-sums) ·
[vector helpers](#vector-helpers-dot-cross-lerp) ·
[compound assignment](#compound-assignment) · [`std::expected`](#when-the-result-is-stdexpected) ·
[quantities](#arithmetic-on-quantities).

## The free functions

`add`, `sub`, `mul`, `div`, `mod` and `divmod` are the operators as functions,
with one of two trailing forms:

| Form | Meaning |
|---|---|
| `op(l, r, policy)` | a flag (`round_floor`, …) or `make_policy<F>()`: how a division rounds |
| `op(l, r, ec)` | an `errc&` that receives a zero divisor |

They take no hooks ([policies.md](policies.md#hooks-on_wrap-on_clamp-on_error)):
the arithmetic cannot fail but for a zero divisor, which stays in the result.

```cpp
errc ec{};
auto q = div(x, y, ec);   // y == 0: ec == errc::division_by_zero, and q holds it
```

## Scalars need a grid

A raw `int` or `double` has no grid, so `b + 1` and `b * 2.5` have no
type-safe result. They do not compile, and the error says what to write
instead: give the scalar a grid.

```cpp
using money = inside<{{0, 1'000'000}, per<100>}, round_nearest>;
money sub{45.07};

auto a = sub + 1;        // ❌ a raw int has no grid
auto c = sub + 1_ins;    // ✅ a one-point inside: the sum widens by 1
auto e = sub + just<1>;  // ✅ the same
auto d = sub * just<2>;  // ✅ inside × inside
```

`1_ins` and `just<N>` give a constant a one-point grid; `inside<{lo, hi}>{n}`
gives a runtime value its range. Compound assignment follows the same rule:
`b += 1_ins`, not `b += 1`. Comparisons with raw scalars are fine
(`b == 5`, `b < 1.5`): they make no new value, and they are exact, so a
`double` is compared at full precision.

### Scaling by an exact fraction

To scale by a factor that is not dyadic, multiply by a one-point inside:

```cpp
money tax{sub * just<8, 100>};   // 8 %, rounded onto the cent by money's policy
auto  half = sub * just<1, 2>;   // exact: notch 1/200
```

Multiplying by a point `c` scales the lattice: the notch becomes `N·|c|`, the
raw number is reused (mirrored for a negative `c`), and the cost is about one
native multiply. Dividing by a point does the same with `N/|c|`, so `x / 5_ins`
on an integer grid has notch 1/5 and is exact and plain. That makes
`c * 9_ins / 5_ins + 32_ins` a ℃ → ℉ conversion without any `expected`
(with units, `point<si::degree_Celsius>(c).point_in(international::degree_Fahrenheit)`
says the same thing).

## Division

`a / b` is exact by default. It returns a plain inside when the divisor's grid
excludes 0, and `std::expected<…, errc>` when it holds 0. Which code runs is
chosen at compile time:

| Path | When | What it does | Result grid |
|---|---|---|---|
| A: Q-format | `snap` is on, and both operands share one Q-format grid (notch 1/N, N ≥ 2, Lower 0) | `(a.raw × N) / b.raw`, rounded by the mode: the textbook fixed-point divide | same notch; Upper widens to `upper(a) / notch(b)` |
| B: integer | `snap` is on, both grids are on whole numbers | a native integer divide, rounded by the mode | `grid(a) / grid(b)`, each limit rounded the same way |
| C: exact | otherwise | the exact fraction | a continuous grid (notch 0) with a denominator bound |
| D: point | the divisor is a nonzero point (`5_ins`), the dividend has a notch | nothing: the dividend's lattice scaled by 1/\|c\| | notch `N/|c|`, integer storage |

Both native paths divide in 32 bits when the operands fit, so `/` and `%` on
small grids run at native speed.

```cpp
using val = inside<{0, 100}>;
val a{7}, b{3};
auto exact = a / b;                            // path C: expected, 7/3
auto quot  = div(a, b, round_toward_zero);   // path B, per call: expected, 2
using fast = inside<{0, 100}, snap>;
auto q     = fast{7} / fast{3};                // path B, from the type: expected, 2
using fp   = inside<{{0, 255}, per<256>}, unsafe>;     // Q8.8 (unsafe includes snap)
auto qfp   = div(fp{200}, fp{3}, round_toward_zero); // path A: raw 17066, about 66.664
```

Path A multiplies before it divides: (51200 × 256) / 768 = 17066, that is
⌊66.667 × 256⌋. Dividing first would give 66 × 256 = 16896 and lose the
fraction.

**Path A widens the interval.** `fp{1} / fp{1}` has the type
`inside<{{0, 65280}, per<256>}>`: the smallest nonzero divisor is 1/256, so 255
divided by it is 65280, and the result type must hold that.

**Path C splits a divisor range around 0.** A divisor grid that straddles 0 is
divided as its positive part `[notch, Upper]` and its negative part
`[Lower, −notch]`, and the results are joined.

### When the result is plain

A divisor grid that excludes 0 (`Lower > 0` or `Upper < 0`) gives a plain
inside, provided the quotient also cannot overflow:

```cpp
using pos = inside<{1, 10}, snap>;
auto d = fast{42} / pos{3};                 // inside: 14
using sval = inside<{-100, 100}>;
auto e = sval{-7} / inside<{1, 100}>{2};    // inside: -3.5 (exact, a denominator ≤ 100)
```

No path can overflow. For path C, x = p/a and y = r/b give x/y = p·b/(a·r),
so the quotient's denominator is at most the dividend's largest denominator
times the divisor's largest numerator. The result grid records that bound
([below](#bounded-continuous-grids)), and the result's fraction is sized for
it, up to `BEMAN_INSIDE_MAX_FRACTION_BITS`. Past that the quotient has no
type: `/` does not compile, and `div_into` rounds the quotient instead.

```cpp
using fine = inside<{{0, 1}, rational{1, umax{1} << 40}}>;
auto f = fine{0.5} / inside<{{1, 2}, rational{1, umax{1} << 40}}>{1.5};
// a plain value: denominators up to 2^81, in a fraction sized for them
```

The one error is `errc::division_by_zero`. Test the result right away, or chain it ([below](#when-the-result-is-stdexpected)), and
store the unwrapped inside.

[`division.cpp`](../examples/division.cpp) shows paths B and C side by side.
`inside_arithmetic.test.cpp` (`InsideArithmeticTest.inside_div_rational_vs_integer_paths`)
and `perf_paths.test.cpp` (the 200/3 → 17066 check) test them.

## Rounding

Storing a value between two notches is a compile-time error when the source
grid can produce one:

```cpp
using coarse = inside<{{0, 10}, 2}>;   // 0, 2, 4, …, 10
using fine   = inside<{{0, 10}, 1}>;   // 0, 1, 2, …, 10
coarse c{0};
fine f{3};
c = f;                                 // ❌ 3 is between notches
```

Two notches are compatible when the target's notch is a whole multiple of
the source's: notch 2 → 1 is always exact, 1 → 2 may round. To round, say so:

```cpp
c.with_snap() = f;                       // this store: toward zero, 2
c.with_snap<round_nearest>() = f;        // this store: to nearest, 4
using coarse_r = inside<{{0, 10}, 2}, round_nearest>;
coarse_r r = f;                          // every store of this type: 4
coarse half(fine x) { return x.with_snap(); }   // as a value: round at the return
```

`.with_snap()` on a temporary, `(a * a).with_snap()`, holds the value, so it is
safe to return. The target's own policy still applies: into a `clamp` type the
range clamps while `with_snap` handles the notch.

**How each mode rounds.** Rounding is in value space, on every grid, also on
one whose points are offset from multiples of the notch (`{{0.5, 10.5}, 1}`):

| Mode | Rounds |
|---|---|
| `snap`, `round_toward_zero` | toward zero: down at or above 0, up below 0 |
| `round_nearest` | to the nearest point; a tie away from zero |
| `round_floor`, `round_ceil` | toward −∞, toward +∞ |
| `round_half_even` | to the nearest point; a tie to the even point, counted from the grid's anchor (…, −1.5, 0.5, 2.5, … on `{{-2.5, 2.5}, 1}`) |

On an offset grid 0 itself can be a tie (between −0.5 and 0.5); it rounds up,
like any value at or above 0. A rounding mode rounds before the range check,
so 9.5 into `inside<{0, 9}, round_floor>` stores 9.

**Rounding a quotient.** On the native paths plain `snap` truncates toward
zero, like C++ `/`. A rounding mode rounds the quotient instead, and `%` stays
consistent with it, so `(a / b) * b + a % b == a` for every mode: under
`round_floor`, `-8 % 3 == 1`; under `round_toward_zero`, `-8 % 3 == -2`.
Without `snap`, `/` is exact and `%` truncates.

For the same fine → coarse conversion as a function, use
`clamp_cast<B, round_nearest>(v)` ([conversions.md](conversions.md#free-function-casts)).

## Rounding straight into a type: `mul_into` and `div_into`

`a * b` and `a / b` give the exact result on its own grid. When that grid is
not wanted, or cannot exist, round the exact result straight into the type you
need:

```cpp
using eth   = inside<{{0, 1'000'000'000}, 1e-18_r}>;      // to the wei
using price = inside<{{0, 100'000'000}, per<100>}>;       // dollars per ETH, to the cent
using usd   = inside<{{0, 1'000'000'000'000'000}, per<100>}, round_nearest>;

usd value = mul_into<usd>(balance, p);          // one rounding, to the cent
auto r    = div_into<usd, round_floor>(a, b);   // Out's policy, plus flags for this call
```

- **`mul_into<Out, F>(a, b)`**: the exact product, rounded once into `Out` by
  `Out`'s policy plus `F`. The `eth × price` grid above would have notch
  10⁻²⁰; `mul_into` never forms it and rounds the exact product once.
- **`div_into<Out, F>(a, b)`**: the exact quotient, rounded once into `Out`.
  It returns `Out` when `b`'s grid excludes 0, else `std::expected<Out, errc>`
  carrying either a zero divisor or a failed store. It never forms a
  continuous quotient, so a quotient that needs a fraction past 64 bits still
  lands on `Out`.

Otherwise a failed store is reported through `Out`'s policy (it throws when
checked).

## Modulo and divmod

`a % b` needs grids on whole numbers (a `static_assert`): a remainder only
means something for whole numbers. It needs no `snap`: the remainder belongs
to an integer quotient, truncated unless the policy names a rounding mode.

```cpp
using val = inside<{0, 100}>;
val a{17}, b{5};
auto r   = a % b;                    // expected (b's grid holds 0): 2
auto r2  = a % inside<{1, 10}>{5};   // inside<{0, 9}>: 2
auto r3  = a % 5_ins;                // inside<{0, 4}>: 2
inside<{0, 9}> d{7};
bool odd = d % 2_ins == 1_ins;       // true (d / 2_ins is 3.5: exact)
```

The result is `[0, m]` for a dividend at or above 0 under truncation, else
`[−m, m]`, where `m` is the largest divisor magnitude minus 1. A remainder
cannot overflow, so the only error is a zero divisor, and the result is an
`expected` only when the divisor's grid holds 0.

`divmod(a, b)` gives both halves from one division, as a
`divmod_result` with `Quotient` and `Remainder`. They are typed as `a / b`
under `snap` and `a % b`, with the same rounding mode. It takes the same
trailing forms as `div`, and every value must fit `imax`:

```cpp
using pos = inside<{1, 10}, snap>;
auto [q, rem] = divmod(val{17}, pos{5});                            // 3, 2
auto [f, m]   = divmod(inside<{-100, 100}>{-8}, pos{3}, round_floor);   // -3, 1
```

## Folds and sums

`add_all` and `mul_all` are `+` and `*` over any number of operands, with the
same widening:

```cpp
using v = inside<{0, 100}>;
v a{10}, b{20}, c{30};
auto s = add_all(a, b, c);                              // inside<{0, 300}>: 60
auto p = mul_all(a, b);                                 // inside<{0, 10000}>: 200
auto capped = clamp_cast<v>(add_all(a, b, c, v{90}));   // 100 (150 clamped)
```

`sum<Target>(range)` adds a whole range with **one** check. A `target += x`
loop checks the running total at every step, a compare and branch that also
blocks vectorization. `sum` adds exactly and checks or clamps only the total,
against `Target`'s policy, and runs within a few percent of a native loop
([performance.md](performance.md)). Wide elements, and totals past 64 bits,
are added in a wide integer and rounded into `Target` once.

```cpp
using elem = inside<{0, 200'000}>;
std::vector<elem> v = /* … */;
auto total   = sum<elem>(v);                         // one range check
auto clipped = sum<inside<{0, 100}, clamp>>(v);      // clamps the total once
```

## Vector helpers: `dot`, `cross`, `lerp`

Three widening helpers for 2-D geometry, built from `+` and `*`, so they
cannot overflow:

```cpp
auto d = dot(ax, ay, bx, by);     // ax·bx + ay·by
auto z = cross(ax, ay, bx, by);   // ax·by − ay·bx: which side of a line
auto p = lerp(a, b, t);           // a + (b − a)·t, t an inside in [0, 1]
```

A squared distance is `dot(dx, dy, dx, dy)`: compare it with a squared radius
to test a hit without a square root.

## Compound assignment

`x op= y` computes `x op y`, then stores the result back into `x`'s type
through its policy. `y` is an inside, an `expected` of one (its error goes
through `x`'s policy), or a `rational`.

```cpp
using pct = inside<{0, 100}, clamp | snap>;
pct x{50};
x += 30_ins;     // 80
x -= 10_ins;     // 70
x *= 2_ins;      // 100 (clamped)
x /= 3_ins;      // 33 (snap truncates 100/3)
x %= 10_ins;     // 3
++x;             // 4
x--;             // 3

using rn = inside<{{0, 100}, per<100>}, round_nearest>;
rn r{0.5};
r += just<1, 4>; // 0.75
r *= 0.5_ins;    // 0.375, stored as 0.38
```

`++` and `--` move one notch: ±1 on a notch-1 grid, ±1/4 on
`inside<{{0, 2}, per<4>}>`. A continuous grid has no `++`.

`x /= 0` goes through the division-by-zero handling: a throw, an `errc`, or
nothing under `unsafe`.

Under a policy with no checks, `+=`, `-=` and `*=` run at the raw type's width:
a loop of byte-wide `b += 1_ins` vectorizes like native `uint8_t`.

## When the result is `std::expected`

An operator returns `std::expected<inside, errc>` in one case: **division
and modulo** by a grid that holds 0 ([above](#division)). `+`, `−` and `×`
are total: the result grid holds every result, and a continuous grid's
denominator bound sizes every fraction
([below](#bounded-continuous-grids)).

Every operator takes `expected` operands and passes the first error on (the
left one when both hold one), so a chain needs no unwrapping:

```cpp
using lvl = inside<{0, 255}>;
lvl a{100}, b{4}, z{0};
auto r1 = a / b + lvl{10};   // expected: 35
auto r2 = a / z + lvl{10};   // expected: errc::division_by_zero
auto r3 = math::sqrt(signed_in{v}) * gain + offset;   // fragment: an expected from the math
```

An operation that fails inside the chain reports its own cause. See
[internals.md](internals.md#7-error-vocabulary) for the full rule. Storing an
`expected` into an inside (`lvl x = a / z;`) reports its error through the
target's policy, as any failed store
([policies.md](policies.md#which-channel-reports-a-failure)).

### Bounded continuous grids

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
auto q = inside<{0, 100}>{50} / inside<{1, 7}>{3};   // a fraction with denominator ≤ 7: 50/3
auto f = q * 9_ins / 5_ins + 32_ins;                 // plain, ≤ 7: 62
auto s = q * q - q;                                  // plain, ≤ 49
```

A continuous grid you declare names its bound with `max_denominator`;
`inside<{{0, 1}, 0}>` does not compile:

```cpp
using thousandths = inside<{{0, 1}, max_denominator{1000}}>;
thousandths a{rational{1, 1000}};
auto p = a * a;                       // plain: ≤ 10^6
a = rational{1, 1001};                // rounding_error: off the grid
```

Any bound works while a value's numerator and denominator together fit
`BEMAN_INSIDE_MAX_FRACTION_BITS` (default 1024). An operation whose exact
result would need more has no result type: the operator is deleted, and its
message names the fix (store an operand into a coarser type first, or round
the result once with `mul_into` or `div_into`). A stored value whose
denominator passes the bound is a `rounding_error` unless the policy rounds.
Then it goes to a neighbouring fraction within the bound (its Farey
neighbours): `round_floor`, `round_ceil` and `snap` pick a side,
`round_nearest` the closer one. So
`inside<{{0, 1}, max_denominator{1000}}, round_nearest>{0.1}` holds exactly
1/10, although the `double` 0.1 is not 1/10.

## Arithmetic on quantities

With `<beman/inside/units.hpp>` every operation above also takes quantities
([units.md](units.md)). `+`, `-`, `%` and the comparisons run in the two
units' common unit; `*` and `/` multiply the units. Each function converts its
operands exactly into the target's unit, then runs on the numbers, so it still
rounds once and checks once:

```cpp
constexpr auto ms = si::milli(si::second);
quantity<ms, inside<{{0, 100}, per<10>}>> lat{rational{955, 10}};
auto bin = div_into<inside<{0, 9}, clamp | round_floor>>(lat, just<10> * ms);   // 9: a plain number

using usd_t = quantity<USD, inside<{{0, 1'000'000}, per<100>}, round_nearest>>;  // USD: a unit of your own
usd_t tax   = mul_into<usd_t>(usd_t{45.07}, just<8> * percent);                 // 3.61 USD: one rounding

quantity<si::second, inside<{0, 86399}>> t{3725};
auto [h, rest] = divmod(t, just<1> * si::hour);   // 1, 125 s: the remainder keeps its unit
```

- `mul_into<Out>` / `div_into<Out>`: `Out` is a quantity type, or a plain
  inside when the result is dimensionless (ms / ms). The factor between the
  units is folded into the exact product.
- `add_all`, `mul_all`, `sum<Q>(range)`: `sum<Q>` takes quantities in any unit
  that converts to Q's.
- `divmod(a, b)`: in the common unit; the quotient is a number, the remainder
  a quantity.
- `dot`, `cross`, `lerp`: `dot` of two px vectors is in px²; `lerp`'s t is a
  number.
- `++q` and `q--` move one unit.
- An `expected` quantity, from a division by a grid that holds 0, chains
  through `+ - * / %` like an `expected` inside.

[`money.cpp`](../examples/money.cpp), [`wei.cpp`](../examples/wei.cpp) and
[`histogram.cpp`](../examples/histogram.cpp) show these on real data.
