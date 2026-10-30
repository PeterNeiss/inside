---
title: "Correctly rounded math for `inside` and quantities"
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

We propose the functions of `<cmath>` for `inside`, the bounded number with an
exact rational step proposed in [*`inside`: numbers whose range and step size
are part of the
type*](https://github.com/PeterNeiss/inside/blob/pureCPP26/docs/proposal/inside.md)
(*the number paper* below), and for the quantities of [*Quantities on
`inside`*](https://github.com/PeterNeiss/inside/blob/pureCPP26/docs/proposal/quantity.md)
(*the quantity paper*).

Each function returns the *correctly rounded* point of its output grid: the
point that the output's rounding mode selects from the true value. The output
grid is part of the type, so the precision is too, with no ceiling. The result
does not depend on how it was computed, so it is the same at compile time and at
run time, on every platform, with or without an FPU. `sin(x)` deduces its
output grid from the input; `sin_into<Out>(x)` names it. On a quantity the unit
follows the function: `sqrt` of an area is a length, and an angle in degrees is
reduced exactly, so `cos` of 120° is exactly −1/2.

This paper depends on the number paper; its section on quantities depends on
the quantity paper. The reference implementation is the `pureCPP26` branch of
[beman.inside](https://github.com/PeterNeiss/inside/tree/pureCPP26).

# Motivation

`std::sin` on a `double` is not required to be correctly rounded. Its result
can differ between library implementations, between library versions, and
between compile time and run time: since C++26 most of `<cmath>` is
`constexpr` ([@P0533R9], [@P1383R2]), and a compiler may fold a constant call
with a different algorithm than the run-time library uses. A program that must
give the same answer everywhere (a lockstep simulation, a replay, a test
expectation, a certification) cannot rely on it.

The precision is the `double`'s, whatever the program needs. A fixed-point
program that wants `sin` on a grid of 1/16384 computes it in `double` and
rounds again: two roundings, and near a midpoint between two grid points the
second one can pick the wrong one. A program without an FPU, or one that needs
more than 53 bits, has no standard function at all.

An `inside` carries the precision a program needs in its type: the output's
notch. So the function can round once, directly onto that grid, and the
contract can be the one that leaves no freedom: the correctly rounded point.
C23 reserves the `cr_` names for correctly rounded functions on the binary
formats ([@N3220]); this paper asks the same of every grid.

# A tour

The listings use the reference implementation's namespaces `beman::inside` and
`beman::inside::units`. Each one is a region of a program that compiles and
checks its own results.

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
```

**The output's rounding mode picks the point; exact values are exact; compile
time and run time agree.**

<!-- listing: math_rounding -->
```cpp
using x_t  = inside<{{1, 16}, per<64>}, round_nearest>;
using down = inside<{{0, 16}, per<1024>}, round_floor>;
using up   = inside<{{0, 16}, per<1024>}, round_ceil>;

auto lo = math::sqrt_into<down>(x_t{2});         // 1.4140625    = 1448/1024 ≤ √2
auto hi = math::sqrt_into<up>(x_t{2});           // 1.4150390625 = 1449/1024 ≥ √2
static_assert(math::sqrt_into<down>(x_t{2.25}) == math::sqrt_into<up>(x_t{2.25})); // 1.5 exactly
static_assert(math::log2_into<down>(x_t{8}) == 3 && math::log2_into<up>(x_t{8}) == 3);

constexpr auto at_compile_time = math::exp(x_t{1.5});
auto           at_run_time     = math::exp(x_t{1.5});
// at_compile_time == at_run_time, on every platform, with or without an FPU
```

**On a quantity the unit follows the function, and π is reduced exactly.**

<!-- listing: math_quantity -->
```cpp
using down = inside<{{-1, 1}, per<16384>}, round_floor>;
auto h = units::cos_into<down>(quantity<si::degree, inside<{0, 360}>>{120}); // exactly -1/2, even rounding down

using area_t = quantity<square(si::metre), inside<{{0, 100}, per<1024>}, round_nearest>>;
auto side = units::sqrt(area_t{2});              // 1.4140625 m: the unit's root
static_assert(std::is_same_v<decltype(side)::rep, inside<{{0, 10}, per<1024>}, round_nearest>>);

using rad_t = inside<{{0, 7}, per<1'000'000>}, round_nearest>;
auto right = value_cast<si::radian, rad_t>(quantity<si::degree, inside<{0, 360}>>{90}); // 1.570796 rad: π/2, rounded once
```

# Design

## The contract

A function's result is a point of its output grid, and it is the *correctly
rounded* point: the one the output's rounding mode selects from the true
value. `round_nearest` takes the nearest point (a tie away from zero),
`round_floor` the one at or below, and so on, as for any store in the number
paper. Because the output grid is part of the type, so is the precision. A
coarse output is cheap, a fine one costs more, and there is no ceiling: an
output notch of 2⁻¹⁰⁰ gets a correctly rounded result too.

The result does not depend on how it was computed. It is the same at compile
time and at run time, on every platform, and with or without an FPU, so a
constant expression and a run-time call agree by specification.

## Two forms

`fn_into<Out>(x)` names the output. `fn(x)` deduces it: the input's notch and
policy, and the function's range over the input's interval, computed at
compile time and rounded outward. A few ranges are fixed instead: `sin` and
`cos` give [−1, 1], `tan` [−1024, 1024] and `atan2` [−π, π]. `hypot` takes the
gcd of both notches, `pow` the base's notch, and `floor` & co. notch 1.

## Rounding needs permission

A transcendental result is never on a grid by accident, so `Out` (or, for
`fn(x)`, the input) must name a rounding mode, as a store that rounds must in
the number paper. A result past `Out`'s interval goes through `Out`'s policy:
`clamp` saturates, `wrap` folds the true value, and the default reports.

## Errors follow the number paper

A domain limit that the input's *type* violates (`log` of a grid that reaches
0, `asin` of a grid past ±1) is ill-formed. A failure that depends on the
*value* returns `std::expected<Out, errc>`: `sqrt` of a grid that crosses 0,
`tan` near a pole, and `pow`. A total function returns a plain `inside`.

## The grid operations are exact

`abs`, `sign`, `copysign`, `floor`, `ceil`, `round`, `trunc`, `fmod` and
`pown<E>` need no rounding. Their outputs are deduced exactly (`abs` of
`[−3, 5]` is `[0, 5]`), they need no rounding mode, and `fmod` is
`std::expected` only when the divisor's grid holds 0, as `%` is.

## Exact cases are exact

`exp(0)`, `log(1)`, `sqrt(9/4)`, `log2(2^k)` and a power with an integer
exponent land on the grid point itself under every rounding mode, as do the
rational values of `sin` and `cos` (0, ±1/2, ±1) of an angle in degrees or
turns.

## On quantities the unit follows the function

- `sqrt` and `cbrt` take the unit's root: of an area, a length.
- `abs`, `floor`, `ceil`, `round` and `trunc` keep the unit.
- `sin`, `cos` and `tan` take an angle in radians and return a number;
  `sin_into` and `cos_into` also take an angle in a unit with a factor of π
  (degrees, turns), which they reduce exactly by quarter turns, so only a
  remainder of at most π/4 is approximated.
- `asin`, `acos` and `atan` of a number, and `atan2` of two quantities of one
  dimension, return radians; `hypot` returns the common unit.
- `exp`, `log`, `pow` and the hyperbolic functions take a dimensionless
  quantity, such as the decibel's 10^(L / 20 dB).
- `value_cast<si::radian, Out>(q)` converts across a factor of π, rounded once
  onto `Out`. The quantity paper leaves such a conversion ill-formed until this
  rounding is named.

## Recommended practice, and termination

How a result is found is recommended practice, not specification. The
reference implementation evaluates in fixed point with an error bound and
doubles the precision while the interval straddles a rounding boundary (Ziv's
strategy). It reads a precomputed table for small inputs and uses
floating-point kernels with proved bounds where an FPU is present, behind an
integer path that always applies and gives the same results.

The requirement is met wherever that loop ends, and it ends for every input:
a rounding boundary is rational, the rational results are exactly the exact
cases above (Lindemann–Weierstrass, Niven's theorem and the rational root
theorem leave no others), and every other result is irrational. How many
doublings an input needs is not known in advance for an arbitrary grid (the
table-maker's dilemma), so the reference implementation stops at four times
its start precision, plus one deeper step for functions that approach a grid
point, such as tanh x → 1. A result still undecided there, within about 2^−4W
of a boundary, is stored as the nearest point: the same in every build, but not
proven correct. A tool certifies a type pair by exhaustion instead, computing
every input and requiring that none is undecided ([Implementation
experience](#implementation-experience)).

# Proposed interface

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

## Semantics

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
| `units::sqrt(q)` | `sqrt` of the number, in the unit's root | ill-formed when the unit has no such root |
| `units::sin_into<Out>(q)` | `q` reduced exactly by quarter turns when its unit has a factor of π | as `sin_into` |
| `value_cast<U, Out>(q)` | the exact `x·r·π^±1`, rounded once onto `Out` | ill-formed without a rounding mode in `Out` |

# Future work

1. **More functions**: `expm1`, `log1p`, `erf`, `erfc`, `tgamma`, `lgamma` and
   the rest of `<cmath>`, under the same contract.
2. **Mathematical constants** as point grids (π, e, √2, ln 2, the golden
   ratio, …), rationals within 2⁻⁵⁷ of the value that compose exactly
   (`angle * math::pi`). The reference implementation has them; for a
   correctly rounded value on a chosen grid the functions above already serve
   (`sqrt_into<Out>(just<2>)`).
3. **Algorithms that round once**: `mul_into<Out>` and `div_into<Out>` (in the
   reference implementation, and a candidate for the number paper's future
   work) round a product or quotient onto `Out` without an intermediate grid,
   as these functions do.

# Implementation experience

Every function is checked on dyadic, decimal and 2⁻⁴⁰ grids against a
`long double` reference, and every maximum error is 0.5 notch (the
repository's `docs/accuracy.md`). 59 type pairs are certified by exhaustion:
39 covering every function, and the 20 pairs the repository's examples use
that are small enough to compute (up to about 10⁹ inputs). Every input is
computed both through the floating-point kernels and through the integer path
alone, with no difference and no result left undecided at the precision cap.
A SHA-256 of each pair's results is committed (`docs/certified.md`), CI
reproduces every pair up to 2²⁰ inputs in every configuration and all of them
weekly, and the tool certifies any other pair the same way. A build without
floating point (`BEMAN_INSIDE_MATH_NO_FP`, one of the CI configurations)
computes every result on the integer path, and the results do not change.

Onto the grid deduced from the input, the functions run from about 3× slower to
3× faster than `<cmath>`. Inputs of up to 256 points use a table, one load per
call, at about 0.13 s of compile time per table. Onto grids as fine as a
`double`, correct rounding costs 4–9× `<cmath>`: `sin` onto 2⁻⁵² in 14.6 ns,
`exp` onto 2⁻⁴⁰ in 12.5 ns, `log` onto 2⁻⁴⁸ in 17.4 ns (x86-64, `-O3 -mfma`;
the repository's `docs/math.md`, "Speed"). 11 of the repository's 39 examples
use the functions, among them an oscillator, range and bearing from `hypot` and
`atan2`, audio gain in decibels and a rocket equation.

# Prior art

- **CRlibm** and **[CORE-MATH](https://core-math.gitlabpages.inria.fr/)**:
  correctly rounded functions for the IEEE binary formats, which IEEE 754-2019
  recommends. C23 reserves the `cr_` prefix for such functions ([@N3220]). The
  functions here round correctly onto any grid, at the precision the grid
  needs, instead of onto a binary format.
- **MPFR**: correctly rounded functions at any precision, on a dynamic
  floating-point type.
- **Ziv's strategy** of increasing precision until the rounding is decided,
  and the table-maker's dilemma that bounds it, are the basis of all three and
  of the reference implementation.
- `constexpr` `<cmath>` [@P0533R9], [@P1383R2]: the functions at compile time,
  with the run-time library's accuracy, which is unspecified.

The repository's `docs/resources.md` compares fixed-point libraries and their
math functions.

# Open questions

1. **Naming.** A namespace `std::math`, as in the implementation, or overloads
   of `std::sin` and the others, with `sin_into<Out>` beside them?
2. **Correct rounding as a requirement.** The proposal requires the correctly
   rounded point. The cost is small onto coarse grids and 4–9× at a `double`'s
   resolution. Should implementations be allowed a faithful result (one of the
   two neighbouring points) instead, at the price of results that differ
   between platforms? And since no bound on the precision a result needs is
   known for an arbitrary grid, may an implementation stop at a precision cap,
   as the reference implementation does, provided the result it then returns
   is the same on every platform?
3. **Deduced outputs.** The deduced output's grid follows the input's notch,
   with fixed ranges for `sin`, `cos`, `tan` and `atan2`. The fixed `tan`
   range [−1024, 1024] is a choice; it could instead be required to be named.
4. **The grid operations.** `abs`, `floor`, `round`, `fmod` and `pown` are
   exact and need no rounding. Should they move to the number paper, leaving
   this one to the functions that round?
