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
and step size (the *notch*, an exact rational) are compile-time properties of
the type. Arithmetic computes its result grid at compile time, so `+`, `-` and
`*` on a lattice cannot overflow. Division is exact unless the type names a
rounding, and it returns a failure value only where the operand grids allow
one. What a store into a narrower grid does (check, clamp, wrap or round) is
part of the declaration. A single call or a single store can name another
policy, and an action can observe what the policy did: the overshoot of a
clamp, the carry of a wrap. Named conversions, `std::numeric_limits`,
`std::hash` and `std::common_type` complete the type for generic code.

This is the first of three papers, and it stands alone. Two companion papers
build on it: [*Quantities on
`inside`*](https://github.com/PeterNeiss/inside/blob/pureCPP26/docs/proposal/quantity.md)
puts units on the number, and [*Correctly rounded math for `inside` and
quantities*](https://github.com/PeterNeiss/inside/blob/pureCPP26/docs/proposal/math.md)
adds the functions of `<cmath>`.

The reference implementation is the `pureCPP26` branch of
[beman.inside](https://github.com/PeterNeiss/inside/tree/pureCPP26). A
long-form motivation with compiled exhibits is [*int considered
harmful*](https://github.com/PeterNeiss/inside/blob/v0.5.0/docs/int-considered-harmful.pdf)
(the *paper* below).

# Motivation

The built-in integer types carry no range and no failure value. The paper
reproduces eleven consequences with compiled programs:

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

- Sanitizers find overflow at run time, in instrumented builds.
- `-fwrapv` makes the wrong answer reliable.
- Checked-integer libraries detect overflow, but not rounding, or a range
  narrower than the underlying type.
- C++26 saturation arithmetic ([@P0543R3]) picks one edge policy for the full
  range of a built-in type.

What each of them lacks is the same: the range a value may occupy, its step,
the rounding rule and the edge behaviour. `inside` records all four in the
type, where the compiler can act on them. Because the compiler knows the range
of every operand, it knows the range of every result, and most checks become
proofs.

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

**A conversion names what it does with a value that does not fit.**

<!-- listing: convert -->
```cpp
using level  = inside<{0, 10}>;
using sensor = inside<{0, 1000}>;
sensor raw   = 725;

static_assert(conversion_overflows<level>(725));
auto top  = clamp_cast<level>(raw);                      // 10: saturated
auto deg  = wrap_cast<inside<{0, 359}>>(raw);            // 5: 725 mod 360
auto fit  = checked_cast<level>(sensor{7});              // 7, or inside_error
auto near = clamp_cast<level, round_nearest>(6.5);       // 7: a tie goes away from zero

using quarter = inside<{{0, 10}, per<4>}>;
quarter q = 2.75_ins;
auto num  = q.numerator();                               // 11: the value is 11/4
auto i    = q.to<int>();                                 // unexpected(errc::rounding_error)
auto down = q.to<int, round_floor>();                    // 2: rounding named in the call
auto neg  = inside<{-5, 5}>{-1}.to<unsigned>();          // unexpected(errc::domain_error)
```

**A call or a single store can take another policy, and an action sees what
the policy did.**

<!-- listing: per_call -->
```cpp
using whole = inside<{0, 100}>;
using step  = inside<{1, 10}>;                           // never zero

auto down       = div(whole{7}, step{2}, rounded_floor); // 3: this call rounds down
auto up         = div(whole{7}, step{2}, rounded_ceil);  // 4
auto [quo, rem] = divmod(whole{17}, step{5});            // 3 and 2

whole x = 50;
x.with_clamp() = 150;                                    // 100: this store saturates

using seconds = inside<{0, 59}, wrap>;
using minutes = inside<{0, 59}>;
seconds s = 50;
minutes m = 0;
s.on_wrap([&](auto&, auto carry) { m += carry; }) += 75_ins; // 125 s: s == 5, m == 2
```

**Generic code sees a numeric type.**

<!-- listing: generic -->
```cpp
using small = inside<{0, 10}>;
using wide  = inside<{-50, 50}>;

using common = std::common_type_t<small, wide>;          // the hull of the two grids
static_assert(std::is_same_v<common, inside<{-50, 50}>>);
common hi = std::max<common>(small{7}, wide{-3});        // 7

using limits = std::numeric_limits<small>;
static_assert(limits::is_specialized && !limits::is_signed && limits::max() == 10);

std::unordered_set<small> seen{small{3}, small{4}, small{3}};  // two elements
```

# Design

## The grid

A grid is an interval `[lower, upper]` and a notch `n ≥ 0`. Its values are
`lower + k·n` for `k = 0, 1, …` up to `upper`, so `n` must divide
`upper − lower`. A notch of zero and a bound `D` on the denominators give a
continuous grid: every rational in the interval whose denominator is at most
`D` (`{{0, 1}, max_denominator{1000}}`). An `inside` on a continuous grid
needs the bound, because it sizes the fraction of every exact result (below). A
grid with `lower == upper` is a point. A floating-point limit is taken at its
exact binary value; when the notch is derived from such a limit, one that is
not a short binary fraction is rejected (`0.1` is not 1/10).

`lower` need not be a multiple of `n`. Pixel centres `{{0.5, 255.5}, 1}` and
sample midpoints are grids. Result grids track such lattices exactly.

A grid is always closed. An open limit is only a spelling that moves the limit
in by one notch at construction: `{0, 360, bounds::half_open}` is the grid
`{0, 359}`, and the same type.

## Grid numbers

The limits and the notch of a grid are exact rationals of any size: `per<100>`
is 1/100, and `inside<{0, 1e30_g}>` holds the integers up to 10³⁰. A grid is a
structural value, so it is a template argument. A grid number past 64 bits
keeps its limbs in static storage through `std::define_static_array`
([@P3491R3]), so equal numbers are the same template argument, and equal grids
give the same type. This is why the proposal requires static reflection
([@P2996R13]).

With bounded grid numbers, result grids would themselves become a source of
failure: `10¹⁸ · 10¹⁸` has no 64-bit grid, so the product would be ill-formed
although every value of it is representable. Unbounded grid numbers make
every result grid exist.

## Storage

Storage follows from the grid alone: there are no storage flags. The reference
implementation stores the narrowest integer that numbers every point, or holds
the value itself for a whole-number grid when that is no wider. Past 2⁶⁴ points
it stores a multi-limb integer, and a continuous grid stores an exact fraction
sized from its bound.
We propose that the storage be unspecified, with this as recommended practice.
Equal grids give the same type, so values of the same grid always share a
representation.

## Arithmetic widens

The result of `a + b`, `a - b`, `a * b` and `-a` is an `inside` whose grid holds
every possible result. `[0, 100] + [0, 100]` is `[0, 200]`, and negation mirrors
the interval. A continuous result's denominator bound follows from the
operands': `D_a·D_b` for `+`, `-` and `*`, where `D` of a lattice is its
notch's denominator. So no operation produces a value outside its result type,
and these operators need no overflow check and always return a plain `inside`.

A chain of exact operations on fractions multiplies the bounds. The reference
implementation gives a result whose fractions would need more than 1024 bits
(a configurable limit) no type: the operator is deleted, and its message names
the fix, which is to round an operand, or the product with `mul_into`, into a
coarser type.

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
`round_nearest` sends a tie away from zero; `round_half_even` to the even
point, counted from the lattice's anchor.

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

Fallible results are values only where failure is possible at run time:
`try_make`, `to<T>()`, and division by a divisor whose grid holds zero. A
total operation returns a plain
value, so the type says whether failure is possible. A failed checked store
throws `inside_error`, which carries the `errc`.

Fallible results chain. The operators also accept a `std::expected<inside,
errc>` operand, so `a / b * gain + offset` stays one `expected` from end to
end. The first error passes through with its cause, and an operation that fails
later in the chain reports its own.

## Conversions

A store applies the target's policy. A conversion names its own, so the reader
sees at the call what happens to a value that does not fit:

- `clamp_cast<B>(v)` saturates into `B`'s interval; `clamp_cast<B,
  round_nearest>(v)` (or another rounding mode) also rounds onto `B`'s notch.
- `wrap_cast<B>(v)` folds `v` modulo `B`'s interval.
- `checked_cast<B>(v)` reports `overflow` or `rounding_error` whatever `B`'s
  policy is.
- `unchecked_cast<B>(v)` makes the fit a precondition.

Each is a store into `B`'s grid under a fixed policy, so each is ill-formed
where that store could never succeed. The predicates `conversion_overflows<B>(v)`,
`conversion_rounds<B>(v)` and `conversion_loses<B>(v)` say beforehand what a
store of `v` into `B` would do under `B`'s own policy; a rounding policy that
brings an out-of-range value onto the grid makes it a rounding, not an
overflow.

Out of an `inside`, `to<T>()` returns `std::expected<T, errc>`. An integer `T`
gets the value exactly; a fractional value needs permission to round, as a
store does: it rounds by the call's mode (`to<int, round_floor>()`), else by
the type's, and with neither it is `rounding_error`. `overflow` reports a value
past `T`'s range
(`domain_error` for a negative value into an unsigned `T`); a floating-point
`T` gets the value rounded once. `as<T>()` returns `T` and reports a failure
like a checked store (`inside_error` with `to<T>()`'s code), and requires
a rounding policy for a floating-point `T`. `numerator()` and `denominator()`
give the exact value in lowest terms, the sign on the numerator, as integers
wide enough for every value of the grid.

## Per-call policies and actions

A type's policy is the default for its stores. Two narrower scopes name
another:

- **One call.** The functions `add`, `sub`, `mul`, `div`, `mod` and `divmod`
  compute what the operators compute, with trailing arguments: a policy object
  (`div(a, b, rounded_floor)`, or `make_policy<F>()`), an `errc&` that receives
  a failure instead of an exception, or actions. `divmod` returns
  `divmod_result{Quotient, Remainder}` from one division, consistent with the
  rounding: `q·b + r == a`.
- **One store.** `b.with_clamp()`, `b.with_wrap()`, `b.with_snap<M>()` and
  `b.policy<F>()` return a proxy whose `=`, `+=`, `-=`, `*=`, `/=` and `%=` store
  into `b` under `P | F`. `b.policy<F>(ec)` reports into `ec`. On a temporary,
  the proxy owns the value, so `(a * b).with_snap()` converts to any `inside`
  that the snapped value fits.

An *action* is a callback that runs when the policy acts, so a program can see
what it did. `b.on_wrap(f) += x` stores into `b` with `f` wired in:

| Action | Runs when | Called as |
|---|---|---|
| `on_clamp(f)` | a stored value leaves the range and is saturated | `f(b, overshoot)` |
| `on_wrap(f)` | a stored value leaves the range and is folded back | `f(b, carry)`; the carry is an `inside` whose grid holds every possible carry |
| `on_error(f)` | a checked store fails, or a compound assignment does (a zero divisor in `/=`); replaces the exception | `f(b, errc, message)` |

`f` receives the destination by reference and may change it. An action implies
its flag (`on_clamp` adds `clamp`, `on_wrap` adds `wrap`). A store takes one
of the three paths, so a proxy carries one action. The arithmetic functions
take no actions: they cannot fail but for a zero divisor, which stays in the
`expected`. An action that is not used costs nothing.

## Generic code

`std::common_type` of two `inside` types is the *grid hull*: the smallest grid
holding both (the interval hull, with the gcd of the notches refined by the
offset between the lattices), under the policy both share, or the default
policy when they differ. Of a type with itself, it is that type. So `std::max<std::common_type_t<A, B>>(a, b)`, ranges
of mixed grids and other generic code compile.

`std::numeric_limits<inside<G, P>>` describes the grid. `min()` and `lowest()`
are `lower`, `max()` is `upper`; `is_signed` holds when `lower < 0`, and
`is_integer` when every value is an integer. `is_modulo` follows `wrap`,
`traps` the checked policies, and `round_style` the policy's rounding mode.
The type is exact and bounded, with no infinity and no NaN. An exact type has
no rounding error, so `epsilon()` and `round_error()` are 0, as for the integer
types; they exist only where 0 is a value of the grid, so generic code on any
other grid gets a compile error rather than a stand-in. `digits` counts the bits
of the narrowest integer that numbers every value, less a sign bit when the grid
holds negatives (8 for `{0, 255}`, 7 for `{-128, 127}`), independent of the
storage.

`std::hash<inside<G, P>>` hashes the value, so an `inside` is a key of the
unordered containers.

# Proposed interface

The names are the reference implementation's (see [Open
questions](#open-questions)). `@*grid-number*@` is exposition-only: an exact
rational of any size. `@*number*@` is an arithmetic type or a
`@*grid-number*@`. `@*integer*@` is an integer type wide enough for the value.

```cpp
namespace std {
  // grids
  template<intmax_t D> constexpr @*grid-number*@ per;           // 1/D
  template<char... Cs> consteval @*grid-number*@ operator""_g();   // exact: 1e30_g, 0.1_g; suffix to be renamed
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
    @*grid-number*@ MaxDenominator;                              // a continuous grid's bound, else 0
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
  template<grid G, policy_flag P = checked>               // G continuous: MaxDenominator != 0
  struct inside {
    constexpr inside() = default;                         // trivial
    template<class A> constexpr inside(A value);          // A arithmetic or inside: a store by P
    template<policy_flag F = none, class A>               // the store by P | F
      static constexpr expected<inside, errc> try_make(A value);

    // out of an inside
    template<class T, policy_flag Round = none>                // T arithmetic; Round: none or a mode
      constexpr expected<T, errc> to() const;
    template<class T, policy_flag Round = none> constexpr T as() const; // to<T, Round>(), failures throw
    constexpr @*integer*@ numerator() const;                     // lowest terms, the sign here
    constexpr @*integer*@ denominator() const;                   // > 0

    // a policy or actions for one store (the proxy owns the value of an rvalue)
    template<policy_flag F = none> constexpr @*store-proxy*@ policy();
    template<policy_flag F = none> constexpr @*store-proxy*@ policy(errc& ec);
    template<policy_flag M = snap> constexpr @*store-proxy*@ with_snap(); // M: a rounding mode
    constexpr @*store-proxy*@ with_clamp();
    constexpr @*store-proxy*@ with_wrap();
    template<class F> constexpr @*store-proxy*@ on_clamp(F f);  // also on_wrap, on_error

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

  // conversions
  template<class B, policy_flag Round = none, class N> constexpr B clamp_cast(N v); // Round: none or a mode
  template<class B, class N> constexpr B wrap_cast(N v);
  template<class B, class N> constexpr B checked_cast(N v);
  template<class B, class N> constexpr B unchecked_cast(N v);
  template<class B, class N> constexpr bool conversion_overflows(N v) noexcept;
  template<class B, class N> constexpr bool conversion_rounds(N v) noexcept;
  template<class B, class N> constexpr bool conversion_loses(N v) noexcept;  // either
  template<class T, policy_flag Round = none, class B> constexpr expected<T, errc> to(const B&);
  template<class T, policy_flag Round = none, class B> constexpr T as(const B&);

  // a policy for one call
  template<policy_flag F = none, class E = @*unspecified*@> struct policy;
  template<policy_flag F = none> constexpr policy<F> make_policy();
  template<policy_flag F = none> constexpr policy<F, @*unspecified*@> make_policy(errc& ec);
  inline constexpr policy<@*see below*@> rounded_toward_zero, rounded_nearest, rounded_floor,
                                       rounded_ceil, rounded_half_even, clamped, wrapped;
  template<class L, class R, class... Args>                   // Args: none, a policy or an errc&;
    constexpr auto add(const L&, const R&, Args&&...);        // also sub, mul, div, mod
  template<class Q, class R> struct divmod_result { Q Quotient; R Remainder; };
  template<class L, class R, class... Args>
    constexpr auto divmod(const L&, const R&, Args&&...);     // divmod_result, or expected of it

  // generic code
  template<grid G, policy_flag P> struct numeric_limits<inside<G, P>>;
  template<grid G, policy_flag P> struct hash<inside<G, P>>;
  template<grid G1, policy_flag P1, grid G2, policy_flag P2>
    struct common_type<inside<G1, P1>, inside<G2, P2>>;       // the grid hull
}
```

## Semantics

| Expression | Result | Notes |
|---|---|---|
| `inside<G, P>{v}` | stores `v` by `P` | ill-formed when no value of `v`'s type can fit (disjoint intervals without `clamp`/`wrap`, incompatible notch without a rounding) |
| `B::try_make<F>(v)` | `expected<B, errc>` | the store under `P | F`, reporting instead of throwing |
| `-a` | `inside<-G, P>` | mirrored interval, same notch |
| `a + b`, `a - b` | interval sum or difference; notch `gcd(n_a, n_b)` | continuous if either operand is, with denominator bound `D_a·D_b` |
| `a * b` | interval product; notch `n_a·n_b`, refined by the operands' offsets | a point `c` scales the other notch by `|c|`; continuous as for `+` |
| `a / b` | a point `c`: `a`'s notch scaled by `1/|c|`; else continuous with denominator bound `D_a·max|b|·D_b` | rounded onto the integers or the shared notch when a policy names a rounding; `expected` iff `b`'s grid holds 0 |
| `a % b` | `[0, m]` or `[−m, m]`, `m = max|b| − 1` | integer grids required; `r = a − q·b` for `q` truncated or rounded by the policy |
| `e op b`, `a op e` (`e` an `expected`) | `expected` of `a op b`'s result | the first error passes through |
| `++a`, `--a` | `a` moved by one notch | a store by `P` (wraps under `wrap`); absent on a continuous grid |
| `a += b` etc. | `a = a + b` | a store by `a`'s policy |
| `a == b`, `a <=> b` | exact comparison of values | any two grids |
| `b.to<T>()` | `expected<T, errc>` | integer `T`: exact, or rounded by `Round`, else by `P`'s mode; `rounding_error` for a fractional value when neither names one; `overflow` out of `T`'s range, `domain_error` for a negative value into an unsigned `T`. Floating-point `T`: rounded once |
| `b.as<T>()` | `T` | a failure of `to<T>()` throws `inside_error`; a floating-point `T` needs a rounding policy |
| `b.numerator()`, `b.denominator()` | the value in lowest terms | denominator `> 0` |
| `clamp_cast<B, M>(v)` | `v` saturated into `B`, rounded by `M` | `M == none`: off the notch as `B`'s policy |
| `wrap_cast<B>(v)` | `v` modulo `B`'s interval | |
| `checked_cast<B>(v)` | `v` | throws `inside_error` (`overflow`, `rounding_error`) |
| `unchecked_cast<B>(v)` | `v` | precondition: `v` fits |
| `conversion_overflows<B>(v)`, `conversion_rounds<B>(v)` | what `B{v}` would report or round | a rounding that brings `v` in is a rounding |
| `op(a, b, policy)` (`op` one of `add` … `mod`) | `a op b`, the store and the division under `P | F` | `errc&`: a zero divisor sets it (and stays in the `expected`) |
| `divmod(a, b, …)` | `{q, r}`, `q·b + r == a` | as `/` and `%` |
| `b.with_clamp() = v` etc. | stores `v` into `b` under `P | F` | an rvalue's proxy converts to any `inside` the value fits |
| `b.on_wrap(f) += v` etc. | `b += v`, `f` called when the policy acts | ill-formed: two of `on_clamp`, `on_wrap`, `on_error`, or two of one kind |
| `common_type_t<A, B>` | `inside<hull(grid_of<A>, grid_of<B>), P>` | `P`: the shared policy, else `checked`; `A` when `A == B` |
| `numeric_limits<B>::min()`, `max()` | `lower`, `upper` | `epsilon()`, `round_error()`: 0, only when 0 is on the grid; `digits` from the number of values |

# Future work

The reference implementation provides more than this paper proposes. Each item
below is a candidate for a later paper:

1. **Text**: `to_string`, `parse`, `parse_exact`, `std::formatter` and stream
   operators, printing and reading the exact value.
2. **Algorithms**: `sum<Target>` with one deferred check, `add_all`,
   `mul_all`, `mul_into<Out>` / `div_into<Out>` (one rounding, no intermediate
   grid), `min`, `max`, `midpoint`, `lerp`, `dot` and `cross`.
3. **Iterating a grid**: `cursor<T, Step>` for loops, the range
   `inside_range` and `values<T>` (every value of `T`), and `uniform<B>(rng)`,
   which samples a grid exactly.
4. **Predefined types**: `uint8` … `int64` on their full ranges,
   `unorm8`/`unorm16`, the Q formats (`q8_8`), `clamp_counter` and
   `wrap_counter`.
5. **A public exact rational type** and its literals.

The companion papers propose quantities and the math functions.

# Implementation experience

The reference implementation is header-only C++26 and needs static reflection:
today GCC 16 with `-freflection`. It ships a single-header amalgamation, a
CMake package and a vcpkg port, and follows the Beman Standard. Its test suite
(868 tests) runs in CI in seven configurations on every change (a
debug preset, release, debug with `-Werror`, coverage, the address, undefined
and leak sanitizers, no floating point, strict SFINAE) and in four more every
week (a release preset, debug, the thread sanitizer, and the full certification
of the math functions). It
includes a check that the integer fast paths compile to call-free, vectorising
code, a property fuzzer, and a differential oracle that recomputes random
results exactly in Python.

Release v0.5.0 is the same library with 64-bit grid numbers, for C++23
compilers (GCC 14+, Clang 19+, Clang 18 with libc++, AppleClang, MSVC 19.51+),
tested in 50 CI configurations. Its measurements carry over: integer-aligned
arithmetic compiles to the same instructions as `int` (paper, "The fast path is
byte-identical to native"), `inside<unsafe>` runs at native speed, and
`inside<checked>` at 88% of the fastest hand-written range-enforcing code
(paper, "The like-for-like comparison").

# Prior art

- **bounded::integer** (David Stone): integer ranges in the type, with widening
  arithmetic. `inside` generalises the idea to rational grids.
- **Ada** fixed-point and range types: a range and a step (`delta`) in the type
  declaration, with the step a power of two unless specified.
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
   standard literal suffix cannot begin with `_`, so `_ins` and `_g` need new
   names.
2. **Failure of a checked store.** An exception (`inside_error`, as proposed),
   a contract violation ([@P2900R14]), or a replaceable handler (the
   implementation has one, which lets the core run without exceptions).
3. **Bare scalars.** Rejecting `a + 1` is deliberate. Should mixed arithmetic
   with an integer of a stated range be allowed instead?
4. **The per-call interface.** Trailing arguments on free functions for one
   call, and a proxy for one store, are the implementation's shape. The proxy
   type is left unspecified.
5. **Interaction** with C++26 saturation arithmetic and with `std::linalg`
   element types.
