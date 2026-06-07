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

## 2. Storage decision tree

`storage_min<G>` (`include/bound/grid.hpp:94`) picks the smallest raw type
that can hold every reachable index in `G`. Three shapes:

```text
                       ┌────────────┐
                       │  Notch == 0│──yes──▶  raw = rational
                       └────────────┘         (IsRawRational)
                              │ no
                              ▼
                  ┌──────────────────────┐
                  │ Lower < 0, Notch == 1│──yes──▶  raw = smallest signed int
                  └──────────────────────┘         (IsDirectStorage)
                              │ no
                              ▼
                  ┌──────────────────────┐
                  │  smallest_uint_for   │────────▶  raw = smallest unsigned int
                  │   (max_notch())      │         (IsNotchStorage)
                  └──────────────────────┘
```

Predicates (`include/bound/generic.hpp`):

- `IsRawRational<B>` (line 39) — the raw type is `rational`. Notch is zero.
- `IsDirectStorage<B>` (line 119) — `Raw == value`. Either signed-integer
  raw with `Notch == 1`, or unsigned raw with `Lower == 0 && Notch == 1`,
  or rational raw.
- `IsNotchStorage<B>` (line 125) — `Raw` is the notch index;
  `value = Raw * Notch + Lower`.

Two more derived predicates classify subsets:

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
&& RawFitsInImax<B>
```

Two helpers, used at three call sites:

| Helper | Direction | Used by |
|---|---|---|
| `q_format_encode<B>(imax)` | value → raw | `from_value`, `assignment::store` |
| `q_format_decode<B>(B)`    | raw → rational | `bound::operator rational()` |

The `RawFitsInImax` clause exists because the Q-format result type of a
multiplication can land on `uint64_t` raw (e.g. `Q16.16 × Q16.16` produces
`NotchCount ≈ 2^64`); widening that to `imax` via `raw_imax` would wrap.
When the gate is false, control falls through to the slow but correct
rational path — `(*(Raw * Notch) + Lower).value()` for decode,
`((rhs - Lower) / Notch).value().Numerator` for encode.

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
│ 4. hard default                    throw std::system_error       │
└─────────────────────────────────────────────────────────────────┘
```

The flag bits live in `policy_flag` (`include/bound/policy_flag.hpp`):
`clamp`, `wrap`, `sentinel`, `checked`, `unsafe`, `ignore_round`,
`ignore_domain`, `ignore_zero`, `round_floor`, `round_ceil`,
`round_nearest`, `round_half_even`.

The cascade is implemented in `assignment.hpp` — see
`try_clamp_or_fail` (boundable RHS, ~line 535), `handle_out_of_range`
(integral RHS, ~line 224), and the `apply_clamp` / `apply_wrap` siblings.
The `is_*_action<A>` traits (defined in `policy.hpp`) decide which step
gets priority for a given callback set.

---

## 5. Conversion summary

`bound::operator imax()` — **implicit**, only when notch is
integer-aligned. Matches native-int performance and ergonomics:
`int n = bound<{0,100}>{42};` just works.

`bound::operator rational()` — **implicit**. Lossless and mathematically
exact, so no risk in letting it happen silently.

`bound::operator double()` — **explicit**. Never silently demote
arithmetic to floating-point and lose exact-rational guarantees; callers
opt in with `double(b)`.

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

For `IsDirectStorage<B>`, `raw_imax(b) == to_value(b)`. For
`IsNotchStorage<B>`, they differ — `Raw` is an index, `to_value`
multiplies by `Notch` and adds `Lower`.

---

## 7. Header layout

After the 2026 cleanup the public API is split across multiple headers,
all transitively included by `bound/bound.hpp`:

| Header | Contains |
|---|---|
| `bound/bound.hpp`       | `bound<G, P>` struct, compound assignments, `<=>`, `==`, `_b` literal, increment/decrement |
| `bound/casts.hpp`       | `clamp_cast`, `wrap_cast`, `checked_cast`, `unchecked_cast`, `clamp_floor` / `clamp_ceil` / `clamp_round` |
| `bound/arithmetic.hpp`  | Free `add` / `sub` / `mul` / `div` / `mod`, variadic folds `add_all` / `mul_all`, `operator+` / `-` / `*` / `/` / `%`, optional-lift overloads |
| `bound/range.hpp`       | `bound_range<G, P>` iterator helper |
| `bound/generic.hpp`     | Traits (`is_*`), metafunctions (`Lower` / `Upper` / `Notch` / `LowerImax` / `UpperImax` / `RawLo` / `RawHi` / `NotchCount`), `detail::as_rational`, `to_value` / `raw_imax`, `q_format_encode/decode`, concepts |
| `bound/assignment.hpp`  | `assignment<L, R>` specialisations for integral / real / boundable rhs |
| `bound/detail/addition.hpp`, `multiplication.hpp`, `division.hpp` | `addition<L, R>`, `multiplication<L, R>`, `division<L, R, F>`, `modulo<L, R, F>` — implementation detail, included via `bound.hpp` |
| `bound/detail/overflow.hpp`, `debug.hpp` | `add_overflow` / `sub_overflow` / `mul_overflow` (builtins + portable fallback), stacktrace plumbing — implementation detail |
| `bound/rational.hpp`    | `rational`, its arithmetic, sentinel traits |
| `bound/grid.hpp`        | `grid`, `storage_min`, grid operators |
| `bound/numeric_limits.hpp` | `std::numeric_limits<bound>` and `std::hash<bound>` specialisations (opt-in) |
| `slim/optional.hpp`     | Reusable sentinel-based optional; `bnd::` consumes it via `sentinel_traits` |
