# Changelog

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

## Unreleased

### Changed

- `to_string`, `operator<<` and `std::format("{}")` print a value with a
  finite decimal as that exact decimal however many digits it has (a 2^-52
  grid, a wide value, a C++26 grid number), and any other value as `N/D`
  (was a mixed number such as `2 1/3`). Both forms read back with
  `from_chars`. A continuous `f64` inside prints the double's exact decimal
  (was `std::to_string`'s six digits).
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
