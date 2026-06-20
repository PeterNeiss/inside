# bound library — internals

This document explains *why* the library is shaped the way it is. It is not a
tutorial; for that see `README.md`. Use this when you need to add a new
arithmetic operator, debug a storage-shape edge case, or reason about
performance.

> The exact-fraction representation type is `bnd::detail::rational` — an
> **internal** type. It is the grid's NTTP substrate and the raw storage for
> non-dyadic grids, so it appears throughout these internals, but it is not on
> the public surface: consumers spell grids with literals / `notch<N,D>` /
> `frac<N,D>`, read exact values out with `numerator()` / `denominator()`, and
> never name the type. The bare word "rational" below always means
> `bnd::detail::rational`.

---

## 1. Grid invariants

Every `bound<G, P>` carries a `grid G` value with the following invariants,
enforced at type-instantiation time by `grid::validate` (`include/bound/grid.hpp:57`):

- **`Lower ≤ Upper`** (rational comparison).
- **`Interval.divides_evenly(Notch)`** — there must be an integer number of
  notches between Lower and Upper. The notch count is exposed as
  `NotchCount<B>` (`include/bound/generic.hpp:80`).
- **`Notch == 0` is legal** and means "any rational in the interval". The
  storage shape changes accordingly (see §2).
- **`Lower/Notch` and `Upper/Notch` resolve to integer rationals** when `Notch != 0`.

These invariants let the library compute result grids at compile time
without runtime overflow checks for grid arithmetic itself — every
reachable value of `a + b` for `a : A, b : B` is by construction inside
`Grid<A> + Grid<B>`.

---

## 2. Storage encoding

