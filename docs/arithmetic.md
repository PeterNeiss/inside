# Arithmetic

Binary operations compute the result grid at compile time:

```cpp
using lvl = inside<{0, 255}>;
lvl a{100}, b{200};

auto sum  = a + b;   // inside<{0, 510}>
auto diff = a - b;   // inside<{-255, 255}>
auto prod = a * b;   // inside<{0, 65025}>
auto quot = a / b;   // std::expected<inside<{rational}>, errc>
```

Division is rich enough to warrant its [own section](#division) below — `inside / inside` picks between three code paths at compile time, and its `std::expected` result has two distinct failure causes.

The free functions `add`, `sub`, `mul`, `div`, and `mod` take one of three
trailing forms:

- `op(l, r [, policy [, action]])` — a named convenience policy
  (`beman::inside::snapped`, `beman::inside::rounded_nearest`, `beman::inside::clamped`,
  `beman::inside::wrapped`, or any `policy<F>`), optionally followed by one action;
- `op(l, r, on_*(…), …)` — one or more `on_*` action factories (at least one
  `on_overflow`); the policy is the flags those actions imply;
- `op(l, r, ec [, action])` — a `beman::inside::errc&` that receives the error.

See
[policies.md § Callbacks](policies.md#callbacks-on_wrap--on_clamp--on_overflow--on_error)
for the action API. Example — recovering from divide-by-zero:

```cpp
auto q = div(x, y, on_overflow([&](auto& res, errc) {
    res = std::remove_cvref_t<decltype(res)>{0};
}));
```

## Division

`inside / inside` returns a plain `inside` when the divisor's grid provably
excludes zero (`Lower > 0 || Upper < 0` — the `divisor_excludes_zero` trait) and
the operation can't otherwise fault; then there is nothing to unwrap.
Otherwise it returns `std::expected<result, errc>`, because division by zero is a
runtime possibility (and on the exact-rational path under `checked`, so is
overflow — which keeps the wrapper even when the divisor is known nonzero).
The library picks one of **three code paths** at compile time, based on the
operand grids and whether `snap` is in effect.

### The three paths

| Path | Triggered when… | Algorithm | Result storage | Result interval |
|---|---|---|---|---|
| **Q-format fast** | `snap` is set **and** both operands share the same Q-format grid (notch `1/N` with `N ≥ 2`, `Lower == 0`) | `(lhs.Raw × N) ÷ rhs.Raw` — the textbook fixed-point divide, **rounded per the policy's mode** (folds to `(a << log2(N)) / b` for power-of-2 N under plain `snap`) | Q-format integer raw, **same notch as L** | `[0, upper_of<L> / notch_of<R>]` — Upper *expands* (see below) |
| **Integer-aligned fast** | `snap` is set **and** both grids are integer-aligned (notch and Lower both have denominator 1) **and** neither operand uses rational raw storage | `to_value(lhs) / to_value(rhs)`, **rounded per the policy's mode** — see below | Integer raw | `grid_of<L> / grid_of<R>` with each endpoint rounded by the same mode |
| **Exact rational** *(fall-through)* | everything else | `as_rational(lhs) / rational{rhs}` — exact rational arithmetic. Under `checked` this is the expected-returning `rational::operator/`; under `unsafe` it's the unchecked variant. | `rational` raw — the result type is `inside<{interval, 0}>` | `*(grid_of<L> / grid_of<R>)` — the grid divider widens the interval when the divisor's range straddles zero |

Both native paths divide in 32 bits when both operand ranges fit (the integer
path excludes `INT32_MIN`, so `a / -1` cannot overflow), so `/` and `%` on small
grids run at native speed rather than as a 64-bit divide.

**Rounding mode (native paths).** Plain `snap` (== `snapped`) truncates toward
zero — the historical, C++-`/` behaviour. Any rounding-mode flag rounds the quotient
instead: `round_nearest` (half away from zero), `round_floor` (toward −∞),
`round_ceil` (toward +∞), `round_half_even` (banker's). The remainder from `%` stays
consistent with the rounded quotient, so `(a / b) * b + a % b == a` for every mode
(e.g. under `round_floor`, `(-8) % 3 == 1`; under `snapped`, `(-8) % 3 == -2`).

```cpp
using val = inside<{0, 100}>;
val a{7}, b{3};

// Exact rational result (path C — the default).
auto exact = a / b;                            // inside<{rational}>, value 7/3

// Per-call integer truncation (path B). The divisor's range includes 1, so the
// quotient's range is 0..100; the divisor's range includes 0, so it is expected.
auto quot  = div(a, b, snapped);             // expected<inside<{0, 100}>> integer raw, value 2

// Type-level integer truncation (path B again — gating is on policy,
// not on the operator's call site).
using fast = inside<{0, 100}, snap>;
auto q     = fast{7} / fast{3};                // expected<inside<{0, 100}>> integer raw, value 2

// Q-format same-notch (path A).
using fp = inside<{{0, 255}, per<256>}, unsafe>;   // Q8.8; unsafe implies snap
auto qfp = div(fp{200}, fp{3}, snapped);     // Q8.8 raw 17066 ≈ 66.6641
```

The Q-format spot check matches `tests/beman/inside/perf_paths.test.cpp` to the bit
(`200 / 3 ≈ 66.6667`; the formula multiplies before dividing, so the result
is `(51200 × 256) / 768 = 17066` — i.e. `floor(66.6667 × 256)`, **not**
`66 × 256 = 16896`, which would lose the fractional precision).

### When the result is `std::expected` (and when it isn't)

When the divisor's grid provably **excludes zero** (`Lower > 0 || Upper < 0` —
the `divisor_excludes_zero` trait in `generic.hpp`) *and* the op can't otherwise
fault, `operator/` returns a **plain `inside`** — no wrapper to unwrap:

```cpp
using num = inside<{0, 100}, snap>;
using pos = inside<{1, 10},  snap>;   // grid excludes zero
auto d = num{42} / pos{3};                   // inside, == 14  (not expected)
```

The integer / Q-format fast paths (A, B) can only fault on divide-by-zero, so a
zero-excluding divisor makes them total. The exact-rational path (C) under
`checked` can also overflow, so it keeps the wrapper even when the divisor is
known nonzero.

Otherwise the result is `std::expected<result, errc>`, which has two error
causes:

1. **`errc::division_by_zero`** — every path runs its own zero check. Path A
   tests `rhs.Raw == 0` (safe because Q-format Lower is 0, so raw-zero means
   value-zero); path B tests `to_value(rhs) == 0`; path C tests
   `rhs.Numerator == 0`.
2. **`errc::overflow`** — rational denominator overflow, only on path C and
   only under `checked`. The inner `rational::operator/` returns an error when
   the resulting denominator can't fit in `imax`; the same code reaches
   `div(a, b, ec)` and an `on_overflow` callback.

Test the result and read the cause when it matters:

```cpp
auto q = a / b;
if (!q) log(errc_message(q.error()));   // "division by zero" or overflow
```

`std::expected<inside, errc>` is larger than the `inside` it wraps (a flag and
an `errc` sit beside the value), so keep it where it belongs — as the result
you test right away, or as an operand passed straight into the next
operation. Unwrap into a plain `inside` before storing it.

### Opting into integer-truncation semantics

Three ways to reach paths A and B:

```cpp
using val = inside<{0, 100}>;

// 1. Type-level: every operator/ on this type takes the integer path.
using fast = inside<{0, 100}, snap>;
auto q1 = fast{7} / fast{3};               // -> 2

// 2. Per-call: OR `snap` into the operation's flags.
auto q2 = div(val{7}, val{3}, snapped);  // -> 2

// 3. Same as (2) using the operation's named policy alias.
//    `snapped = make_policy<snap>()`; siblings include
//    `rounded_*`, `clamped`, `wrapped` — see policies.md#named-policies.
```

Without any of these, `operator/` always takes path C and returns a
`inside<rational>` — exact, but uses rational raw storage with the perf
characteristics described in [storage.md](storage.md).

### Result-grid widening on path A (the subtle bit)

The Q-format fast path keeps the *notch* but expands the *interval*:

```cpp
using fp = inside<{{0, 255}, per<256>}, unsafe>;   // Q8.8
auto q = fp{1} / fp{1};   // expected<inside<{{0, 65280}, per<256>}>>, value 1
```

The result's upper bound is `upper_of<L> / notch_of<R> = 255 / (1/256) = 65 280`,
not `255`. The smallest non-zero divisor in a Q8.8 grid is `1/256`, so an
input of `255` could be divided by `1/256` and produce `65 280` — the
result type must be wide enough to hold every possible quotient.

Path C similarly widens when the divisor's range straddles zero:
`grid::operator/` splits into the positive `[step, Upper]` and negative
`[Lower, -step]` halves (excluding the zero gap), divides each, and unions
the results.

### Examples and tests

- [examples/division.cpp](../examples/division.cpp) — paths B and C side by
  side; default exact-rational vs `div(a, b, snapped)`.
- [examples/integer_division.cpp](../examples/integer_division.cpp) — the
  type-level vs per-call forms of path B.
- [tests/beman/inside/inside_arithmetic.test.cpp](../tests/beman/inside/inside_arithmetic.test.cpp) — the
  `"inside div: rational vs integer paths"` case covers paths B and C across
  unit / non-unit notch and signed bounds.
- [tests/beman/inside/perf_paths.test.cpp](../tests/beman/inside/perf_paths.test.cpp) — the
  Q-format fast-path correctness test with the bit-exact 200/3 → 17066
  reference.

## Scalars in inside arithmetic need a grid

A raw `int` or `double` carries no grid, so `inside op rawscalar` has no
type-safe result and is **ill-formed by design**. Writing `b + 1` or
`b * 2.5` triggers a `static_assert` that hands you the fix: give the scalar a
grid.

```cpp
using money = inside<{{0, 1'000'000}, per<100>}, round_nearest>;
money sub{45.07};

auto a = sub + 1;        // ❌ ill-formed: a raw int has no grid
auto b = sub * 2.5;      // ❌ ill-formed: a raw double has no grid

auto c = sub + 1_ins;      // ✅ inside + inside → widened inside (stays bounded)
auto e = sub + one;      // ✅ same thing — `one` is the built-in point-inside 1
auto d = sub * just<2>;  // ✅ inside × inside → widened inside
```

Use `1_ins` / `just<N>` to give a compile-time literal a tight point grid, or
`inside<{lo,hi}>{n}` to give a runtime value a known range. For the values 0 and
1, reach for the built-in `zero` / `one` (`sub + one`, `b == zero`). Comparisons
(`b == 5`, `b < 10`, `b < 1.5`) with raw scalars are fine — they don't manufacture
a new value/type — and exact: a floating scalar is compared at full precision,
never truncated to an integer. Compound assignment with a raw scalar (`b += 1`)
is ill-formed like the binary operators ("an inside cannot be combined with a raw
scalar"); write `b += 1_ins`.

### Scaling by an exact fraction

There is **no** `inside op rational` mixed-mode operator. To scale an inside by an
exact non-dyadic factor, wrap the factor in a point-inside and multiply
inside-by-inside; the result stays an inside (which a rounding target snaps):

```cpp
money tax{ sub * just<frac<8, 100>> };   // inside × point-inside → inside → money
auto    half = sub * just<frac<1, 2>>;   // exact ×½, stays an inside
```

`just<frac<N, D>>` is the exact-fraction point-inside; `0.5_ins` / `2_ins` cover
dyadic factors. Multiplying by a point `c` scales the lattice: the result's notch
is `N·|c|`, so it keeps integer storage (`sub * just<frac<1, 2>>` has notch 1/200)
and costs about a native multiply — the raw value is reused, mirrored for
negative `c`. Read an exact value back out with `numerator()` /
`denominator()` (see [Conversions](conversions.md)) — never a `rational`.

## Vector helpers: `dot` / `cross` / `lerp`

For 2-D geometry that would otherwise tempt you out of inside-space, the
library ships three widening helpers. Each composes `+`/`*` so the result grid
is computed at compile time and overflow is impossible — no scalar ever
appears.

```cpp
auto d = beman::inside::dot  (ax, ay, bx, by);   // ax*bx + ay*by
auto z = beman::inside::cross(ax, ay, bx, by);   // ax*by - ay*bx  (2-D "which side")
auto p = beman::inside::lerp (a, b, t);          // a + (b - a) * t   (t a [0,1] inside)
```

A sqrt-free squared distance is just `dot(dx, dy, dx, dy)`; compare it against
a squared-radius point-inside to test a hit without leaving the bounded world —
e.g. a 2-D space sim can drive collision and autopilot steering entirely in
inside-space.

## Rounding

Assigning between insides with incompatible notches is a compile-time error:

```cpp
using coarse = inside<{{0, 10}, 2}>;   // notch 2: values 0, 2, 4, 6, 8, 10
using fine   = inside<{{0, 10}, 1}>;   // notch 1: values 0, 1, 2, …, 10

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
c.with_snap() = f;

// Round to nearest notch.
c.with_snap<round_nearest>() = f;

// Per-call with explicit policy.
c.policy<snap>() = f;
c.policy<round_nearest>() = f;

// Type-level: all assignments allow rounding.
using coarse_r = inside<{{0, 10}, 2}, snap>;
coarse_r cr = f;  // OK, truncates to nearest notch
```

The same `.with_snap()` also reads out as a **value**, not only as an
assignment target — so you can round inside an expression or a `return`:

```cpp
coarse result  = f.with_snap();                  // 2  — value form, truncates
coarse nearest = f.with_snap<round_nearest>();   // 4

coarse half(fine x) { return x.with_snap(); }    // round at the return boundary
```

The target's own policy still applies through the conversion: returning into a
`inside<…, clamp>` clamps the range while `with_snap` handles the notch. For the
same fine→coarse conversion as a free function (including an inside source), use
`clamp_round<B>(v)` — see [conversions](conversions.md).

`.with_snap()` adapts to its receiver: on a named inside it yields a lightweight
reference proxy, but on a **temporary** — `(a * a).with_snap()` — it returns a
value-owning buffer that moves the result in, so it is safe to return or store
even with an `auto` return type (no dangling reference to the expired temporary).

## Modulo

`inside % inside` requires integer-aligned grids **and** `snap`. Both
are **hard requirements** enforced by `static_assert` in
[`include/beman/inside/detail/division.hpp`](../include/beman/inside/detail/division.hpp) —
there is no rational fallback, because `a mod b` is only meaningfully
defined when both operands are integers.

```cpp
using val = inside<{0, 100}, snap>;
val a{17}, b{5};
auto r = a % b;  // std::expected<inside<{0, 99}>, errc>, value 2
```

The result interval is `[0, max_rem]` when the rounding mode is plain
truncation (`snap`) and `lower_of<L> >= 0`, and `[-max_rem, max_rem]`
otherwise (a negative dividend, or a directional rounding mode, which can flip
the remainder's sign). Here `max_rem = max(|lower_of<R>|, |upper_of<R>|) - 1`
is the largest remainder magnitude any divisor in R's range could produce.

Modulo never overflows, so its only failure is a zero divisor. Like division,
it returns `std::expected` (`errc::division_by_zero`) when R's grid holds zero,
and a plain inside when R's grid excludes zero:

```cpp
using pos = inside<{1, 10}, snap>;
auto r2 = a % pos{5};   // inside<{0, 9}>, value 2 — no expected
auto r3 = a % 5_ins;    // inside<{0, 4}>, value 2 — a literal divisor, no expected
```

An integer literal is an integer-valued point, so `a % 5_ins` and, under
`snap`, `a / 5_ins` take the native integer paths. A non-integer point
(`0.5_ins`) is rejected by `%` like any non-integer grid.

## Bulk reduction: `beman::inside::sum<Target>(range)`

A per-element `target += b` loop re-validates the running total on every
step: a compare-and-branch per element, which also stands in the way of
vectorization (on the reference machine: 9 vs 4 instructions per element).
`beman::inside::sum<Target>(range)` accumulates exactly with **one** deferred check:
the *total* is validated/clamped against `Target`'s policy, not every running
prefix — and runs within a few percent of the native loop (see
[performance.md](performance.md), "accumulate 1000").

```cpp
using elem = inside<{0, 200'000}, checked>;
std::vector<elem> v = ...;
auto total = beman::inside::sum<elem>(v);            // one range check, vectorized

using bus = inside<{0, 100}, clamp>;
auto clipped = beman::inside::sum<bus>(v);           // clamps the TOTAL once
```

## Compound assignment

Compound assignment takes another inside (or an `expected<inside>`, or a
`rational`); the result is computed as by the binary operator, then narrowed
back into the left-hand type through its policy. A raw `int` / `double` is
ill-formed, as for the binary operators: give it a grid with `_ins`,
`just<…>`, or an inside over its range. Under an unchecked policy (no
`checked`/`clamp`/`wrap`) with value storage, `+=`/`-=`/`*=` operate directly
at the raw type's width — a loop of byte-wide `b += 1_ins` vectorizes at the
same lane count as native `uint8_t`:

```cpp
using pct = inside<{0, 100}, clamp | snap>;
pct x{50};
x += 30_ins;        // x == 80
x -= 10_ins;        // x == 70
x *= 2_ins;         // x == 100 (clamped)
x /= 3_ins;         // x == 33  (snap truncates 100/3)
x %= 10_ins;        // x == 3   (% needs snap)

++x;                // x == 4
x--;                // x == 3
```

An `expected` right-hand side (`x += a / b`) is unwrapped once; an error in it
is reported through the left-hand type's policy.

`x /= 0` triggers the inside's divide-by-zero handling (throws, sets the
error code, or is silent under `ignore_zero` — see
[policies.md](policies.md#error-code-mode)). This is the same per-path
zero check used by `inside / inside`; see
[Division § When the result is `std::expected`](#when-the-result-is-stdexpected-and-when-it-isnt).

```cpp
using rn = inside<{{0, 100}, per<100>}, round_nearest>;
rn a{0.5};
a += just<frac<1, 4>>;  // 0.75 — exact inside-space accumulation, snaps to 1/100
a *= 0.5_ins;           // 0.375, rounded to 0.38 on assign
```

## Variadic folds

`add_all` and `mul_all` are variadic equivalents of `+` and `*` over insides:

```cpp
using v = inside<{0, 100}>;
v a{10}, b{20}, c{30};
auto sum  = add_all(a, b, c);   // inside<{0, 300}>, value 60
auto prod = mul_all(a, b);      // inside<{0, 10000}>, value 200
```

To collapse the widened result into a narrower type, apply a cast or let the
target's policy do it:

```cpp
auto capped = clamp_cast<v>(add_all(a, b, c, v{90}));   // v, value 100 (150 clamped)
```

## When `std::expected` is returned

Operations return `std::expected<inside, errc>` in two cases:

1. **Division and modulo** — the divisor could be zero. See
   [Division § When the result is `std::expected`](#when-the-result-is-stdexpected-and-when-it-isnt)
   for the two distinct failure modes division has (divide-by-zero on every
   path, plus denominator overflow on the rational path under `checked`).
   `f64` (double-backed) division participates identically: an `f64` `÷` whose
   divisor grid can be zero returns `std::expected` and reports
   `errc::division_by_zero` on a zero divisor — it is not a silent path.

2. **Rational raw storage** — when the result grid can't be represented with
   an integer raw type (see [storage.md](storage.md)), the result uses
   `rational` as its raw storage. Addition and multiplication on such types
   return `std::expected` (`errc::overflow`) because rational arithmetic can overflow
   (`lcm(b, d)` of the denominators may exceed `imax`). This also covers an `f64`
   result grid too fine for `double`: `f64` is dropped and the exact result is
   stored as `rational`, so an unrepresentable `f64 ×` returns `std::expected`
   (overflow-checked) rather than silently losing precision.

All operators accept `std::expected` operands and propagate errors: if either
operand holds an error, the result holds that error (the left one when both
do).

```cpp
using lvl = inside<{0, 255}>;     // divisor grid holds zero → `/` returns expected
lvl a{100}, b{4}, z{0};

auto r1 = a / b + lvl{10};   // expected<inside<…>, errc>, value 35
auto r2 = a / z + lvl{10};   // expected<inside<…>, errc>, errc::division_by_zero
```

### Chaining `expected` results

`beman::inside::math`'s fallible functions use the same
`std::expected<inside, errc>` vocabulary, so their results chain into
arithmetic directly:

```cpp
// expected-lift operators — first (left) error wins and keeps its cause.
auto r = math::sqrt(signed_in{v}) * gain + offset;   // expected<inside, errc>
if (!r) log(r.error());                              // domain_error from sqrt
```

An operation that fails inside the chain reports its own cause — a division
by zero reports `errc::division_by_zero`, a rational overflow reports
`errc::overflow`.

See [internals.md](internals.md#7-error-vocabulary) for the full rule and the
per-operation audit.
