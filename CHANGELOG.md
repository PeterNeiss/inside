# Changelog

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

## Unreleased

The API freeze review before 1.0: one spelling per flag, implementation names
out of the public namespaces, and result types that hold exactly the values
an operation can produce (result types are frozen from 1.0 on).

### Upgrading from 0.7.0

- `snap` is gone: write `round_toward_zero`, its other name. The flag and its
  bit are unchanged, so `inside<G, snap>` becomes `inside<G, round_toward_zero>`
  and `x.policy<snap>()` becomes `x.policy<round_toward_zero>()`; diagnostics
  name `policy<round_toward_zero>()`.
- `none` is gone: write `checked`, the default policy (both were 0).
- Moved into `detail`, as implementation: `math::adaptive` (the math functions
  stay in `math`), `math::fmod_nonzero` (call `math::fmod_into`), the free hook
  factories `on_clamp`, `on_wrap`, `on_error` with `on_clamp_t`, `on_wrap_t`,
  `on_error_t` and `no_action` (the members `x.on_clamp(f)`, `x.on_wrap(f)`,
  `x.on_error(f)` stay), `is_inside_v` (use `insidable`), `numeric`,
  `builtin_arithmetic`, `grid_number_like`, `has_any_flag`, `is_checked`,
  `range_handled` and `inside_assignable_why` (use `explain_assignable`).
- In `units`, moved into `units::detail`: `is_quantity_v` (use
  `quantity_like`), `is_quantity_point_v`, `is_reference_tag_v`,
  `is_unit_tag_v`, `spec_operand` and `expr_of`.
- `x.numerator()` and `x.denominator()` return insides on the integers
  (`numerator_grid`, `denominator_grid`) instead of `imax` or a wide integer;
  on an integer grid the numerator is the value itself. `.as<imax>()` reads
  the old value; comparisons with integers are unchanged.
- Grid-number arithmetic is `consteval`: the operators of `grid_number`, the
  grid and interval operators, and `grid::step_count()`, `slot_bits()`,
  `value_unit()`. `N - 1` with a constant `N` is evaluated while compiling
  wherever it appears; with a run-time operand it no longer compiles (it
  could only fail at run time past 64 bits). Count at run time with insides:
  `step_count_of<T>`, `x.index()`, `T::from_index(i)`.
- `grid::max_index()` and `grid::max_index_representable()` moved into
  `detail` (they read 0 past 2⁶⁴): use `step_count_of<T>`.

### Changed

Result types are narrower where the old ones held values the operation can
never produce; the values are unchanged, and the new types convert to the old
ones implicitly.

- `a % b` and `divmod`'s remainder: bounded by the dividend too, with the sign
  the rounding mode allows. `inside<{0, 3}> % inside<{1, 100}>` is
  `inside<{0, 3}>` (was `{0, 99}`); under `round_floor` with a positive
  divisor the remainder is never negative.
- `min(a, b)` and `max(a, b)` of two grids: `[min of the lowers, min of the
  uppers]` and `[max of the lowers, max of the uppers]` instead of the hull.
  `std::common_type` is still the hull.
- `math::abs` of a grid without 0 starts at the least magnitude
  (`abs` of `[5, 10]` is `[5, 10]`, was `[0, 10]`); an even `math::pown`
  is never below 0 (`pown<2>` of `[−10, 10]` is `[0, 100]`, was `[−100, 100]`).
- `lerp(a, b, t)` with `t` in [0, 1]: the hull of `a`'s and `b`'s ranges
  (was the interval of `a + (b − a)·t`, wider on both sides). On quantities
  it runs in the common unit, like `min` and `max`.

### Added

