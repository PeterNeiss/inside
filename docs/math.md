# `bnd::math` — constexpr, bit-exact math

`bound/cmath.hpp` provides a `<cmath>`-shaped function set that operates on
`bound` values instead of `float`/`double`. Every function is `constexpr`,
integer-only internally, and **bit-exact across compilers and platforms** —
see the reproducibility contract at the top of `cmath.hpp`.

```cpp
#include "bound/cmath.hpp"
using namespace bnd;

using angle = bound<{{-8, 8}, notch<1, 16384>}, round_nearest>;  // radians
auto s = math::sin(angle{1});       // amplitude bound in [-1, 1]
auto h = math::hypot(s, s);         // √(s²+s²), output grid auto-deduced
```

## Conventions

- **Angles are radians**, everywhere — `sin`/`cos`/`tan` take radians;
  `asin`/`acos`/`atan`/`atan2` return radians. (There is no turns-valued
  public API.)
- **Output grids are auto-deduced.** Calling `f(x)` with no explicit template
  argument deduces the result `bound` from the input's interval and notch:
  the interval is the function's true range over the input, rounded *outward*
  to the input's notch; the notch and policy are inherited (with
  `round_nearest` added, since transcendental results carry sub-notch drift).
  Spell `f<Out>(x)` to pick the output grid yourself.
- **Error model.** A domain limit that is knowable from the *type* is a
  `static_assert` (compile error). A failure that depends on the *runtime
  value* is reported through `slim::expected<Out, errc>`. Total functions
  return the bound directly.
- **Precision.** The transcendental core runs in Q.30 fixed point, so results
  are accurate to roughly 1e-6–1e-9 depending on the composition depth, then
  quantized onto the output grid. Algebraically-exact results (e.g.
  `cbrt(8)`, `hypot(3,4)`, `pow(2,10)`) land exactly.
- **Input-range limits** below exist because the current revision uses a
  single Q.30 internal tier; a wider Q.62 tier is planned.

## Algebraic tier (exact, no polynomials)

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `abs(x)` | all | `[0, max\|·\|]` | — | exact |
| `floor(x)` / `ceil(x)` / `round(x)` / `trunc(x)` | all | integer notch | — | exact; `round` is half-away-from-zero |
| `fmod(x, y)` | `y` must not span 0 | sign of `x` | — | truncated-division convention, exact |

## Roots

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sqrt(x)` (`Lower == 0`) | `[0, 4]`, notch `1/2^K`, `K ≤ 30` | `[0, ≈√Upper]` | — | Newton/Q.30 |
| `sqrt(x)` (`Lower < 0`) | `max\|·\| ≤ 4`, notch `1/2^K` | — | `expected`; `domain_error` if value < 0 | mixed-sign overload |
| `cbrt(x)` | `\|x\| ≤ 2^20` | monotone range | — | `sign(x)·2^(log2\|x\|/3)` |
| `hypot(x, y)` | `\|x\|,\|y\| ≤ 2^20` | `[0, √(maxX²+maxY²)]` | — | scaled so radicand ∈ [1,2] |

## Trigonometric (radians)

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sin(x)` / `cos(x)` | `[-1024, 1024]` rad | `[-1, 1]` | — | Q.30 turn + Taylor |
| `tan(x)` | `[-1024, 1024]` rad | `[-1024, 1024]` | `expected`; `division_by_zero` at a pole, `overflow` past `Out` | sin/cos ratio |
| `atan(x)` | `[-1, 1]` | `[-π/4, π/4]` | — | CORDIC, normalize wider inputs first |
| `asin(x)` | `[-1, 1]` | `[-π/2, π/2]` | — | `atan2(x, √(1-x²))` |
| `acos(x)` | `[-1, 1]` | `[0, π]` | — | `π/2 - asin(x)` |
| `atan2(y, x)` | `[-1, 1]` each | `[-π, π]` | — | CORDIC vectoring + quadrant pre-rotation |

## Hyperbolic

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sinh(x)` / `cosh(x)` / `tanh(x)` | `[-10, 10]` | monotone (cosh: even, min 1) | — | from `e^x` via the exp core |

## Exponential & logarithmic

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `exp(x)` / `exp2(x)` | `exp`: `[-20, 20]`, `exp2`: `[-30, 30]` | `≥ 0` | — | `exp = exp2(x·log2 e)` |
| `log(x)` / `log2(x)` / `log10(x)` | `x > 0` | monotone | — | leading-bit reduction + atanh series |
| `pow_base<B>(x)` | integer `B ≥ 2` | `≥ 0` | — | `exp2(x·log2 B)`, `B` compile-time |
| `pow(base, exp)` | `Lower<base> > 0` | corner-deduced | `expected`; `overflow` if `exp·log2 base` leaves `[-30,30]` or result leaves `Out` | runtime base |

## Constants

`math::pi` and `math::two_pi` are point-bounds (`just<…>`), so they compose
directly in bound-space: `angle * math::two_pi`.

## Using `expected` results

```cpp
auto t = math::tan(angle{x});          // expected<bound, errc>
if (t) use(*t);
else if (t.error() == errc::division_by_zero) /* at a pole */;

auto r = math::sqrt(signed_in{v});     // mixed-sign → expected
auto p = math::pow(base, exponent);    // expected
```
