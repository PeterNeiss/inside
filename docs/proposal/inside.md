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

This paper proposes the minimal kernel: the type, its grid, the policies and the
operators. Correctly rounded transcendental functions onto a grid, text
conversion and the convenience functions of the reference implementation are
left to later papers ([Future work](#future-work)).

The reference implementation is [beman.inside
v0.3.0](https://github.com/PeterNeiss/inside/tree/v0.3.0). A long-form
motivation with compiled exhibits is [*int considered
harmful*](https://github.com/PeterNeiss/inside/blob/v0.3.0/docs/int-considered-harmful.pdf)
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

What each of them lacks is the same: the range a value may occupy, its scale,
the rounding rule and the edge behaviour. `inside` records all four in the type,
where the compiler can act on them.

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
| `snap`, `round_nearest`, `round_floor`, `round_ceil`, `round_half_even` | (as checked) | rounds (`snap` truncates toward zero) |
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
and is 22% faster than a native checked type in the paper's benchmark ("Where
are the checks?").

Fallible results are values only where failure is possible at runtime:
`try_make`, `to<T>()`, and division by a divisor whose grid holds zero. A total
operation returns a plain value, so the type says whether failure is possible.
A failed checked store throws `inside_error`, which carries the `errc`.

## Grids past 64 bits

With C++26 static reflection (`std::define_static_array`) grid numbers have no
size limit, and equal grids remain the same type: a grid from 0 to 10³⁰ in
steps of 1 stores a two-limb integer. Without reflection, grid numbers are 64-bit rationals and a
grid past them is a compile-time error.

# Proposed interface

The names are the reference implementation's (see [Open
questions](#open-questions)). `@*grid-number*@` is exposition-only: an exact
rational, unbounded where static reflection is available and 64-bit otherwise.
`@*number*@` is an arithmetic type or a `@*grid-number*@`.

```cpp
namespace std {
  // grids
  template<intmax_t D> constexpr @*grid-number*@ per;          // 1/D
  struct interval {
    @*grid-number*@ Lower, Upper;                                // Lower <= Upper
  };
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
    constexpr grid(@*number*@ point);                            // notch 0, lower == upper
    constexpr grid(interval, @*grid-number*@ notch);
    constexpr grid(interval, max_denominator);                  // continuous, bounded
  };

  // policies
  using policy_flag = @*unsigned integer type*@;
  inline constexpr policy_flag checked, clamp, wrap, snap, round_nearest,
                               round_floor, round_ceil, round_half_even, unsafe;

  // errors
  enum class errc { domain_error = 1, division_by_zero, overflow,
                    rounding_error, not_finite, invalid_format };
  struct inside_error : runtime_error { errc Code; };

  // the type
  template<grid G, policy_flag P = checked>
  struct inside {
    constexpr inside() = default;                         // trivial
    template<class A> constexpr inside(A value);          // A arithmetic or inside: a store by P
    template<class A> static constexpr expected<inside, errc> try_make(A value);
    template<class T> constexpr expected<T, errc> to() const;

    constexpr inside<-G, P> operator-() const;
    constexpr inside& operator++();                       // one notch
    constexpr inside& operator--();
    template<class R> constexpr inside& operator+=(const R&); // also -= *= /= %=: a store by P
  };

  // observers
  template<class B> constexpr grid grid_of;
  template<class B> constexpr policy_flag flags_of;
  template<class B> constexpr @*grid-number*@ lower_of, upper_of, notch_of;

  // constants
  template<auto V> constexpr inside<grid{V}> just;
  template<char... Cs> constexpr auto operator""_ins();      // a point grid; suffix to be renamed

  // operators (L, R are inside types)
  template<class L, class R> constexpr auto operator+(L, R);   // widened result grid
  template<class L, class R> constexpr auto operator-(L, R);
  template<class L, class R> constexpr auto operator*(L, R);
  template<class L, class R> constexpr auto operator/(L, R);   // inside or expected
  template<class L, class R> constexpr auto operator%(L, R);
  template<class L, class R> constexpr bool operator==(const L&, const R&);
  template<class L, class R> constexpr auto operator<=>(const L&, const R&);
}
```

## Semantics

| Expression | Result | Notes |
|---|---|---|
| `inside<G, P>{v}` | stores `v` by `P` | ill-formed when no value of `v`'s type can fit (disjoint intervals without `clamp`/`wrap`, incompatible notch without a rounding) |
| `B::try_make(v)` | `expected<B, errc>` | the store, reporting instead of throwing |
| `b.to<T>()` | `expected<T, errc>` | `overflow` out of `T`'s range, `domain_error` for a negative value into an unsigned `T`; a fractional value truncates toward zero |
| `-a` | `inside<-G, P>` | mirrored interval, same notch |
| `a + b`, `a - b` | interval sum or difference; notch `gcd(n_a, n_b)` | continuous if either operand is, with denominator bound `D_a·D_b` |
| `a * b` | interval product; notch `n_a·n_b`, refined by the operands' offsets | a point `c` scales the other notch by `|c|` |
| `a / b` | a point `c`: `a`'s notch scaled by `1/|c|`; else continuous with denominator bound `D_a·max|b|·D_b` | rounded onto the integers or the shared notch when a policy names a rounding; `expected` iff `b`'s grid holds 0 |
| `a % b` | `[0, m]` or `[−m, m]`, `m = max|b| − 1` | integer grids required; `r = a − q·b` for `q` truncated or rounded by the policy |
| `++a`, `--a` | `a` moved by one notch | a store by `P` (wraps under `wrap`); absent on a continuous grid |
| `a += b` etc. | `a = a + b` | a store by `a`'s policy |
| `a == b`, `a <=> b` | exact comparison of values | any two grids |

# Future work

The reference implementation provides more than this kernel. Each item below is
in v0.3.0, and each is a candidate for a later paper:

1. **Correctly rounded math**: `math::sin_into<Out>(x)` and its siblings
   (`exp`, `log`, `pow`, the trigonometric and hyperbolic functions and their
   inverses, `sqrt`, `cbrt`, `hypot`) return the correctly rounded point of
   `Out`. The result is the same at compile time and at run time, on every
   platform, with or without an FPU. At coarse output grids it is as fast as
   `<cmath>`; at the full resolution of a `double` it costs 4–9× (paper,
   "Performance"). This is the companion paper.
2. **Per-call policies and actions**: `div(a, b, rounded_floor)`, `mod`,
   `divmod`, and callbacks that receive the overshoot (`on_clamp`, `on_wrap`,
   `on_overflow`). With them a clock's seconds carry into minutes.
3. **Conversions**: `clamp_cast`, `wrap_cast`, `checked_cast`, `as<T>()`, and
   the predicates `conversion_overflows`, `conversion_rounds`.
4. **Text**: `to_string`, `parse`, `parse_exact`, `std::formatter`
   and stream operators, printing the exact value.
5. **Algorithms and generic code**: `sum<Target>` with one deferred check,
   `mul_into<Out>` / `div_into<Out>` (one rounding, no intermediate grid),
   `min`, `max`, `midpoint`, a `std::common_type` that is the grid hull,
   `numeric_limits`, `hash`, and `cursor<T, Step>` for loops over a grid.
6. **A public exact rational type** and its literals.
7. **Physical units**: `quantity<si::metre, inside<…>>` with an inside as the
   number, the ISQ quantity kinds, quantity points with exact origins, and
   units and base dimensions of your own. A unit conversion rescales the grid
   and keeps the raw value, so it is exact and costs nothing at run time.

# Implementation experience

The reference implementation is header-only C++23 (GCC 14+, Clang 19+, Clang 18
with libc++). It ships a single-header amalgamation, a CMake package and a
vcpkg port, and follows the Beman Standard. Its test suite (798 tests, 809 with
C++26 reflection) runs in CI on 48 compiler, library and sanitizer
configurations. It includes a check that the integer fast paths compile to
call-free, vectorising code, a property fuzzer, and a differential oracle that
recomputes random results exactly in Python.

Integer-aligned arithmetic compiles to the same instructions as `int` (paper,
"The fast path is byte-identical to native"). `inside<unsafe>` runs at native
speed, and `inside<checked>` at 87% of the fastest hand-written
range-enforcing code (paper, "The like-for-like comparison").

# Prior art

- **bounded::integer** (David Stone): integer ranges in the type, with widening
  arithmetic. `inside` generalises the idea to rational grids.
- **Boost.SafeNumerics** (Robert Ramey) and **SafeInt**: overflow detection for
  built-in integers.
- **CNL** (John McFarlane) and its proposals: fixed-point numbers [@P0037R7],
  elastic integers [@P0828R0], composition of arithmetic types [@P0554R0].
- **Intel safe-arithmetic**: interval unions and compile-time proofs.
- Numerics work in progress [@P1889R1]; saturation arithmetic [@P0543R3].

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
6. **Interaction** with C++26 saturation arithmetic and with `std::linalg`
   element types.
