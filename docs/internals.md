# inside library — internals

This document explains *why* the library is shaped the way it is. It is not a
tutorial; for that see [tutorial.md](tutorial.md). Use this when you need to add a new
arithmetic operator, debug a storage-shape edge case, or reason about
performance.

> The exact-fraction representation type is `beman::inside::detail::rational` — an
> **internal** type. It is the raw storage for continuous grids and (under C++23)
> the grid's NTTP substrate — under C++26 grids use `detail::big_rational`, see
> §2a — so it appears throughout these internals, but it is not on
> the public surface: consumers spell grids with literals / `per<D>` /
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
  `max_index_v<B>` (`include/beman/inside/generic.hpp`).
- **`Notch == 0` is legal** and means "any rational in the interval". The
  storage shape changes accordingly (see §2).
- **`Lower/Notch` and `Upper/Notch` resolve to integer rationals** when `Notch != 0`.
- **`Notch ≥ 0`** — decoding is `Lower + raw·Notch`, so a negative notch would
  count downward (`per<D>` is positive by construction; any other spelling
  of a negative notch is rejected here).

**Two-limit grids.** `grid{lo, hi}` derives `Notch = gcd(1, Lower, Upper)`
(rational gcd: gcd of numerators over lcm of denominators) — the coarsest step
`1/k` keeping every integer and both limits on the lattice. Integer limits give
1, so every grid that once took the fixed notch 1 is unchanged. With 64-bit
grid numbers a combined denominator past `imax` falls back to `Notch = 0`;
under C++26 the exact gcd is kept (§2a). A floating-point limit is its
exact binary value, so a double that needs a notch finer than 1/1024 (0.1 is
3602879701896397/2^55) is rejected at compile time in favour of `0.1_r` or an
explicit notch; rational, `frac`, `_r` and inside limits are taken exactly.

`grid::try_make(interval, notch)` checks the same invariants at runtime and
returns `std::expected<grid, errc>` — `domain_error` for `Lower > Upper`,
`rounding_error` when the notch does not divide the interval or `Lower` is off
the lattice. It serves grids built from runtime configuration (for instance, to
validate a config before choosing between precompiled types); a runtime `grid`
cannot become an `inside<G, P>` template argument.

