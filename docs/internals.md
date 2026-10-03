# inside library — internals

This document explains *why* the library is shaped the way it is. It is not a
tutorial; for that see [tutorial.md](tutorial.md). Use this when you need to add a new
arithmetic operator, debug a storage-shape edge case, or reason about
performance.

> The exact-fraction representation type is `beman::inside::detail::rational` — an
> **internal** type. It is the grid's NTTP substrate and the raw storage for
> non-dyadic grids, so it appears throughout these internals, but it is not on
> the public surface: consumers spell grids with literals / `notch<N,D>` /
> `frac<N,D>`, read exact values out with `numerator()` / `denominator()`, and
> never name the type. The bare word "rational" below always means
> `beman::inside::detail::rational`.

---

## 1. Grid invariants

Every `inside<G, P>` carries a `grid G` value with the following invariants,
enforced at type-instantiation time by `grid::validate` (`grid::validate` in `include/beman/inside/grid.hpp`):

- **`Lower ≤ Upper`** (rational comparison).
- **`Interval.divides_evenly(Notch)`** — there must be an integer number of
  notches between Lower and Upper. The notch count is exposed as
  `NotchCount<B>` (`include/beman/inside/generic.hpp`).
- **`Notch == 0` is legal** and means "any rational in the interval". The
  storage shape changes accordingly (see §2).
- **`Lower/Notch` and `Upper/Notch` resolve to integer rationals** when `Notch != 0`.

These invariants let the library compute result grids at compile time
without runtime overflow checks for grid arithmetic itself — every
reachable value of `a + b` for `a : A, b : B` is by construction inside
`Grid<A> + Grid<B>`.

**Point operands.** A point grid (`just<c>`, `1_ins`; Lower == Upper, notch 0)
in a product scales the other operand's lattice: `grid × point{c}` has notch
`N·|c|` (`operator*` in `grid.hpp`) rather than notch 0, so the product keeps
integer storage. `multiplication::point_scale` then reuses the operand's offset
as the result's (counted from the far end for `c < 0`) — no multiply at all.

---

## 2. Storage encoding

Representation is selected by the policy's **representation flags**
(`exact` / `f64` / `f32` / `direct` / `indexed` / the `i8`…`u64` width flags, see
[policies.md](policies.md#representation-flags)), with grid deduction as the
default. `storage_pick<G, P>` (`include/beman/inside/grid.hpp`) resolves the flags
**widest-wins** — a result of mixed-representation arithmetic ORs both
operand policies, and the widest representation present wins:

```text
  exact in P ──────────────────────────▶  rational raw   (raw IS the value, exact fraction)
       │ no
  f64 in P AND double_exact grid ──────▶  double raw     (raw IS the value; default engine
       │ no   (elided under BEMAN_INSIDE_MATH_FIXED)               only — fixed engine falls through.
       │                                                  Direct misuse on a too-fine grid is a
       │                                                  static_assert; arithmetic instead DROPS
       │                                                  `f64` when the result isn't double_exact)
  direct in P AND Notch == 1 ──────────▶  integer raw    (raw IS the value)
       │ no
  indexed in P AND Notch != 0 ─────────▶  unsigned raw   (raw = 0-based notch index)
       │ no
  deduced (storage_min<G>):
        Notch == 0                  ───▶  rational raw   (continuous grid)
        index count > umax          ───▶  rational raw   (too fine for any integer index)
        Notch == 1 AND (Lower == 0
          or signed raw)            ───▶  integer raw    (raw IS the value)
        otherwise                   ───▶  unsigned raw   (raw = 0-based notch index)
```

`storage_min<G>` picks the smallest integer type that can hold every
reachable index, using the type's full range (see [storage.md](storage.md)).

Four **disjoint predicates** in `include/beman/inside/generic.hpp` classify a
inside's encoding (the first two read the raw type alone; the integer pair
also consults the policy, mirroring `storage_pick` exactly):

| Predicate | Meaning |
|---|---|
| `rational_raw<B>` | raw IS the value, as an exact fraction |
| `real_raw<B>`     | raw IS the value, as an IEEE-754 `double` |
| `value_raw<B>`    | raw IS the value, as a plain integer |
| `index_raw<B>`    | raw is a 0-based notch index; value = Lower + raw·Notch |

The common query `!index_raw<B>` means "raw is the value" (any of the first
three). Note the decode direction must dispatch on the **encoding, not the
raw type's signedness** — a `direct` inside with Lower ≥ 0 has an *unsigned*
value raw; `detail::as_double` is the kind-aware raw → double decoder.

Two more predicates classify the grid's integer-ness (independent of the
storage encoding), gating arithmetic fast paths:

