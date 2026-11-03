# Arithmetic

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

Every operation computes its **result type** at compile time, wide enough for
every value it can produce. So arithmetic never goes out of range; only a
store into a narrower type can fail, and there the type's
[policy](policies.md) decides.

```cpp
using lvl = inside<{0, 255}>;
lvl a{100}, b{200};

auto s = a + b;   // inside<{0, 510}>: 300
auto d = a - b;   // inside<{-255, 255}>: -100
auto p = a * b;   // inside<{0, 65025}>: 20000
auto q = a / b;   // std::expected (b's grid holds 0): exactly 1/2
```

On integer grids `+ - *` compile to the native integer instructions, with no
check at run time.

## Scalars need a grid

A plain `int` or `double` says nothing about its range, so `a + 1` and
`a * 2.5` do not compile. Give the constant a grid:

```cpp
auto c = a + 1_ins;        // a one-point inside: the sum widens by 1
auto d = a * just<5, 2>;   // the exact factor 5/2
// auto e = a + 1;         // does not compile; the message says what to write
```

A runtime value gets its range from a type: `a + lvl{n}`. Comparisons need no
grid: `a == 5` and `a < 1.5_r` are exact. A `double` does not compare with an
inside, because 0.1 is not 1/10 in binary.

Multiplying by a constant scales by an exact fraction, and a store rounds the
result where the type allows it:

```cpp
using money = inside<{{0, 1'000'000}, per<100>}, round_nearest>;
money subtotal{45.07};
money tax{subtotal * just<8, 100>};   // 3.61: 8 %, rounded once to the cent
```

## Division

`a / b` is **exact**: 7 / 3 is the fraction 7/3, stored on a
[continuous grid](grids.md#continuous-grids). Whether the result is a plain
value depends on the divisor's grid:

```cpp
using val = inside<{0, 100}>;
val x{7};
auto e = x / val{3};                // std::expected: val's grid holds 0
auto f = x / inside<{1, 10}>{3};    // a plain value, 7/3: this divisor cannot be 0
auto g = x / 5_ins;                 // a plain value, 1.4, on notch 1/5
```

A `std::expected` result carries `errc::division_by_zero` when the divisor
was 0. Check it before you store it.

**Integer division.** A rounding mode on either operand, or on the call,
makes `/` divide on the grid and round:

```cpp
auto i = div(x, val{3}, round_toward_zero);      // std::expected: 2
using down = inside<{0, 100}, round_floor>;
auto j = down{7} / inside<{1, 10}>{2};           // 3
```

## Round once: `mul_into` and `div_into`

`a * b` and `a / b` give the exact result on its own grid. When you want the
result on a type of your choice, round the exact result straight into it,
once:

```cpp
using eth   = inside<{{0, 1'000'000'000}, 1e-18_r}>;            // to the wei
using price = inside<{{0, 100'000'000}, per<100>}>;             // dollars per ETH
using usd   = inside<{{0, 1'000'000'000'000'000}, per<100>}, round_nearest>;

eth   balance{rational{15, 10}};
price rate{rational{345678, 100}};
usd   value = mul_into<usd>(balance, rate);                     // 5185.17
auto  share = div_into<inside<{0, 100}, round_floor>>(val{50}, inside<{1, 10}>{3});   // 16
```

`mul_into<Out, F>(a, b)` and `div_into<Out, F>(a, b)` round by `Out`'s policy
plus the flags `F`. The exact product `balance × rate` above would need a
notch of 10⁻²⁰; `mul_into` never forms it. `div_into` returns a
`std::expected` when the divisor's grid holds 0.

## Remainder and `divmod`

`%` works on grids of whole numbers:

```cpp
val n{17};
auto r1 = n % 5_ins;                  // inside<{0, 4}>: 2
auto r2 = n % inside<{1, 10}>{5};     // inside<{0, 9}>: 2
auto r3 = n % val{5};                 // std::expected: val's grid holds 0
auto [q, rest] = divmod(n, inside<{1, 10}>{5});   // 3 and 2, from one division
```

The remainder belongs to the rounded quotient: under `round_floor`,
`divmod(-8, 3)` is −3 and 1.

## Sums of many values

`add_all` and `mul_all` take any number of operands, with the same widening.
`sum<Target>(range)` adds a whole range and checks only the total, once,
against `Target`'s policy:

```cpp
using elem = inside<{0, 200'000}>;
std::vector<elem> v{elem{100'000}, elem{150'000}, elem{20}};
auto total   = sum<inside<{0, 1'000'000}>>(v);   // 250020
auto clipped = sum<inside<{0, 100}, clamp>>(v);  // 100: the total, clamped once
```

A `total += x` loop checks at every step, which also stops the compiler from
vectorizing it; `sum` runs at about half the speed of a native loop.

For 2-D geometry, `dot(ax, ay, bx, by)`, `cross(ax, ay, bx, by)` and
`lerp(a, b, t)` widen the same way. A squared distance is
`dot(dx, dy, dx, dy)`.

## Compound assignment

`x op= y` computes `x op y` and stores the result back through `x`'s policy:

```cpp
using pct = inside<{0, 100}, clamp | round_toward_zero>;
pct p{50};
p += 30_ins;     // 80
p *= 2_ins;      // 100: clamped
p /= 3_ins;      // 33: 100/3, rounded toward zero
++p;             // 34: one notch up
```

The right-hand side is an inside or a `rational` (`p += 0.5_r`), never a
plain `int` or `double`. `++` and `--` move one notch: ±1/4 on
`inside<{{0, 2}, per<4>}>`.

## Results that are `std::expected`

Only `/` and `%` by a grid that holds 0 return a `std::expected`. Every
operator accepts one and passes the first error on, so a chain needs no
unwrapping:

```cpp
lvl zero{0};
auto r1 = a / inside<{1, 255}>{4} + lvl{10};   // a plain value: 35
auto r2 = a / zero + lvl{10};                  // std::expected: errc::division_by_zero
```

To store the end of a chain, check it and store `*r`, or call
`T::try_make(r)`, which passes the error on.

## Details

- [Exact arithmetic](../advanced/exact-arithmetic.md): how result grids and
  policies are formed, the division paths, when a quotient is plain, and the
  bound of continuous grids.
- [Arithmetic on quantities](../advanced/units-model.md#insides-algorithms-on-quantities).
- [Performance](../performance.md): the measured cost of each operation.