These invariants let the library compute result grids at compile time
without runtime overflow checks for grid arithmetic itself — every
reachable value of `a + b` for `a : A, b : B` is by construction inside
`grid_of<A> + grid_of<B>`.

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
  Lower == Upper (point), no width flag ▶  empty raw      (value lives in the type; reads as
       │ no                                               index 0, so value = Lower)
  exact in P ──────────────────────────▶  rational raw   (raw IS the value, exact fraction)
       │ no
  f64 in P AND double_exact grid ──────▶  double raw     (raw IS the value; with an FPU
       │ no   (elided under BEMAN_INSIDE_MATH_NO_FP)                only — NO_FP falls through.
       │                                                  Direct misuse on a too-fine grid is a
       │                                                  static_assert; arithmetic instead DROPS
       │                                                  `f64` when the result isn't double_exact)
  f32 in P AND float_exact grid ───────▶  float raw      (else widened to double when
       │ no   (elided under BEMAN_INSIDE_MATH_NO_FP)                double_exact; same misuse rule)
  i8…u64 width flag in P ──────────────▶  that integer   (value storage, or index with
       │ no                                               `indexed`; too small = static_assert)
  direct in P AND Notch == 1 ──────────▶  integer raw    (raw IS the value)
       │ no
  indexed in P AND Notch != 0 ─────────▶  unsigned raw   (raw = 0-based notch index)
       │ no
  deduced (storage_min_t<G>):
        Notch == 0                  ───▶  rational raw   (continuous grid)
        index count > umax          ───▶  wide_int raw   (index in 64-bit limbs; exact
                                                          wide paths, detail/wide_value.hpp)
        Notch == 1 AND (Lower == 0
          or signed raw)            ───▶  integer raw    (raw IS the value)
        otherwise                   ───▶  unsigned raw   (raw = 0-based notch index)
```

`storage_min_t<G>` picks the smallest integer type that can hold every
reachable index, using the type's full range (see [storage.md](storage.md)).

Four **disjoint predicates** in `include/beman/inside/generic.hpp` classify an
inside's encoding (the first two read the raw type alone; the integer pair
also consults the policy, mirroring `storage_pick` exactly):

| Predicate | Meaning |
|---|---|
| `rational_raw<B>` | raw IS the value, as an exact fraction |
| `fp_raw<B>`       | raw IS the value, as an IEEE-754 `double` or `float` (`f64_raw` / `f32_raw`) |
| `value_raw<B>`    | raw IS the value, as a plain integer |
| `index_raw<B>`    | raw is a 0-based notch index; value = Lower + raw·Notch. Includes `point_raw<B>`: a point's empty `point_slot` raw, which reads as index 0 |

The common query `!index_raw<B>` means "raw is the value" (any of the first
three). Note the decode direction must dispatch on the **encoding, not the
raw type's signedness** — a `direct` inside with Lower ≥ 0 has an *unsigned*
value raw; `detail::as_double` is the kind-aware raw → double decoder.

Two more predicates classify the grid's integer-ness (independent of the
storage encoding), gating arithmetic fast paths:

- `is_integer_interval<B>` — `Lower` and `Upper` have integer denominators
  (Notch may still be fractional, e.g. `{0, 100}, 1/10`).
- `is_integer_aligned<B>` — `Notch` and `Lower` have integer denominators.
  Under the divides-evenly invariant this implies `is_integer_interval`,
  but the converse is not true. Both predicates exist because they gate
  different fast paths.

---

## 2a. Grid numbers, wide raws and the exact paths

**Grid numbers.** `detail/grid_rational.hpp` names one vocabulary for both
language modes: `grid_rational` (the type of a grid's limits and notch, and of
`lower_of` / `upper_of` / `notch_of`), `grid_wide` (exact integers for slot
counts and value indices), `wide_numerator` / `wide_denominator`,
`grid_divides_evenly`, `grid_same_lattice`, `grid_gcd` and `to_rational`.
Under C++23 these are the 64-bit `rational` and a 4-limb `wide_int`. Under
C++26 with static reflection (`BEMAN_INSIDE_BIG_GRIDS`) they are `big_rational`
and `big_int` (`detail/big_rational.hpp`): canonical values whose magnitude
past one limb is interned with `std::define_static_array`, so equal values are
equal template arguments. Interning is consteval: a big value exists only at
compile time, and runtime code reads big grid numbers through constants.

The integer fast paths work in 64 bits. They read `detail::lower64` /
`upper64` / `notch64`, which fail the build for a grid number past 64 bits
instead of truncating; the math engine and the grid operations read
`lower_of` / `upper_of` / `notch_of` and exact values, so they take any size. Naming such a view instantiates it
even in a short-circuited `&&`, so predicates every inside instantiates
compare grid numbers, or hide the view behind `if constexpr`.

**Wide raws and exact-valued insides.** `int_for_bits_t<Bits, Signed>`
(`detail/int_for_bits.hpp`) is the one width rule: a builtin integer up to 64
bits, else a `wide_int` (`detail/wide_int.hpp`, Knuth-D division, the only
`__int128` site). A grid with more than 2⁶⁴ slots gets a `wide_int` index
(`wide_raw<B>`); a grid number past 64 bits makes `big_valued<B>`. Either makes
`exact_valued<B>`, which routes every operation to the exact paths of
`detail/wide_value.hpp`.

**Value indices.** On a valid grid `m = Lower/Notch` is an integer
(`slot_base<B>`), so every value is `(m + raw)·Notch`, exactly. The exact paths
carry values as `exact_frac<K>` (an unreduced fraction of `K`-limb integers);
`exact_limbs<Bs...>` sizes `K` from the grids involved — three times the
widest value, since the worst case is a product of two values over a third
notch — and binary operations widen to the wider operand. A store divides by
the target notch and rounds by the policy's mode (`exact_index`), then runs the
usual out-of-range cascade (`assign_exact`).

**Integer `+` and `×`.** Both run on value indices for every integer-raw
grid: `J_L·(N_L/N) + J_R·(N_R/N)` and `J_L·J_R` (in each operand's unit from
the product grid). The work type is `imax` when compile-time bounds prove no
value index can overflow — the compiler then keeps the value ranges, as the old
builtin paths did — else an unsigned type as wide as the result raw, whose
wrapping ring arithmetic still yields the exact raw (`index_work_t`). Compound
`+=` / `-=` size one signed work type from the raw and delta ranges
(`raw_work_t`), and clamp/wrap onto unit grids use `unit_fold`, sized the same
way; both are `imax` for every grid within int64.

---

## 3. Q-format integer fast path

For grids with **integer Lower, unit-numerator Notch** (e.g. `1/256`,
`1/65536`), and a raw that fits in `imax`, the rational ↔ value conversion
collapses to integer arithmetic. The gate is `has_qformat_fast_path<B>`
(`include/beman/inside/generic.hpp`):

```cpp
abs_den(lower_of<B>.Denominator) == 1
&& notch_of<B>.Numerator == 1
&& !rational_raw<B>
&& (std::signed_integral<raw_t<B>>          // raw fits imax
    || max_index_v<B> <= imax_max)
```

Two helpers, used at three call sites:

| Helper | Direction | Used by |
|---|---|---|
| `q_format_encode<B>(imax)` | value → raw | `from_value`, `assignment::store` |
| `q_format_decode<B>(B)`    | raw → rational | `inside::operator rational()` |

The raw-fits-in-`imax` clause exists because the Q-format result type of a
multiplication can land on `uint64_t` raw (e.g. `Q16.16 × Q16.16` produces
`max_index_v ≈ 2^64`); widening that to `imax` via `raw_imax` would wrap.
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
`ignore_range`, `ignore_zero`, `round_floor`, `round_ceil`,
`round_nearest`, `round_half_even`, plus the representation flags.
`is_checked(P)` decides whether step 4 runs: true unless `P` carries `unsafe`
(an explicit `checked` overrides that), so every spelled policy is checked by
default, not only the default `P`.

The cascade is implemented in `detail/assignment.hpp` — see
`dispatch_out_of_range` (shared by every source kind), `try_clamp_or_fail`
(insidable RHS), `handle_out_of_range` (integral RHS), and the `apply_clamp` /
`apply_wrap` siblings. The `is_*_action<A>` traits (defined in
`policy_flag.hpp`) decide which step gets priority for a given callback set.

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

`inside::operator double()` — `explicit(!has_flag(P, f64) && !has_flag(P, f32))`,
and present only when `P` carries a rounding flag. An `f64`/`f32`-policy inside
lives on a double-exact grid, so every value is exactly representable in
`double` and the conversion is lossless — implicit, by the same rule as
`operator rational`. For everything else the conversion can round, so it is
**explicit** AND gated on a rounding policy flag; a strict inside opts in
through `to<double>()`. `as<floating>()` shares the gate so
the two spellings agree.

`to<T>(b)` / `as<T>(b)` — free-function forms of the members, for generic
code (no `.template` disambiguator); ADL-found, same constraints.

`rational::operator T()` (for unsigned, signed, floating) — **explicit**
in all cases; rationals truncate toward zero on integer conversion.
`r.to<T>()` is the typed-error form (`expected<T, errc>`).

The named integer reductions on `rational` are free functions —
`trunc(r)`, `floor(r)`, `ceil(r)`, `round(r)` (with `abs`, `sign`, `gcd`) —
and replace ad-hoc `static_cast<imax>(r)` calls when intent matters.

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
| `math::pow` | `expected` | `domain_error` (base ≤ 0), `overflow` (envelope, or past `Out`) |
| `math::sqrt` (mixed-sign) | `expected` | `domain_error` (negative value) |
| `to<T>()` | `expected` | `overflow` (out of range), `domain_error` (negative→unsigned) |
| `as<T>()` | `T` | `to<T>().value()` — throws on error (caller vouches for the range) |
| `try_make` | `expected` | `overflow` (out of range), `rounding_error` (off-notch) |
| construction / assignment | policy cascade | per the inside's policy; reported codes `overflow` / `rounding_error` / `not_finite` |
| `beman::inside::sum<Target>` | `Target` | Target's policy, applied once to the total |

Chaining (`lift.hpp` / `arithmetic.hpp`): the expected-lift operators keep
`expected` chains intact — `a / b * gain + offset` is an
`expected<inside, errc>` end to end. The first (left) error wins and keeps its
cause; an operation that fails inside the chain reports its own.

## 8. Header layout

The public API is split across multiple headers. The umbrella
`beman/inside/inside.hpp` pulls the core (`core.hpp`, then casts, arithmetic and
range, which need the complete type); the **opt-in** layers — `beman/inside/io.hpp`, `beman/inside/cmath.hpp`,
`beman/inside/numeric_limits.hpp`, `beman/inside/formats.hpp` — are included only on demand, which
is what keeps the core free of `<string>`/`<ostream>`/`<format>`/`<cmath>`:

| Header | Contains |
|---|---|
| `beman/inside/inside.hpp`       | The umbrella: includes `core.hpp`, `casts.hpp`, `arithmetic.hpp`, `range.hpp` |
| `beman/inside/core.hpp`         | `inside<G, P>` struct, compound assignments, `<=>`, `==`, `just` / `zero` / `one`, `_ins` literal, increment/decrement |
| `beman/inside/policy_flag.hpp`  | The `policy_flag` bits, `has_flag` / `is_checked`, rounding precedence, the `on_*` action factories and `is_*_action` traits |
| `beman/inside/policy.hpp`       | `policy<F, E>` (report / error-code mode), `make_policy`, the named policies (`snapped`, `rounded_*`, `clamped`, `wrapped`), `policy_ref` |
| `beman/inside/predicates.hpp`   | `conversion_overflows` / `conversion_rounds` / `conversion_is_lossy` |
| `beman/inside/interval.hpp`     | `interval` and its operators, `includes` / `excludes` / `overlaps` |
| `beman/inside/math.hpp`         | `umax` / `imax`, `smallest_uint_for` / `smallest_int_for`, the `arithmetic` / `fractional` concepts, constexpr `frexp` / `ldexp`, `abs_fraction` |
| `beman/inside/detail/rep.hpp`   | `fp_rep` — representation-flag propagation for arithmetic results (widest-wins, fp drop/widen) |
| `beman/inside/casts.hpp`       | `clamp_cast`, `wrap_cast`, `checked_cast`, `unchecked_cast`, `clamp_floor` / `clamp_ceil` / `clamp_round` |
| `beman/inside/arithmetic.hpp`  | Free `add` / `sub` / `mul` / `div` / `mod` (one variadic overload each; `detail::arith` maps the three call forms — policy, actions, `errc&` — onto the op's core), variadic folds `add_all` / `mul_all`, `sum<Target>`, `common_inside_t` and its `std::common_type` specialisation, `min` / `max` / `midpoint`, `dot` / `cross` / `lerp`, `operator+` / `-` / `*` / `/` / `%`, expected-lift overloads |
| `beman/inside/range.hpp`       | `inside_range<G, P>` iterator helper |
| `beman/inside/generic.hpp`     | Public grid/policy introspection (`grid_of` / `policy_of` / `interval_of` / `lower_of` / `upper_of` / `notch_of`) and the `insidable` / `numeric` / `inside_assignable` concepts. Storage/raw/dispatch plumbing (`raw_t`, the `rational_raw` / `fp_raw` / `value_raw` / `index_raw` predicates, `as_double`, `to_value` / `from_value`, `raw_cast` / `raw_imax`, `q_format_encode/decode`, `max_index_v`, `raw_lo` / `raw_hi`, `detail::as_rational`, …) lives in `beman::inside::detail` |
| `beman/inside/detail/assignment.hpp`  | `beman::inside::detail::assignment<L, R>` specialisations for integral / fractional / insidable rhs (incl. the Q-format integer shortcut for fractional rhs) |
| `beman/inside/cmath.hpp`       | `beman::inside::math` — the `<cmath>`-shaped public API: the constants, the grid operations (abs, sign, copysign, floor, ceil, round, trunc, fmod, pown, `amp<K>`) and the transcendentals of `cmath_adaptive.hpp`. See [math.md](math.md) |
| `beman/inside/cmath_adaptive.hpp` | The math engine: one core per function (exact inputs, a fixed-point result with an error bound, the exact rational results), the `_into` and deduced forms, the double, dd and table tiers |
| `beman/inside/detail/math_adaptive.hpp` | The engine's foundation: the decision step (`decide`, `fast_index`), the Ziv driver (`evaluate`), the stores, π / ln 2 / ln 10 at any precision, the Horner coefficient tables and the series kernels |
| `beman/inside/detail/math_fp.hpp` | The double tier's kernels (`fp_sin`, `fp_exp`, … — own `std::fma`-Horner polynomials, Cody-Waite reduction, correctly-rounded `std::sqrt`); compiled out under `BEMAN_INSIDE_MATH_NO_FP` |
| `beman/inside/detail/math_dd.hpp` | The dd tier's double-double arithmetic and kernels (table-driven exp, sin/cos and atan, Newton steps for log, sqrt and cbrt); tables from the integer path's series, instantiated on first use; compiled out under `BEMAN_INSIDE_MATH_NO_FP` |
| `beman/inside/detail/addition.hpp`, `multiplication.hpp`, `division.hpp` | `beman::inside::detail::addition<L, R>`, `multiplication<L, R>`, `division<L, R, F>`, `modulo<L, R, F>` — implementation detail, included via `inside.hpp` |
| `beman/inside/detail/overflow.hpp`, `debug.hpp` | `add_overflow` / `sub_overflow` / `mul_overflow` (the GCC/Clang `__builtin_*_overflow`); `errc`, the replaceable `error_handler` + `detail::raise` funnel — implementation detail |
| `beman/inside/detail/rational.hpp`    | `rational` and its checked / unchecked arithmetic |
| `beman/inside/detail/grid_rational.hpp` | The grid-number vocabulary and the `BEMAN_INSIDE_BIG_GRIDS` switch (§2a) |
| `beman/inside/detail/big_rational.hpp` | `big_int` / `big_rational`, interned grid numbers of any size (C++26 only) |
| `beman/inside/detail/wide_int.hpp`    | `wide_int<N, Signed>` and the limb kernels (the only `__int128` site) |
| `beman/inside/detail/int_for_bits.hpp` | `int_for_bits_t`, the `raw_integer` concept |
| `beman/inside/detail/wide_value.hpp`  | Exact values of exact-valued insides, `exact_frac<K>`, the wrapping value-index arithmetic of `+` / `×` |
| `beman/inside/grid.hpp`        | `grid`, `storage_min`, grid operators |
| `beman/inside/numeric_limits.hpp` | `std::numeric_limits<inside>` and `std::hash<inside>` specialisations (opt-in; `std::common_type` is in arithmetic.hpp, always on) |
| `beman/inside/io.hpp`          | **All** string/stream/`std::format` support — `to_string`, `to_string_debug`, `operator<<`, `operator>>`, `from_chars(std::string_view)`, `std::formatter`, `type_name`. Opt-in and the *only* place `<string>`/`<ostream>`/`<format>` enter; gated by `BEMAN_INSIDE_NO_STRING` in the single header (see [freestanding.md](freestanding.md)) |
| `beman/inside/random.hpp`      | `uniform<B>(rng)` — uniform sampling over a grid's slots; opt-in (`<random>`), dropped from the single header when freestanding or FP-free |
| `beman/inside/formats.hpp`     | Curated hardware aliases (`byte`…`qword`, `sbyte`…`sqword`, `unorm8`…, `q4_4` / `q8_8` / `q16_16`) and `counter` / `ring_counter`; opt-in |
| `beman/inside/lift.hpp`        | `detail::lift` — the internal combinator behind the expected-lift operators |

The whole tree is also amalgamated into `single_include/beman/inside/inside.hpp` by a
pure-CMake generator — see [single-header.md](single-header.md) for usage and
the regeneration workflow.
