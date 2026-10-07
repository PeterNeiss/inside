# Changelog

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

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