- `IsIntegerInterval<B>` — `Lower` and `Upper` have integer denominators
  (Notch may still be fractional, e.g. `{0, 100}, 1/10`).
- `IsIntegerAligned<B>` — `Notch` and `Lower` have integer denominators.
  Under the divides-evenly invariant this implies `IsIntegerInterval`,
  but the converse is not true. Both predicates exist because they gate
  different fast paths.

---

## 3. Q-format integer fast path

For grids with **integer Lower, unit-numerator Notch** (e.g. `1/256`,
`1/65536`), and a raw that fits in `imax`, the rational ↔ value conversion
collapses to integer arithmetic. The gate is `HasQFormatFastPath<B>`
(`include/beman/inside/generic.hpp`):

```cpp
abs_den(Lower<B>.Denominator) == 1
&& Notch<B>.Numerator == 1
&& !rational_raw<B>
&& (std::signed_integral<raw_t<B>>          // raw fits imax
    || NotchCount<B> <= imax_max)
```

Two helpers, used at three call sites:

| Helper | Direction | Used by |
|---|---|---|
| `q_format_encode<B>(imax)` | value → raw | `from_value`, `assignment::store` |
| `q_format_decode<B>(B)`    | raw → rational | `inside::operator rational()` |

The raw-fits-in-`imax` clause exists because the Q-format result type of a
multiplication can land on `uint64_t` raw (e.g. `Q16.16 × Q16.16` produces
`NotchCount ≈ 2^64`); widening that to `imax` via `raw_imax` would wrap.
When the gate is false, control falls through to the slow but correct
rational path — `(*(Raw * Notch) + Lower).value()` for decode,
`((rhs - Lower) / Notch).value().Numerator` for encode.

**Fractional rhs takes the same shortcut.** `store_checked`
(`detail/assignment.hpp`) stores a rational/double rhs on a Q-format grid without
the two gcd-reducing rational operations: with notch `1/K`, the offset
`((num/aden) − Lo)·K` reduces to `(num − Lo·aden)·(K/g) / (aden/g)`,
`g = gcd(aden, K)` — one `std::gcd` and three integer multiplies, then
`round_quotient` on the remaining fraction. `round_quotient` is invariant
under fraction reduction (the same equivalence the `grid_fast_store` path in
`cmath.hpp` relies on), so the chosen slot is bit-identical to the rational
path. A saturating compile-time fit inside keeps every intermediate inside
`imax`; oversized denominators fall through to the rational path.

---

## 4. Policy cascade

When a value is assigned into an inside, the runtime behaviour on
out-of-range is determined by a four-level cascade:

```text
┌─────────────────────────────────────────────────────────────────┐
│ 1. per-operation action callback   on_clamp(λ), on_wrap(λ), …    │
│                                       │                          │
│                                       ▼ if no callback           │
│ 2. per-operation policy override   b.with_clamp() = …            │
│                                    b.policy<F>() = …             │
│                                       │                          │
│                                       ▼ if not overridden        │
│ 3. default policy from type        inside<G, clamp>, inside<G, P>  │
│                                       │                          │
│                                       ▼ if P does not handle it  │
│ 4. hard default                    error_handler → inside_error  │
└─────────────────────────────────────────────────────────────────┘
```

