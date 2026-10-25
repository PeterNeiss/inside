# Changelog

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

## Unreleased

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

- The certify tool ([certify.md](docs/certify.md)): for an In → Out pair of
  a math function it computes every input as a run-time call and on the
  integer path alone, fails on any difference or any result left undecided
  at the precision cap, and hashes every result. `find_grids.py` finds the
  pairs a build uses in its symbols and certifies them, with progress and a
  time estimate. [certified.md](docs/certified.md) lists the library's
  certified pairs and their hashes; every CI configuration must reproduce
  them.
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
  is a deterministic guess ([math.md](docs/math.md#what-is-guaranteed),
  [determinism.md](docs/determinism.md#when-is-a-result-decided)). Before,
  they claimed correct rounding always.
- The math tables tier skips an input whose result is a guess, so the
  run-time counters see it. The results are unchanged.
- [determinism.md](docs/determinism.md) covers `float`, `long double`,
  text, random sampling, hashing, MSVC's `/fp` modes and how the guarantee
  is tested.

## 0.4.0 — 2026-10-19

### Upgrading from 0.3.0

- The named policy `snapped` is now `rounded_toward_zero`: replace
  `div(a, b, snapped)` with `div(a, b, rounded_toward_zero)`. The flag
  `snap` and `with_snap<Mode>()` are unchanged.

### Added

- inside's algorithms on quantities ([units.md](docs/units.md#insides-algorithms-on-quantities)):
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
  ([diagnostics.md](docs/diagnostics.md)).
- MSVC 19.51 (Visual Studio 2026) and later is supported, /W4 clean, with
  three CI cells. `just<N, D>` spells the ratio `just<N>.per<D>`; on MSVC
  only `just<N, D>` works, because of a compiler bug.
- Example [`adc_calibration.cpp`](examples/adc_calibration.cpp): ADC counts
  as a base dimension of their own, and a two-point calibration whose gain
  is checked once, after which no division can fail.
- `<beman/inside/constants.hpp>` ([constants.md](docs/constants.md)):
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
  ([docs/units.md](docs/units.md)). Units, dimensions, ISQ quantity kinds
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