- `std::mdspan` support (opt-in `<beman/inside/inside/mdspan.hpp>` and
  `<beman/inside/units/mdspan.hpp>`): `inside_mdspan`, a `std::mdspan` that
  takes inside indices (no check when the index type lies within a static
  extent, else the index's policy: `clamp` pads, `wrap` is periodic);
  `extents_of<I...>`, `extent_index_t` and `indices(m)`; `value_accessor`
  and `raw_accessor`, views of plain or raw buffers; exact `sum`, `dot`,
  `matrix_vector_product`, `matrix_product` and `transform_into`, each output
  stored once by its policy; quantities and `units::view_in`. Examples
  `image.cpp` and `matrix.cpp`. The single header guards them with
  `BEMAN_INSIDE_NO_MDSPAN`.
- Counting and number theory in `math` (`<beman/inside/math/integer.hpp>`):
  `factorial`, `binomial`, `falling_factorial`, `rising_factorial`,
  `multinomial`, `gcd` and `lcm`, each with an `_into<Out>` form. Results are
  exact, and each result type is the range the inputs can produce
  (`factorial` of `inside<{0, 20}>` is `inside<{1, 20!}>`), past 64 bits too;
  a result past 16384 bits does not compile. `examples/combinatorics.cpp`
  uses them instead of its hand-written factorial and binomial.
- Run-time counting on any grid, past 2⁶⁴ points too: `x.index()` gives the
  notch index as an `index_t<T>` = `inside<{0, step_count}>` (T's raw type,
  a copy); `T::from_index(i)`, `from_index(i, policy)` and
  `try_from_index(i)` go back; `step_count_of<T>` is the step count as a
  point inside.
- The literals `_ins`, `_r` and `_g` live in the inline namespace
  `beman::inside::literals`, so `using namespace beman::inside::literals;`
  brings only them; `using namespace beman::inside;` still brings them too.

## 0.7.0 — 2026-11-03

Safer defaults (every store checked unless `unsafe`, a defined default value,
no implicit narrowing), the headers in three components (`inside`, `math`,
`units`), constraints that answer honestly, quantities on a par with insides,
faster division and math kernels, and the docs in three levels.

### Upgrading from 0.6.0

- The headers live in three components: `inside.hpp` and `inside/`,
  `math.hpp` and `math/`, `units.hpp` and `units/`. The umbrellas
  `<beman/inside/inside.hpp>` and `<beman/inside/units.hpp>` are unchanged;
  other includes move:

  | Before | Now |
  |---|---|
  | `beman/inside/cmath.hpp` | `beman/inside/math.hpp` |
  | `beman/inside/io.hpp`, `random.hpp`, `formats.hpp` | `beman/inside/inside/io.hpp`, `inside/random.hpp`, `inside/formats.hpp` |
  | `beman/inside/grid.hpp`, `interval.hpp`, `casts.hpp`, … | `beman/inside/inside/grid.hpp`, … |
  | `beman/inside/units_io.hpp`, `units_math.hpp` | `beman/inside/units/io.hpp`, `units/math.hpp` |
  | `beman/inside/constants.hpp` | `beman/inside/units/constants.hpp` (physical) and `beman/inside/math/constants.hpp` (`math::pi` & co., without units) |
  | `beman/inside/detail/…` | `beman/inside/inside/detail/…`, `beman/inside/math/detail/…` (internal) |
  | `beman/inside/core.hpp`, `numbers.hpp`, `policy.hpp` | internal: `inside/detail/core.hpp`, `numbers.hpp`, `call_policy.hpp` |
  | `beman/inside/detail/compare.hpp` | `beman/inside/inside/compare.hpp` (public) |

  `units.hpp` is now `units/core.hpp` (the machinery) plus `units/systems.hpp`
  (the ISQ and SI catalog); units of your own need only `units/core.hpp`.

- A default-constructed inside is no longer uninitialized: `T x;` and `T{}`
  are 0, or Lower on a grid without 0, and no inside is trivially
  default-constructible.
- `rational{}` and `rational r;` are 0 (0/1), and `==` / `<=>` compare
  values, so a `rational` written through `Num`/`Den` compares by its value.
- An inside no longer converts to `imax` implicitly. Read an integer with
  `as<T>()` (no check is left where `T` holds the grid) or `to<T>()`:
  `int i = b;` becomes `int i = b.as<int>();`, `vec[b]` becomes
  `vec[b.as<std::size_t>()]`, `static_cast<imax>(b)` becomes `b.as<imax>()`.
  A hook parameter typed `imax` takes `auto` and reads it with `as<imax>(c)`.
- `ignore_range` and `ignore_zero` are gone: write `unsafe`, the one opt-out
  of the runtime checks, or give a divisor a grid without 0.
- `checked` has no bits: `inside<G>`, `inside<G, none>` and
  `inside<G, checked>` are one type, so mangled names change and a policy
  prints as `0` instead of `17179869184`.
- An inside rejects a policy with bits no flag names or with two rounding
  modes (`round_floor | round_ceil`); build policies from the flags.
- `add_overflow`, `sub_overflow` and `mul_overflow` are internal
  (`beman::inside::detail`); use `std::ckd_add` & co. or the builtins.
- The class `policy<F, E>` is `detail::call_policy<F, E>`: write
  `make_policy<F>()` / `make_policy<F>(ec)`, or pass a flag.
- Fewer ways to store a value. The removed spellings and their replacements:

  | Before | Now |
  |---|---|
  | `x.with_clamp()`, `x.with_wrap()` | `x.policy<clamp>()`, `x.policy<wrap>()` |
  | `x.with_snap<M>()`, `x.with_snap()` | `x.policy<M>()`, `x.policy<snap>()` |
  | `clamp_cast<T>(v)`, `clamp_cast<T, R>(v)` | `T{v, clamp}`, `T{v, clamp \| R}` |
  | `wrap_cast<T>(v)` | `T{v, wrap}` |
  | `unchecked_cast<T>(v)` | `T{v, unsafe}` |
  | `clamp_cast<Q>(x)`, `wrap_cast<Q>(x)` (quantities) | `Q{x, clamp}`, `Q{x, wrap}` |
  | `T x = a / b;`, `x = a / b;`, `x += a / b;` (an `expected`) | `T::try_make(a / b)`, or check it and store `*e` |
  | `div(a, b, rounded_floor)`, `B{v, clamped}` & co. | `div(a, b, round_floor)`, `B{v, clamp}` |

  The flags of the constructor `T{v, F}` replace `T`'s own of the same kind
  (a rounding mode, clamp or wrap). `checked_cast<T>(v)` stays.
- The CMake option `BEMAN_INSIDE_STRICT_SFINAE` is gone: its behaviour, honest
  traits, is now the only one. An impossible store's message names the
  failing clause; `explain_assignable<To, From>` prints the grids' numbers.

### Changed

- Default construction stores a value of the grid: 0 when the grid holds 0,
  else Lower. Before, `T x;` left the raw uninitialized and `T{}` zero-filled
  it, which on `{5, 100}` read as 0, on a Q1.14 sample as −1, and on a
  continuous grid as the fraction 0/0. A failed error-code construction,
  `T x(v, make_policy(ec))`, now keeps that default value.
- An inside no longer stores a `std::expected`: `T x = a / b;`, `x = a / b;`,
  `x += a / b;` and `x.policy(ec) = a / b;` are deleted overloads (they threw
  `std::bad_expected_access` before). Check the `expected` and store `*e`, or
  call `T::try_make(a / b)`, which passes the error on. `rational`'s sinks
  (the converting constructor, `+= -= *= /=`, `<=>` against an `expected`)
  raise through the error handler instead of `std::bad_expected_access`.
- The error handler is atomic, with default visibility: one thread may install
  a handler while others raise, and shared libraries share it.
- An inside no longer compares with a floating-point value (`x == 0.1`,
  `x < 1.5`): 0.1 is not 1/10 in binary, so an inside holding 1/10 compared
  unequal to 0.1. Compare with `0.1_r`, `just<1, 10>` or an inside.
- An inside (or an `expected` of one) combined with a `rational` by `+ - * / %`
  is a deleted overload: through `operator rational()` it silently became an
  `expected<rational, errc>` without a grid. Write the constant as
  `just<1, 2>` or `0.5_ins`; `x += 0.5_r` still stores into `x`'s grid.
- A quantity's `on_clamp` hook gets the overshoot as a quantity in its unit
  and kind (it was the bare number): `f(q, over)` with `over` printing as
  "-163 HP". `on_wrap`'s carry stays a count of periods.
- A result of an operator keeps the policy its operands share (a point with
  the default policy, such as `2_ins`, counts as neutral; different policies
  give the default). Before, `+ - *` returned the default policy, so with
  `inside<{0, 10}, round_floor> a{7}`, `a / 2_ins` was 3 but
  `(a + 0_ins) / 2_ins` was 7/2; both are 3 now.
- The `on_clamp` overshoot is an inside for every source kind, as the
  `on_wrap` carry already was, and exact: the source minus the limit. Before,
  it was an `imax` from an integer (saturated), a `double` from a `double`
  (computed in floating point), a `rational` from a `rational` (0 when the
  difference left 64 bits), and an inside only from an inside. Its grid holds
  every value of the source kind minus either limit; from a `double` that is
  an inside of about 2100 bits. ±∞ reports the end of that grid. A hook typed
  `imax` reads it with `as<imax>(over)`.
- A store, operator or free function that cannot compile is a deleted
  overload with the reason, everywhere: an impossible store (a disjoint
  interval, an off-notch source), an inside with a raw scalar (`x + 1`), and
  a hook given to `add` … `divmod`. `std::is_constructible`,
  `std::convertible_to` and `requires` probes now answer `false` for them; a
  `static_assert` body made them answer `true` and fail on use.
- `unsafe` is the one opt-out of the runtime checks, and it wins over
  `checked` wherever it appears (`unsafe` is now `unsafe_marker | snap`).
  `unsafe | checked` was documented to turn the checks back on, but the range
  check stayed off.

### Added

- `beman::inside::grid_number`, the type of a grid's limits, notch and bound,
  of `_g` and of `lower_of<T>`: it was reachable only as
  `detail::grid_rational`.

- Quantities catch up with insides: `q.policy<F>()` and `q.policy(ec)` (their
  handles, and the hooks', take `*=` and `/=` too), `checked_cast<Q>(x)`,
  `min`, `max` and `midpoint` in the common unit, `q.as<T>()` / `q.to<T>()`
  and the free `as<T>(q)` / `to<T>(q)` for a dimensionless quantity, and
  `uniform<Q>(rng)` in the new opt-in `<beman/inside/units/random.hpp>`. A
  test lists every inside function by reflection, so a new one needs a
  quantity form or a reason.
- `add`, `sub`, `mul`, `div` and `mod` take quantities, with a per-call
  policy, a flag or an `errc&` as on insides: `div(d, t, round_floor)`. Sums
  run in the common unit; products and quotients multiply the units, and a
  number scales. A hook is a deleted overload with the reason.
- `cursor<Q, Step>`, `values<Q>` and `end(t)` take quantities: they step over
  the number's grid and yield quantities, Step a number in Q's unit
  (`for (cursor<db_t, 3> n; n != end(n); ++n)`). One trait,
  `detail::stepping`, serves both kinds, so no twin in `units::` collides
  under two using-directives.
- `std::hash<Q>` and `std::numeric_limits<Q>` for quantities: the number's,
  with `min()`, `max()`, `lowest()`, `epsilon()` and `round_error()` as
  quantities in Q's unit.

- `on_clamp` and `on_wrap` work on a continuous grid past 64 bits and for a
  source past 64 bits (such as a wide inside) into a continuous inside; both
  were a `static_assert`. The `on_wrap` carry grid is computed on grid
  numbers, so a notched grid past 64 bits takes `on_wrap` too (it did not
  compile).
- `+`, `-` and `*` of an operand past 64 bits whose result is a continuous
  grid within 64 bits (`{2^64, 2^64 + 4}` plus a continuous
  `{-2^64, -2^64 + 4}`): it was a deleted overload, now the wide kernels
  compute it.

- A flag is also a call's policy: each flag is a constant of its own type
  that converts to `policy_flag`, so `round_floor` is a template argument
  (`inside<G, round_floor>`), combines with `|`, and is the last argument of
  `add` … `divmod` or a constructor's second argument.

### Performance

- A store that rounds a rational onto a notch divides once (it divided
  twice): a Q8.8 store from a rational takes 2.1 ns instead of 4.1.
- `div_into` onto a Q-format grid needs no gcd and one division:
  `div_into<1/16>` of two integers 90 → 30 ins/op (8.3 → 2.1 ns), its
  `std::expected` form 197 → 34.
- `x += r` and `x -= r` with a `rational` on a binary Q format divide no
  more (two divisions became shifts): 2.0 → 1.3 ns on Q8.8.
- Products and quotients of dyadic rationals cancel by shifts, without a
  gcd: rational `*` 195 → 78 ins/op, a continuous-grid product 9.2 → 2.2 ns,
  a quotient 19.4 → 9.7 ns. A rational comparison is one 128-bit
  cross-multiplication, with no division against an integer.
- `sum` adds 32-bit raws in 32-bit lanes and `inside_range` counts in 32
  bits where they fit: both run at native speed (`sum<checked>` 2.47 → 1.22
  instructions per element).
- `asin` and `acos` have their own double kernel, a minimax polynomial
  with no division (one sqrt past ½) instead of atan with up to three
  divisions: `asin` 8.9 → 4.0 ns, `acos` 8.7 → 3.3 ns on 2^-14 grids.
  `atan` and `atan2` reduce with at most one division (`atan2` 7.7 → 6.7 ns).
- `exp`, `log` and `atan` in the double tier (and what builds on them:
  `exp2`, `pow`, `cbrt`, the hyperbolics, `log2`, `log10`, `atan2`) evaluate
  minimax polynomials instead of Taylor series: 2–5 fewer terms (exp 11 → 9
  at 39 bits, 15 → 13 at full size), 3–13% faster.
- The double tier's minimax fits are generated by
  `tests/beman/inside/minimax/gen.py` with Sollya, which certifies each fit's
  error bound (`supnorm`); the generated header is plain C++, and nothing
  else needs Sollya.
- The fast paths of the math functions, of rational stores and of `+=` with
  a rational keep their inputs in registers: no stack canary under
  `-fstack-protector-strong` (the default of Ubuntu's GCC), 1–5 fewer
  instructions on every math function.
- `x += r` and `x -= r` with a `rational` on a Q-format grid: 12 fewer
  instructions per call (`+= rational{k, 256}` on Q8.8: 41.8 → 28.8 ins/op).
  The notch count came back as an `optional<imax>`, which GCC 16 kept in
  memory; it is now a bool and an out-parameter, with a branchless sign.

### Removed

- The named policies `rounded_toward_zero`, `rounded_nearest`, `rounded_floor`,
  `rounded_ceil`, `rounded_half_even`, `clamped` and `wrapped`: they repeated
  the flags under a second name.
- The option `BEMAN_INSIDE_STRICT_SFINAE` and its CI cells.
- The flags `ignore_range` and `ignore_zero`: each turned a runtime check off
  without saying `unsafe`. They compiled to the same code as `unsafe`.
- The macros `BEMAN_INSIDE_MAX_FRACTION_BITS` and
  `BEMAN_INSIDE_MATH_TABLE_SLOTS`: the limits are fixed at 1024 bits and 256
  slots, so translation units built with different values can no longer form
  different types and tables in one program. An instrumented build
  (`BEMAN_INSIDE_MATH_INSTRUMENT`) has no table path, so its counters, and
  the certify tool, see every call.
- The implicit `operator imax()`. It narrowed silently into smaller built-ins:
  `int i = b;` on a grid past `int` truncated, and `unsigned char c = b;` on
  `{-5, 5}` read −1 as 255, with no warning under `-Wall -Wextra`.

### Fixed

- The docs said a quantity's `++` adds one unit; it steps one notch of the
  number, as on an inside, and now say so.
- An inside had no unary `+`, so `+q` on a quantity did not compile (its
  `operator+()` was ill-formed for every number). `+x` is now `x`, for both.
- `++` and `--` on a grid without a notch were a `static_assert` body, so a
  `requires` probe said they existed. They are deleted overloads with the
  reason now. A new test instantiates the class templates explicitly, which
  compiles every member whose constraints hold, as some compilers do while
  reflecting on a class.
- A `double` past the magnitude of a grid past 64 bits stood in as a value
  just past the grid, which a wrap then folded: `inside<{-0x1p70, 0x1p70},
  wrap>{-0x1p72}` was 4 instead of 2. A wrap, and an `on_clamp` overshoot, now
  use the double's exact value.

### Documentation

- The docs are rewritten in three levels: [Getting started](docs/getting-started.md),
  seven short guides in `docs/guide/`, and the details in `docs/advanced/`.
  [docs/README.md](docs/README.md) maps every page, header and example. The
  README and CONTRIBUTING.md no longer follow the Beman exemplar template.
- The [Compiler Explorer example](https://godbolt.org/z/MafnPMMxh) uses the
  number, the math and the units together, with the single header from `main`.
- Pages that moved:

  | Before | Now |
  |---|---|
  | `docs/tutorial.md` | `docs/getting-started.md` |
  | `docs/policies.md` | `docs/guide/policies.md`, `docs/advanced/hooks-and-errors.md` |
  | `docs/arithmetic.md` | `docs/guide/arithmetic.md`, `docs/advanced/exact-arithmetic.md` |
  | `docs/conversions.md` | `docs/guide/conversions.md`, `docs/advanced/text.md` |
  | `docs/storage.md` | `docs/guide/grids.md`, `docs/guide/ranges.md`, `docs/advanced/storage.md` |
  | `docs/units.md` | `docs/guide/units.md`, `docs/advanced/units-model.md` |
  | `docs/math.md` | `docs/guide/math.md`, `docs/advanced/math-engine.md` |
  | `docs/constants.md`, `fixed-point.md`, `determinism.md`, `certify.md`, `freestanding.md`, `diagnostics.md`, `single-header.md` | the same names in `docs/advanced/` |

- Corrected: quantities have `std::hash` and `std::numeric_limits`; a quotient
  never reports `errc::overflow` (past 1024 bits it has no type); `%` on a
  grid of non-integers and a compound assignment from a `std::expected` are
  deleted overloads; `divmod` takes values past `imax`; a hook reads its carry
  with `as<imax>(c)`; `BEMAN_INSIDE_NO_STRING` drops only the text headers;
  the double tier's minimax coefficients are generated offline (Sollya); the
  fine-grid math speeds are measured again.

## 0.6.0 — 2026-10-30

The first C++26-only release: it needs static reflection (GCC 16 with
`-freflection`). 0.5.0 stays the release for C++23 compilers; the tag
`preCPP26` is the last C++23 state of main.

### Upgrading from 0.5.0

- Build with GCC 16 and `-std=c++26 -freflection` (the CMake target adds the
  flag). Clang, AppleClang, MSVC and C++23 builds stay on 0.5.0.
- A continuous grid names its denominator bound:
  `inside<{{0, 1}, max_denominator{1000}}>`, not `inside<{{0, 1}, 0}>`.
- `to<int>()` and `as<int>()` of a fractional value need a rounding:
  `to<int, round_floor>()`, or a rounding policy on the type.
- `on_overflow` and `x.with(...)` are gone: `+ - * /` cannot overflow, and a
  store takes one hook (`x.on_clamp(f)`, `x.on_wrap(f)`, `x.on_error(f)`).
- Code that prints a quantity includes `<beman/inside/units_io.hpp>`.
- `rational`'s `Numerator` / `Denominator` are `Num` / `Den`, and
  `grid::slot_count()` is `grid::step_count()`.

### Changed

- Requires C++26 with static reflection: GCC 16 with `-freflection` (the
  CMake target adds it; configuring fails without it). C++23, Clang,
  AppleClang and MSVC are no longer supported, and `-ffreestanding` waits
  for a freestanding `<meta>` ([freestanding.md](docs/advanced/freestanding.md)).
- Grid numbers have any size, always: the 64-bit grid fallback,
  `BEMAN_INSIDE_BIG_GRIDS` and `BEMAN_INSIDE_REFLECTION` are gone, and so
  are the diagnostics for results past the 64-bit grid numbers.
- `grid` and `interval` `+ - *` and `hull` return plain values (they cannot
  fail); `/` still returns `std::expected`. `common_inside_t` and
  `std::common_type` always have a type.
- The `small_grids_v2` / `big_grids_v2` ABI namespaces are gone.
- `inside(value, errc&)` is a deleted constructor with the reason.
- `grid::slot_count()` is `grid::step_count()`: it returns (Upper − Lower)/Notch,
  the number of steps, one less than the number of values (255 for `uint8`).
- `rational`'s members are `Num` and `Den` (were `Numerator` and
  `Denominator`), as in every other fraction type of the library and in
  `std::ratio`.
- Quantity text is in the new `units_io.hpp` (`to_string`, `<<`,
  `std::format`, `unit_symbol`, `parse<Q>`), as `io.hpp` is for insides:
  `units.hpp` no longer includes `io.hpp`, `<format>` or `<ostream>`, so it
  parses in 0.72 s instead of 1.13 s, and `BEMAN_INSIDE_NO_STRING` keeps the
  units. Code that prints a quantity includes `units_io.hpp`.
- The umbrella `inside.hpp` includes `numeric_limits.hpp`: without it a TU
  silently got the primary `std::numeric_limits` for an inside, and an ODR
  violation beside a TU that had the specialization.
- `on_overflow` is removed: arithmetic on insides cannot overflow (continuous
  grids have a bound, and a quotient always fits its result type). A zero
  divisor in the free `div`, `mod` and `divmod` stays in the returned
  `expected`, as for `/`; the free functions take a policy or an `errc&`, no
  hook. A compound op's failure (a zero divisor in `/=`, a `rational`
  right-hand side past 64 bits) goes to `on_error`, else the policy. `/` by a
  zero-free divisor is never `expected`: `just<1e30_g> / just<3>` is a plain
  point.
- `x.with(hooks...)` is removed: a store takes one hook, so it repeated
  `x.on_clamp(f)`, `x.on_wrap(f)` and `x.on_error(f)`.
- `std::common_type` (and `common_inside_t`, `min`, `max`) of two insides
  keeps the policy they share; only different policies give `checked`.
- `as<T>()` reports a value that does not convert like a failed checked
  store (`inside_error`, or the installed handler), not as
  `std::bad_expected_access`.
- `to<T>()` / `as<T>()` into an integer `T` no longer truncate a fractional
  value silently: they round by the call's mode (`to<int, round_floor>()`, also
  on the free forms), else by the type's rounding mode, and with neither report
  `errc::rounding_error`. A `round_nearest` type now rounds to nearest here.
- `std::numeric_limits` of an inside: `digits` / `digits10` come from the grid
  (the narrowest integer that numbers every value, less a sign bit for a grid
  with negatives), not the storage; `epsilon()` and `round_error()` are 0 and
  exist only when 0 is a value of the grid (they returned an ad-hoc stand-in
  otherwise).
- A continuous inside needs a denominator bound:
  `inside<{{0, 1}, max_denominator{1000}}>`. `inside<{{0, 1}, 0}>` no longer
  compiles, and its message names the fix. `+`, `-` and `*` are therefore
  total and never return `std::expected`, and neither does a division by a
  divisor whose grid excludes 0. An exact result whose fractions would pass
  `BEMAN_INSIDE_MAX_FRACTION_BITS` has no type: the operator is deleted, and
  its message names the fix (it was unbounded, with checked operations).
  `grid::try_make(interval, 0)` on two limits is a `domain_error`; the new
  `grid::try_make(interval, max_denominator)` makes a continuous grid.

### Added

- A checked store that fails in a constant expression says why and how to
  fix it, through C++26 constexpr exceptions: `constexpr inside<{0, 10}> x{11};`
  stops with "a checked store in a constant expression is outside the grid's
  interval - widen the interval, or store with clamp or wrap".
- `math::fn<mode>(x)` beside every deduced math form (`pow_base<B, mode>`,
  `atan2<mode>`, and the `units::` forms): the rounding mode on the call, so
  the input's type need not carry one.
- `divmod` on integer grids past imax: uint64 grids and big grids,
  with quotient and remainder from one exact division.
- Grid numbers take built-in integers in `+ - * /`: `N - 1` was
  ambiguous and had to be spelled `N - 1_g`.
- Big grids: an inside takes a grid number at run time (constructor,
  `=`, `try_make`), checked like any store: `residue r = top;` for a
  `constexpr` big `top`. Before, only `just<top>` worked.
- Example [`rsa.cpp`](examples/rsa.cpp): textbook RSA-1024 on big
  grids (extended Euclid, square-and-multiply, CRT, signatures).
- [docs/adoption.md](docs/adoption.md): why safer number types are hard to
  adopt (Mooers' Law), what arithmetic bugs have cost, and how to make
  knowing cheap. The paper gains a matching section.
- Three WG21 drafts replace the single one in `docs/proposal`: the number
  (`inside.md`), quantities (`quantity.md`) and the math functions
  (`math.md`). ctest (label `proposal`) checks their listings against
  compiled programs.

### Removed

- The compiler shims in `detail/config.hpp` (MSVC warnings, `_umul128`,
  portable overflow checks, `BEMAN_INSIDE_DELETE`, the valued-diagnostic and
  `std::format` detection).
- `<beman/inside/lift.hpp>`: it held only `detail::` names and is
  `detail/lift.hpp`.

### Fixed

- Big grids: an integer division under a rounding policy
  (`div(a, b, rounded_floor)`, or `snap`) returned the exact quotient
  (7 / 2 gave 3.5). Now it rounds onto the integers, as on 64-bit grids.

## 0.5.0 — 2026-10-25

### Upgrading from 0.4.0

- `just<N>.per<D>` is gone: it miscompiled on MSVC. Write `just<N, D>`
  (`just<1>.per<3>` is `just<1, 3>`; in dependent code,
  `just<N>.template per<D>` is `just<N, D>`).
- `uniform<B>(rng)` draws a different sequence for the same seed: it now
  draws from the generator's raw outputs by its own recipe, so the sequence
  is the same on every standard library. Re-record seeded samples.
- A build with excess floating-point precision (`__FLT_EVAL_METHOD__` not
  0: 32-bit x86 on the x87 unit, or 32-bit MSVC below `/arch:SSE2`) stops
  with an `#error`. Compile for SSE2 (`-msse2 -mfpmath=sse`).

### Added

- The certify tool ([certify.md](docs/advanced/certify.md)): for an In → Out pair of
  a math function it computes every input as a run-time call and on the
  integer path alone, fails on any difference or any result left undecided
  at the precision cap, and hashes every result. `find_grids.py` finds the
  pairs a build uses in its symbols and certifies them, with progress and a
  time estimate. [certified.md](docs/certified.md) lists the library's
  certified pairs and their hashes, which CI must reproduce.
- [certified.md](docs/certified.md) also certifies the math pairs the
  examples use: 20 pairs, among them `atan2` and `hypot` over two 2⁻¹⁴-grid
  inputs (1.07 G inputs each). Five more, of up to 10¹⁹ inputs, are listed
  as too large to compute. A debug build's test scans the examples' symbols
  and fails when one of their pairs has no row. The certify tool's
  `--require-all` makes that check, and `runner::section` groups the pairs
  under headings.
- `BEMAN_INSIDE_CERTIFY_FULL=ON` checks every certified pair. By default
  the certify test checks the pairs up to 2²⁰ inputs, which takes seconds,
  also under sanitizers. The weekly CI run sets it on GCC, Clang with
  libc++, AppleClang and MSVC.
- `BEMAN_INSIDE_MATH_INSTRUMENT=1`: `math::driver` counts the integer path's
  calls, deep steps and guesses, and `IntegerOnly` sends every call to the
  integer path. Off by default, at no cost.
- `tests/beman/inside/math_exact.test.cpp` puts every kind of rational math
  result (`2^−1`, `sqrt(9/4)`, `8^(2/3)`, `hypot(3/2, 2)`, sin 30°, …) on a
  tie or a grid point, under every rounding mode, at compile time and at run
  time.

### Changed

- The math docs say exactly what holds. Results are the same in every build
  and correctly rounded wherever the engine decides them. A result still
  undecided at the precision cap (within about 2^−4W of a rounding boundary)
  is a deterministic guess ([math.md](docs/advanced/math-engine.md#what-is-guaranteed),
  [determinism.md](docs/advanced/determinism.md#when-is-a-result-decided)). Before,
  they claimed correct rounding always.
- The math tables tier skips an input whose result is a guess, so the
  run-time counters see it. The results are unchanged.
- [determinism.md](docs/advanced/determinism.md) covers `float`, `long double`,
  text, random sampling, hashing, MSVC's `/fp` modes and how the guarantee
  is tested.

## 0.4.0 — 2026-10-19

### Upgrading from 0.3.0

- The named policy `snapped` is now `rounded_toward_zero`: replace
  `div(a, b, snapped)` with `div(a, b, rounded_toward_zero)`. The flag
  `snap` and `with_snap<Mode>()` are unchanged.

### Added

- inside's algorithms on quantities ([units.md](docs/advanced/units-model.md#insides-algorithms-on-quantities)):
  the hooks `q.on_wrap/on_clamp/on_error/on_overflow` (stores take
  quantities, the hook gets the quantity), `++`/`--` by one unit,
  `Q::try_make`, `mul_into`/`div_into` into a quantity type (the unit
  factor folds into the one rounding), `add_all`, `mul_all`, `sum<Q>`,
  `divmod` in the common unit, `parse<Q>`/`parse_exact<Q>`, the conversion
  predicates, `clamp_cast`/`wrap_cast` into a quantity type, and
  `dot`/`cross`/`lerp`. An expected quantity chains through the
  expected-lift operators.
- `units::sin_into`/`cos_into` take an angle in any unit with a factor of
  π (°, a turn, 2π/65536 rad): it is reduced exactly by quarter turns, so
  sin 30° is exactly 1/2 under any rounding.
- `units::exp_into`, `log10_into`, `pow_base_into`, `pow_into` and the
  other number functions take a dimensionless quantity: 10^(L / 20 dB).
- `units::percent` (%) and `units::per_mille` (‰).
- 25 more examples on units, half of them with units of their own: ETH,
  gwei, wei, gas and USD (`wei`), USD and ¢ (`money`, `json_io`), HP
  (`game_hp`), rounds (`rifle`), months and weeks (`clock`), px
  (`torus_map`, `geometry`), a phase slot (`oscillator`), packets
  (`sequence_number`, `jitter_buffer`), dB (`decibels`, `audio_mixer`).
- Under C++26 the grid and assignment diagnostics print the values that
  failed (P2741 static_assert messages): `grid: the notch must divide
  Upper − Lower evenly (notch 3, Upper − Lower 10)`, the two notches of an
  incompatible assignment, the two intervals of a disjoint one
  ([diagnostics.md](docs/advanced/diagnostics.md)).
- MSVC 19.51 (Visual Studio 2026) and later is supported, /W4 clean, with
  three CI cells. `just<N, D>` spells the ratio `just<N>.per<D>`; on MSVC
  only `just<N, D>` works, because of a compiler bug.
- Example [`adc_calibration.cpp`](examples/adc_calibration.cpp): ADC counts
  as a base dimension of their own, and a two-point calibration whose gain
  is checked once, after which no division can fail.
- `<beman/inside/constants.hpp>` ([constants.md](docs/advanced/constants.md)):
  mathematical constants in `math::` (π, e, √2, ln 2, φ, γ, Catalan's G,
  ζ(3), … as point insides within 2^-57), and physical constants in
  `units::constants::` as quantities of their ISQ kind. The SI 2019 defining
  constants and what follows from them (ħ, R, F, K_J, R_K, G₀, Φ₀, σ, c₁, c₂)
  are exact, along with the standard atmosphere and the CODATA 2022 measured
  values (G, α, R_∞, μ₀, Z₀, and under C++26 big grids the particle masses,
  a₀, ε₀).
- `value_cast` across a factor of π takes a factor past 64 bits under C++26
  big grids, so ħ converts to J s.
- `round_toward_zero`, the rounding flag that names truncation toward zero
  (the `snap` bit alone), and its named policy `rounded_toward_zero`.

### Changed

- `si::dalton` uses the CODATA 2022 value, 1.66053906892e-27 kg (was 2018's
  1.66053906660e-27 kg).
- **Breaking:** the named policy `snapped` is now `rounded_toward_zero`,
  next to `rounded_nearest`, `rounded_floor`, `rounded_ceil` and
  `rounded_half_even`. Every mode snaps onto the grid, so the old name did
  not say which way. `snap` stays the flag that permits rounding, and
  `with_snap<Mode>()` is unchanged. Replace `div(a, b, snapped)` with
  `div(a, b, rounded_toward_zero)`.

### Performance

- `mul_into` / `div_into` compute natively when the numbers fit: the exact
  product as an inside, or the product or quotient as a 64-bit rational,
  rounded once as before. The wide rational path remains the fallback.
  ins/op: `mul_into` 974 → 10, `div_into` 1212 → 92 (u8 grids).
- The exp-family kernels (exp, exp2, pow10, sinh, cosh, tanh) scale by a
  power of two built from its bits instead of a branchy `ldexp`: 15–21
  fewer instructions and about 20–25% fewer cycles per call, the same bits.

### Fixed

- Two units error messages said "detail::recipe" instead of "recipe".
- The single header's banner and the drift-check error name the real
  regenerate target, `beman.inside.amalgamate`.

### Testing

- A differential oracle (`tests/beman/inside/differential`): random cases of
  arithmetic, `%`/`divmod`, rounding stores, bounded continuous grids,
  `to<float>`/`to<double>` and π conversions, each recomputed exactly in
  Python. ctest runs a fixed seed; a scheduled CI run takes a random seed
  and twenty times the cases.
- The property fuzzer and the math accuracy sweep run in every build as
  tests (`beman.inside.tests.fuzz`, `beman.inside.tests.accuracy`).

## 0.3.0 — 2026-10-17

### Upgrading from 0.2.0

- `inside_range::strided(n)` and `indexed()` are gone: pipe
  `std::views::stride(n)` / `std::views::enumerate` (C++23), or step with a
  `cursor<T, Step>`. `enumerate` yields a `ptrdiff_t` position, not `size_t`.
- `grid` has a third member, `MaxDenominator` (0 except on a bounded
  continuous grid): a structured binding of a grid takes three names. The
  grid ABI namespaces are now `small_grids_v2` / `big_grids_v2`.
- Result types change where they gained precision: `x / just<c>` is on x's
  lattice scaled by `1/|c|` (it was continuous), and every continuous
  quotient carries a denominator bound. Code that names those types, or
  unwraps a result that is now plain, needs updating.
- `frac<N, D>` is gone: an exact ratio is the point `just<N>.per<D>`
  (`just<frac<1, 2>>` is `just<1>.per<2>`; as a grid limit or step,
  `{{just<-6>.per<5>, just<3>.per<5>}, per<40>}`).
- `zero` and `one` are gone (`units::one` is the dimensionless unit): write
  `just<0>` / `just<1>`, or `0_ins` / `1_ins`.
- Renamed:

  | Before | After |
  |---|---|
  | `clamp_floor<B>(v)` / `clamp_ceil<B>(v)` / `clamp_round<B>(v)` | `clamp_cast<B, round_floor>(v)` / `round_ceil` / `round_nearest` |
  | `byte` `word` `dword` `qword` | `uint8` `uint16` `uint32` `uint64` |
  | `sbyte` `sword` `sdword` `sqword` | `int8` `int16` `int32` `int64` |
  | `counter<Max>` / `ring_counter<Max>` | `clamp_counter<Max>` / `wrap_counter<Max>` |
  | `from_chars<B>` / `from_chars_exact<B>` | `parse<B>` / `parse_exact<B>` |
  | `policy_of<B>` | `flags_of<B>` (also takes a `policy<F, E>`) |
  | `inside<G, P>::negative` | `inside<G, P>::negated_type` |
  | `math::amp<K>` | `math::unit_amplitude<K>` |
  | `why_assignable` | `explain_assignable` |
  | `conversion_is_lossy` | `conversion_loses` |
  | `<beman/inside/math.hpp>` | `<beman/inside/numbers.hpp>` |

### Added

- `<beman/inside/units.hpp>`: physical quantities with an inside as the
  number — the main features of mp-units, without depending on it
  ([docs/units.md](docs/guide/units.md)). Units, dimensions, ISQ quantity kinds
  and references are compile-time values: `quantity<si::metre, inside<…>>`,
  `isq::height[si::metre]`. A unit conversion rescales the grid and keeps
  the raw (km → mm is a `mov`), and a sum of mixed units runs in their
  common unit, exactly. `quantity_point` with absolute and relative origins
  (K, ℃, ℉ are exact grid shifts). Kinds convert as in mp-units: up the
  tree and by recipe implicitly, `quantity_cast` down; `is_kind` and
  separate roots never mix (Hz vs Bq). The catalog covers `isq`, `si` (with
  prefixes; quecto–quetta, eV and Da under C++26 big grids), `si2019`
  constants and `international` units. `std::format` and `to_string`
  print `"5 km"`. `<beman/inside/units_math.hpp>` adds `value_cast` across
  a factor of π (degree → radian, rounded once by the correctly rounded
  engine), `sqrt`/`cbrt` of the unit, trigonometry and `hypot`.
  `examples/units.cpp`, `circuit.cpp`, `moon_rocket.cpp` and `kitchen.cpp`
  show them. A dimensionless quantity converts to an inside (its value in
  unit one); `mag_pi * q` (and `mag_pi * just<2> * si::second`) keeps π
  in the unit until one rounding; `sqrt_into<Out>`, `atan2_into<Out>`, …
  take checked arithmetic results; `p.point_in(u)` and `value_cast` work on
  points; `try_value_cast<u, Rep2, F>(q)` converts and stores into an
  expected, and `value_cast<Q>(q)` / `try_value_cast<Q, F>(q)` into a
  quantity type. A sum of two units that divide neither way runs in the
  smaller unit scaled (ft + m and m + ft are `[1/381 ft]`), and two
  spellings of one size in the shorter one (J + N·m is in J).
  `q.quantity_spec == isq::length` queries a spec; `unit * mag`,
  `p.quantity_from_zero()`. Every quantity operator, conversion and cast is
  constrained on the number operation it runs, and every unit mismatch is a
  constraint failure with a message (a deleted overload per reason), so
  `requires { m + s; }` and `requires { q * 2; }` are false.
- Units of your own: a `named_unit` with `unit_options` (an ASCII symbol, a
  kind, an origin, no space before the symbol), specs and origins, each
  with a tag (`unit_c<obj>`, `spec_c<obj>`, `origin_c<obj>`), and up to
  four `base_dimension`s beside the seven SI ones (currency, information).
  `examples/own_units.cpp` shows a unit, a temperature scale and two base
  dimensions.
- `to_string(errc)` and `operator<<` for `errc` print the error message.
- Open grid limits as a spelling: `inside<{0, 360, bounds::half_open}, wrap>`
  is `inside<{0, 359}, wrap>` (period 360), and `{{0, 64}, per<16>,
  bounds::half_open}` is `{{0, 63.9375}, per<16>}`. `bounds::open` moves both
  limits in. The grid is stored closed, so both spellings are one type;
  `grid::try_make` takes the bound too.
- `values<T>`: every value of an inside type as a random-access sized range,
  `for (T x : values<T>)`, `std::ranges::find(values<T>, x)`.
- A cursor is a bidirectional iterator with `end(t)` as its sentinel: `*t`
  gives its value (converting into `T` exactly), so
  `std::ranges::subrange(t, end(t))`, `std::views::iota(t, end(t))` and the
  iterator-pair algorithms take it. Plain insides stay non-iterators.
- `inside_range` is a view (`view_interface`: `values<T>[i]`, `front()`,
  `back()`, `empty()`) and a borrowed range.
- `%` and `divmod` on integer grids without `snap`: truncating like C++ `%`,
  or rounding by the policy's mode. `/` stays exact without `snap`.
- Bounded continuous grids: a continuous grid can carry a denominator bound,
  `inside<{{0, 1}, max_denominator{1000}}>`, and every exact quotient gets one
  (`inside<{0, 100}> / inside<{1, 7}>` has denominators ≤ 7). `+`, `−`, `×`,
  `÷`, `abs`, `copysign` and `fmod` propagate it; when it sizes the result's
  fraction the op returns a plain value instead of `std::expected`. In C++26 a
  bound past 64 bits stores a fraction sized from it (up to
  `BEMAN_INSIDE_MAX_FRACTION_BITS`); in C++23 such a bound is dropped. A store
  past the bound reports `rounding_error`.

### Changed

- Division by a point keeps the dividend's lattice, so `c * 9_ins / 5_ins +
  32_ins` is plain and exact.

### Fixed

- `wrap` folds a value whose 64-bit rational steps overflow exactly instead
  of reporting `errc::overflow`: a double of 2^63 or more (`1e30` into
  `{0, 9}` is 6), a full-int64 value into a fine grid, a far value on a
  2^-62 grid. A wrap action's carry saturates there, as on the wide paths.
- A point grid (`inside<{3}>`) range-checks an inside stored into it: any
  inside used to store as the point (4 into `{3}` was 3). A wide or fine
  source into a point grid, and `wrap` onto one, terminated (a division by
  the zero notch); they report `errc::overflow`.
- `mul_into`, `div_into` and wide `sum` totals range-check before the notch,
  as every other store: an off-notch value past a limit clamps under `clamp`
  and reports `overflow` (it reported `rounding_error`).
- Signed overflow on the way to a value index whose raw passes 2^63 (a grid
  of more than 2^63 slots below a negative base).
- A named policy (`clamped`, `rounded_floor`, a const `policy(ec)`) works as a
  constructor's one-shot policy.
- `clamp_cast`/`wrap_cast`/`clamp_floor`/`clamp_ceil`/`clamp_round`/
  `checked_cast`/`unchecked_cast`, `math::abs_into` and `math::pown` are
  constrained like the stores and products they make; the product test no
  longer stops the build when a notch product passes the 64-bit rational.
- The arithmetic operators, `add`/`sub`/`mul`/`div`/`mod`/`divmod`, the
  compound assignments and `try_make` are constrained on whether the
  operation has a result, so `requires { a / b; }` and `if constexpr`
  probes are false where it cannot compile (a zero or unbounded continuous
  divisor grid spanning 0, a grid past the 64-bit grid numbers, `%` of a
  fractional grid, a store `try_make` rejects). An impossible operator is a
  deleted overload carrying the old message (`= delete("…")`, GCC 15+ /
  Clang 19+), so an integer inside no longer falls back on its `imax`
  conversion and the built-in operator. The `grid` operators take only
  grids: an inside no longer converts to a point grid for them.
- A `long double` with bits past the double's is exact everywhere, not only
  in stores: `rational{long double}`, comparisons of a `rational` or an
  inside with it, `includes(interval, …)` and `grid::representable(…)` read
  it as a double (`rational{0} == 1e-30L` was true). `to<long double>()`
  rounds the exact value once to the long double's digits instead of going
  through a double (1 + 2^-60 came back as 1).
- `rational` compares with a double of magnitude 2^64 or more by its sign
  (it threw).
- An `on_error` action no longer overrides the type's `clamp` or `wrap`: the
  store saturates or folds, and `on_error` fires only where the store would
  fail.
- A point inside (`just<1/2>`, `5_ins`) stores into a grid whose raws map
  neither by an integer nor by a 64-bit multiply-add (a 2^-62 notch,
  `uint64`); it did not compile.
- C++26 big grids: `grid::representable`, `includes` and grid-number
  comparisons work at runtime when a value or limit passes 64 bits (they
  terminated): a fine double on a 64-bit grid, any double on a grid past
  2^64.
- `to<float>()` rounds once: a value whose double is a tie between two
  floats rounded to even from the double, not from the value.

## 0.2.0 — 2026-10-10

### Upgrading from 0.1.0

- Drop the storage flags `exact`, `direct`, `indexed`, `i8`…`u64`, `f64` and
  `f32`: storage now follows from the grid, and values, rounding and errors
  are unchanged. Where a wire format needs a fixed integer width, store
  `x.as<std::uint16_t>()`, or `x.to<std::uint16_t>()` for a
  `std::expected`.
- `++` / `--` move one notch, not 1: on `per<1024>` write `x += 1_ins` to add 1.
- Text: a value with no finite decimal prints as `N/D` (was a mixed number
  such as `2 1/3`). Off-grid text that no 64-bit rational holds reports
  `rounding_error` (was `overflow`).

### Added

- `cursor<T, Step>`: an inside stepping through `T`'s range by `Step` and one
  step past its Upper, for `for (cursor<T, per<4>> t; t != end(t); ++t)`.
  It starts at `T`'s Lower by default; `end(t)` is its past-the-end value.
- Grids whose Lower is not a multiple of the notch: `inside<{{0.5, 255.5}, 1}>`
  holds 0.5, 1.5, …, 255.5 in a `uint8_t`. Every operation works on them —
  rounding stores (in value space, every mode), conversions, `+ − × /`,
  comparison, clamp and wrap, text, ranges, `sum`, `numeric_limits`, random
  and the math engine, correctly rounded onto such an output too (`wrap`
  outputs excepted). Result grids track the lattice: a product of two
  half-offset grids has notch 1/2, `hull` refines by the offset between two
  lattices, and `abs` the lattice of ±x. `grid::anchored()` and
  `grid::value_unit()` describe a grid; `grid::try_make` no longer rejects
  such a Lower. Grids that were valid before behave and compile as before.
- The implicit `operator imax()` exists when the policy may round (`snap`,
  `round_*`), rounding the value by that mode; without a rounding flag it
  still needs integer values.
- `mul_into<Out, F>(a, b)` and `div_into<Out, F>(a, b)`: the exact product or
  quotient, rounded once onto `Out` by its policy, without forming the
  product or quotient grid — for products whose grid would need numbers past
  64 bits under C++23, and quotients with no 64-bit fraction. `div_into`
  returns `std::expected<Out, errc>` when the divisor's grid holds zero.
- `from_chars<B, F>(text)` adds per-call policy flags; `from_chars_exact<B>`
  accepts only a value `B` holds exactly (off the grid `rounding_error`, out
  of range `overflow`, whatever `B`'s policy).
- `B::try_make<F>(value)` adds per-call flags, and `B::try_make` takes a
  `std::expected` and passes its error on.
- `beman::inside::rational` is public (was `detail::rational`).
- `divmod(a, b)`: the rounded quotient and its remainder from one division,
  `auto [q, r] = divmod(a, b, snapped);`. The halves have the types of `a / b`
  and `a % b`. It has the same requirements as `%` (integer grids and a
  rounding policy), and every value must fit `imax`. It returns
  `std::expected` only when the divisor's grid holds zero.

### Changed

- `++` / `--` move one notch on every grid (was ±1): on `per<1024>` they add
  1/1024. A grid without a notch has no `++`. Breaking for code that relied on
  `++` adding 1 on a fine grid (`std::iota` over such a type steps by the notch).
- **Storage follows from the grid alone** (breaking): `exact`, `direct`,
  `indexed` and the width flags `i8`…`u64` are gone. They only picked a raw
  layout; values never changed. A notched grid's index is already exact (a
  wide index past 2^64 slots); a whole-number grid stores its value wherever
  that is no wider than the index (`inside<{5, 100}>` holds 5..100 in a
  `uint8_t`, was the index 0..95; `{200, 300}` keeps the index), so `direct`
  is the default where it is free. For a fixed wire layout, write
  `to<std::uint16_t>()` into the field. Arithmetic results are plain checked
  insides.
- **Removed `f64` and `f32` storage** (breaking). The flags only picked a
  double/float raw, and every result already equalled the flag-free type's;
  drop the flag and the type stores an integer index with the same values,
  rounding, errors and printing. Spell rounding as before (`round_nearest`).
  `math::amp<K>` is `round_nearest` on integer storage for every K. The math
  engine's double and dd tiers are unchanged.
- Examples: `wei`, `huge_angles`, `planck_to_cosmos`, `solar_system`,
  `expected_pipeline` and `json_io` drop their workarounds — exact decimal
  output, `sum`, `mul_into` / `div_into`, `just<…_g>` and the exact big-grid
  ratio, a million `+=` steps, and `from_chars_exact`.
- `errc_message(errc::rounding_error)` reads "value is not on the grid" (was
  "notch incompatibility").
- `to_string`, `operator<<` and `std::format("{}")` print the exact value,
  its form chosen by the grid: a decimal notch (`per<100>`, `0.05`,
  `1e-18_g`) prints its decimals for every value (`19.90`, `2.00`); any other
  grid prints the value's shortest exact form — its decimal however many
  digits (a 2^-52 grid, a wide value, a C++26 grid number), else `N/D` (was a
  mixed number such as `2 1/3`). Every form reads back with `from_chars`.
  Documented in conversions.md, "Writing text".
- Format specs other than an integer grid's (`{:.2f}`, `{:e}`, `{:g}`, with
  fill, align, sign, `#`, `0`, width) round the exact value once, by the
  type's rounding policy (`round_floor`, `round_ceil`, `round_nearest`,
  `round_half_even`, `snap`; ties to even without one, and for `rational`),
  for every inside (was through `double`: wrong digits past 2^53 and for big
  grids).
- `from_chars` parses a number past the 64-bit rational (a long decimal)
  exactly for every grid, not only wide ones: off-grid text reports
  `rounding_error` (was `overflow`), and a rounding policy rounds it.

### Performance

- Integer raws skip the rational detours: comparisons of any two integer
  grids are one integer compare (counts of a common unit); `/` into an exact
  quotient builds one fraction from the value indices; signed fixed-point
  `/` under `snap` (e.g. Q15) stays on its notch like the unsigned one;
  `+=`/`-=` of a rational on a Q-format grid is a raw add (248 → 36
  instructions); floor/ceil/round on a 2^-k notch are shifts (native
  parity); `copysign`'s and a wider `fmod`'s integer paths. Power-of-two
  fractions reduce without a gcd (rational add 130 → 71 instructions).

- Integer storage reads and writes doubles directly: storing a `double` into
  a double-exact grid with a power-of-two notch rounds `v / Notch` in double
  (about 2 ns, was 5–9 with a rational detour), and reading an index raw as a
  `double` is one convert and one multiply or divide. `math::floor`, `ceil`,
  `round`, `trunc` and `abs` work on the value index; comparing with a
  `double` on such a grid compares in double, exactly. Dyadic division
  reduces its operands by trailing zeros instead of a gcd (24 → 12 ns).

- `+=` and `-=` of same-notch insides with an offset (Lower ≠ 0) are a raw
  add for every grid, and a same-notch assignment of a wide value is a raw
  shift: a wide-index `x += step` takes about 1 ns instead of 150.
- Math at full `double` resolution: the dd tier tries lean `sin`, `cos`,
  `exp`, `exp2`, `log`, `log2` and `log10` kernels first (about 2^-70, with
  proved bounds), and lean compositions of them for `tan`, the hyperbolics
  and their inverses, `atan`, `atan2`, `asin`, `acos`, `pow` and `Base^x`
  (1.4–2.2× faster onto outputs past the double tier). `sin` onto a 2^-52 grid takes 14.6 ns instead of 36.0,
  `exp` onto 2^-40 12.5 instead of 21.1, `log` onto 2^-48 17.4 instead of
  30.1; correct rounding at `double` resolution now costs 4–9× `<cmath>`
  (was 8–13×).
- `sinh`, `asinh` and `acosh` stay in the double tier up to 47-bit outputs
  (was 46), through sharper forms used only past 46 bits.

### Fixed

- Continuous grids: `/` and `%` truncated their operands under `snap`
  (7/2 ÷ 3/2 gave 3); wrapping a continuous source folded its numerator;
  clamping into a continuous target stored truncated endpoints; a notched
  source into a continuous target needed `snap`; `std::hash` of a point or
  an exact-fraction raw did not compile; and (C++26) `-x` of a big
  continuous value divided by zero.
- `min`/`max`/`common_type` with a constant (`max(x, just<0>)`) went to a
  continuous, rational-raw type; the hull of a point keeps the lattice.
- (C++26) A same-notch store between unanchored wide grids landed one notch
  off.

- A continuous inside plus a notched one took the notched grid's notch
  (gcd(0, n) = n, a point's rule): `3/10 + 5` stored a nonsense index. The
  sum is continuous now, as `hull` already was.

- `snap` alone (truncate toward zero) floored a negative off-notch value
  whose exact signed index passed 64 bits — a double, or a rational with a
  fine denominator, on a fine signed grid — instead of truncating it.

- Storing a continuous inside (an exact quotient) into a checked notched
  inside rounded it silently when it fell between notches; it reports
  `rounding_error` now, and a rounding policy rounds it.
- C++26 grids past 64 bits: `just<V>` takes any grid number (`just<5_g>`,
  `just<1.616255e-35_g>`); a big point stores into a grid, adds as a
  constant, converts to `double` and prints. A continuous grid whose limits
  pass 64 bits — a quotient of big-grid values among them — stores an exact
  fraction of fixed-width integers sized from the limits, so `a / b` keeps
  and prints its exact value (it did not compile); `+`, `×` and `/` report
  `overflow` when a fraction outgrows the raw. Comparing a big-grid value with
  a `double` uses the double's exact value (doubles past 2^64 compared wrong).
- `numerator()` / `denominator()` work for every inside: a wide integer when
  the grid's values pass `imax` (they did not compile).
- `sum<Target>` sums wide-index elements and totals past 64 bits exactly (it
  did not compile, or threw `bad_expected_access`); a continuous total past
  the 64-bit rational is reported through `Target`'s policy.
- `std::format` of an inside on a C++26 grid with numbers past 64 bits
  failed to compile.
- `sinh` and `cosh` onto outputs past about 2^60, and `log`-based functions of
  arguments past 2^64, shifted an error bound by 64 bits or more in the
  integer path: undefined behaviour, and a compile error in constant
  evaluation.

## 0.1.0 — 2026-10-07

First release. The library is under development and its API may change
between versions.

### The type

- `inside<grid, policy>`: a number whose range (interval) and step size
  (notch, an exact rational) are part of its type. Storage is picked from the
  grid: the narrowest integer that numbers every point, an exact fraction for
  a continuous grid, `f64`/`f32` on request for grids exact in them.
- Arithmetic widens its result grid at compile time, so `+ - *` cannot
  overflow. Division is exact by default; rounding modes (`snapped`,
  `rounded_nearest`, `rounded_floor`, `rounded_ceil`, `rounded_half_even`) are
  named per call or in the type.
- Policies decide what happens when a value is stored into a narrower grid:
  checked (the default for every policy unless `unsafe`), `clamp`, `wrap`,
  error codes, and callbacks (`on_clamp`, `on_wrap`, `on_overflow`).
- `std::expected<inside, errc>` only where an operation can fail at runtime:
  division by a divisor whose grid holds zero, `try_make`, `to<T>()`, and the
  math functions with poles. A zero-free divisor whose exact quotient the grids
  prove fits gives a plain value.
- Grids past 64 bits under C++26 static reflection (GCC 16 `-freflection`):
  limits and notches of any size, raw storage sized from the grid.

### Math

- `beman::inside::math`: `sqrt`, `cbrt`, `hypot`, `exp`, `exp2`, `log`, `log2`,
  `log10`, `pow`, the trigonometric, inverse trigonometric, hyperbolic and
  inverse hyperbolic functions, plus `abs`, rounding and `fmod`.
- Every result is the correctly rounded point of its output grid, the same at
  compile time and at runtime, on every platform, with or without an FPU.
  Cheap tiers (compile-time tables, `double` and double-`double` kernels with
  proved error bounds) answer most calls; an integer path answers the rest.
- `BEMAN_INSIDE_MATH_NO_FP` (automatic under `-ffreestanding`) compiles the
  floating-point tiers out; results do not change. `-ffast-math` builds are
  rejected.

### Packaging

- Header-only; C++23 (GCC 14+, Clang 19+, Clang 18 with libc++).
- A committed single header, `single_include/beman/inside/inside.hpp`.
- CMake package `beman.inside` (target `beman::inside`) and vcpkg port
  templates in `port/`.
- Documentation in `docs/`, and the paper
  [*int considered harmful*](docs/int-considered-harmful.pdf) with its
  runnable programs.
