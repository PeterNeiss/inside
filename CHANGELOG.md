# Changelog

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

## Unreleased

### Added

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
  mixed number such as `2 1/3`). Every form reads back with `from_chars`. A
  continuous `f64` inside prints the double's exact decimal (was
  `std::to_string`'s six digits). Documented in conversions.md, "Writing
  text".
- Format specs other than an integer grid's (`{:.2f}`, `{:e}`, `{:g}`, with
  fill, align, sign, `#`, `0`, width) round the exact value, ties to even, for
  every inside and `rational` (was through `double`: wrong digits past 2^53
  and for big grids).
- `from_chars` parses a number past the 64-bit rational (a long decimal)
  exactly for every grid, not only wide ones: off-grid text reports
  `rounding_error` (was `overflow`), and a rounding policy rounds it.

### Performance

- `+=` and `-=` of same-notch insides with an offset (Lower ≠ 0) are a raw
  add for every grid, and a same-notch assignment of a wide value is a raw
  shift: a wide-index `x += step` takes about 1 ns instead of 150.
- Math at full `double` resolution: the dd tier tries lean `sin`, `cos`,
  `exp`, `exp2`, `log`, `log2` and `log10` kernels first (about 2^-70, with
  proved bounds), and lean compositions of them for `tan`, the hyperbolics
  and their inverses, `atan`, `atan2`, `asin`, `acos`, `pow` and `Base^x`
  (1.4–2.2× faster onto outputs past the double tier). `sin` onto a 2^-52 grid takes 12.9 ns instead of 36.0,
  `exp` onto 2^-40 11.1 instead of 21.1, `log` onto 2^-48 16.0 instead of
  30.1; correct rounding at `double` resolution now costs 4–8× `<cmath>`
  (was 8–13×).
- `sinh`, `asinh` and `acosh` stay in the double tier up to 47-bit outputs
  (was 46), through sharper forms used only past 46 bits.

### Fixed

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