The hard default funnels through the replaceable `beman::inside::error_handler` (see
[policies.md](policies.md#replacing-the-throw-handler-freestanding--bare-metal)): the
installed handler throws `beman::inside::inside_error` (carrying the `errc`) by default, or traps
under `-fno-exceptions`. There is no `<system_error>` dependency.

The flag bits live in `policy_flag` (`include/beman/inside/policy_flag.hpp`):
`clamp`, `wrap`, `checked`, `unsafe`, `snap`,
`ignore_domain`, `ignore_zero`, `round_floor`, `round_ceil`,
`round_nearest`, `round_half_even`.

The cascade is implemented in `detail/assignment.hpp` — see
`try_clamp_or_fail` (insidable RHS, ~line 535), `handle_out_of_range`
(integral RHS, ~line 224), and the `apply_clamp` / `apply_wrap` siblings.
The `is_*_action<A>` traits (defined in `policy.hpp`) decide which step
gets priority for a given callback set.

---

## 5. Conversion summary

`inside::operator imax()` — **implicit**, only when notch is
integer-aligned. Matches native-int performance and ergonomics:
`int n = inside<{0,100}>{42};` just works. It is deliberately the **only**
implicit integer conversion — a second one (a removed `operator size_t`
once existed) makes built-in mixed arithmetic like `imax_var += b`
ambiguous. Indexing reaches `size_t` through imax's standard conversion.

`inside::operator rational()` — **implicit**. Lossless and mathematically
exact, so no risk in letting it happen silently.

`inside::operator double()` — `explicit((P & f64) != f64)`. An
`f64`-policy inside lives on a double-exact grid, so every value is exactly
representable in `double` and the conversion is lossless — implicit, by the
same rule as `operator rational`. For everything else the conversion can
round, so it is **explicit** AND gated on a rounding policy flag; strict
bounds opt in through `to<double>()`. `as<floating>()` shares the gate so
the two spellings agree.

`to<T>(b)` / `as<T>(b)` — free-function forms of the members, for generic
code (no `.template` disambiguator); ADL-found, same constraints.

`rational::operator T()` (for unsigned, signed, floating) — **explicit**
in all cases; rationals truncate toward zero on integer conversion.

The named integer reductions on `rational` — `r.trunc()`, `r.floor()`,
`r.ceil()`, `r.round()` — replace ad-hoc `static_cast<imax>(r)` calls when
intent matters.

---

## 6. The `as_rational` / `raw_imax` / `to_value` triad

These three helpers in `include/beman/inside/generic.hpp` exist because three
different "extract the value" intents used to spell the same
`static_cast<imax>(...)`:

| Helper | Returns | Use when |
|---|---|---|
| `detail::as_rational(x)` | `rational` (lossless) | You want exact-rational arithmetic on `x` |
| `raw_imax(b)`  | `imax` (raw widened) | You want the **raw** as a signed integer (e.g. inside offset arithmetic) |
| `to_value(b)`    | `imax` (truncated value) | You want the inside's **value** as an integer |

When `!index_raw<B>` (raw is the value), `raw_imax(b) == to_value(b)`. For
index storage they differ — `Raw` is an index, `to_value` multiplies by
`Notch` and adds `Lower`. On integer-aligned grids that is one integer
multiply-add, on Q-format grids one truncating integer divide; only other grids
go through `rational`. `from_value` mirrors it.

---

## 7. Error vocabulary

Two shapes, each by role:

| Shape | Role | Why this shape |
|---|---|---|
| policy cascade (`error_handler`/throw / `errc` / clamp / wrap / `on_*`) | NARROWING a value *into* an inside (construction, assignment, compound ops) | the caller chose the failure semantics on the type or operation |
| `std::expected<T, errc>` | every fallible RESULT: inside ARITHMETIC (`/`, `%`, checked exact `+`/`×`), QUERIES and MATH (`to<T>()`, `try_make`, `tan`, `pow`, mixed-sign `sqrt`) | the caller tests the result and can dispatch on the cause; auto-chains through the lift operators; uniform across `beman::inside::math` |

`std::expected<inside, errc>` is larger than `inside` (it adds a flag and an
`errc`), so it only ever appears as a **parameter or return value** — never as
stored state (a data member, container element or iterator state) — and only
where the operation can actually fail: a result type the grids prove total stays
a plain `inside`.

Per-operation audit:

| Operation | Shape | Causes |
|---|---|---|
| `a + b`, `a − b`, `a × b` (integer/float-backed raws) | `inside` | total — result grid contains every value by construction |
| same, rational raw + `checked`, overflow not provably excluded | `expected` | `overflow`. Notched grids inside the denominators, so most `exact` arithmetic PROVES safety at compile time and returns a plain `inside`; continuous (Notch 0) grids hold arbitrary rationals and keep the wrapper |
| `a / b`, `mod` (divisor grid excludes 0) | `inside` | total |
| `a / b`, `mod` (divisor may be 0) | `expected` | `division_by_zero`, `overflow` (rational raw) |
| `math::sin/cos/exp/log/…` | `inside` | total over the asserted domain |
| `math::tan` | `expected` | `division_by_zero` (pole), `overflow` (past Out) |
| `math::pow` | `expected` | `division_by_zero`, `overflow` (envelope) |
| `math::sqrt` (mixed-sign) | `expected` | `domain_error` (negative value) |
| `to<T>()` | `expected` | `overflow` (out of range), `domain_error` (negative→unsigned) |
| `as<T>()` | `T` | asserts (caller vouches for the range) |
| `try_make` | `expected` | `errc`: out of range / off-notch |
| construction / assignment | policy cascade | per the inside's policy |
| `beman::inside::sum<Target>` | `Target` | Target's policy, applied once to the total |

Chaining (`lift.hpp` / `arithmetic.hpp`): the expected-lift operators keep
`expected` chains intact — `a / b * gain + offset` is an
`expected<inside, errc>` end to end. The first (left) error wins and keeps its
cause; an operation that fails inside the chain reports its own.

## 8. Header layout

After the 2026 cleanup the public API is split across multiple headers. The
umbrella `beman/inside/inside.hpp` transitively pulls the core (arithmetic, casts, range,
storage, error funnel); the **opt-in** layers — `beman/inside/io.hpp`, `beman/inside/cmath.hpp`,
`beman/inside/numeric_limits.hpp`, `beman/inside/formats.hpp` — are included only on demand, which
is what keeps the core free of `<string>`/`<ostream>`/`<format>`/`<cmath>`:

| Header | Contains |
|---|---|
| `beman/inside/inside.hpp`       | `inside<G, P>` struct, compound assignments, `<=>`, `==`, `_ins` literal, increment/decrement |
| `beman/inside/casts.hpp`       | `clamp_cast`, `wrap_cast`, `checked_cast`, `unchecked_cast`, `clamp_floor` / `clamp_ceil` / `clamp_round` |
| `beman/inside/arithmetic.hpp`  | Free `add` / `sub` / `mul` / `div` / `mod` (one variadic overload each; `detail::arith` maps the three call forms — policy, actions, `errc&` — onto the op's core), variadic folds `add_all` / `mul_all`, `operator+` / `-` / `*` / `/` / `%`, expected-lift overloads |
| `beman/inside/range.hpp`       | `inside_range<G, P>` iterator helper |
| `beman/inside/generic.hpp`     | Public grid/policy introspection (`Grid` / `InsidePolicy` / `Interval` / `Lower` / `Upper` / `Notch`) and the `insidable` / `numeric` / `inside_assignable` concepts. Storage/raw/dispatch plumbing (`raw_t`, the `rational_raw` / `real_raw` / `value_raw` / `index_raw` predicates, `as_double`, `to_value` / `from_value`, `raw_cast` / `raw_imax`, `q_format_encode/decode`, `NotchCount`, `RawLo/Hi`, `detail::as_rational`, …) lives in `beman::inside::detail` |
| `beman/inside/detail/assignment.hpp`  | `beman::inside::detail::assignment<L, R>` specialisations for integral / fractional / insidable rhs (incl. the Q-format integer shortcut for fractional rhs) |
| `beman/inside/cmath.hpp`       | `beman::inside::math` — the `<cmath>`-shaped public API (trig, inverse trig, hyperbolic, exp/log/pow, sqrt/cbrt/hypot) over bounds, re-exported from the build's `default_engine` (`dbl` / `flt` / `cordic`). The integer/CORDIC cores live in `beman::inside::math::detail` here — they also serve as the compile-time output-grid oracle for **every** engine. See [math.md](math.md) |
| `beman/inside/cmath_double.hpp` | The default **double engine** cores (`d_sin`, `d_exp`, … — own `std::fma`-Horner polynomials, Cody-Waite reduction, correctly-rounded `std::sqrt`); compiled out under `BEMAN_INSIDE_MATH_NO_FP` |
| `beman/inside/cmath_float.hpp` | The **float engine** cores (binary32 siblings of the double cores, own compile-time-derived range-reduction constants); default under `BEMAN_INSIDE_MATH_FLOAT`, compiled out under `BEMAN_INSIDE_MATH_NO_FP` |
| `beman/inside/detail/addition.hpp`, `multiplication.hpp`, `division.hpp` | `beman::inside::detail::addition<L, R>`, `multiplication<L, R>`, `division<L, R, F>`, `modulo<L, R, F>` — implementation detail, included via `inside.hpp` |
| `beman/inside/detail/overflow.hpp`, `debug.hpp` | `add_overflow` / `sub_overflow` / `mul_overflow` (the GCC/Clang `__builtin_*_overflow`); `errc`, the replaceable `error_handler` + `detail::raise` funnel — implementation detail |
| `beman/inside/detail/rational.hpp`    | `rational` and its checked / unchecked arithmetic |
| `beman/inside/grid.hpp`        | `grid`, `storage_min`, grid operators |
| `beman/inside/numeric_limits.hpp` | `std::numeric_limits<inside>` and `std::hash<inside>` specialisations (opt-in) |
| `beman/inside/io.hpp`          | **All** string/stream/`std::format` support — `to_string`, `to_string_debug`, `operator<<`, `std::formatter`, `type_name`. Opt-in and the *only* place `<string>`/`<ostream>`/`<format>` enter; gated by `BEMAN_INSIDE_NO_STRING` in the single header (see [freestanding.md](freestanding.md)) |
| `beman/inside/formats.hpp`     | Curated Q-format aliases (`q4_4`, `q8_8`, `q16_16`, …); opt-in |
| `beman/inside/lift.hpp`        | `lift` — monadic composition over `std::expected<T, errc>` arguments |

The whole tree is also amalgamated into `single_include/beman/inside/inside.hpp` by a
pure-CMake generator — see [single-header.md](single-header.md) for usage and
the regeneration workflow.
