---
title: "`inside`: numbers whose range and step size are part of the type"
document: DxxxxR0
date: today
audience:
  - SG6 (Numerics)
  - LEWG Incubator
author:
  - name: Peter Neiss
    email: <https://github.com/PeterNeiss>
toc: true
toc-depth: 2
---

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

# Abstract

We propose a class template `inside<grid, policy>` for numbers whose interval
and step size (*notch*, an exact rational) are compile-time properties of the
type. Arithmetic computes its result grid at compile time, so `+`, `-` and `*`
cannot overflow. Division is exact unless the type names a rounding, and it
returns a failure value only where the operand grids allow one. What a store
into a narrower grid does (check, clamp, wrap or round) is part of the
declaration.

The proposal has three layers. The first is the number: the type, its grid,
the policies and the operators. The second is quantities:
`quantity<unit, inside>`, which follows the model of mp-units [@P3045R9] with
an `inside` as the only representation. Because a grid is a compile-time
value, a unit conversion rescales the grid and keeps the stored integer, so it
is exact, cannot fail, and costs nothing at run time. The third is math: the
functions of `<cmath>` on an `inside`, each returning the correctly rounded
point of its output grid. The result is the same at compile time and at run
time, on every platform, with or without an FPU. `sin(x)` deduces its output
grid from the input; `sin_into<Out>(x)` names it. Text conversion and the
convenience functions of the reference implementation are left to later
papers ([Future work](#future-work)).

The reference implementation is [beman.inside
v0.4.0](https://github.com/PeterNeiss/inside/tree/v0.4.0). A long-form
motivation with compiled exhibits is [*int considered
harmful*](https://github.com/PeterNeiss/inside/blob/v0.4.0/docs/int-considered-harmful.pdf)
(the *paper* below).

# Motivation

The built-in integer types carry no range, no unit and no failure value. The
paper reproduces eleven consequences with compiled programs:

- signed overflow that lets the optimiser delete range checks, and in one
  exhibit the whole of `main`;
- `-INT_MIN` and `abs(INT_MIN)` are negative, and `INT_MIN / -1` raises
  `SIGFPE`;
- the signed/unsigned comparison that iterates over an empty vector;
- promotion and silent narrowing (the Ariane 5 shape);
- truncating division, the midpoint bug, and cents added to dollars.

`double` replaces these with a different set: inexact decimals, accumulating
drift, non-associativity and `NaN`.

Each existing defence recovers part of the missing information.

- Sanitizers find overflow at runtime, in instrumented builds.
- `-fwrapv` makes the wrong answer reliable.
- Checked-integer libraries detect overflow, but not units, rounding, or a range
  narrower than the underlying type.
- C++26 saturation arithmetic ([@P0543R3]) picks one edge policy for the full
  range of a built-in type.
- Units libraries catch the dimension error (the Mars Climate Orbiter's
  pound-force seconds read as newton seconds), but on an `int` or `double`
  representation a conversion can overflow or round.

What each of them lacks is the same: the range a value may occupy, its scale,
the rounding rule and the edge behaviour. `inside` records all four in the type,
where the compiler can act on them, and the unit layer adds the fifth: what the
number counts.

The same holds for functions. `std::sin` on a `double` is not required to be
correctly rounded, so its result can differ between platforms, between
compile time and run time, and between library versions, and its precision is
the `double`'s, whatever the program needs. The math layer takes the precision
from the output's grid and fixes the result to one value: the nearest point,
or the one the output's rounding mode selects.

# A tour

The listings use the reference implementation's namespace `beman::inside`.
Each one is a region of a program that compiles and checks its own results.

**The range is part of the type, and arithmetic widens it.**

<!-- listing: range -->
```cpp
using percent = inside<{0, 100}>;   // the integers 0..100, in one byte

percent a = 42, b = 58;
auto    sum  = a + b;               // inside<{0, 200}>
auto    diff = a - b;               // inside<{-100, 100}>
auto    neg  = -a;                  // inside<{-100, 0}>: no -INT_MIN
static_assert(std::is_same_v<decltype(sum), inside<{0, 200}>>);
static_assert(sizeof(percent) == 1);

percent c = sum - just<100>;        // a checked store: 0..100 fits
auto    bad = percent::try_make(sum); // expected<percent, errc>
```

**The notch is an exact rational, and the rounding is named in the type.**

<!-- listing: money -->
```cpp
using cents = inside<{{0, 1'000'000}, per<100>}, round_nearest>;

cents price = 0.1_ins, tax = 0.2_ins;   // exactly 1/10 and 2/10
auto  total = price + tax;              // exactly 3/10 — no 0.30000000000000004
static_assert(notch_of<decltype(total)> == per<100>);

cents share = total / 3_ins;            // 1/10 exactly
cents third = cents{1} / 3_ins;         // 1/3 rounds to nearest: 0.33
```

**The policy decides what a narrowing store does.**

<!-- listing: wrap -->
```cpp
using degrees = inside<{0, 359}, wrap>;   // modular
using level   = inside<{0, 10}, clamp>;   // saturating
using count   = inside<{0, 10}>;          // checked (the default)

degrees heading = 350;
heading += 20_ins;                        // 370 wraps to 10
level   volume  = 9;
volume += 5_ins;                          // 14 clamps to 10

auto n = count::try_make(14);             // unexpected(errc::overflow)
// count m = 14;                          // a checked store: throws inside_error
```

**The lowest value need not be a multiple of the notch.**

<!-- listing: offset -->
```cpp
using centre = inside<{{0.5, 255.5}, 1}>;    // 0.5, 1.5, ..., 255.5
static_assert(sizeof(centre) == 1);          // 256 points: one byte

centre x = 2.5_ins, y = 0.5_ins;
auto   s = x + y;                            // inside<{1, 511}>: whole numbers again
auto   p = x * y;                            // 0.25, 0.75, ...: Lower 1/4, notch 1/2
static_assert(std::is_same_v<decltype(s), inside<{1, 511}>>);
static_assert(lower_of<decltype(p)> == per<4> && notch_of<decltype(p)> == per<2>);
```

**Division is exact, and fallible only when the divisor may be zero.**

<!-- listing: division -->
```cpp
using num  = inside<{0, 100}>;
using any  = inside<{0, 10}>;              // may be zero
using some = inside<{1, 10}>;              // never zero

auto q = num{7} / some{3};                 // exactly 7/3, a plain value
auto r = num{7} / any{3};                  // std::expected<..., errc>
auto z = num{7} / any{0};                  // unexpected(errc::division_by_zero)
static_assert(!std::is_same_v<decltype(q), decltype(r)>);

using whole = inside<{0, 100}, round_floor>;
auto f = whole{7} / some{3};               // 2: rounding named in the type
auto m = whole{7} % some{3};               // 1, consistent: 2·3 + 1 == 7
```

**A quantity carries its unit, and a conversion rescales the grid.**

<!-- listing: units -->
```cpp
quantity<si::kilo(si::metre), inside<{{0, 500}, per<1000>}>> trip{12.5};
auto m = trip.in(si::metre);                     // 12500 m: the same raw, grid × 1000
static_assert(grid_of<decltype(m)::rep> == grid{0, 500'000});

auto room = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
auto k    = room.point_for(si::absolute_zero).in(si::kelvin); // 293.15 K, exactly

quantity<si::hertz, inside<{0, 1000}>>     f{50};
quantity<si::becquerel, inside<{0, 1000}>> a{50};
static_assert(!addable<decltype(f), decltype(a)>); // both 1/s, but not one kind

quantity<isq::width[si::metre], inside<{1, 10}>>  w{3};
quantity<isq::height[si::metre], inside<{1, 10}>> h{4};
quantity<isq::area[square(si::metre)], inside<{1, 100}>> area = w * h;
inside<{0, 10'000}, round_nearest> ratio = trip / w; // km / m: a plain number
```

**A function rounds once, onto the grid it returns.**

<!-- listing: math -->
```cpp
using angle = inside<{{-8, 8}, per<16384>}, round_nearest>;
auto s = math::sin(angle{1});                    // 0.84149169921875, the nearest 1/16384
static_assert(grid_of<decltype(s)> == grid{{-1, 1}, per<16384>});
auto c = math::cos_into<math::unit_amplitude<256>>(angle{1}); // 0.5390625: a coarser output, named

using level = inside<{{-4, 4}, per<256>}, round_nearest>;
auto r = math::sqrt(level{2.25});                // expected: the grid holds negative values
auto n = math::sqrt(level{-1});                  // unexpected(errc::domain_error)

using down = inside<{{-1, 1}, per<16384>}, round_floor>;
auto h = units::cos_into<down>(quantity<si::degree, inside<{0, 360}>>{120}); // exactly -1/2, even rounding down
```

# Design

## The grid

A grid is an interval `[lower, upper]` and a notch `n ≥ 0`. Its values are
`lower + k·n` for `k = 0, 1, …` up to `upper`, so `n` must divide
`upper − lower`. A notch of zero gives a continuous grid: every rational in the
interval. A grid with `lower == upper` is a point. Grid numbers are exact
rationals: `per<100>` is 1/100. A floating-point literal is taken at its exact
binary value; when the notch is derived from such a limit, one that is not a
short binary fraction is rejected (`0.1` is not 1/10).

`lower` need not be a multiple of `n`. Pixel centres `{{0.5, 255.5}, 1}` and
sample midpoints are grids. Result grids track such lattices exactly.

A grid is always closed. An open limit is only a spelling that moves the limit
in by one notch at construction: `{0, 360, bounds::half_open}` is the grid
`{0, 359}`, and the same type.

## Storage

Storage follows from the grid alone: there are no storage flags. The reference
implementation stores the narrowest integer that numbers every point, or holds
the value itself for a whole-number grid when that is no wider. A continuous
grid stores an exact fraction. We propose that the storage be unspecified, with
this as recommended practice. Equal grids give the same type, so values of the
same grid always share a representation.

## Arithmetic widens

The result of `a + b`, `a - b`, `a * b` and `-a` is an `inside` whose grid holds
every possible result. `[0, 100] + [0, 100]` is `[0, 200]`, and negation mirrors
the interval. No operation produces a value outside its result type, so these
operators need no overflow check: overflow cannot happen. A result grid that
cannot be represented is a compile-time error (with 64-bit grid numbers, see
[Grids past 64 bits](#grids-past-64-bits)).

## Division

`a / b` is exact by default. A point divisor `c` scales the dividend's lattice
by `1/|c|`; any other quotient lies on a continuous grid whose denominators are
bounded — x = p/a over y = r/b is p·b/(a·r), so they are at most the dividend's
largest denominator times the divisor's largest numerator. That bound travels
with the type through `+`, `-` and `*`, so exact arithmetic on quotients stays
plain and its precision is known at compile time. When either operand's policy
names a rounding, an integer division rounds onto the integers, and a division
of two values on the same notch `1/K` rounds onto that notch. `a % b` requires
integer grids; its remainder belongs to the quotient truncated, or rounded by
the policy's mode. Either operator returns `std::expected<inside, errc>` when the
divisor's grid holds zero, and a plain `inside` when it does not.

## Policies

A policy says what a store into a grid does with a value that is not on it.

| Policy | Out of range | Off the notch |
|---|---|---|
| `checked` (default) | reports `errc::overflow` | reports `errc::rounding_error` |
| `clamp` | the nearest limit | (as checked) |
| `wrap` | modulo the interval | (as checked) |
| `snap` (= `round_toward_zero`), `round_nearest`, `round_floor`, `round_ceil`, `round_half_even` | (as checked) | rounds |
| `unsafe` | precondition | truncates |

Flags combine (`clamp | round_nearest`). A store that can never succeed (a
source interval disjoint from the target without `clamp` or `wrap`, or a source
notch that does not land on the target's without a rounding) is ill-formed.

## No bare scalars

`a + 1` is ill-formed, with a diagnostic that names the fix. `1` has no grid,
so the result grid cannot be computed. Constants are written `just<1>` or
`1_ins`, which are point grids. An arithmetic value enters an `inside` only
through a store, which its policy checks.

## Checked by default, `std::expected` only at boundaries

Every policy range-checks a narrowing store unless it says `unsafe`. The checks
that remain are the ones the grids cannot discharge. On a chain of operations
the library therefore emits fewer checks than a hand-written checked integer,
and is 21% faster than a native checked type in the paper's benchmark ("Where
are the checks?").

Fallible results are values only where failure is possible at runtime:
`try_make`, `to<T>()`, and division by a divisor whose grid holds zero. A total
operation returns a plain value, so the type says whether failure is possible.
A failed checked store throws `inside_error`, which carries the `errc`.

Fallible results chain. The operators also accept a `std::expected<inside,
errc>` operand, so `a / b * gain + offset` stays one `expected` from end to
end. The first error passes through with its cause, and an operation that fails
later in the chain reports its own.

## Grids past 64 bits

With C++26 static reflection (`std::define_static_array`) grid numbers have no
size limit, and equal grids remain the same type: a grid from 0 to 10³⁰ in
steps of 1 stores a two-limb integer. Without reflection, grid numbers are
64-bit rationals and a grid past them is a compile-time error.

## Quantities

The second layer puts a unit on the number. `quantity<R, Rep>` is a number
`Rep`, always an `inside`, of a *reference* `R`: a unit, optionally with a
quantity kind (`isq::height[si::metre]`). The model is that of mp-units
[@P3045R9]: dimensions, units with exact magnitudes, the ISQ tree of quantity
kinds, and quantity points with origins. What changes is that the number is a
grid.

- **A conversion rescales the grid, not the number.** km to m multiplies the
  interval and the notch by 1000 and keeps the stored integer. A conversion is
  therefore exact and total, and at run time it is a copy.
- **A sum runs in the common unit,** whose magnitude is the gcd of the two
  magnitudes. Both sides are rescaled exactly, then the `inside` addition runs,
  so a valid mixed-unit sum cannot overflow and its grid is the tight hull of
  the operands. `*` and `/` multiply the units and the kinds, and the numbers
  take `inside`'s result grids, including `expected` for a divisor whose grid
  holds 0.
- **Kinds separate quantities of one dimension.** A height is a length, and
  width · height is an area, but hertz and becquerel (both s⁻¹) do not add. A
  quotient of dimension one is a plain `inside`.
- **Points change origin exactly.** A temperature point is counted from an
  origin, and 20 ℃ becomes 293.15 K by an exact shift of the grid.
- **π is never approximated.** A magnitude is an exact rational times a power
  of π. A conversion with a factor of π (degree to radian) has no exact
  rescale, so it is ill-formed until it is rounded once onto a declared grid
  by the math layer's `value_cast` ([Correctly rounded
  math](#correctly-rounded-math)).
- **No bare scalars here either.** `42 * si::metre` and `q * 2` are ill-formed.
  A quantity is made from a number in its unit
  (`quantity<si::metre, inside<{0, 100}>>{42}`) or from an `inside` times a
  unit (`just<42> * si::metre`).

Dimensions, magnitudes, units and kinds are structural values, combined by
`consteval` functions. They are not type lists that the compiler must sort, so
m·s and s·m are one unit. Users define their own units, kinds, origins and up
to four base dimensions of their own (currency, information, pixels) as
`inline constexpr` objects. An entry is identified by its content, not by its
address.

A unit operation that cannot happen is a constraint failure, so
`requires { a + b; }` is false for a length and a duration. A call that is
written anyway names its reason as a deleted overload, for example
`units: adding or comparing quantities of different kinds`.

## Correctly rounded math

The third layer is `<cmath>` on an `inside`. A function's result is a point of
its output grid, and the contract is that it is the *correctly rounded* point:
the one the output's rounding mode selects from the true value. Because the
output grid is part of the type, so is the precision. A coarse output is cheap,
a fine one costs more, and there is no ceiling: an output notch of 2⁻¹⁰⁰ gets a
correctly rounded result too. The result does not depend on how it was
computed, so it is the same at compile time and at run time, on every
platform, with or without an FPU, and under any compiler flags.

- **Two forms.** `fn_into<Out>(x)` names the output. `fn(x)` deduces it: the
  input's notch and policy, and the function's range over the input's
  interval, computed at compile time and rounded outward. A few ranges are
  fixed instead: `sin` and `cos` give [−1, 1], `tan` [−1024, 1024] and `atan2`
  [−π, π]. `hypot` takes the gcd of both notches, `pow` the base's notch and
  `floor` & co. notch 1.
- **Rounding needs permission.** A transcendental result is never on a grid by
  accident, so `Out` (or, for `fn(x)`, the input) must name a rounding mode,
  as a store that rounds must. A result past `Out`'s interval goes through
  `Out`'s policy: `clamp` saturates, `wrap` folds the true value, and the
  default reports.
- **Errors follow the number layer.** A domain limit that the input's *type*
  violates (`log` of a grid that reaches 0, `asin` of a grid past ±1) is
  ill-formed. A failure that depends on the *value* returns
  `expected<Out, errc>`: `sqrt` of a grid that crosses 0, `tan` near a pole,
  and `pow`. A total function returns a plain `inside`.
- **The grid operations are exact.** `abs`, `sign`, `copysign`, `floor`,
  `ceil`, `round`, `trunc`, `fmod` and `pown<E>` need no rounding. Their
  outputs are deduced exactly (`abs` of `[−3, 5]` is `[0, 5]`), and `fmod` is
  `expected` only when the divisor's grid holds 0, as `%` is.
- **Exact cases are exact.** `exp(0)`, `log(1)`, `sqrt(9/4)`, `log2(2^k)` and a
  power with an integer exponent land on the grid point itself under every
  rounding mode, as do the rational values of `sin` and `cos` (0, ±1/2, ±1) of
  an angle in degrees or turns.
- **On quantities the unit follows the function.** `sqrt` of an area is a
  length, `atan2` and `hypot` take two quantities of one dimension, inverse
  trigonometry returns radians, and `exp`, `log` and `pow` take a
  dimensionless quantity (the decibel's 10^(L / 20 dB)). An angle in a unit
  with a factor of π (degrees, turns) is reduced exactly by `sin_into` and
  `cos_into`, so only a remainder of at most π/4 is approximated, and
  `value_cast<si::radian, Out>(q)` rounds a degree-to-radian conversion once.

How a result is found is recommended practice, not specification. The
reference implementation evaluates in fixed point with an error bound and
doubles the precision while the interval straddles a rounding boundary (Ziv's
strategy). It reads a precomputed table for small inputs and uses
floating-point kernels with proved bounds where an FPU is present, behind an
integer path that always applies and gives the same results.

# Proposed interface

The names are the reference implementation's (see [Open
questions](#open-questions)). `@*grid-number*@` is exposition-only: an exact
rational, unbounded where static reflection is available and 64-bit otherwise.
`@*number*@` is an arithmetic type or a `@*grid-number*@`.

## The number

```cpp
namespace std {
  // grids
  template<uintmax_t D> constexpr @*grid-number*@ per;         // 1/D
  struct interval {
    @*grid-number*@ Lower, Upper;                                // Lower <= Upper
  };
  enum class bounds { closed, half_open, open };               // a spelling: grids stay closed
  struct max_denominator {
    @*grid-number*@ Value;                                       // a positive integer
    constexpr explicit max_denominator(@*number*@ v);
  };
  struct grid {
    interval Interval;
    @*grid-number*@ Notch;                                       // >= 0, divides Upper - Lower
    @*grid-number*@ MaxDenominator;                              // continuous grids: 0 or a bound
    constexpr grid(@*number*@ lower, @*number*@ upper, @*number*@ notch);
    constexpr grid(@*number*@ lower, @*number*@ upper);          // notch gcd(1, lower, upper)
    constexpr grid(@*number*@ lower, @*number*@ upper, @*number*@ notch, bounds);
    constexpr grid(@*number*@ lower, @*number*@ upper, bounds);
    constexpr grid(@*number*@ point);                            // notch 0, lower == upper
    constexpr grid(interval, @*grid-number*@ notch);
    constexpr grid(interval, max_denominator);                  // continuous, bounded
  };

  // policies
  using policy_flag = @*unsigned integer type*@;
  inline constexpr policy_flag none, checked, clamp, wrap, snap, round_toward_zero,
                               round_nearest, round_floor, round_ceil,
                               round_half_even, unsafe;

  // errors
  enum class errc { domain_error = 1, division_by_zero, overflow,
                    rounding_error, not_finite, invalid_format };
  struct inside_error : runtime_error { errc Code; };

  // the type
  template<grid G, policy_flag P = checked>
  struct inside {
    constexpr inside() = default;                         // trivial
    template<class A> constexpr inside(A value);          // A arithmetic or inside: a store by P
    template<policy_flag F = none, class A>               // the store by P | F
      static constexpr expected<inside, errc> try_make(A value);
    template<class T> constexpr expected<T, errc> to() const;

    constexpr inside<-G, P> operator-() const;
    constexpr inside& operator++();                       // one notch
    constexpr inside  operator++(int);
    constexpr inside& operator--();
    constexpr inside  operator--(int);
    template<class R> constexpr inside& operator+=(const R&); // also -= *= /= %=: a store by P
  };

  // observers
  template<class B> constexpr grid grid_of;
  template<class B> constexpr policy_flag flags_of;
  template<class B> constexpr @*grid-number*@ lower_of, upper_of, notch_of;

  // constants
  template<auto N, auto D = 1> constexpr inside<grid{N/D}> just; // the point N/D
  template<char... Cs> constexpr auto operator""_ins();      // a point grid; suffix to be renamed

  // operators (L, R are inside types, or expected<inside, errc>)
  template<class L, class R> constexpr auto operator+(L, R);   // widened result grid
  template<class L, class R> constexpr auto operator-(L, R);
  template<class L, class R> constexpr auto operator*(L, R);
  template<class L, class R> constexpr auto operator/(L, R);   // inside or expected
  template<class L, class R> constexpr auto operator%(L, R);
  template<class L, class R> constexpr bool operator==(const L&, const R&);
  template<class L, class R> constexpr auto operator<=>(const L&, const R&);
}
```

### Semantics

| Expression | Result | Notes |
|---|---|---|
| `inside<G, P>{v}` | stores `v` by `P` | ill-formed when no value of `v`'s type can fit (disjoint intervals without `clamp`/`wrap`, incompatible notch without a rounding) |
| `B::try_make<F>(v)` | `expected<B, errc>` | the store under `P | F`, reporting instead of throwing |
| `b.to<T>()` | `expected<T, errc>` | `overflow` out of `T`'s range, `domain_error` for a negative value into an unsigned `T`; a fractional value truncates toward zero |
| `-a` | `inside<-G, P>` | mirrored interval, same notch |
| `a + b`, `a - b` | interval sum or difference; notch `gcd(n_a, n_b)` | continuous if either operand is, with denominator bound `D_a·D_b` |
| `a * b` | interval product; notch `n_a·n_b`, refined by the operands' offsets | a point `c` scales the other notch by `|c|` |
| `a / b` | a point `c`: `a`'s notch scaled by `1/|c|`; else continuous with denominator bound `D_a·max|b|·D_b` | rounded onto the integers or the shared notch when a policy names a rounding; `expected` iff `b`'s grid holds 0 |
| `a % b` | `[0, m]` or `[−m, m]`, `m = max|b| − 1` | integer grids required; `r = a − q·b` for `q` truncated or rounded by the policy |
| `e op b`, `a op e` (`e` an `expected`) | `expected` of `a op b`'s result | the first error passes through |
| `++a`, `--a` | `a` moved by one notch | a store by `P` (wraps under `wrap`); absent on a continuous grid |
| `a += b` etc. | `a = a + b` | a store by `a`'s policy |
| `a == b`, `a <=> b` | exact comparison of values | any two grids |

## Quantities

`@*unit*@`, `@*spec*@` and `@*origin*@` are exposition-only tag types: empty
objects whose type carries a `unit`, a `quantity_spec` or a `point_origin` as a
value.

```cpp
namespace std::units {
  // dimensions and magnitudes
  struct base_dimension { string_view Name; };                 // a dimension of your own
  struct dimension;                                           // exponents of L M T I Θ N J and of base dimensions
  struct magnitude;                                           // an exact rational times a power of π
  template<@*grid-number*@ R> constexpr @*magnitude*@ mag;          // R
  inline constexpr @*magnitude*@ mag_pi;                          // π

  // units
  struct unit;                                                // named units to integer powers, prefixes, a magnitude
  struct unit_options { string_view ascii; @*spec*@ kind; @*origin*@ origin; bool space_before = true; };
  struct named_unit {                                         // m, N, °C: a static object
    constexpr named_unit(string_view symbol, @*spec*@ kind, unit_options = {});
    constexpr named_unit(string_view symbol, @*magnitude*@, @*spec*@ kind, unit_options = {});
    constexpr named_unit(string_view symbol, @*unit*@ definition, unit_options = {});
    constexpr named_unit(string_view symbol, @*unit*@ definition, @*spec*@ kind, unit_options = {});
  };
  template<const named_unit& U> constexpr @*unit*@ unit_c;
  inline constexpr @*unit*@ one;                                  // the unit 1
  // @*unit*@ * @*unit*@, @*unit*@ / @*unit*@, @*magnitude*@ * @*unit*@, pow<N>(u), square(u), cubic(u), inverse(u)
  // prefixes: kilo(u), milli(u), ... on a named unit

  // quantity kinds
  struct quantity_spec {                                      // isq::length, isq::height
    constexpr quantity_spec(string_view name, dimension);             // a base quantity
    constexpr quantity_spec(string_view name, const base_dimension&);
    constexpr quantity_spec(string_view name, @*spec*@ parent [, is_kind]);  // a child
    constexpr quantity_spec(string_view name, @*spec-expression*@ [, is_kind]); // a derived root
    constexpr quantity_spec(string_view name, @*spec*@ parent, @*spec-expression*@ [, is_kind]);
  };
  template<const quantity_spec& S> constexpr @*spec*@ spec_c;
  // @*spec*@ * @*spec*@, @*spec*@ / @*spec*@ (recipes); s[u] names a reference

  // references and quantities
  struct reference;                                           // a unit, a kind or recipe; a unit converts to one
  template<reference R, class Rep>                            // Rep: an inside
  class quantity {
  public:
    using rep = Rep;
    static constexpr units::reference reference = R;
    constexpr quantity() = default;
    template<class V> constexpr explicit quantity(V&& number);     // a number in the unit R
    template<reference R2, class V2>                               // implicit when R2's kind converts and
      constexpr explicit(@*see below*@) quantity(const quantity<R2, V2>&); // the rescaled number does
    constexpr const Rep& numerical_value() const noexcept;
    template<@*unit*@ U> constexpr auto in(U) const;                 // Rep's grid rescaled; same raw
    template<@*unit*@ U> constexpr auto numerical_value_in(U) const;
    template<policy_flag F = none, class V> static constexpr auto try_make(const V&);
    // ++, --, compound assignment: a store by Rep's policy
  };
  // quantity + - % <=> == quantity: in the common unit
  // quantity * / quantity, inside * @*unit*@, quantity * @*unit*@: units and kinds multiply
  // quantity * / inside: the number scales
  template<class Spec, reference R, class V> constexpr auto quantity_cast(const quantity<R, V>&);
  template<class Rep2, reference R, class V> constexpr quantity<R, Rep2> value_cast(const quantity<R, V>&);
  template<auto U, class Rep2, reference R, class V> constexpr auto value_cast(const quantity<R, V>&);
  template<class Q, reference R, class V> constexpr Q value_cast(const quantity<R, V>&);
  // try_value_cast<...>(q): the same through Rep2::try_make, an expected

  // points
  struct point_origin {                                       // absolute, or at an exact offset from one
    string_view Name; dimension Dim; @*origin*@ Absolute; @*grid-number*@ Offset;
  };
  template<const point_origin& O> constexpr @*origin*@ origin_c;
  template<reference R, @*origin*@ O, class Rep>
  class quantity_point {
  public:
    constexpr explicit quantity_point(const quantity<R, Rep>&);
    constexpr const quantity<R, Rep>& quantity_from_origin() const noexcept;
    constexpr auto quantity_from(@*origin*@) const;
    constexpr auto quantity_from_zero() const;
    constexpr auto point_for(@*origin*@) const;                     // the same point from another origin
    template<@*unit*@ U> constexpr auto in(U) const;                 // keeps the origin
    template<@*unit*@ U> constexpr auto point_in(U) const;           // from U's origin
  };
  template<auto U, class V> constexpr auto point(const V&);      // a point from U's origin
  // point ± quantity: a point; point − point: a quantity

  // the catalog: namespaces isq, si, international (contents in R1)
}
```

### Semantics

| Expression | Result | Notes |
|---|---|---|
| `quantity<R, Rep>{v}` | `v` stored in `Rep`, in the unit of `R` | explicit |
| `n * u` (`n` an inside) | `quantity<u, decltype(n)>` | `42 * u` is ill-formed |
| `q.in(u)` | `q` in `u`: `Rep`'s interval and notch times the conversion factor | the same raw; ill-formed for another dimension or a factor of π |
| `quantity<R2, Rep2>{q}` | `q` in `R2`'s unit, stored by `Rep2` | implicit when `R`'s kind converts to `R2`'s and the rescaled number converts implicitly |
| `a + b`, `a - b`, `a % b`, `a <=> b` | in the common unit (the gcd of the magnitudes); kind: the nearest common ancestor | ill-formed for different dimensions or kinds |
| `a * b`, `a / b` | units, kinds and numbers multiply or divide | `expected` iff `b`'s grid holds 0; a result of dimension one converts to its `inside` |
| `quantity_cast<S>(q)` | the same number, kind `S` | down or sideways within one kind; never across an `is_kind` boundary |
| `value_cast<Rep2>(q)`, `value_cast<u, Rep2>(q)` | the exact value, stored by `Rep2` | `try_value_cast` reports through `expected` |
| `point<u>(v)` | a point counted from `u`'s origin | |
| `p.point_for(o)`, `p.quantity_from(o)` | the point or quantity from origin `o` | an exact shift of the grid |
| `p - p`, `p ± q` | a quantity; a point | no difference between unrelated absolute origins |

## Math

Every function below except `pown` also has the form `fn_into<Out>(args)`,
which returns `Out` (or `expected<Out, errc>` where the deduced form is
fallible). Except for the grid operations, `Out` must be an `inside` whose
policy names a rounding mode, and the deduced form `fn(args)` requires the
same of its input.

```cpp
namespace std::math {
  template<uint64_t K>                                        // [-1, 1] at resolution 1/K
    using unit_amplitude = inside<{{-1, 1}, per<K>}, round_nearest>;

  // grid operations: exact
  template<class In> constexpr auto abs(In x);               // [0, max|x|]
  template<class In> constexpr auto sign(In x);              // -1, 0, 1
  template<class M, class S> constexpr auto copysign(M mag, S sgn);
  template<class In> constexpr auto floor(In x);             // also ceil, round, trunc: notch 1
  template<class X, class Y> constexpr auto fmod(X x, Y y);  // expected iff y's grid holds 0
  template<intmax_t E, class In> constexpr auto pown(In x);  // E >= 0: widened at every multiply

  // roots
  template<class In> constexpr auto sqrt(In x);              // expected iff x's grid holds a negative
  template<class In> constexpr auto cbrt(In x);
  template<class X, class Y> constexpr auto hypot(X x, Y y);

  // trigonometric, in radians
  template<class In> constexpr auto sin(In x);               // also cos: [-1, 1]
  template<class In> constexpr auto tan(In x);               // expected: [-1024, 1024]
  template<class In> constexpr auto asin(In x);              // also acos; x's grid within [-1, 1]
  template<class In> constexpr auto atan(In x);
  template<class Y, class X> constexpr auto atan2(Y y, X x); // [-pi, pi]; atan2(0, 0) == 0

  // hyperbolic
  template<class In> constexpr auto sinh(In x);              // also cosh, tanh, asinh
  template<class In> constexpr auto acosh(In x);             // x's grid within [1, inf)
  template<class In> constexpr auto atanh(In x);             // x's grid within (-1, 1)

  // exponential and logarithmic
  template<class In> constexpr auto exp(In x);               // also exp2
  template<class In> constexpr auto log(In x);               // also log2, log10; x's grid > 0
  template<intmax_t B, class In> constexpr auto pow_base(In x); // B^x, B >= 2
  template<class B, class E> constexpr auto pow(B base, E e);  // expected; deduced: base's grid > 0
}

namespace std::units {
  // q's unit:              abs, floor, ceil, round, trunc
  // the unit's root:       sqrt, cbrt (m² → m), and _into
  // an angle in rad:       sin, cos, tan; sin_into, cos_into also take a unit with a factor of pi
  // to rad:                asin, acos, atan of an inside or a dimensionless quantity; atan2
  // the common unit:       hypot
  // a dimensionless input: exp_into ... atanh_into, log10_into, pow_base_into<Out, B>, pow_into
  template<auto U, class Out, reference R, class V>          // across a factor of pi: one rounding
    constexpr auto value_cast(const quantity<R, V>&);
}
```

### Semantics

| Expression | Result | Errors |
|---|---|---|
| `fn_into<Out>(x)` | the true value rounded onto `Out`'s grid by `Out`'s mode, then stored by `Out`'s policy | ill-formed without a rounding mode in `Out`, except for a grid operation |
| `fn(x)` | `fn_into<Out>(x)`, `Out` deduced from `x`'s grid and policy | ill-formed without a rounding mode in `x`'s policy, except for a grid operation |
| `abs`, `sign`, `copysign`, `floor`, `ceil`, `round`, `trunc`, `pown<E>` | exact | none; `round` sends a half away from zero |
| `fmod(x, y)` | `x − trunc(x/y)·y`, exact, the sign of `x` | `division_by_zero`, iff `y`'s grid holds 0 |
| `sqrt(x)` | | `domain_error` for `x < 0`, iff `x`'s grid holds a negative |
| `log`, `log2`, `log10`, `asin`, `acos`, `acosh`, `atanh` | | ill-formed unless `x`'s grid lies in the domain |
| `tan(x)` | | `overflow` past `Out` (saturates under `clamp`) |
| `pow(b, e)` | exact for an integer `e`, and for `p/q` when `b` is a perfect `q`-th power | `domain_error` for `b < 0`, `division_by_zero` for 0 to a negative power, `overflow` past `Out` |
| `units::sin_into<Out>(q)` | `q` reduced exactly by quarter turns when its unit has a factor of π | as `sin_into` |
| `value_cast<U, Out>(q)` | the exact `x·r·π^±1`, rounded once onto `Out` | ill-formed without a rounding mode in `Out` |

# Future work

The reference implementation provides more than these three layers. Each item
below is in v0.4.0, and each is a candidate for a later paper:

1. **Per-call policies and actions**: `div(a, b, rounded_floor)`, `mod`,
   `divmod`, and callbacks that receive the overshoot (`on_clamp`, `on_wrap`,
   `on_error`, `on_overflow`). With them a clock's seconds carry into minutes.
2. **Conversions**: `clamp_cast`, `wrap_cast`, `checked_cast`, `as<T>()`,
   `numerator()` and `denominator()`, and the predicates
   `conversion_overflows`, `conversion_rounds` and `conversion_loses`.
3. **Text**: `to_string`, `parse`, `parse_exact`, `std::formatter` and stream
   operators, printing the exact value; quantities print the number, then the
   symbol.
4. **Algorithms and generic code**: `sum<Target>` with one deferred check,
   `add_all`, `mul_all`, `mul_into<Out>` / `div_into<Out>` (one rounding, no
   intermediate grid), `min`, `max`, `midpoint`, `lerp`, `dot`, `cross`, a
   `std::common_type` that is the grid hull, `numeric_limits` and `hash`. All
   of them work on quantities too.
5. **Iterating a grid**: `cursor<T, Step>` for loops, the range
   `inside_range` and `values<T>` (every value of `T`), and `uniform<B>(rng)`,
   which samples a grid exactly.
6. **Predefined types and constants**: `uint8` … `int64` on their full ranges,
   `unorm8`/`unorm16` and the Q formats (`q8_8`), the mathematical constants
   as point grids, and the physical constants as quantities of their kind.
7. **A public exact rational type** and its literals.
8. **The full unit catalog**: about 80 ISQ kinds, the SI units and prefixes
   (quecto … quetta past 64 bits), the defining constants as units,
   the international units, and `percent` and `per_mille`.

# Implementation experience

The reference implementation is header-only C++23 (GCC 14+, Clang 19+, Clang 18
with libc++, AppleClang, MSVC 19.51+). Grids past 64 bits use C++26 static
reflection (GCC 16 with `-freflection`). It ships a single-header amalgamation,
a CMake package and a vcpkg port, and follows the Beman Standard. Its test
suite (818 tests, 831 with C++26 reflection) runs in CI on 50 compiler,
library and sanitizer configurations. It includes a check that the integer
fast paths compile to call-free, vectorising code, a property fuzzer, and a
differential oracle that recomputes random results exactly in Python.

The math layer is checked for every function on dyadic, decimal and 2⁻⁴⁰
grids against a `long double` reference, and every maximum error is 0.5
notch (the repository's `docs/accuracy.md`). A build without floating point
(`-ffreestanding`) computes every result on the integer path, and the results
do not change. Onto the grid deduced from the input, the functions run from
about 3× slower to 3× faster than `<cmath>`. Onto grids as fine as a `double`,
correct rounding costs 4–9× (paper, "Performance").

Integer-aligned arithmetic compiles to the same instructions as `int` (paper,
"The fast path is byte-identical to native"). `inside<unsafe>` runs at native
speed, and `inside<checked>` at 88% of the fastest hand-written
range-enforcing code (paper, "The like-for-like comparison"). A unit conversion
compiles to one `mov`. The unit layer is about 2700 lines, adds about 0.1 s to
a translation unit, and parses in about a third of the time of mp-units' `si` and
`isq` headers (the repository's `docs/units.md`, "Compile time"). 23 of the
repository's examples use both layers.

# Prior art

- **mp-units** (Mateusz Pusz) and its proposal [@P3045R9]: the model of the
  unit layer. References, the ISQ tree of quantity kinds, quantity points with
  origins, magnitudes with powers of π and `consteval` unit arithmetic all
  follow mp-units. The unit layer here differs in that the representation is
  always an `inside`, which makes conversions and mixed-unit sums exact and
  gives every result an inferred grid. It has no logarithmic units and no
  vector or tensor characters.
- **bounded::integer** (David Stone): integer ranges in the type, with widening
  arithmetic. `inside` generalises the idea to rational grids.
- **Boost.SafeNumerics** (Robert Ramey) and **SafeInt**: overflow detection for
  built-in integers.
- **CNL** (John McFarlane) and its proposals: fixed-point numbers [@P0037R7],
  elastic integers [@P0828R0], composition of arithmetic types [@P0554R0].
- **Intel safe-arithmetic**: interval unions and compile-time proofs.
- **Boost.Units** and **Au** (Aurora): units libraries over a generic
  representation.
- Numerics work in progress [@P1889R1]; saturation arithmetic [@P0543R3].
- **CRlibm** and **[CORE-MATH](https://core-math.gitlabpages.inria.fr/)**:
  correctly rounded functions for the IEEE binary formats. C23 reserves the
  `cr_` prefix for such functions. The math layer here rounds correctly onto
  any grid, at the precision the grid needs, instead of onto a binary format.

The repository's `docs/resources.md` compares their features.

# Open questions

1. **Naming.** `inside`, `grid`, `notch`, `per` and the policy names are the
   implementation's. The committee may prefer others (`bounded`, `ranged`). A
   standard literal suffix cannot begin with `_`, so `_ins` needs a new name.
2. **`to<T>()` truncates** a fractional value into an integer `T`. It could
   instead report `rounding_error` unless the type names a rounding, as a store does.
3. **Failure of a checked store.** An exception (`inside_error`, as proposed),
   a contract violation ([@P2900R14]), or a replaceable handler (the
   implementation has one, which lets the core run freestanding).
4. **Reflection as a minimum.** Grids past 64 bits need static reflection. A
   standard version could require it from the start and drop the 64-bit mode.
5. **Bare scalars.** Rejecting `a + 1` is deliberate. Should mixed arithmetic
   with an integer of a stated range be allowed instead?
6. **The unit layer and P3045.** There are two ways to standardize quantities on
   `inside`: (a) the `std::quantity` of [@P3045R9], with `inside` as one more
   representation, or (b) the unit layer proposed here. The reference
   implementation shows that (b) gives exact, free conversions and mixed-unit
   sums that cannot overflow, which a generic representation cannot promise.
   Should the unit layer be a separate paper, coordinated with P3045?
7. **Interaction** with C++26 saturation arithmetic and with `std::linalg`
   element types.
8. **Math naming.** A namespace `std::math`, as in the implementation, or
   overloads of `std::sin` and the others, with `sin_into<Out>` beside them?
9. **Correct rounding as a requirement.** The proposal requires the correctly
   rounded point. The cost is small onto coarse grids and 4–9× at a `double`'s
   resolution. Should implementations be allowed a faithful result (one of the
   two neighbouring points) instead, at the price of results that differ
   between platforms?
10. **Deduced outputs.** The deduced output's grid follows the input's notch,
    with fixed ranges for `sin`, `cos`, `tan` and `atan2`. The fixed `tan`
    range [−1024, 1024] is a choice; it could instead be required to be named.
