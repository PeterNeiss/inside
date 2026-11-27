# Arithmetic

Every operation computes its **result type** at compile time, wide enough for
every value it can produce. So arithmetic never goes out of range; only a
store into a narrower type can fail, and there the type's
[policy](policies.md) decides.

```cpp
using small = id<"[0..255]">;
small a{100}, b{200};

auto s = a + b;   // id<"[0..510]">: 300
auto d = a - b;   // id<"[-255..255]">: -100
auto p = a * b;   // id<"[0..65025]">: 20000
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

A runtime value gets its range from a type: `a + small{n}`. Comparisons need no
grid: `a == 5` and `a < 1.5_ins` are exact. A `double` does not compare with an
inside, because 0.1 is not 1/10 in binary.

Multiplying by a constant scales by an exact fraction, and a store rounds the
result where the type allows it. Money is a quantity in a currency of your own
([Units](units.md)):

```cpp
inline constexpr auto USD = base_unit<"USD", "dollar">;
using money = id<"[0..1'000'000] 1/100 [USD] round_half_away">;
money subtotal{45.07, USD};
money tax{subtotal * just<8, 100>};   // 3.61 USD: 8 %, rounded once to the cent
```

## Division

`a / b` is **exact**: 7 / 3 is the fraction 7/3, stored on a
[continuous grid](grids.md#continuous-grids). Whether the result is a plain
value depends on the divisor's grid:

```cpp
using val = id<"[0..100]">;
val x{7};
auto e = x / val{3};                // std::expected: val's grid holds 0
auto f = x / id<"[1..10]">{3};      // a plain value, 7/3: this divisor cannot be 0
auto g = x / 5_ins;                 // a plain value, 1.4, on notch 1/5
```

A `std::expected` result carries `error::division_by_zero` when the divisor
was 0. Check it before you store it.

**Integer division.** `/` is always the exact quotient, whatever the
operands' types round. To divide on the grid and round, name the mode at the
call, or store into a type that rounds:

```cpp
auto i = div(x, val{3}, round_toward_zero);      // std::expected: 2
using down = id<"[0..100] round_floor">;
down j{7};
j /= id<"[1..10]">{2};                           // 3: a store into a type that rounds
down k{x / id<"[1..10]">{2}};                    // 3: the exact 3.5, rounded by the store
```

## Round once: `mul<Out>` and `div<Out>`

`a * b` and `a / b` give the exact result on its own grid. When you want the
result on a type of your choice, round the exact result straight into it,
once:

```cpp
inline constexpr auto ETH = base_unit<"ETH", "ether">;   // USD as above
using ether   = id<"[0..1'000'000'000] 1e-18 [ETH]">;   // to the wei
using price   = id<"[0..100'000'000] 1/100 [USD/ETH]">;
using dollars = id<"[0..100'000'000'000'000'000] 1/100 [USD] round_half_away">;   // holds every product

ether   balance{just<15, 10>, ETH};
price   rate{just<345678, 100>, USD / ETH};
dollars value = mul<dollars>(balance, rate);                   // 5185.17 USD
auto    share = div<id<"[0..100] round_floor">>(val{50}, id<"[1..10]">{3});   // 16
```

`mul<Out>(a, b)` and `div<Out>(a, b)` round by `Out`'s policy; a flag after
the operands, `mul<Out>(a, b, round_floor)`, replaces its rounding mode for
the call. The exact product `balance × rate` above would need a notch of
10⁻²⁰; `mul<Out>` never forms it. `div<Out>` returns a `std::expected` when
the divisor's grid holds 0.

## Remainder and `divmod`

`%` works on any two lattices, whole numbers or fractions:

```cpp
val n{17};
auto r1 = n % 5_ins;                  // id<"[0..4]">: 2
auto r2 = n % id<"[1..10]">{5};       // id<"[0..9]">: 2
auto r3 = n % val{5};                 // std::expected: val's grid holds 0
auto [q, rest] = divmod(n, id<"[1..10]">{5});   // 3 and 2, from one division
auto r4 = id<"[0..8] 1/2">{7.5} % id<"[1..4] 1/3">{just<4, 3>};  // 7.5 − 5·4/3 = 5/6
```

The remainder belongs to the rounded quotient: under `round_floor`,
`divmod(-8, 3)` is −3 and 1. On fractional grids it lies on the lattice of
the gcd of their steps (1/6 for halves and thirds), and the quotient is a
whole number; a continuous grid has no lattice, so `%` does not take one.

## Sums of many values

`add_all` and `mul_all` take any number of operands, with the same widening.
`sum<Target>(range)` adds a whole range and checks only the total, once,
against `Target`'s policy:

```cpp
using elem = id<"[0..200'000]">;
std::vector<elem> v{elem{100'000}, elem{150'000}, elem{20}};
auto total   = sum<id<"[0..1'000'000]">>(v);     // 250020
auto clipped = sum<id<"[0..100] clamp">>(v);     // 100: the total, clamped once
```

A `total += x` loop checks at every step, which also stops the compiler from
vectorizing it; `sum` runs at about half the speed of a native loop.

Geometry needs no helpers: `ax * bx + ay * by` and `ax * by - ay * bx` widen
the same way, so a dot or cross product cannot overflow. `lerp(a, b, t)`
with `t` in [0, 1] lies between `a` and `b`, and its type says so.

## Compound assignment

`x op= y` computes `x op y` and stores the result back through `x`'s policy:

```cpp
using bounded = id<"[0..100] clamp|round_toward_zero">;
bounded p{50};
p += 30;         // 80
p *= 2;          // 100: clamped
p /= 3_ins;      // 33: 100/3, rounded toward zero
++p;             // 34: one notch up
```

`+=`, `-=` and `*=` also take a builtin integer, a literal or a variable:
`p += 30`, `p *= 2`, `p -= n`. The result has a type, `p`'s, so the integer
needs no grid of its own; it counts as an inside on the whole range of its
type, and the store back follows the same rules (a `clamp` or `wrap` type
takes it, a checked one names a channel: `x.on_error(ec) += n`). Everything
else on the right is an inside (`p += 0.5_ins`, `p /= 3_ins`), never a
`double`, and a division takes no builtin integer, whose
range holds 0. `++` and `--` move one notch: ±1/4 on
`id<"[0..2] 1/4">`.

## Results that are `std::expected`

Only `/` and `%` by a grid that holds 0 return a `std::expected`. Every
operator accepts one and passes the first error on, so a chain needs no
unwrapping:

```cpp
small zero{0};
auto r1 = a / id<"[1..255]">{4} + small{10};     // a plain value: 35
auto r2 = a / zero + small{10};                  // std::expected: error::division_by_zero
```

To store the end of a chain, check it and store `*r`, or call
`T::try_make(r)`, which passes the error on.

## Details

- [Exact arithmetic](../advanced/exact-arithmetic.md): how result grids and
  policies are formed, the division paths, when a quotient is plain, and the
  bound of continuous grids.
- [Arithmetic on quantities](../advanced/units-model.md#insides-algorithms-on-quantities).
- [Performance](../performance.md): the measured cost of each operation.
