# Arithmetic

Binary operations compute the result grid at compile time:

```cpp
using u8 = bound<{0, 255}>;
u8 a{100}, b{200};

auto sum  = a + b;   // bound<{0, 510}>
auto diff = a - b;   // bound<{-255, 255}>
auto prod = a * b;   // bound<{0, 65025}>
auto quot = a / b;   // slim::optional<bound<{rational}>>
```

Division is rich enough to warrant its [own section](#division) below — `bound / bound` picks between three code paths at compile time, and its `slim::optional` result has two distinct failure modes.

The free functions `add`, `sub`, `mul`, `div`, and `mod` each accept a named
convenience policy (`bnd::truncated`, `bnd::round_to_nearest`, `bnd::clamped`,
`bnd::wrapped`), one or more `on_*` action factories, or an
`std::error_code&`, in any order. See
[policies.md § Callbacks](policies.md#callbacks-on_wrap--on_clamp--on_overflow--on_sentinel--on_error)
for the action API. Example — recovering from divide-by-zero:

```cpp
auto q = div(x, y, on_overflow([&](auto& res, errc) {
    res = std::remove_cvref_t<decltype(res)>{0};
}));
```

## Division

`bound / bound` returns a plain `bound` when the divisor's grid provably
excludes zero (`Lower > 0 || Upper < 0` — the `DivisorExcludesZero` trait) and
the operation can't otherwise fault; then there is nothing to unwrap.
Otherwise it returns `slim::optional<result>`, because division by zero is a
runtime possibility (and on the exact-rational path under `checked`, so is
overflow — which keeps the optional even when the divisor is known nonzero).
The library picks one of **three code paths** at compile time, based on the
operand grids and whether `ignore_round` is in effect.

### The three paths

| Path | Triggered when… | Algorithm | Result storage | Result interval |
|---|---|---|---|---|
| **Q-format fast** | `ignore_round` is set **and** both operands share the same Q-format grid (notch `1/N` with `N ≥ 2`, `Lower == 0`) | `(lhs.Raw × N) ÷ rhs.Raw` — the textbook fixed-point divide; folds to `(a << log2(N)) / b` for power-of-2 N | Q-format integer raw, **same notch as L** | `[0, Upper<L> / Notch<R>]` — Upper *expands* (see below) |
| **Integer-aligned fast** | `ignore_round` is set **and** both grids are integer-aligned (notch and Lower both have denominator 1) **and** neither operand uses rational raw storage | `to_value(lhs) / to_value(rhs)` — plain native integer divide; truncates toward zero | Integer raw | `Grid<L> / Grid<R>` with `.trunc()` applied to both endpoints |
| **Exact rational** *(fall-through)* | everything else | `as_rational(lhs) / rational{rhs}` — exact rational arithmetic. Under `checked` this is the optional-returning `rational::operator/`; under `unsafe` it's the unchecked variant. | `rational` raw — the result type is `bound<{interval, 0_r}>` | `*(Grid<L> / Grid<R>)` — the grid divider widens the interval when the divisor's range straddles zero |

```cpp
using val = bound<{0, 100}>;
val a{7}, b{3};

// Exact rational result (path C — the default).
auto exact = a / b;                            // bound<{rational}>, value 7/3

// Per-call integer truncation (path B).
auto quot  = div(a, b, truncated);             // bound<{0, 33}> integer raw, value 2

// Type-level integer truncation (path B again — gating is on policy,
// not on the operator's call site).
using fast = bound<{0, 100}, ignore_round>;
auto q     = fast{7} / fast{3};                // bound<{0, 33}> integer raw, value 2

// Q-format same-notch (path A).
using fp = bound<{{0, 255}, notch<1, 256>}, unsafe>;   // Q8.8; unsafe implies ignore_round
auto qfp = div(fp{200}, fp{3}, truncated);     // Q8.8 raw 17066 ≈ 66.6641
```

The Q-format spot check matches `tests/test_perf_paths.cpp:70-95` to the bit
(`200 / 3 ≈ 66.6667`; the formula multiplies before dividing, so the result
is `(51200 × 256) / 768 = 17066` — i.e. `floor(66.6667 × 256)`, **not**
`66 × 256 = 16896`, which would lose the fractional precision).

### When the result is `slim::optional` (and when it isn't)

When the divisor's grid provably **excludes zero** (`Lower > 0 || Upper < 0` —
the `DivisorExcludesZero` trait in `generic.hpp`) *and* the op can't otherwise
fault, `operator/` returns a **plain `bound`** — no wrapper to unwrap:

```cpp
using num = bound<{0, 100}, ignore_round>;
using pos = bound<{1, 10},  ignore_round>;   // grid excludes zero
auto d = num{42} / pos{3};                   // bound, == 14  (not optional)
```

The integer / Q-format fast paths (A, B) can only fault on divide-by-zero, so a
zero-excluding divisor makes them total. The exact-rational path (C) under
`checked` can also overflow, so it keeps the optional even when the divisor is
known nonzero.

Otherwise the result is `slim::optional<result>`, which has two ways to be
`nullopt`:

1. **Divide by zero** — every path runs its own zero check. Path A tests
   `rhs.Raw == 0` (safe because Q-format Lower is 0, so raw-zero means
   value-zero); path B tests `to_value(rhs) == 0`; path C tests
   `rhs.Numerator == 0` (the canonical zero rather than the rational
   sentinel state).
2. **Rational denominator overflow** — only on path C, and only under
   `checked`. The inner `rational::operator/` returns `nullopt` when the
   resulting denominator can't fit in `imax`; that surfaces as
   `errc::overflow` (not `errc::division_by_zero`) through
   `div(a, b, ec)` or an `on_overflow` callback.

### Opting into integer-truncation semantics

Three ways to reach paths A and B:

```cpp
using val = bound<{0, 100}>;

// 1. Type-level: every operator/ on this type takes the integer path.
using fast = bound<{0, 100}, ignore_round>;
auto q1 = fast{7} / fast{3};               // -> 2

// 2. Per-call: OR `ignore_round` into the operation's flags.
auto q2 = div(val{7}, val{3}, truncated);  // -> 2

// 3. Same as (2) using the operation's named policy alias.
//    `truncated = make_policy<ignore_round>()`; siblings include
//    `round_to_nearest`, `clamped`, `wrapped` — see `bound/policy.hpp`.
```

Without any of these, `operator/` always takes path C and returns a
`bound<rational>` — exact, but uses rational raw storage with the perf
characteristics described in [storage.md](storage.md).

### Result-grid widening on path A (the subtle bit)

The Q-format fast path keeps the *notch* but expands the *interval*:

```cpp
using fp = bound<{{0, 255}, notch<1, 256>}, unsafe>;   // Q8.8
auto q = fp{1} / fp{1};   // type: bound<{{0, 65280}, notch<1, 256>}>, value 1
```

The result's upper bound is `Upper<L> / Notch<R> = 255 / (1/256) = 65 280`,
not `255`. The smallest non-zero divisor in a Q8.8 grid is `1/256`, so an
input of `255` could be divided by `1/256` and produce `65 280` — the
result type must be wide enough to hold every possible quotient.

Path C similarly widens when the divisor's range straddles zero:
`grid::operator/` splits into the positive `[step, Upper]` and negative
`[Lower, -step]` halves (excluding the zero gap), divides each, and unions
the results.

### Examples and tests

- [examples/division.cpp](../examples/division.cpp) — paths B and C side by
  side; default exact-rational vs `div(a, b, truncated)`.
- [examples/integer_division.cpp](../examples/integer_division.cpp) — the
  type-level vs per-call forms of path B.
- [tests/test_bound_arithmetic.cpp](../tests/test_bound_arithmetic.cpp) — the
  `"bound div: rational vs integer paths"` case covers paths B and C across
  unit / non-unit notch and signed bounds.
- [tests/test_perf_paths.cpp](../tests/test_perf_paths.cpp) — the
  Q-format fast-path correctness test with the bit-exact 200/3 → 17066
  reference.

## Scalars in bound arithmetic need a grid

A raw `int` or `double` carries no grid, so `bound op rawscalar` has no
type-safe result and is **ill-formed by design**. Writing `b + 1` or
`b * 2.5` triggers a `static_assert` that hands you the fix: give the scalar a
grid.

```cpp
using money = bound<{{0, 1'000'000}, notch<1, 100>}, round_nearest>;
money sub{45.07};

auto a = sub + 1;        // ❌ ill-formed: a raw int has no grid
auto b = sub * 2.5;      // ❌ ill-formed: a raw double has no grid

auto c = sub + 1_b;      // ✅ bound + bound → widened bound (stays bounded)
auto d = sub * just<2>;  // ✅ bound × bound → widened bound
```

Use `1_b` / `just<N>` to give a compile-time literal a tight point grid, or
`bound<{lo,hi}>{n}` to give a runtime value a known range. Comparisons
(`b == 5`, `b < 10`) and compound assignment (`b += 1`) with raw scalars are
unaffected — they don't manufacture a new value/type, so they never leave the
bounded world.

### Exception: `bound op rational`

A `rational` is an exact, deliberate scalar, so the mixed-mode operators are
kept for it. `+`, `-`, `*`, `/` with a `rational` operand return `rational`
(staying in the exact domain), going through `rational::operator op` and
unwrapping the checked optional via `.value()` (panics on overflow).

```cpp
money tax = sub * rational{8, 100};   // bound × rational → rational → money (round)
auto half = sub * 0.5_r;              // bound × rational → rational (exact)
```

For division specifically, the `bound / rational` form **bypasses the
`bound / bound` machinery** described under [Division](#division) entirely: it
doesn't pick between the three paths, doesn't return `slim::optional`, and
panics on rational overflow via `.value()` instead of surfacing it as
`nullopt`. Use it when you have a *runtime `rational`* on the RHS known to be
non-zero; use `bound / bound` when you want the optional-protected, three-path
machinery (which now also returns a plain bound when the divisor's grid
excludes zero — see [Division](#division)).

## Rounding

Assigning between bounds with incompatible notches is a compile-time error:

```cpp
using coarse = bound<{{0, 10}, 2}>;   // notch 2: values 0, 2, 4, 6, 8, 10
using fine   = bound<{{0, 10}, 1}>;   // notch 1: values 0, 1, 2, …, 10

coarse c{0};
fine f{3};
c = f;  // ERROR: incompatible notches (3 would round to 2)
```

Notches are "compatible" when the target notch is an integer multiple of the
source notch. Notch 2 → notch 1 is always exact (every even number is an
integer); notch 1 → notch 2 may round.

To opt in to rounding:

```cpp
// Per-operation (truncates toward zero).
c.with_truncate() = f;

// Round to nearest notch.
c.with_round_nearest() = f;

// Per-call with explicit policy.
c.policy<ignore_round>() = f;
c.policy<round_nearest>() = f;

// Type-level: all assignments allow rounding.
using coarse_r = bound<{{0, 10}, 2}, ignore_round>;
coarse_r cr = f;  // OK, truncates to nearest notch
```

## Modulo

`bound % bound` requires integer-aligned grids **and** `ignore_round`. Both
are **hard requirements** enforced by `static_assert` in
[`include/bound/detail/division.hpp`](../include/bound/detail/division.hpp) —
there is no rational fallback, because `a mod b` is only meaningfully
defined when both operands are integers.

```cpp
using val = bound<{0, 100}, ignore_round>;
val a{17}, b{5};
auto r = a % b;  // slim::optional<bound<{0, 99}>>, value 2
```

The result interval is `[0, max_rem]` for non-negative L, or
`[-max_rem, max_rem]` if `Lower<L> < 0`, where
`max_rem = max(|Lower<R>|, |Upper<R>|) - 1` — the largest remainder
magnitude any divisor in R's range could produce.

Like division, modulo returns `slim::optional` (division by zero yields
`nullopt`). Unlike division, modulo has no overflow case — the result's
range is fixed by R's grid and can't exceed it.

## Compound assignment

Compound assignment works with arithmetic scalars, `rational`, and other
bounds with compatible grids:

```cpp
using pct = bound<{0, 100}, clamp>;
pct x{50};
x += 30;            // x == 80
x -= 10;            // x == 70
x *= 2;             // x == 100 (clamped)
x /= 3;             // x == 33
x %= 10;            // x == 3

++x;                // x == 4
x--;                // x == 3
```

The `arithmetic` overload now dispatches per RHS kind:
- **integral** — integer-fast path with overflow detection.
- **rational** — lift `*this` to rational, run the checked op, snap back via
  the bound's policy.
- **floating-point** — route through `double`.

`x /= 0` triggers the bound's divide-by-zero handling (throws, sets the
error code, or is silent under `ignore_zero` — see
[policies.md](policies.md#error-code-mode)). This is the same per-path
zero check used by `bound / bound`; see
[Division § Why the result is `slim::optional`](#why-the-result-is-slimoptional).

```cpp
using rn = bound<{{0, 100}, notch<1, 100>}, round_nearest>;
rn a{0.5};
a += rational{1, 4};   // 0.75 — exact rational accumulation, snap to 1/100 grid
a *= 0.5;              // 0.375 — double path, snap on assign
```

### `rational` compound assignment

`rational` itself has `operator+=, -=, *=, /=` taking `rational`,
`arithmetic`, or `slim::optional<rational>` RHS:

```cpp
rational sum{0};
sum += rational{1, 3};                       // chained .value() unwrap
sum *= 5;                                    // arithmetic RHS
sum += rational{1, 4} * rational{1, 2};      // optional<rational> RHS auto-unwraps
```

The compound op unwraps the checked rational op's optional with `.value()` —
overflow surfaces as `slim::bad_optional_access` rather than silent UB,
matching the panic semantics of `rational::mul_unchecked`.

## Variadic folds

`add_all` and `mul_all` are variadic equivalents of `+` and `*` over bounds:

```cpp
using v = bound<{0, 100}>;
v a{10}, b{20}, c{30};
auto sum  = add_all(a, b, c);   // bound<{0, 300}>, value 60
auto prod = mul_all(a, b);      // bound<{0, 10000}>, value 200
```

## When `slim::optional` is returned

Operations return `slim::optional<bound>` in two cases:

1. **Division and modulo** — the divisor could be zero. See
   [Division § Why the result is `slim::optional`](#why-the-result-is-slimoptional)
   for the two distinct failure modes division has (divide-by-zero on every
   path, plus denominator overflow on the rational path under `checked`).

2. **Rational raw storage** — when the result grid can't be represented with
   an integer raw type (see [storage.md](storage.md)), the result uses
   `rational` as its raw storage. Addition and multiplication on such types
   return `slim::optional` because rational arithmetic can overflow
   (`lcm(b, d)` of the denominators may exceed `imax`).

All `optional`-returning operators propagate `nullopt`: if either operand is
`nullopt`, the result is `nullopt`.

```cpp
using u8 = bound<{1, 255}>;
slim::optional<u8> a{u8{100}};
slim::optional<u8> none{slim::nullopt};

auto r1 = a + u8{10};     // optional<bound<{2, 510}>>, has value 110
auto r2 = none + u8{10};  // optional<bound<{2, 510}>>, nullopt
```
