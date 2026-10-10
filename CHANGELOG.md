# Changelog

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

## Unreleased

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