Representation is selected by the policy's **representation flags**
(`exact` / `real` / `direct` / `indexed`, see
[policies.md](policies.md#representation-flags)), with grid deduction as the
default. `storage_pick<G, P>` (`include/bound/grid.hpp`) resolves the flags
**widest-wins** — a result of mixed-representation arithmetic ORs both
operand policies, and the widest representation present wins:

```text
  exact in P ──────────────────────────▶  rational raw   (raw IS the value, exact fraction)
       │ no
  real in P AND double_exact grid ─────▶  double raw     (raw IS the value; default engine
       │ no   (elided under BND_MATH_FIXED)               only — fixed engine falls through.
       │                                                  Direct misuse on a too-fine grid is a
       │                                                  static_assert; arithmetic instead DROPS
       │                                                  `real` when the result isn't double_exact)
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
reachable index (with the sentinel-slot margin, see
[storage.md](storage.md)).

Four **disjoint predicates** in `include/bound/generic.hpp` classify a
bound's encoding (the first two read the raw type alone; the integer pair
also consults the policy, mirroring `storage_pick` exactly):

| Predicate | Meaning |
|---|---|
| `rational_raw<B>` | raw IS the value, as an exact fraction |
| `real_raw<B>`     | raw IS the value, as an IEEE-754 `double` |
| `value_raw<B>`    | raw IS the value, as a plain integer |
| `index_raw<B>`    | raw is a 0-based notch index; value = Lower + raw·Notch |

The common query `!index_raw<B>` means "raw is the value" (any of the first
three). Note the decode direction must dispatch on the **encoding, not the
raw type's signedness** — a `direct` bound with Lower ≥ 0 has an *unsigned*
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
(`include/bound/generic.hpp:142`):

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
| `q_format_decode<B>(B)`    | raw → rational | `bound::operator rational()` |

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
path. A saturating compile-time fit bound keeps every intermediate inside
`imax`; oversized denominators fall through to the rational path.

---

## 4. Policy cascade

When a value is assigned into a bound, the runtime behaviour on
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
│ 3. default policy from type        bound<G, clamp>, bound<G, P>  │
│                                       │                          │
│                                       ▼ if P does not handle it  │
│ 4. hard default                    throw bnd::bound_error       │
└─────────────────────────────────────────────────────────────────┘
```

The flag bits live in `policy_flag` (`include/bound/policy_flag.hpp`):
`clamp`, `wrap`, `sentinel`, `checked`, `unsafe`, `snapping`,
`ignore_domain`, `ignore_zero`, `round_floor`, `round_ceil`,
`round_nearest`, `round_half_even`.

The cascade is implemented in `detail/assignment.hpp` — see
`try_clamp_or_fail` (boundable RHS, ~line 535), `handle_out_of_range`
(integral RHS, ~line 224), and the `apply_clamp` / `apply_wrap` siblings.
The `is_*_action<A>` traits (defined in `policy.hpp`) decide which step
gets priority for a given callback set.

---

## 5. Conversion summary

`bound::operator imax()` — **implicit**, only when notch is
integer-aligned. Matches native-int performance and ergonomics:
`int n = bound<{0,100}>{42};` just works. It is deliberately the **only**
implicit integer conversion — a second one (a removed `operator size_t`
once existed) makes built-in mixed arithmetic like `imax_var += b`
ambiguous. Indexing reaches `size_t` through imax's standard conversion.

`bound::operator rational()` — **implicit**. Lossless and mathematically
exact, so no risk in letting it happen silently.

`bound::operator double()` — `explicit((P & real) != real)`. A
`real`-policy bound lives on a double-exact grid, so every value is exactly
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

These three helpers in `include/bound/generic.hpp` exist because three
different "extract the value" intents used to spell the same
`static_cast<imax>(...)`:

| Helper | Returns | Use when |
|---|---|---|
| `detail::as_rational(x)` | `rational` (lossless) | You want exact-rational arithmetic on `x` |
| `raw_imax(b)`  | `imax` (raw widened) | You want the **raw** as a signed integer (e.g. inside offset arithmetic) |
| `to_value(b)`    | `imax` (truncated value) | You want the bound's **value** as an integer |

When `!index_raw<B>` (raw is the value), `raw_imax(b) == to_value(b)`. For
index storage they differ — `Raw` is an index, `to_value` multiplies by
`Notch` and adds `Lower`.

---

## 7. Error vocabulary

Three shapes, each by role — nothing is flattened; each earns its place:

| Shape | Role | Why this shape |
|---|---|---|
| policy cascade (throw / `error_code` / sentinel / clamp / wrap / `on_*`) | NARROWING a value *into* a bound (construction, assignment, compound ops) | the caller chose the failure semantics on the type or operation |
| `slim::optional<bound>` | fallible bound-valued ARITHMETIC (`/`, `%`, checked exact `+`/`×`) | single dominant cause per op; **zero size overhead** (sentinel encoding); auto-chains through the lift operators |
| `slim::expected<T, errc>` | fallible QUERIES and MATH (`to<T>()`, `tan`, `pow`, mixed-sign `sqrt`) | multiple causes the caller dispatches on; uniform across `bnd::math` |

Per-operation audit:

| Operation | Shape | Causes |
|---|---|---|
| `a + b`, `a − b`, `a × b` (integer/real raws) | `bound` | total — result grid contains every value by construction |
| same, rational raw + `checked`, overflow not provably excluded | `optional` | exact-arithmetic overflow. Notched grids bound the denominators, so most `exact` arithmetic PROVES safety at compile time and returns a plain `bound`; continuous (Notch 0) grids hold arbitrary rationals and keep the wrapper |
| `a / b`, `mod` (divisor grid excludes 0) | `bound` | total |
| `a / b`, `mod` (divisor may be 0) | `optional` | division by zero (rational overflow folds in) |
| `math::sin/cos/exp/log/…` | `bound` | total over the asserted domain |
| `math::tan` | `expected` | `division_by_zero` (pole), `overflow` (past Out) |
| `math::pow` | `expected` | `division_by_zero`, `overflow` (envelope) |
| `math::sqrt` (mixed-sign) | `expected` | `domain_error` (negative value) |
| `to<T>()` | `expected` | `not_a_value` (sentinel), `overflow` (out of range), `domain_error` (negative→unsigned) |
| `as<T>()` | `T` | asserts (caller vouches for the range) |
| `try_make` | `optional` | out of range |
| construction / assignment | policy cascade | per the bound's policy |
| `bnd::sum<Target>` | `Target` | Target's policy, applied once to the total |

Bridging the families (`lift.hpp` / `arithmetic.hpp`): `bnd::ok(e)` converts
an expected into the optional world (deliberately dropping the cause);
the expected-lift operators keep `expected` chains intact (first/left error
wins; a division's nullopt maps to `division_by_zero`). Mixing an `expected`
and an `optional` operand in one expression is a compile error with guidance
— the optional's original cause is unknowable, so the library won't invent
one.

## 8. Header layout

After the 2026 cleanup the public API is split across multiple headers,
all transitively included by `bound/bound.hpp`:

| Header | Contains |
|---|---|
| `bound/bound.hpp`       | `bound<G, P>` struct, compound assignments, `<=>`, `==`, `_b` literal, increment/decrement |
| `bound/casts.hpp`       | `clamp_cast`, `wrap_cast`, `checked_cast`, `unchecked_cast`, `clamp_floor` / `clamp_ceil` / `clamp_round` |
| `bound/arithmetic.hpp`  | Free `add` / `sub` / `mul` / `div` / `mod`, variadic folds `add_all` / `mul_all`, `operator+` / `-` / `*` / `/` / `%`, optional-lift overloads |
| `bound/range.hpp`       | `bound_range<G, P>` iterator helper |
| `bound/generic.hpp`     | Public grid/policy introspection (`Grid` / `BoundPolicy` / `Interval` / `Lower` / `Upper` / `Notch`) and the `boundable` / `numeric` / `bound_assignable` concepts. Storage/raw/dispatch plumbing (`raw_t`, the `rational_raw` / `real_raw` / `value_raw` / `index_raw` predicates, `as_double`, `to_value` / `from_value`, `raw_cast` / `raw_imax`, `q_format_encode/decode`, `NotchCount`, `RawLo/Hi`, `sentinel_raw`, `detail::as_rational`, …) lives in `bnd::detail` |
| `bound/detail/assignment.hpp`  | `bnd::detail::assignment<L, R>` specialisations for integral / fractional / boundable rhs (incl. the Q-format integer shortcut for fractional rhs) |
| `bound/cmath.hpp`       | `bnd::math` — the `<cmath>`-shaped public API (trig, inverse trig, hyperbolic, exp/log/pow, sqrt/cbrt/hypot) over bounds, dispatching to one of two engines. The integer/CORDIC cores live in `bnd::math::detail` here — they also serve as the compile-time output-grid oracle for **both** engines. See [math.md](math.md) |
| `bound/cmath_double.hpp` | The default **double engine** cores (`d_sin`, `d_exp`, … — own `std::fma`-Horner polynomials, Cody-Waite reduction, correctly-rounded `std::sqrt`); selected unless `BND_MATH_FIXED` is defined |
| `bound/detail/addition.hpp`, `multiplication.hpp`, `division.hpp` | `bnd::detail::addition<L, R>`, `multiplication<L, R>`, `division<L, R, F>`, `modulo<L, R, F>` — implementation detail, included via `bound.hpp` |
| `bound/detail/overflow.hpp`, `debug.hpp` | `add_overflow` / `sub_overflow` / `mul_overflow` (builtins + portable fallback); `errc`, the replaceable `error_handler` + `detail::raise` funnel — implementation detail |
| `bound/detail/rational.hpp`    | `rational`, its arithmetic, sentinel traits |
| `bound/grid.hpp`        | `grid`, `storage_min`, grid operators |
| `bound/numeric_limits.hpp` | `std::numeric_limits<bound>` and `std::hash<bound>` specialisations (opt-in) |
| `slim/optional.hpp`     | Reusable sentinel-based optional; `bnd::` consumes it via `sentinel_traits` |
