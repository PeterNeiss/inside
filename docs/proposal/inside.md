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

# Abstract

We propose a class template `inside<grid, policy>` for numbers whose interval
and step size (the *notch*, an exact rational) are compile-time properties of
the type. Arithmetic computes its result grid at compile time, so `+`, `-` and
`*` on a lattice cannot overflow. Division is exact; a rounded division names
its mode at the call, and division returns a failure value only where the
divisor's grid holds zero. What a store into a narrower grid does (check, clamp, wrap or round) is
part of the declaration. A single call or a single store can name another
policy, and an action can observe what the policy did: the overshoot of a
clamp, the carry of a wrap. Named conversions, `std::numeric_limits`,
`std::hash` and `std::common_type` complete the type for generic code.

This is the first of three papers, and it stands alone. Two companion papers
build on it: [*Quantities on
`inside`*](https://github.com/PeterNeiss/inside/blob/v0.10.0/docs/proposal/quantity.md)
puts units on the number, and [*Correctly rounded math for `inside` and
quantities*](https://github.com/PeterNeiss/inside/blob/v0.10.0/docs/proposal/math.md)
adds the functions of `<cmath>`.

The reference implementation is
[inside v0.10.0](https://github.com/PeterNeiss/inside/tree/v0.10.0). A
long-form motivation with compiled exhibits is [*int considered
harmful*](https://github.com/PeterNeiss/inside/blob/v0.5.0/docs/int-considered-harmful.pdf)
(the *paper* below).

# Revision history {- .unlisted}

R0: first revision.

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
- C++26 saturation arithmetic ([@P0543R3]{.title}, with the names of
  [@P4052R0]: `saturating_add`, `saturating_cast`) picks one edge policy for
  the full range of a built-in type.

What each of them lacks is the same: the range a value may occupy, its step,
the rounding rule and the edge behaviour. `inside` records all four in the
type, where the compiler can act on them. Because the compiler knows the range
of every operand, it knows the range of every result, and most checks become
proofs.

The same gap shows up one layer higher. A units library converts a quantity by
multiplying its number, and a conversion from kilometres to millimetres can
overflow an `int` that the type cannot describe. When SG6 reviewed the units
library [@P3045R10] at Brno, the question came up whether a quantities library
should take on integer-overflow avoidance at all or leave it to a dedicated
integer type; the room agreed that the library should go the extra mile to
prevent overflow [@BRNO2026]. A number type that knows its range makes that
protection exact, and for a conversion free: the companion quantity paper
shows how.

::: cmptable
### Today
```cpp
int add_percent(int a, int b) {         // 0..100 each, by convention
  return a + b;                         // 0..200, if every caller obeys
}
auto volume = std::saturating_cast<std::uint8_t>(v);  // saturates at 255, not 10
double total = 0.1 + 0.2;               // 0.30000000000000004
int third = 100 / 3;                    // 33: the rounding is implicit
```

### With `inside`
```cpp
using percent = inside<{0, 100}>;
auto add_percent(percent a, percent b) {
  return a + b;                         // inside<{0, 200}>, by type
}
inside<{0, 10}> volume{v, clamp};       // saturates at 10
auto total = 0.1_q + 0.2_q;             // exactly 3/10
auto third = div(100_q, 3_q, round_floor);  // 33: the rounding is named
```
:::

# A tour

The listings use the reference implementation's namespace `ins`.
Each one is a region of a program that compiles and checks its own results.

**The range is part of the type, and arithmetic widens it.**

<!-- listing: range -->
```cpp
using percent = id<"[0..100]">;   // the integers 0..100, in one byte

percent a = 42, b = 58;
auto    sum  = a + b;               // id<"[0..200]">
auto    diff = a - b;               // id<"[-100..100]">
auto    neg  = -a;                  // id<"[-100..0]">: no -INT_MIN
static_assert(std::is_same_v<decltype(sum), id<"[0..200]">>);
static_assert(sizeof(percent) == 1);

percent c{sum - 100_q, clamp};  // -100..100 into 0..100: the store names what happens
auto    bad = percent::try_make(sum); // expected<percent, error>
```

**The notch is an exact rational, and the rounding is named in the type.**

<!-- listing: money -->
```cpp
using cents = id<"[0..1'000'000] 1/100 round_half_away">;

cents price = 0.1_q, tax = 0.2_q;       // exactly 1/10 and 2/10
auto  total = price + tax;              // exactly 3/10 — no 0.30000000000000004
static_assert(notch_of<decltype(total)> == per<100>);

cents share = total / 3_q;            // 1/10 exactly
cents third = cents{1} / 3_q;         // 1/3 rounds to nearest: 0.33
```

**The policy decides what a narrowing store does.**

<!-- listing: wrap -->
```cpp
using degrees = id<"[0..359] wrap">;      // modular
using level   = id<"[0..10] clamp">;      // saturating
using count   = id<"[0..10]">;            // checked (the default)

degrees heading = 350;
heading += 20_q;                        // 370 wraps to 10
level   volume  = 9;
volume += 5_q;                          // 14 clamps to 10

auto n = count::try_make(14);             // unexpected(error::overflow)
// count m = 14;                          // ill-formed: 14 is outside [0, 10]
```

**The lowest value need not be a multiple of the notch.**

<!-- listing: offset -->
```cpp
using centre = id<"[0.5..255.5] 1">;         // 0.5, 1.5, ..., 255.5
static_assert(sizeof(centre) == 1);          // 256 points: one byte

centre x = 2.5_q, y = 0.5_q;
auto   s = x + y;                            // id<"[1..511]">: whole numbers again
auto   p = x * y;                            // 0.25, 0.75, ...: Lower 1/4, notch 1/2
static_assert(std::is_same_v<decltype(s), id<"[1..511]">>);
static_assert(lower_of<decltype(p)> == per<4> && notch_of<decltype(p)> == per<2>);
```

**Division is exact, and fallible only when the divisor may be zero.**

<!-- listing: division -->
```cpp
using num  = id<"[0..100]">;
using any  = id<"[0..10]">;                // may be zero
using some = id<"[1..10]">;                // never zero

auto q = num{7} / some{3};                 // exactly 7/3, a plain value
auto r = num{7} / any{3};                  // std::expected<..., error>
auto z = num{7} / any{0};                  // unexpected(error::division_by_zero)
static_assert(!std::is_same_v<decltype(q), decltype(r)>);

auto f = div(num{7}, some{3}, round_floor);  // 2: rounding named at the call
auto m = mod(num{7}, some{3}, round_floor);  // 1, consistent: 2·3 + 1 == 7
```

**A conversion names what it does with a value that does not fit.**

<!-- listing: convert -->
```cpp
using level  = id<"[0..10]">;
using sensor = id<"[0..1000]">;
sensor raw   = 725;

static_assert(conversion_overflows<level>(725));
auto top  = level{raw, clamp};                      // 10: saturated
auto deg  = id<"[0..359]">{raw, wrap};              // 5: 725 mod 360
auto fit  = exact_cast<level>(sensor{7});              // expected<level, error>: 7
auto near = level{6.5, clamp | round_half_away};       // 7: a tie goes away from zero

using quarter = id<"[0..10] 1/4">;
quarter q = 2.75_q;
auto num  = q.numerator();                               // 11: the value is 11/4
auto i    = q.to<int>();                                 // unexpected(error::rounding_error)
auto down = q.to<int, round_floor>();                    // 2: rounding named in the call
auto neg  = id<"[-5..5]">{-1}.to<unsigned>();            // unexpected(error::overflow)
```

**A call or a single store can take another policy, and an action sees what
the policy did.**

<!-- listing: per_call -->
```cpp
using whole = id<"[0..100]">;
using step  = id<"[1..10]">;                           // never zero

auto down       = div(whole{7}, step{2}, round_floor); // 3: this call rounds down
auto up         = div(whole{7}, step{2}, round_ceil);  // 4
auto [quo, rem] = divmod(whole{17}, step{5});            // 3 and 2

whole x = 50;
x.with<clamp>() = 150;                                      // 100: this store saturates

using seconds = id<"[0..59] wrap">;
using minutes = id<"[0..59] clamp">; // m += carry names what happens past 59
seconds s = 50;
minutes m = 0;
s.on_wrap([&](auto&, auto carry) { m += carry; }) += 75_q; // 125 s: s == 5, m == 2
```

**Generic code sees a numeric type.**

<!-- listing: generic -->
```cpp
using small = id<"[0..10]">;
using wide  = id<"[-50..50]">;

using common = std::common_type_t<small, wide>;          // the hull of the two grids
static_assert(std::is_same_v<common, id<"[-50..50]">>);
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

A default-constructed `inside` holds a value of its grid: 0 when the grid
holds 0, else `Lower`. A zero-filled storage would not do: it decodes to
`Lower` on an index, and to 0 (out of range) on `{5, 100}` stored by value.

## Arithmetic widens

The result of `a + b`, `a - b`, `a * b` and `-a` is an `inside` whose grid holds
every possible result. `[0, 100] + [0, 100]` is `[0, 200]`, and negation mirrors
the interval. A continuous result's denominator bound follows from the
operands': `D_a·D_b` for `+`, `-` and `*`, where `D` of a lattice is the lcm
of the denominators of its lower limit and its notch. So no operation produces a value outside its result type,
and these operators need no overflow check and always return a plain `inside`.
The result keeps the policy its operands share, a point of the default policy
counting as neutral; different policies give the default. A rounding mode in
the operands' type therefore survives an intermediate step, and still governs
that value's `to<T>()` and its compound assignments.

A chain of exact operations on fractions multiplies the bounds. The reference
implementation gives a result whose fractions would need more than 1024 bits
(a fixed limit, so that every translation unit forms the same types) no type: the operator is deleted, and its message names
the fix, which is to round an operand, or the product with `mul<Out>`, into a
coarser type.

## Division

`a / b` is exact by default. A point divisor `c` scales the dividend's lattice
by `1/|c|`; any other quotient lies on a continuous grid whose denominators are
bounded — x = p/a over y = r/b is p·b/(a·r), so they are at most the dividend's
largest denominator times the divisor's largest numerator. That bound travels
with the type through `+`, `-` and `*`, so exact arithmetic on quotients stays
plain and its precision is known at compile time. The operators take no rounding
from their operands' policies: a rounded division names its mode at the call
(`div(a, b, round_floor)`), where an integer division rounds onto the integers
and any other dividend on a lattice rounds onto the multiples of its notch (a
division of two values on the same notch `1/K` onto that notch).
`a % b` takes two lattices; its remainder belongs to the quotient truncated
(`mod(a, b, mode)`: rounded by the mode), and lies on the lattice of the gcd
of the two operands' value units (the largest rational that every value of a
grid is an integer multiple of: its notch, or finer on a grid whose lower limit
is off the notch). Either operator returns `std::expected<inside, error>` when
the divisor's grid holds zero, and a plain `inside` when it does not.

## Policies

A policy says what a store into a grid does with a value that is not on it.

| Policy | Out of range | Off the notch |
|---|---|---|
| `checked` (default) | reports `error::overflow` | reports `error::rounding_error` |
| `clamp` | the nearest limit | (as checked) |
| `wrap` | modulo the interval | (as checked) |
| `round_toward_zero`, `round_half_away`, `round_floor`, `round_ceil`, `round_half_even` | (as checked) | rounds |
| `unsafe` | precondition | truncates |

Flags combine (`clamp | round_half_away`). A store that can never succeed (a
source interval disjoint from the target without `clamp` or `wrap`, or a source
notch that does not land on the target's without a rounding) is ill-formed.
`round_half_away` sends a tie away from zero; `round_half_even` to the even
point, counted from the lattice's anchor.

## No bare scalars

`a + 1` is ill-formed, with a diagnostic that names the fix. `1` has no grid,
so the result grid cannot be computed. Constants are written `1_q`, which are point grids. An arithmetic value enters an `inside` only
through a store, which the grids prove or the store's channel checks.

Compound assignment is the one exception: `x += n`, `x -= n` and `x *= n` take
a builtin integer `n`. The result has a type, `x`'s, so `n` needs no grid of its
own; it enters as the inside on its type's range, and the result is stored back
under the rules of every store, so a checked `x` names a channel
(`x.on_error(ec) += n`). Division stays with insides, because a builtin
integer's range holds 0.

## Checked by default, `std::expected` only at boundaries

Every policy checks a narrowing store unless it says `unsafe`, and nothing
throws. A store has five outcomes: it clamps, wraps, takes an alternative
value, reports, or does not compile. A store the grids prove (every value of
the source fits) is plain. Any other store names its channel before it runs:
a flag (`clamp`, `wrap`, a rounding mode), an `error&` (`b.on_error(ec) = v`), an
`on_error` action, `try_make`, or `unsafe`; without one it is ill-formed. A
constant is checked while compiling: the constructor from a value the grids
cannot prove is `consteval`, so `count m = 14;` with `count` on `[0, 10]` is
a compile error, and `count m = n;` for a run-time `n` is one too. The checks
that remain are the ones the grids cannot discharge. On a chain of operations
the library therefore emits fewer checks than a hand-written checked integer,
and is 21% faster than a native checked type in the paper's benchmark ("Where
are the checks?").

Fallible results are values only where failure is possible at run time:
`try_make`, `to<T>()` where `T` cannot hold every value, `from_step(i)` where
`i`'s grid reaches past the steps, and division by a divisor whose grid holds
zero. A total operation returns a plain value, so the type says whether
failure is possible. A function that stores
into an output it names (`mul<Out>`, `sum<T>`, `exact_cast<T>`, `f<Out>(x)`)
returns `expected<Out, error>` exactly where the result can miss `Out`.

Fallible results chain. The operators also accept a `std::expected<inside,
error>` operand, so `a / b * gain + offset` stays one `expected` from end to
end. The first error passes through with its cause, and an operation that fails
later in the chain reports its own. An `inside` never stores an `expected`
implicitly: `B x = a / b;` is ill-formed, and `B::try_make(a / b)` passes the
error on. What is left is a precondition with no call to report through (a
cursor stepped past its end, an index past a dynamic extent): it reaches a
replaceable handler, one per program, which prints and aborts by default;
installing one is safe while other threads raise.

## Conversions

A store applies the target's policy. A conversion names its own, so the reader
sees at the call what happens to a value that does not fit:

- `B{v, clamp}` saturates into `B`'s interval; `B{v, clamp | round_half_away}` (or
  another rounding mode) also rounds onto `B`'s notch. The flags of `B{v, F}`
  replace `B`'s own of the same kind.
- `B{v, wrap}` folds `v` modulo `B`'s interval.
- `exact_cast<B>(v)` stores `v` exactly whatever `B`'s policy is: `B`
  where the grids prove it, else `expected<B, error>` with `overflow` or
  `rounding_error`.
- `B{v, unsafe}` makes the fit a precondition.

Each is a store into `B`'s grid under a fixed policy, so each is ill-formed
where that store could never succeed. The predicates `conversion_overflows<B>(v)`,
`conversion_rounds<B>(v)` and `conversion_loses<B>(v)` say beforehand what a
store of `v` into `B` would do under `B`'s own policy; a rounding policy that
brings an out-of-range value onto the grid makes it a rounding, not an
overflow.

Out of an `inside`, `to<T>()` returns `T` where the grid proves the
conversion total (every value an integer in `T`'s range, or a rounding named;
every floating-point `T`), else `std::expected<T, error>`. An integer `T`
gets the value exactly; a fractional value needs permission to round, as a
store does: it rounds by the call's mode (`to<int, round_floor>()`), else by
the type's, and with neither it is `rounding_error`. `overflow` reports a value
past `T`'s range, a negative value into an unsigned `T` included; a floating-point
`T` gets the value rounded once. `numerator()` and `denominator()`
give the exact value in lowest terms, the sign on the numerator, as insides on
the integers that hold those parts of every value of the grid. `step()` gives
a value's step (its count of notches above the lower limit) as a `step_t<B>`,
the inside on `{0, step_count}`, and `B::from_step(i)` goes back: it is plain
where `i`'s grid lies in `[0, step_count]`, else an `expected`. Run-time values are insides: a
`grid_number` is a compile-time value, its arithmetic `consteval`, so `N - 1`
with a constant `N` is evaluated while compiling wherever it appears, and
counting at run time stays exact on any grid, past 2⁶⁴ points too.

## Per-call policies and actions

A type's policy is the default for its stores. Two narrower scopes name
another:

- **One call.** The functions `add`, `sub`, `mul`, `div`, `mod` and `divmod`
  compute what the operators compute, with trailing arguments: a flag or a
  policy (`div(a, b, round_floor)`, or `make_policy<F>(ec)`), or an `error&`
  that receives a failure. `add`, `sub` and `mul` cannot fail and take the
  argument only for symmetry; `div` and `mod` round by its mode. `divmod` returns
  `divmod_result{Quotient, Remainder}` from one division, consistent with the
  rounding: `q·b + r == a`.
- **One store.** `b.with<F>()` returns a proxy whose `=`, `+=`, `-=`, `*=`,
  `/=` and `%=` store into `b` under `P | F` (`b.with<clamp>() = v`). `b.on_error<F>(ec)` reports into `ec`. On a temporary,
  the proxy owns the value, so `(a * b).with<round_toward_zero>()` converts to any
  `inside` that the rounded value fits.

An *action* is a callback that runs when the policy acts, so a program can see
what it did. `b.on_wrap(f) += x` stores into `b` with `f` wired in:

| Action | Runs when | Called as |
|---|---|---|
| `on_clamp(f)` | a stored value leaves the range and is saturated | `f(b, overshoot)`; the overshoot is an `inside`, the source minus the limit, exactly |
| `on_wrap(f)` | a stored value leaves the range and is folded back | `f(b, carry)`; the carry is an `inside` whose grid holds every possible carry |
| `on_error(f)` | a checked store fails, or a compound assignment does (a zero divisor in `/=`); the store's channel | `f(b, error, message)` |

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
other grid gets a compile error rather than a stand-in. `digits` is the
number of bits whose every pattern names a value of the grid, `⌊log₂ n⌋` for
`n` values, less a sign bit when the grid holds negatives (8 for `{0, 255}`, 7
for `{-128, 127}`, 6 for `{0, 100}`), independent of the storage.

`std::hash<inside<G, P>>` hashes the value, so an `inside` is a key of the
unordered containers.

## Alternatives considered

- **Bounds checked at run time.** A constrained number that checks each
  assignment against a run-time or declared constraint [@P2993R0] cannot
  compute the type of `a + b`, so every operation stays a check. Here the
  bounds are compile-time values, and most checks become proofs.
- **A binary exponent instead of a rational notch.** Fixed-point types with a
  power-of-two step [@P0037R7] cannot hold cents or degrees exactly. The notch
  is any rational, and a power of two is one case of it.
- **Integer ranges only.** Bounded integers widen as `inside` does, but stop
  at the integers. An `inside` on an integer grid is that type.
- **A failed store that throws.** The reference implementation's checked
  stores threw until v0.8. Now a store has no failure path of its own: it
  clamps, wraps, rounds, reports through a channel named before it, or does
  not compile. This works without exceptions and freestanding.
- **Mixed arithmetic with builtin numbers.** `a + 1` would need a grid for `1`,
  and every literal would widen the result to its type's range. Constants are
  points instead (`1_q`), and only compound assignment takes a builtin
  integer, because its result has a type.
- **Grid numbers of 64 bits.** Release v0.5.0 had them. Result grids then
  stop existing (`10¹⁸ · 10¹⁸`), and a valid expression becomes ill-formed.
- **Storage chosen by flags.** Earlier versions let a type pick an index or a
  value encoding. Storage now follows from the grid, so equal grids are one
  type.
- **Division rounding read from the operands' types.** Earlier versions
  divided by the operands' rounding mode. Now `/` and `%` are always exact
  and truncated, and a rounded division names its mode at the call, so the
  meaning of `a / b` does not depend on declarations elsewhere.

# Proposed interface

The names are the reference implementation's, except for the few types it
keeps internal, which this paper names ([Implementation
experience](#implementation-experience) lists them); see also [Open
questions](#open-questions). A `grid_number` is an exact rational of any size
and a structural type; its arithmetic is `consteval`, so a grid number exists
only while compiling, and a run-time value is an `inside`.

```cpp
namespace std {
  // grid numbers
  class grid_number {                                    // structural: equal values are one template argument
  public:
    constexpr grid_number() noexcept;                    // 0
    template<integral T> constexpr grid_number(T v);
    template<floating_point T> constexpr grid_number(T v);  // the exact binary value; v finite
    template<class B> constexpr grid_number(const B& b);    // B an inside: b's exact value
    friend consteval grid_number operator-(const grid_number&);
    friend consteval grid_number operator+(const grid_number&, const grid_number&);  // also -, *, /
    friend constexpr bool operator==(const grid_number&, const grid_number&);
    friend constexpr strong_ordering operator<=>(const grid_number&, const grid_number&);
  };
  template<auto D> constexpr grid_number per = grid_number(1) / grid_number(D);  // D an integer >= 1
  template<char... Cs> consteval grid_number operator""_g();  // exact: 1e30_g, 0.1_g; suffix to be renamed

  // grids
  struct interval {
    grid_number Lower, Upper;                            // Lower <= Upper
  };
  enum class bounds : unsigned char { closed, half_open, open };  // a spelling: grids stay closed
  struct max_denominator {
    grid_number Value;                                   // a positive integer
    constexpr explicit max_denominator(convertible_to<grid_number> auto v);
  };
  struct grid {
    interval    Interval;
    grid_number Notch;                                   // >= 0, divides Upper - Lower
    grid_number MaxDenominator;                          // a continuous grid's bound, else 0
    grid() = default;
    // the limits are templates, so a braced {lower, upper} selects the interval forms
    constexpr grid(convertible_to<grid_number> auto lower, convertible_to<grid_number> auto upper,
                   convertible_to<grid_number> auto notch);
    constexpr grid(convertible_to<grid_number> auto lower,
                   convertible_to<grid_number> auto upper);  // notch gcd(1, lower, upper)
    constexpr grid(convertible_to<grid_number> auto lower, convertible_to<grid_number> auto upper,
                   convertible_to<grid_number> auto notch, bounds);
    constexpr grid(convertible_to<grid_number> auto lower, convertible_to<grid_number> auto upper,
                   bounds);
    constexpr grid(convertible_to<grid_number> auto point);   // notch 0, lower == upper
    constexpr grid(interval, grid_number notch);
    constexpr grid(interval, grid_number notch, bounds);
    constexpr grid(interval, max_denominator);           // continuous, bounded
    consteval grid_number step_count() const;            // (Upper - Lower) / Notch, an integer
    consteval grid operator-() const;                    // mirrored: [-Upper, -Lower]
    constexpr bool operator==(const grid&) const = default;
  };
  consteval grid operator+(const grid&, const grid&);    // also -, *, /: the result grids
  consteval grid hull(const grid&, const grid&);         // the smallest grid holding both
  consteval grid numerator_grid(const grid& g);          // the numerators of g's values in lowest terms
  consteval grid denominator_grid(const grid& g);        // their denominators

  // policies: each flag is a constant of its own type, so it is a template argument,
  // combines with |, and is a call's policy (div(a, b, round_floor))
  using policy_flag = unsigned long long;
  template<policy_flag V> struct policy_constant {
    static constexpr policy_flag value = V;
    constexpr operator policy_flag() const noexcept { return V; }
  };
  template<policy_flag A, policy_flag B>
    constexpr policy_constant<A | B> operator|(policy_constant<A>, policy_constant<B>) noexcept;
  // the bits are the reference implementation's; any distinct bits will do
  inline constexpr policy_constant<0>                         checked;   // the default: no bits
  inline constexpr policy_constant<1ull << 32>                clamp;
  inline constexpr policy_constant<1ull << 33>                wrap;
  inline constexpr policy_constant<1ull << 4>                 round_toward_zero;  // every mode has this bit
  inline constexpr policy_constant<(1ull << 5) | (1ull << 4)> round_half_away;
  inline constexpr policy_constant<(1ull << 6) | (1ull << 4)> round_floor;
  inline constexpr policy_constant<(1ull << 7) | (1ull << 4)> round_ceil;
  inline constexpr policy_constant<(1ull << 8) | (1ull << 4)> round_half_even;
  inline constexpr policy_constant<(1ull << 36) | (1ull << 4)> unsafe;   // no check; off the notch truncates

  // errors (no exception type: a run-time failure is a value or a precondition)
  enum class error { domain_error = 1, division_by_zero, overflow,
                     rounding_error, not_finite, invalid_format };
  constexpr const char* error_message(error) noexcept;
  using error_handler_t = void (*)(error code, const char* what);  // a precondition with no call to report through
  error_handler_t set_error_handler(error_handler_t) noexcept;      // default: print and abort; trap if freestanding
  error_handler_t get_error_handler() noexcept;

  // a policy for one call or one store
  struct empty_ref {};                                   // no channel
  struct error_ref {                                     // a failure sets Code (the first one is kept)
    constexpr error_ref(error& ec);
    error& Code;
  };
  template<policy_flag F = checked, class Channel = empty_ref>
  struct call_policy : Channel {
    static constexpr policy_flag Flags = F;
    constexpr call_policy() = default;
    constexpr call_policy(error& ec) requires same_as<Channel, error_ref>;
  };
  template<policy_flag F = checked> constexpr call_policy<F> make_policy();
  template<policy_flag F = checked> constexpr call_policy<F, error_ref> make_policy(error& ec);

  // actions: callbacks that see what a policy did
  template<class F> struct on_clamp_t { F Fn; };          // f(b, overshoot)
  template<class F> struct on_wrap_t  { F Fn; };          // f(b, carry)
  template<class F> struct on_error_t { F Fn; };          // f(b, error, message)

  // one store into b under the policy P (and at most one action)
  template<class B, class P, class... Actions>
  struct policy_ref {
    template<class V> constexpr B& operator=(const V& v);    // also += -= *= /= %=
    template<class T> constexpr operator T();                 // T an inside that the stored value fits
  };
  template<class B, class P, class... Actions>            // the same for a temporary: owns the value
  struct policy_buffer {
    template<class T> constexpr operator T();
  };

  // the type
  template<grid G, policy_flag P = checked>               // G continuous: MaxDenominator != 0
  struct inside {
    constexpr inside();                                   // 0, or Lower on a grid without 0
    template<class A> constexpr inside(A value);          // A arithmetic or an inside: a store the grids prove
    template<class A> consteval inside(A value);          // A arithmetic, not proven: a constant, checked
                                                          // while compiling
    consteval inside(const grid_number& value);           // a constant
    template<class A, class Pol>
      constexpr inside(A value, Pol p);                   // p a policy_constant or a call_policy: its rounding
                                                          // mode, clamp or wrap replace P's
    template<class A> constexpr inside& operator=(const A& value);  // as the constructor
    template<policy_flag F = checked, class A>
      static constexpr expected<inside, error> try_make(const A& value);  // also of expected<A, error>
                                                                          // and of a grid_number

    // out of an inside
    template<integral T, policy_flag Round = checked>
      constexpr auto to() const;                          // T where every value converts (an integer in
                                                          // T's range, or Round names a mode), else
                                                          // expected<T, error>
    template<floating_point T> constexpr T to() const;    // rounded once (no operator double)
    constexpr inside<numerator_grid(G)> numerator() const;      // lowest terms, the sign here
    constexpr inside<denominator_grid(G)> denominator() const;  // > 0
    constexpr step_t<inside> step() const noexcept;       // (value - Lower) / Notch; not continuous
    template<class I> static constexpr auto from_step(const I& i);  // Lower + i·Notch; I an inside on
                                                          // the integers or a builtin integer: inside
                                                          // where i's grid lies in [0, step_count],
                                                          // else expected<inside, error>
    template<class I, class Pol> static constexpr auto from_step(const I& i, Pol p);

    // one store under another policy, or with an action
    template<policy_flag F = checked>
      constexpr policy_ref<inside, call_policy<P | F>> with() &;
    template<policy_flag F = checked>
      constexpr policy_buffer<inside, call_policy<P | F>> with() &&;
    template<policy_flag F = checked>
      constexpr policy_ref<inside, call_policy<P | F, error_ref>> on_error(error& ec);
    template<class F> constexpr policy_ref<inside, call_policy<P | clamp>, on_clamp_t<F>> on_clamp(F f);
    template<class F> constexpr policy_ref<inside, call_policy<P | wrap>, on_wrap_t<F>> on_wrap(F f);
    template<class F> constexpr policy_ref<inside, call_policy<P>, on_error_t<F>> on_error(F f);

    constexpr inside operator+() const noexcept;
    constexpr inside<-G, P> operator-() const;
    constexpr inside& operator++();                       // one notch; not continuous
    constexpr inside  operator++(int);
    constexpr inside& operator--();
    constexpr inside  operator--(int);
    template<class R> constexpr inside& operator+=(const R& r);  // also -= *= /= %=: a store by P;
                                                          // R an inside, or a builtin integer for += -= *=
  };

  // observers
  template<class T> constexpr bool is_inside_v = false;
  template<grid G, policy_flag P> constexpr bool is_inside_v<inside<G, P>> = true;
  template<class T> concept insidable = is_inside_v<remove_cvref_t<T>>;
  template<class B> constexpr grid grid_of;
  template<class B> constexpr interval interval_of;        // also of a builtin integer type: its range
  template<class B> constexpr policy_flag policy_of;
  template<class B> constexpr grid_number lower_of, upper_of, notch_of;
  template<insidable B> requires(notch_of<B> != 0 || lower_of<B> == upper_of<B>)
    using step_t = inside<grid_of<B>.step_count() == 0 ? grid{0}
                                                        : grid{0, grid_of<B>.step_count(), 1}>;
  template<insidable B> requires(notch_of<B> != 0 || lower_of<B> == upper_of<B>)
    constexpr inside<grid{grid_of<B>.step_count()}> step_count_of{};  // the largest step, a point

  // constants: a literal is a point grid, exactly
  template<char... Cs> constexpr auto operator""_q();       // 3_q, 0.25_q, 0x1p-15_q; suffix to be renamed
  template<value-text T> consteval auto operator""_q();     // "2/3"_q a point, "42 [1..100)"_q a typed value

  // operators (L, R are inside types, or expected<inside, error>; a std::constant_wrapper<V>, std::cw<N>,
  // is the point V, so a value computed while compiling needs no spelling of its own)
  template<class L, class R> constexpr auto operator+(const L&, const R&);  // the widened result grid
  template<class L, class R> constexpr auto operator-(const L&, const R&);
  template<class L, class R> constexpr auto operator*(const L&, const R&);
  template<class L, class R> constexpr auto operator/(const L&, const R&);  // inside or expected
  template<class L, class R> constexpr auto operator%(const L&, const R&);
  template<class L, class R> constexpr bool operator==(const L&, const R&);
  template<class L, class R> constexpr auto operator<=>(const L&, const R&);

  // conversions
  template<class B, class A> constexpr auto exact_cast(A v);  // B, or expected<B, error> where v may not fit
  template<class B, class A> constexpr bool conversion_overflows(A v) noexcept;
  template<class B, class A> constexpr bool conversion_rounds(A v) noexcept;
  template<class B, class A> constexpr bool conversion_loses(A v) noexcept;  // either
  template<class T, policy_flag Round = checked, class B> constexpr auto to(const B& b);  // b.to<T, Round>()

  // one call under another policy: Args is empty, or one policy_constant, call_policy or error&
  template<class L, class R, class... Args> constexpr auto add(const L&, const R&, Args&&...);
  template<class L, class R, class... Args> constexpr auto sub(const L&, const R&, Args&&...);
  template<class L, class R, class... Args> constexpr auto mul(const L&, const R&, Args&&...);
  template<class L, class R, class... Args> constexpr auto div(const L&, const R&, Args&&...);
  template<class L, class R, class... Args> constexpr auto mod(const L&, const R&, Args&&...);
  template<class Q, class R> struct divmod_result { Q Quotient; R Remainder; };
  template<class L, class R, class... Args>
    constexpr auto divmod(const L&, const R&, Args&&...);  // divmod_result, or expected of it

  // generic code
  template<grid G, policy_flag P> struct numeric_limits<inside<G, P>>;
  template<grid G, policy_flag P> struct hash<inside<G, P>>;
  template<grid G1, policy_flag P1, grid G2, policy_flag P2>
    struct common_type<inside<G1, P1>, inside<G2, P2>>;  // the grid hull
}
```

## Semantics

| Expression | Result | Notes |
|---|---|---|
| `inside<G, P>{v}` | stores `v` by `P` | proven by the grids, or a constant (`consteval`); ill-formed when no value of `v`'s type can fit, and for a run-time `v` the grids cannot prove |
| `B::try_make<F>(v)` | `expected<B, error>` | the store under `P | F`, reporting |
| `-a` | `inside<-G, P>` | mirrored interval, same notch |
| `a + b`, `a - b` | interval sum or difference; notch `gcd(n_a, n_b)` | continuous if either operand is, with denominator bound `D_a·D_b` |
| `a * b` | interval product; notch `n_a·n_b`, refined by the operands' offsets | a point `c` scales the other notch by `|c|`; continuous as for `+` |
| `a / b` | a point `c`: `a`'s notch scaled by `1/|c|`; else continuous with denominator bound `D_a·max|b|·D_b` | always exact; `div(a, b, mode)` rounds onto the dividend's notch (the integers for an integer dividend), and a continuous dividend's quotient stays exact; `expected` iff `b`'s grid holds 0 |
| `a % b` | on the lattice of `u`, the gcd of the two value units (1 on integers), the values `r` can take: truncation `r` has `a`'s sign, `|r| ≤ min(m, max|a|)`; floor `b`'s sign, ceil the opposite, `|r| ≤ m`; nearest `|r| ≤ min(⌊max|b|/2u⌋·u, max|a|)`; `m = max|b| − u` | notched grids required; `r = a − q·b` for the integer `q` truncated or rounded by the policy |
| `e op b`, `a op e` (`e` an `expected`) | `expected` of `a op b`'s result | the first error passes through |
| `++a`, `--a` | `a` moved by one notch | a store by `P` (wraps under `wrap`); ill-formed where the step may leave the grid without `clamp`/`wrap`; absent on a continuous grid |
| `a += b`, `a -= b`, `a *= b` | `a = a op b` | a store by `a`'s policy: ill-formed where the grids cannot prove it and `P` has no `clamp`/`wrap`; `b` an inside or a builtin integer; `a.on_error(ec) += b` reports |
| `a /= b`, `a %= b` | the exact quotient stored by `P` (rounded by `P`'s mode); `mod(a, b, P)` | ill-formed when `b`'s grid holds 0 (also under `clamp` or `wrap`): `a.on_error(ec) /= b` reports a zero divisor |
| `a == b`, `a <=> b` | exact comparison of values | any two grids, or an inside and an integer; no floating-point operand (0.1 is not 1/10) |
| `b.to<T>()` | `T` where total, else `expected<T, error>` | integer `T`: exact, or rounded by `Round`, else by `P`'s mode; `rounding_error` for a fractional value when neither names one; `overflow` out of `T`'s range. Floating-point `T`: rounded once |
| `b.numerator()`, `b.denominator()` | the value in lowest terms, as insides | with `D` the grid's largest denominator: numerator in `[⌈L⌉, ⌊U·D⌋]` (`⌈L·D⌉` for `L < 0`, `⌊U⌋` for `U ≤ 0`), denominator in `[1, D]` |
| `b.step()`, `B::from_step(i)` | `(b − L)/n` as a `step_t<B>`; `L + i·n` | `i` an inside on the integers or a builtin integer; `from_step` is plain when `i`'s grid lies in `[0, step_count]`, else `expected<B, error>` (plain under a call's `clamp`/`wrap`); `step()` is ill-formed on a continuous grid |
| `B{v, clamp | M}` | `v` saturated into `B`, rounded by `M` | `M == checked`: off the notch as `B`'s policy |
| `B{v, wrap}` | `v` modulo `B`'s interval | |
| `exact_cast<B>(v)` | `v`, or `expected<B, error>` where it can fail | `overflow`, `rounding_error`, `not_finite` (a floating-point `v` that is not finite) |
| `B{v, unsafe}` | `v` | precondition: `v` fits |
| `conversion_overflows<B>(v)`, `conversion_rounds<B>(v)` | what `B{v}` would report or round | a rounding that brings `v` in is a rounding |
| `op(a, b, p)` (`op` one of `add` … `mod`) | `a op b` | `add`, `sub` and `mul` are total and ignore `p`; `div` and `mod` round by `p`'s mode (the operands contribute only `unsafe`); an `error&` is set by a zero divisor, which also stays in the `expected`; actions are ill-formed |
| `divmod(a, b, …)` | `{q, r}`, `q·b + r == a` | as `/` and `%` |
| `b.with<F>() = v` etc. | stores `v` into `b` under `P | F` | a `policy_ref`; on an rvalue a `policy_buffer`, which converts to any `inside` the value fits |
| `b.on_wrap(f) += v` etc. | `b += v`, `f` called when the policy acts | one action per store: a `policy_ref` has no `on_*` members, so actions do not chain |
| `common_type_t<A, B>` | `inside<hull(grid_of<A>, grid_of<B>), P>` | `P`: the shared policy, a point of the default policy counting as neutral, else `checked`; `A` when `A == B` |
| `numeric_limits<B>::min()`, `max()` | `lower`, `upper` | `epsilon()`, `round_error()`: 0, only when 0 is on the grid; `digits`: `bit_width(step_count + 1) − 1`, one less when the grid holds negatives, 0 on a point or a continuous grid |

# Future work

The reference implementation provides more than this paper proposes. Each item
below is a candidate for a later paper:

1. **Text**: `to_string`, `parse`, `parse_exact`, `std::formatter` and stream
   operators, printing and reading the exact value.
2. **Algorithms**: `sum<Target>` with one deferred check, `add_all`,
   `mul_all`, `mul<Out>` / `div<Out>` (one rounding, no intermediate
   grid), `min`, `max`, `midpoint` and `lerp`.
3. **Iterating a grid**: `cursor<T, Step>` for loops, the range
   `inside_range` and `values<T>` (every value of `T`), and `uniform<B>(rng)`,
   which samples a grid exactly.
4. **Predefined types**: `uint8` … `int64` on their full ranges,
   `unorm8`, `unorm16` and `unorm32`, the Q formats `uq4_4`, `uq8_8` and
   `uq16_16`, and the counters `clamp_counter<Max>` and `wrap_counter<Max>`.
5. **Indices**: an `inside` that holds only valid indices converts implicitly
   to the builtin integers that hold its whole grid, so it subscripts a
   container directly; with `std::mdspan`, `extents_of<B...>` builds the
   extents from index types, `index_for` and `indices` give the index insides
   of an extent, and value and raw accessors read and write insides.

The companion papers propose quantities and the math functions.

# Implementation experience

The reference implementation is header-only C++26 and needs static reflection:
today GCC 16 with `-freflection`. It ships a single-header amalgamation and
a CMake package. Its test suite (1133 tests: unit tests, 123 programs that
must not compile, the examples and this paper's listings) runs in CI in four
configurations on every change (debug with the address, undefined and leak
sanitizers, release, debug with `-Werror`, no floating point) and in three
more every week (debug with the slow tests, the thread sanitizer, and the full
certification of the math functions). It includes a check that the integer
fast paths compile to call-free, vectorising code, a property fuzzer, and a
differential oracle that recomputes random results exactly in Python.

Release v0.5.0 is the same library with 64-bit grid numbers, for C++23
compilers (GCC 14+, Clang 19+, Clang 18 with libc++, AppleClang, MSVC 19.51+),
tested in 50 CI configurations. Its measurements carry over: integer-aligned
arithmetic compiles to the same instructions as `int` [@INTHARMFUL], "The fast
path is byte-identical to native"; `inside<unsafe>` runs at native speed, and
`inside<checked>` at 88% of the fastest hand-written range-enforcing code
[@INTHARMFUL], "The like-for-like comparison". The current release's
measurements, every operation against a native twin, are in the repository's
`docs/performance.md`.

The synopsis names a few types that the reference implementation keeps in
`ins::detail`, under the same names unless noted:
- `policy_constant` is `detail::flag`;
- `call_policy`, `empty_ref` and `error_ref`;
- `policy_ref` and `policy_buffer`;
- `on_clamp_t`, `on_wrap_t` and `on_error_t`;
- `numerator_grid`, `denominator_grid` and `is_inside_v`.

`grid_number` is a class of its own: a 64-bit fraction held inline while it fits, an interned
`detail::big_rational` past 64 bits. `per<D>` is a 64-bit
`detail::rational` where 1/D fits, and `step_count()` returns the integer type
`detail::grid_int`; both convert to `grid_number`.

# Impact on the standard

A pure library addition: a new header (`<inside>`, name open) with the
declarations above, and specializations of `numeric_limits`, `hash` and
`common_type`. Nothing in the core language or the existing library changes.

- **Dependencies.** Structural class types as template arguments (C++20), and
  `std::define_static_array` [@P3491R3], so that a grid number past 64 bits is
  one template argument in every translation unit.
- **Freestanding.** Everything but the default error handler's message works
  without a hosted library and without floating point; the reference
  implementation's freestanding build runs its integer tests.
- **ABI.** The storage is unspecified; the reference implementation stores the
  narrowest integer that numbers the grid. Equal grids give the same type.
- **Feature-test macro.** `__cpp_lib_inside`.

# Wording

Wording will follow once SG6 and LEWG have agreed on the design. Until then
the semantics tables above state the intended behaviour.

# Relation to other proposals and prior art

| Paper | Group | Relation |
|---|---|---|
| [@P0543R3], [@P4052R0] | C++26 | `saturating_cast<T>(v)` saturates into a builtin type's full range; `B{v, clamp}` saturates into any grid, and `clamp` is one of several policies. |
| [@P2993R0] | SG6, 2024 | Constrained numbers: a constraint (intervals, masks, multiples) on a builtin type, checked or clamped on assignment. `inside` keeps the constraint exact and rational and computes it for every result. |
| [@P4412R0] | SG6, 2026 | Range validation (check, clamp, wrap, reflect) on quantity points. `inside` puts the same policies on the number, so any quantity, point or not, has them. |
| [@P3724R4] | LEWG | Integer division with named rounding. `div(a, b, mode)` names the same modes; the names should agree. |
| [@P3161R6], [@P3018R0] | SG6 | Carry and wide primitives on builtin integers. An implementation of wide grids can use them. |
| [@P4444R1] | SG6, 2026 | `std::big_int` at run time. `grid_number` is an exact rational at compile time; run-time values past 64 bits are `inside`s with wide storage. |
| [@P0037R7], [@P0828R1], [@P0554R1], [@P1050R1] | — | CNL: fixed point with a binary exponent, elastic integers, composition. `inside` generalises the exponent to a rational notch and an interval. |
| [@P1438R1] | — | Rational numbers at run time. A continuous `inside` holds a rational whose size its type bounds. |
| [@P0105R1], [@P0103R1], [@P0106R0] | — | Rounding and overflow modes as run-time choices, overflow detection, binary fixed point. Here the modes are part of the type or of a call. |
| [@P0228R0] | — | Safe integers (Boost.SafeNumerics). |
| [@P1619R2] | — | Boundary tests for integer operations; `conversion_overflows` and `conversion_rounds` are the same questions for grids. |
| [@N2137] | — | Interval arithmetic on floating point at run time; the intervals here are exact and compile time. |
| [@P1889R1] | — | The numerics TS draft. |

Libraries: **bounded::integer** (David Stone) has integer ranges in the type
and widening arithmetic; `inside` generalises it to rational grids. **Ada**
range and fixed-point types put a range and a step (`delta`) in a declaration.
**Boost.SafeNumerics** (Robert Ramey) and **SafeInt** detect overflow of
builtin integers. **Intel safe-arithmetic** proves interval unions at compile
time. The repository's `docs/resources.md` compares their features.

# Open questions

1. **Naming.** `inside`, `grid`, `notch`, `per` and the policy names are the
   implementation's. The committee may prefer others (`bounded`, `ranged`). A
   standard literal suffix cannot begin with `_`, so `_q` and `_g` need new
   names.
2. **Failure of a checked store.** Proposed: no run-time failure path of its
   own. A store the grids cannot prove names its channel (a flag, an
   `error&`, an action, `try_make`) or is ill-formed; a constant is checked
   while compiling. The cost is that `is_constructible_v<B, int>` is `true`
   for a `consteval` constructor that a run-time `int` cannot call. What
   remains are preconditions (a cursor past its end), which reach a
   replaceable handler; a contract violation ([@P2900R14]) is the
   alternative there.
3. **Bare scalars.** Rejecting `a + 1` is deliberate. Should mixed arithmetic
   with an integer of a stated range be allowed instead?
4. **The per-call interface.** Trailing arguments on free functions for one
   call, and a proxy (`policy_ref`) for one store, are the implementation's
   shape. Should the proxy types be named, as here, or left unspecified?
5. **Rounding-mode names.** `round_half_away`, `round_floor` and the others
   should agree with [@P3724R4] and with the `float_round_style` work of
   [@P4231R0].
6. **Interaction** with C++26 saturation arithmetic and with `std::linalg`
   element types.

# Proposed polls

1. SG6 is interested in a standard number type whose interval and exact
   rational step are part of its type, with arithmetic whose result type holds
   every result.
2. Such a number type is a useful representation for quantities, in addition
   to the overflow prevention of the units library itself.
3. A store that the operand types cannot prove must name what happens to a
   value that does not fit (a policy, an error channel, a fallback) or be
   ill-formed; no store has a hidden failure path.
4. Arithmetic with a bare integer (`a + 1`) should be ill-formed.
5. The limits and the step should be exact rationals of any size (needing
   `std::define_static_array`), rather than 64-bit values.
6. SG6 encourages further work on this paper, with the companion papers on
   quantities and on correctly rounded functions.

# Acknowledgements

The design builds on bounded::integer (David Stone), CNL and its proposals
(John McFarlane), Boost.SafeNumerics (Robert Ramey), Intel safe-arithmetic and
P2993 (Luke Valenty), and mp-units and P3045 (Mateusz Pusz and its co-authors).
The reference implementation follows the Beman Project's standard. The paper
is built with Michael Park's mpark/wg21.

---
references:
  - id: INTHARMFUL
    citation-label: int-considered-harmful
    title: "int considered harmful"
    author:
      - family: Neiss
        given: Peter
    URL: https://github.com/PeterNeiss/inside/blob/v0.9.0/docs/int-considered-harmful.pdf
  - id: BRNO2026
    citation-label: Brno2026
    title: "Report from the Brno 2026 ISO C++ Committee meeting"
    author:
      - family: Pusz
        given: Mateusz
    URL: https://mpusz.github.io/mp-units/HEAD/blog/2026/06/13/report-from-the-brno-2026-iso-c-committee-meeting/
---
