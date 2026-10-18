# Internals

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

This page explains *why* the library is shaped the way it is, for
contributors: adding an operator, debugging a storage edge case, reasoning
about performance or about the units layer. For the user's view start with
the [tutorial](tutorial.md).

The exact-fraction type is `beman::inside::rational` (defined in `detail`,
public through a using-declaration). It is the raw of continuous grids and,
under C++23, the grid's template-argument substrate; under C++26 grids use
`detail::big_rational` (§2a). "Rational" below always means this type.

Contents: [1 grid invariants](#1-grid-invariants) ·
[2 storage encoding](#2-storage-encoding) ·
[2a grid numbers, wide raws, exact paths](#2a-grid-numbers-wide-raws-and-the-exact-paths) ·
[3 the Q-format path](#3-the-q-format-integer-path) ·
[4 the policy cascade](#4-the-policy-cascade) ·
[5 conversions](#5-conversions) ·
[6 value extraction](#6-as_rational-raw_imax-and-to_value) ·
[7 error vocabulary](#7-error-vocabulary) ·
[8 header layout](#8-header-layout) ·
[9 the units layer](#9-the-units-layer) ·
[10 the math engine](#10-the-math-engine)

---

## 1. Grid invariants

Every `inside<G, P>` carries a `grid G` value. `grid::validate` (`grid.hpp`)
checks these when the type is instantiated:

- **Lower ≤ Upper**.
- **The notch divides the interval** (`Interval.divides_evenly(Notch)`): a
  whole number of notches lies between Lower and Upper. The count is
  `max_index_of<B>` (`detail/storage.hpp`).
- **Notch 0 is legal** and means any fraction in the interval; the storage
  changes accordingly (§2).
- **Notch ≥ 0.** Decoding is Lower + raw·Notch, so a negative notch would
  count downward; `per<D>` is positive by construction, and any other negative
  notch is rejected.
- **Lower need not be a multiple of the notch.** The values are Lower,
  Lower + Notch, …, Upper, so `{{0.5, 10.5}, 1}` holds 0.5, 1.5, …, 10.5. A
  grid whose lattice passes through 0 (Lower/Notch an integer) is *anchored*
  (`grid::anchored()`), and every value is a whole number of notches, its
  value index. On an unanchored grid every value is a whole number of its
  *value unit* gcd(Notch, Lower) (`grid::value_unit()`, the notch when
  anchored).

These invariants let the library compute result grids at compile time with no
runtime check on the grid arithmetic itself: every value of `a + b` for
`a : A, b : B` lies in `grid_of<A> + grid_of<B>` by construction.

**Two-limit grids.** `grid{lo, hi}` derives Notch = gcd(1, Lower, Upper) (the
gcd of the numerators over the lcm of the denominators): the coarsest 1/k that
keeps every integer and both limits on the lattice. Integer limits give 1.
With 64-bit grid numbers a combined denominator past `imax` falls back to
notch 0; under C++26 the exact gcd is kept. A floating-point limit is its exact
binary value, so a `double` that would need a notch finer than 1/1024 (0.1 is
3602879701896397/2⁵⁵) is rejected in favour of `0.1_r` or an explicit notch.

**Open limits.** `grid{lo, hi, bounds::half_open}` (or `bounds::open`, or with
a notch) moves each open limit in by one notch at construction. The grid is
stored closed, so it is the same value, and type, as the closed spelling. A
continuous grid has no next point, and an open grid must keep at least one;
both are compile errors.

**Runtime grids.** `grid::try_make(interval, notch, bounds)` checks the same
invariants at run time and returns `std::expected<grid, errc>`:
`domain_error` for Lower > Upper, a negative notch, or an open limit without
a notch; `rounding_error` when the notch does not divide the interval. It
validates configuration before choosing between precompiled types; a runtime
grid cannot become a template argument.

**Point operands.** A point (`just<c>`, `1_ins`: Lower == Upper, notch 0) in a
product scales the other operand's lattice: `grid × point{c}` has notch N·|c|
rather than 0, so the product keeps integer storage, and
`multiplication::point_scale` reuses the operand's offset as the result's
(counted from the far end for c < 0): no multiply at all.

**Unanchored operands.** A sum lies on (La + Lb) + gcd(Na, Nb)·ℤ, so `+` keeps
the gcd notch whatever the anchors. A product (La + i·Na)(Lb + j·Nb) differs
from La·Lb by multiples of Na·Nb, Na·Lb and Nb·La; the product notch is their
gcd, and an anchored operand's term is already a multiple of Na·Nb
(`detail::product_notch`). `hull` refines the notch gcd by the offset Lb − La
when two lattices do not line up. The integer paths of `+` and `×` count each
operand in its value unit (`value_in_units`) and the result in the matching
unit (`from_value_in_units`); on anchored grids those units are the notches.

**Rounding on an unanchored grid** follows the same value-space rule as
everywhere: toward zero is down at or above 0 and up below it, a tie of
`nearest` goes away from zero (up at 0 itself), and `half_even` picks the
point with an even index counted from the anchor. `rounds_up`
(`detail/rounding.hpp`) is the floor-based form of that decision;
`round_quotient`, `try_round_to_lattice` and `exact_index` use it.

---

## 2. Storage encoding

Storage is a function of the grid alone: `raw_for_t<G>` (`grid.hpp`). No
policy flag changes it, so two insides on one grid share their encoding:

```text
  Lower == Upper (a point)          ───▶  empty raw      (the value is in the type;
                                                          reads as index 0, value = Lower)
  Notch == 0                        ───▶  rational raw   (continuous; wide_rational past
                                                          64-bit limits)
  more than 2⁶⁴ indices             ───▶  wide_int raw   (the index in 64-bit limbs,
                                                          exact paths in detail/wide_value.hpp)
  deduces_value<G>: Notch 1, integer
    limits, and signed within int64,
    or from 0, or above 0 no wider
    than the index                  ───▶  integer raw    (the raw IS the value)
  otherwise                         ───▶  unsigned raw   (the 0-based notch index)
```

`raw_for_t<G>` picks the smallest integer type that holds every index, using
each type's full range ([storage.md](storage.md)).

The storage **concepts** in `detail/storage.hpp` partition every inside by what
its raw holds. The leaves are disjoint; the raw type alone decides all but the
integer pair, which the grid decides (`integer_raw_holds_value` is
`deduces_value<G>`). The groups are disjunctions of leaves, so they subsume
them in `requires` clauses:

| Concept | Raw | Meaning |
|---|---|---|
| `value_storage<B>` | | the raw is the value: one of the next three |
| ├ `rational_storage<B>` | `rational` | an exact 64-bit fraction |
| ├ `fraction_storage<B>` | `wide_rational<K>` | a continuous grid past 64-bit limits |
| └ `integer_value_storage<B>` | a builtin integer | the plain value |
| `index_storage<B>` | | the raw is a 0-based index; value = Lower + raw·Notch |
| ├ `point_storage<B>` | `point_slot` | a point's empty raw, read as index 0 |
| ├ `integer_index_storage<B>` | a builtin integer | |
| └ `wide_index_storage<B>` | `wide_int` | more than 2⁶⁴ indices |
| `integer_storage<B>` | | `index_storage` or `integer_value_storage` |

Decoding dispatches on the **storage, not on the raw type's signedness**: a
whole-number grid above 0 has an *unsigned* value raw. `detail::as_double` is
the storage-aware raw → `double` decoder.

Beside them sit grid-shape traits: `notched<B>` (Notch ≠ 0; a point may still
have notch 0), `point_grid<B>` (Lower == Upper), and `wide_valued<B>` (values
past 64 bits: a wide index, or grid numbers past 64 bits; these take the exact
paths of §2a). Two predicates classify the integer-ness of the grid,
independent of the encoding, and gate different fast paths:

- `integer_limits<B>`: Lower and Upper are integers (the notch may be
  fractional);
- `integer_lattice<B>`: the notch and Lower are integers, which implies
  `integer_limits` but not the other way round.

---

## 2a. Grid numbers, wide raws and the exact paths

**Grid numbers.** `detail/grid_rational.hpp` names one vocabulary for both
language modes: `grid_rational` (the type of a grid's limits and notch, and of
`lower_of`, `upper_of`, `notch_of`), `grid_int` (exact integers for index
counts), `wide_numerator`, `wide_denominator`, `grid_divides_evenly`,
`grid_same_lattice`, `grid_gcd` and `to_rational`. Under C++23 these are the
64-bit `rational` and a 4-limb `wide_int`. Under C++26 with static reflection
(`BEMAN_INSIDE_BIG_GRIDS`) they are `big_rational` and `big_int`
(`detail/big_rational.hpp`): canonical values whose magnitude past one limb is
interned with `std::define_static_array`, so equal values are equal template
arguments. Interning is `consteval`: a big value exists only at compile time,
and runtime code reads big grid numbers through constants.

The integer fast paths work in 64 bits and read `detail::lower64`, `upper64`
and `notch64`, which fail the build for a grid number past 64 bits instead of
truncating it. The math engine and the grid operations read `lower_of`,
`upper_of`, `notch_of` and exact values, so they take any size. Naming such a
view instantiates it even in a short-circuited `&&`, so predicates that every
inside instantiates compare grid numbers, or hide the view behind
`if constexpr`.

**Wide raws.** `int_for_bits_t<Bits, Signed>` (`detail/int_for_bits.hpp`) is
the one width rule: a builtin integer up to 64 bits, else a `wide_int`
(`detail/wide_int.hpp`: Knuth's algorithm D, the only `__int128` site). A grid
with more than 2⁶⁴ indices gets a `wide_int` index; a grid number past 64 bits
makes `wide_grid_numbers<B>`. Either makes `wide_valued<B>`, which routes every
operation to the exact paths of `detail/wide_value.hpp`.

**Value indices.** On a valid grid m = Lower/Notch is an integer
(`slot_base<B>`), so every value is (m + raw)·Notch, exactly. The exact paths
carry values as `wide_rational<K>`, an unreduced fraction of K-limb integers.
`exact_limbs<Bs...>` sizes K from the grids involved, three times the widest
value (the worst case is a product of two values over a third notch). A store
divides by the target notch and rounds by the policy's mode (`exact_index`),
then runs the usual out-of-range cascade (`assign_exact`).

**Integer `+` and `×`** run on value indices for every integer-raw grid:
J_L·(N_L/N) + J_R·(N_R/N) and J_L·J_R. The work type is `imax` when
compile-time bounds prove no value index can overflow, so the compiler keeps
the value ranges; otherwise an unsigned type as wide as the result raw, whose
wrapping arithmetic still yields the exact raw (`index_work_t`). Compound
`+=`/`-=` size one signed work type from the raw and delta ranges
(`raw_work_t`), and clamp/wrap onto unit grids use `unit_fold`; both are
`imax` for every grid within int64.

---

## 3. The Q-format integer path

On a grid with an **integer Lower and a notch 1/nd** (1/256, 1/65536) whose
values times nd fit `imax`, the conversion between value and raw is integer
arithmetic. The gate is `qformat_codec_fits<B>` (`detail/storage.hpp`):

```cpp
abs_den(lower64<B>.Denominator) == 1         // an integer Lower
&& notch64<B>.Numerator == 1                 // notch 1/nd
&& values_fit_imax<B>
&& Lower·nd and Upper·nd within imax         // so every value·nd, and raw + Lower·nd
```

| Helper | Direction | Used by |
|---|---|---|
| `q_format_encode<B>(imax)` | value → raw | `from_value`, `assignment::store` |
| `q_format_decode<B>(B)` | raw → rational | `inside::operator rational()` |

The value·nd clause matters because the result of a Q-format product can reach
past it (Q16.16 × Q16.16 has `max_index_of` ≈ 2⁶⁴), where the integer codec
would wrap. When the gate is false, control falls through to the exact
rational path.

**A fractional source takes the same shortcut.** `store_checked`
(`detail/assignment.hpp`) stores a rational or `double` onto a Q-format grid
without two gcd-reducing operations: with notch 1/K the offset
((num/aden) − Lo)·K reduces to (num − Lo·aden)·(K/g) / (aden/g),
g = gcd(aden, K), one `std::gcd` and three multiplies, then `round_quotient` on
the rest. `round_quotient` does not change under reduction, so the point chosen
is the rational path's. A compile-time fit check keeps every intermediate
within `imax`; larger denominators take the rational path.

---

## 4. The policy cascade

When a value is stored into an inside, an out-of-range value meets a
four-level cascade:

```text
┌──────────────────────────────────────────────────────────────────┐
│ 1. a hook on this store          x.on_clamp(f), x.on_wrap(f), …  │
│                                       │ no hook                  │
│                                       ▼                          │
│ 2. a policy for this store       x.with_clamp() = …              │
│                                  x.policy<F>() = …               │
│                                       │ none                     │
│                                       ▼                          │
│ 3. the type's policy             inside<G, clamp>, inside<G, P>  │
│                                       │ P does not handle it     │
│                                       ▼                          │
│ 4. the hard default              error handler → inside_error    │
└──────────────────────────────────────────────────────────────────┘
```

The hard default goes through the replaceable error handler
([policies.md](policies.md#replacing-the-throw-handler)). The default handler
throws `beman::inside::inside_error` (carrying the `errc`), or traps under
`-fno-exceptions`. There is no `<system_error>` dependency.

The flag bits are in `policy_flag.hpp`: `none`, `ignore_zero`,
`ignore_range`, `snap`, `round_toward_zero` (the `snap` bit alone),
`round_nearest`, `round_floor`, `round_ceil`, `round_half_even`, `clamp`,
`wrap`, `checked`, `unsafe` (= `detail::unsafe_marker | ignore_range | snap | ignore_zero`),
and the internal `detail::cursor_marker`. `is_checked(P)` decides whether step
4 runs: true unless P carries `unsafe_marker`, and an explicit `checked`
overrides that, so every policy is checked unless it says `unsafe`. The bit
values are listed in [diagnostics.md](diagnostics.md#decoding-an-inside).

The cascade lives in `detail/assignment.hpp` and `detail/assign_exact.hpp`:
`dispatch_out_of_range` (shared by every source kind), `try_clamp_or_fail` (an
inside source), `handle_out_of_range` (an integral source), and `apply_clamp`
and `apply_wrap`. The `is_*_action<A>` traits in `policy_flag.hpp` decide
which step takes a given set of hooks. A quantity's hooks wrap the inside's
(§9).

---

## 5. Conversions

The user-facing rules (one implicit `operator imax` on whole-number grids or
under a rounding policy, an implicit lossless `operator rational`, an explicit
`operator double` that needs a rounding mode) are in
[conversions.md](conversions.md). On `rational` itself every `operator T()` is
explicit and truncates toward zero; `r.to<T>()` is the typed-error form, and
`trunc`, `floor`, `ceil` and `round` name the integer reductions.

---

## 6. `as_rational`, `raw_imax` and `to_value`

Three helpers (`detail/introspection.hpp`, `detail/storage.hpp`), one per
intent:

| Helper | Returns | Use when |
|---|---|---|
| `detail::as_rational(x)` | `rational`, lossless | you want exact fraction arithmetic on `x` |
| `raw_imax(b)` | the raw, widened to `imax` | you want the **raw** as a signed integer |
| `to_value(b)` | the value, truncated to `imax` | you want the **value** as an integer |

Under `value_storage<B>` the raw is the value, so `raw_imax(b) == to_value(b)`.
For index storage they differ: `to_value` multiplies by the notch and adds
Lower. That is one multiply-add on a whole-number grid and one truncating
divide on a Q-format grid; only other grids go through `rational`.
`from_value` is the inverse.

---

## 7. Error vocabulary

Two shapes, by role:

| Shape | Role | Why |
|---|---|---|
| the policy cascade (handler / throw, `errc&`, clamp, wrap, hooks) | **storing** a value into an inside: construction, assignment, compound assignment | the caller chose the failure behaviour on the type or the store |
| `std::expected<T, errc>` | every fallible **result**: arithmetic (`/`, `%`, unbounded continuous `+`/`×`), queries (`to<T>()`, `try_make`, `parse`) and math (`tan`, `pow`, `sqrt` of a grid that crosses 0) | the caller tests the result and can read the cause; it chains through the lift operators |

`std::expected<inside, errc>` is larger than an inside (a flag and an `errc`),
so it appears only as a **parameter or return value**, never as stored state
(a member, a container element, iterator state), and only where the operation
can fail: a result the grids prove total stays a plain inside.

| Operation | Shape | Causes |
|---|---|---|
| `a + b`, `a − b`, `a × b` on integer raws | inside | total: the result grid holds every value |
| the same with a bounded continuous result whose fraction holds its bound | inside | total: the bound sizes the raw (`exact_fits`, `detail/rep.hpp`) |
| the same with an unbounded continuous result, checked | expected | `overflow`: the denominator may outgrow the raw |
| `a / b`, `a % b`, divisor grid without 0 | inside | total |
| `a / b`, `a % b`, divisor grid with 0 | expected | `division_by_zero`; `overflow` for a fraction raw |
| `math::sin`, `cos`, `exp`, `log`, … | inside | total over the asserted domain |
| `math::tan` | expected | `division_by_zero` (a pole), `overflow` (past `Out`) |
| `math::pow` | expected | `domain_error` (a negative base), `division_by_zero` (0 to a negative power), `overflow` |
| `math::sqrt` of a grid that crosses 0 | expected | `domain_error` |
| `to<T>()` | expected | `overflow`, `domain_error` (negative into unsigned) |
| `as<T>()` | `T` | `to<T>().value()`: throws |
| `try_make`, `parse` | expected | `overflow`, `rounding_error`, `invalid_format` (text) |
| construction, assignment | the cascade | by the policy: `overflow`, `rounding_error`, `not_finite` |
| `sum<Target>` | `Target` | Target's policy, applied once to the total |
| a quantity's `/`, `div_into`, `try_value_cast`, `parse<Q>`, `Q::try_make` | as the inside's | the inside's causes, with the quantity as the value |

Chaining (`lift.hpp`, `arithmetic.hpp`): the expected-lift operators keep an
`expected` chain intact, so `a / b * gain + offset` stays an
`expected<inside, errc>` end to end. The first (left) error wins and keeps its
cause; an operation that fails inside the chain reports its own.
`detail::lift_operand_v<T>` decides which types the lift operators take:
insides, and quantities (units.hpp specializes it).

---

## 8. Header layout

The umbrella `inside.hpp` includes the core (`core.hpp`, then the casts,
arithmetic, range and cursor layers, which need the complete type). The
**opt-in** headers are included only on demand, which keeps the core free of
`<string>`, `<ostream>`, `<format>` and `<cmath>`.

**Public headers**

| Header | Holds |
|---|---|
| `inside.hpp` | the umbrella: `core.hpp`, `detail/compare.hpp`, `casts.hpp`, `arithmetic.hpp`, `range.hpp`, `cursor.hpp` |
| `core.hpp` | `inside<G, P>`: construction, compound assignment, `++`/`--`, `to`/`as`, `parse(first, last)`, hooks, `just`, `_ins` |
| `grid.hpp` | `grid`, `bounds`, `grid::try_make`, `raw_for`, the grid operators |
| `interval.hpp` | `interval`, its operators, `includes`/`excludes`/`overlaps` |
| `numbers.hpp` | `umax`/`imax`, `smallest_uint_for_t`/`smallest_int_for_t`, `plain_number`/`fractional`, `safe_abs` |
| `policy_flag.hpp` | the flag bits, `has_flag`/`is_checked`, the rounding precedence, the `on_*` factories and `is_*_action` traits |
| `policy.hpp` | `policy<F, E>`, `make_policy`, the named policies, `policy_ref` |
| `casts.hpp` | `clamp_cast` (optionally rounding), `wrap_cast`, `checked_cast`, `unchecked_cast` |
| `arithmetic.hpp` | `add`, `sub`, `mul`, `div`, `mod`, `divmod`, `mul_into`, `div_into`, `add_all`, `mul_all`, `sum`, `min`/`max`/`midpoint`, `dot`/`cross`/`lerp`, `common_inside_t`, the operators and the expected-lift overloads |
| `range.hpp` | `inside_range<G, P>`, `values<T>` |
| `cursor.hpp` | `cursor<T, Step>`, `end(t)` |
| `predicates.hpp` | `conversion_overflows`, `conversion_rounds`, `conversion_loses` |
| `generic.hpp` | the `inside_assignable` concept, `inside_assignable_why`, `explain_assignable` |
| `lift.hpp` | `detail::lift`, the combinator behind the lift operators |
| `numeric_limits.hpp` | `std::numeric_limits` and `std::hash` for insides (opt-in) |
| `formats.hpp` | the native aliases `uint8` … `q16_16`, `clamp_counter`, `wrap_counter` (opt-in) |
| `io.hpp` | **all** text: `to_string`, `to_string_debug`, `<<`, `>>`, `parse(string_view)`, `std::formatter`, `type_name_of`. Opt-in; the only header that brings `<string>`, `<ostream>`, `<format>` |
| `cmath.hpp` | `beman::inside::math`: the math constants, the grid operations, `unit_amplitude<K>`, the transcendentals ([math.md](math.md)) |
| `random.hpp` | `uniform<B>(rng)` (opt-in, `<random>`) |
| `units.hpp` | quantities, points, units, dimensions, kinds, origins, the catalog, units of your own, and inside's algorithms on quantities (§9) |
| `units_math.hpp` | math on quantities: roots, trig (with π-scaled angles), `atan2`, `hypot`, the number functions, `value_cast` across π |
| `constants.hpp` | `math::` constants and `units::constants::` physical constants |

**Implementation headers** (`detail/`, included through the public ones)

| Header | Holds |
|---|---|
| `config.hpp` | every compiler- and platform-dependent macro; included first everywhere (`BEMAN_INSIDE_DELETE`, warning control, feature detection) |
| `compare.hpp` | `<=>` and `==` across grids and against integers, floating points of any width and `rational` |
| `introspection.hpp` | `grid_of`, `flags_of`, `interval_of`, `lower_of`, `upper_of`, `notch_of`, the `insidable` and `numeric` concepts, `raw_t`, `as_rational` |
| `storage.hpp` | the storage concepts, `as_double`, `to_value`/`from_value`, `raw_imax`, the Q-format codec, `max_index_of`, `round_quotient` |
| `rep.hpp` | `lattice_op_checked`: whether `+` or `×` into a result runs its overflow check |
| `rounding.hpp` | `classify_remainder`, `rounds_up`, `rounds_away`: the one rounding decision every integer path shares |
| `assignment.hpp`, `assign_exact.hpp` | stores for integral, fractional and inside sources; the exact and wide store, the out-of-range cascade, the `on_wrap` carry grid |
| `addition.hpp`, `multiplication.hpp`, `division.hpp` | `addition<L, R>`, `multiplication<L, R>`, `division<L, R, F>`, `modulo<L, R, F>`, `divmod_result` |
| `overflow.hpp`, `debug.hpp` | `add_overflow` and kin; `errc`, `errc_message`, the error handler and `raise` |
| `diag_text.hpp` | `diag_text` and `BEMAN_INSIDE_DIAG`: `static_assert` messages with values (C++26) |
| `rational.hpp` | `rational` and its checked and unchecked arithmetic |
| `number_text.hpp` | the literal and text grammar, and the `_r` literal |
| `float_bits.hpp` | floating points from their IEEE-754 bits: `frexp`, `ldexp`, `is_finite`, `abs_fraction` |
| `grid_rational.hpp` | the grid-number vocabulary and the `BEMAN_INSIDE_BIG_GRIDS` switch (§2a) |
| `big_rational.hpp` | `big_int`, `big_rational`: interned grid numbers of any size (C++26) |
| `wide_int.hpp`, `int_for_bits.hpp`, `wide_value.hpp` | `wide_int<N, Signed>` and its kernels; the width rule; exact values and wide value-index arithmetic |
| `math_constants.hpp` | the 18 math constants as rationals below 2³¹ |
| `math_adaptive.hpp` | the engine's foundation: the decision step, the Ziv driver, the stores, π / ln 2 / ln 10 at any precision, the series kernels |
| `math_cores.hpp` | the integer path: one core per function |
| `math_entry.hpp` | the entry points: `_into` and deduced forms, the table, `double` and double-double tiers |
| `math_fp.hpp`, `math_dd.hpp` | the `double` and double-double kernels and their proved bounds; compiled out under `BEMAN_INSIDE_MATH_NO_FP` |

`single_include/beman/inside/inside.hpp` amalgamates the whole tree
([single-header.md](single-header.md)).

---

## 9. The units layer

`units.hpp` builds quantities on top of insides, with no type-list
metaprogramming.

**Values, not type lists.** A dimension (seven SI exponents plus up to four
`base_dimension`s), a magnitude (an exact rational × π^k), a unit (up to six
named-unit atoms, each with a decimal prefix and an exponent, plus a
magnitude), a quantity spec and a reference are all structural values,
combined by `consteval` functions. The names users write are empty tags whose
type carries the value (`unit_t<V>`, `spec_t<P>`, `origin_t<P>`), so
`si::metre * si::second / si::second` is computed, not sorted, and m·s equals
s·m. Identity is by content: a unit by its symbols, dimension, magnitude, kind
and origin; a spec by its name, dimension, recipe, kind boundary and parent
chain. Optional pointers inside these values are `ref_or_none<T>` (a pointer
and a flag), because some compilers do not fold `&obj == nullptr` in constant
expressions.

**A conversion is a rescale.** `conversion_factor<From, To>` is the ratio of
the two magnitudes. `detail::rescale<M>(v)` multiplies an inside by the
rational M as `v * just<M>`, which scales the grid and keeps the raw (§1, point
operands), then re-wraps it in v's policy. So km → mm on `inside<{0, 10}>` is
the same `uint8_t`. A factor with π has no exact grid; `in()` refuses it, and
`value_cast` across π goes through `ax::pi_scale_core`, which multiplies by π
at the bits the output needs.

**The common unit.** `+`, `-`, `%` and the comparisons convert both operands
to `sum_reference<A, B>()`, whose magnitude is the gcd of the two, then run
inside's operator. The common spec is the nearest common ancestor in the ISQ
tree. `*` and `/` combine units and specs with `product_reference<A, B, ±1>()`.

**Kinds.** `implicit_v<From, To>` decides implicit conversion: up the tree, from
a matching recipe, never across an `is_kind` boundary.

**Operations as constraints.** Every operator is constrained on the number
operation it runs (`number_op`, `summable`), so `requires` probes are honest.
Each reason a units operation can be wrong is a deleted overload with a
message (`BEMAN_INSIDE_DELETE`, `detail/config.hpp`).

**inside's algorithms on quantities** (`mul_into`, `div_into`, `sum`, `divmod`,
`parse`, the predicates, the casts, `dot`/`cross`/`lerp`) all follow one
pattern: convert the operands exactly into the target's unit with `rescale`,
call the inside function on the numbers, and wrap the result
(`detail::wrap_out`, `make_quantity`). `mul_into` and `div_into` fold the
factor between the product's unit and `Out`'s into the dividend's grid, so
the one rounding stays one. The hooks (`q.on_wrap(f)` …) return a
`quantity_action_ref` around the inside's `policy_ref`; its stores take
quantities, and `on_quantity` wraps `f` so that it sees the quantity.

**Trig of π-scaled angles.** `units_math.hpp`'s `ax::pi_trig_core` takes an
angle x in a unit of magnitude r·π. t = x·r half turns is exact, so
k = round(2t) quarter turns is exact, and only r' = π·(t − k/2), at most π/4,
is approximated, at the bits the output needs. The core's `exact()` returns
Niven's rational values (6t an integer: 0, ±1/2, ±1) exactly, so they round
correctly under directed modes too. The quadrant k mod 4 picks sin or cos of
r' and the sign.

**Text.** `to_string` and the `std::formatter` print the number and then the
unit symbol. Clang 20 miscompiles a runtime reference to a class NTTP object
that holds pointers, so the printers copy the reference into a
`static constexpr` local first.

---

## 10. The math engine

[math.md](math.md#how-it-works) describes the engine from the user's side:
the four paths, the precision that follows the output grid, Ziv's strategy.
This section has the details of the floating-point tiers and the guards that
keep them equal to the integer path.

### The double and double-double tiers

The double tier's kernels are Taylor polynomials sized to the output: each
gets the fewest terms whose truncation stays 10 bits below the output's notch.
`sin` onto a 2^-20 grid evaluates 6 terms where the full kernel has 9; `exp`
onto 10^-6 evaluates 10 of 15. Every kernel carries a bound proved at compile
time from its own coefficients and argument range: the first omitted term, the
rounding of each Horner step, the coefficients' and constants' roundings, and
the range reduction's. The full kernels prove about 2^-51 of the result
(`log` 2^-50.4). The bounds are tight: measured against `__float128`, the
kernels' worst errors reach up to 0.99 of them. An input that is not a double
exactly adds its rounding times the function's slope. A kernel takes outputs
up to the bits where its full-size bound still decides about 3 results in 4
at the output's largest values: 48 for `sin`, `cos` and `exp`, 47 for `log`,
`atan`, `pow`, `cbrt`, `sinh`, `asinh` and `acosh`. Past 46 bits `sinh`,
`asinh` and `acosh` take sharper forms that cost 15–40% more: `sinh` the odd
series below ln 2 (no cancellation) and (e − 1/e)/2 above it; `asinh` and
`acosh` ln(1 + u) with u as an exact sum of two doubles, through the log's
two-double result. `pow` and `cbrt` multiply the log's error by
|y| = |e·ln b|, so past 42 bits they take the log as a sum of two doubles
(within 2^-56 relative) and their limit holds up to |y| = 16. Whenever
the bound does not place the result in a single slot, the dd tier or the
integer path decides. Tests check every kernel against its bound and that the
tiers agree slot for slot.

The dd tier works the same way with values carried as a sum of two doubles. Its
kernels use table-driven reductions (2^(i/64)·2^(j/4096) for exp, sin(jπ/128),
atan(j/32), ln(j/128) for log) and Newton steps for sqrt and cbrt. Every table and coefficient
comes at compile time from the integer path's series, computed only in
translation units that use the tier. Its bound is 2^-88 of the result plus
2^-92·max(1, |x|), with the same condition-number terms. A test checks that the
kernels' errors against the integer path at 150 bits stay at least 2^8 below
this bound; the measured worst is about 2^-96 relative.

Outputs in the dd tier have notches no finer than 2^-62, so about 2^-70
decides nearly all of them. For every function but `sqrt`, `cbrt` and `hypot`
the tier first runs a lean kernel: the same tables and reduction, but only the
table values, the reduced argument and the leading products (all error-free)
in double-double, the polynomial tails in double. Their bounds are proved like
the double tier's: 2^-70.3 absolute for `sin` and `cos` (plus 2^-104·max(1,
|x|) from the reduction), 2^-77.6 relative for `exp`, 2^-75.3 absolute for
`log` (plus 2^-101 per unit of the binary exponent), 2^-69 absolute for
`atan`; measured errors reach 0.15, 0.29 and 0.26 of the first three. The
others are built from these (`tan` = `sin`/`cos`, the hyperbolics from `exp`,
their inverses from `log`, `asin`, `acos` and `atan2` from `atan`, `pow` from
`log` and `exp`), each bound adding the proved error of the double-double
operations in between. Onto a 2^-52 amplitude grid they leave about one
result in 60 000 undecided, onto 2^-60 about one in 400; those go to the full
kernels.

### Where correctness comes first

The fast tiers exist only to return the integer path's answer sooner. Wherever
a faster choice could, under some build, input or storage, return a different
grid point, the engine takes the slower one. Each choice below gives up speed
measured on x86-64 with `-mfma`.

| Choice | What it guards against | What it costs |
|---|---|---|
| The double tier's bounds proved, not measured: every rounding of every step counted at its worst, times 1.5 | a kernel error the measurements missed (an input, a compiler, a platform) turning into a wrong slot | the proofs are worst cases, about 2–4 times the error measured, so outputs near a kernel's limit fall back to the dd tier for up to 13% of inputs |
| The full dd kernels' bound far wider than their measured error: 2^-88 + 2^-92·max(1, \|x\|) (measured at worst 2^-96), times 1.5 | the same, for kernels whose bounds are not proved (the lean kernels' are) | outputs past the double tier's limits take the dd tier, 2–9 times slower than the double tier |
| `-ffast-math`, `-fassociative-math` and `-ffinite-math-only` builds rejected with an `#error` | reassociation adding roundings the proofs did not count and breaking the error-free sums; NaN and infinity tests folded away | such builds do not compile |
| Strict decision tests: a value within the bound of a slot boundary, exact ties and exact grid points under directed rounding go to the integer path | a rounding the double arithmetic cannot settle | the integer path's time for those inputs; rare for irrational results, every time for exact ones such as `sqrt` of a perfect square under `round_floor` |
| `nearbyint` to round the value index, not adding and subtracting 1.5·2^52 | reassociation (Clang's `-fassociative-math`, which no macro announces) folding the add-subtract away; the tests then pass a non-integer and return a wrong slot | 0.1–0.4 ns per call |
| Range checked in double before the index becomes an integer; no finite checks, since NaN and infinities fail every comparison | a huge or non-finite kernel value converted to an integer (undefined behaviour) | none measured |
| Inputs that are not doubles exactly (decimal notches, rational storage) add their conversion error times the function's slope to the bound | a decimal input's rounding moving a result across a boundary | slightly more fallbacks for those inputs |
| Error-free sums fenced against FMA contraction (`__builtin_assoc_barrier`), the error-free product's rounded part computed as `fma(a, b, +0)` | GCC's default `-ffp-contract=fast` fusing a rounded product into a later sum, which made the dd tier 1–2 notches wrong at `-O2` | about 2% more instructions in the dd tier, no measurable time |
| The dd tier only for value indices up to 2^62 | index arithmetic overflowing 64 bits | finer or wider outputs take the integer path |
| The full dd kernels carry about 100 bits even when the output needs 50, behind a lean kernel with a proved bound | an undersized kernel for some output, or a lean kernel whose bound is only measured | a result the lean kernel leaves undecided pays both |
| Every constant and table computed at compile time from the integer path's own series, never written out as literals | a constant that drifts from the series it should equal | compile time only: about 0.1–0.3 s in a translation unit that uses the dd tier, nothing in one that does not |

What the tiers never trade away: they return a result only when it provably
equals the integer path's, and the integer path is the only path at compile
time and without an FPU. A build cannot change a value; it can only change how
fast the value comes.

Speed that can still be won without giving any of this up: tighter proofs
for `cbrt` and `pow`, whose double-tier bounds grow with the input.
