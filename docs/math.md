# `beman::inside::math` — correctly rounded math on any grid

`beman/inside/cmath.hpp` provides a `<cmath>`-shaped function set that operates on
`inside` values instead of `float`/`double`. Every transcendental returns the
**correctly rounded point of its output grid**, under the output's rounding mode:
the engine works at whatever precision that grid needs. Two consequences follow:

- **Results do not depend on how they were computed.** The same value comes out on
  every platform and compiler, at compile time and at runtime, with or without an
  FPU, under any flags.
- **No precision ceiling.** A 2^-100 output grid gets a correctly rounded result,
  and inputs and outputs may be grids past 64 bits (C++26, see
  [storage.md](storage.md#grids-past-64-bits-c26)).

```cpp
#include <beman/inside/cmath.hpp>
using namespace beman::inside;

using angle = inside<{{-8, 8}, per<16384>}, round_nearest>;
auto s = math::sin(angle{1});         // amplitude in [-1, 1], notch 1/16384
auto h = math::hypot(s, s);           // √(s²+s²), output grid deduced
```

For the reproducibility story across the whole library, see
[determinism.md](determinism.md).

## How it works

A result is computed in fixed point together with an error bound. The engine maps
both ends of that interval onto the output grid. When both land on the same grid
point, that point is the correctly rounded result. When they straddle a rounding
boundary, the engine computes again at twice the precision (Ziv's strategy). A
transcendental value lies exactly on a boundary only at the few exact rational
inputs (`exp(0)`, `log(1)`, `sqrt(9/4)`, `log2(2^k)`, integer powers, …); the
engine recognises those and stores them exactly.

Precision follows the **output** grid: the start precision is the bits that
resolve the output notch plus 8 guard bits. A coarse output is cheap; a fine one
costs more, with no fixed limit.

Each call takes the first of three paths that applies — all three give the same
grid point:

| Path | When | Cost |
|---|---|---|
| **Table** | the input has at most `BEMAN_INSIDE_MATH_TABLE_SLOTS` slots (default 256) and every result lies in the output's range | one load; the table is computed at compile time |
| **Double tier** | an FPU is present (not `BEMAN_INSIDE_MATH_NO_FP`) and the output needs at most 36 bits | the library's own double kernels, plus an error bound; decided results are stored as raws |
| **Integer path** | always available; the only path at compile time and without an FPU | Taylor polynomials with compile-time coefficient tables in wide fixed point |

The double tier bounds the kernels' error generously — 2^-40 of the result plus
2^-44·max(1, |x|), with tan, pow and acosh adding their condition numbers and an
input that is not a double exactly adding its rounding times the function's
slope — against a measured error of about one ulp. Whenever the bound does not
place the result in a single slot, the integer path decides. A test checks that
the tiers agree slot for slot.

## Conventions

- **Angles are radians**, as in `<cmath>`. `math::amp<K>` (`[-1, 1]` at
  resolution `1/K`) is a ready-made output grid: `math::sin_into<math::amp<32768>>(x)`.
- **Rounding needs permission.** A transcendental result is rounded onto a grid,
  so the operand of a deduced form (or the `Out` of an explicit one) must permit
  rounding: `round_nearest`, `round_floor`, `round_ceil`, `round_half_even`,
  `snap` or `f64`. Omitting it is a compile error.
- **Deduced outputs: `fn(x)`.** The output takes the input's notch and policy
  (minus any fixed storage width, plus `round_nearest`). Its interval is the
  function's range over the input, computed at compile time by the same engine
  and rounded outward to the notch. Exceptions: `sin`/`cos` give `[-1, 1]`,
  `tan` gives `[-1024, 1024]`, `atan2` gives `[-π, π]` and `hypot` `[0, …]` on
  the gcd of both notches, `pow` uses the base's notch, and the integer
  roundings (`floor`, …) use notch 1. A fine input notch gives a fine (and
  slower) output; name a coarser output with `fn_into` when you need less.
- **Explicit output: `fn_into<Out>(x)`.** Every function has an `_into` form
  taking the output type — `math::sin_into<amp_t>(a)`, `math::pow_into<Out>(b, e)`,
  `math::pow_base_into<Out, 10>(x)`. The result is correctly rounded under
  `Out`'s rounding mode; a result past `Out`'s range goes through `Out`'s policy
  (`clamp` saturates, `wrap` wraps, the default reports).
- **Error model.** A domain limit knowable from the *type* is a `static_assert`.
  A failure that depends on the *runtime value* is reported through
  `std::expected<Out, errc>` (`tan`, `pow`, mixed-sign `sqrt`). Total functions
  return the inside directly.
- **Domains are mathematical.** There is no working-scale envelope: `sin` takes
  any finite argument, `exp` any argument whose result fits the output, `log`
  any positive one. The deduced forms of `exp`, `exp2`, `sinh` and `cosh` need
  `|x| ≤ 4096` and `pow`'s deduced output must stay below 2^65536 — past that,
  name an output grid.
- **constexpr.** Every function is `constexpr` in every build.
- **Measured accuracy.** [accuracy.md](accuracy.md) checks every function on
  dyadic, decimal and 2^-40 grids against a long-double reference: every maximum
  error is at most 0.5 notch.

## Grid operations (exact)

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `abs(x)` | all | `[0, max\|·\|]` | — | exact |
| `floor(x)` / `ceil(x)` / `round(x)` / `trunc(x)` | all | integer notch | — | exact; `round` is half-away-from-zero |
| `sign(x)` | all | `{sign(Lower), sign(Upper)}`, notch 1 | — | −1 / 0 / 1 by exact comparison |
| `copysign(mag, sgn)` | all | `±\|mag\|` for the signs `sgn` can take, `mag`'s notch | — | `sgn == 0` counts as positive |
| `fmod(x, y)` | all | sign of `x` | `expected` (`division_by_zero`) when `y`'s grid holds 0 | truncated-division convention, exact |
| `pown<E>(x)` | all, `E ≥ 0` compile-time | corner-widened per multiply | per the checked-exact rules | repeated squaring in inside-space |

These work on grids past 64 bits too.

## Roots

| Function | Domain | Errors | Notes |
|---|---|---|---|
| `sqrt(x)` (`Lower ≥ 0`) | non-negative grids | — | exact integer square root |
| `sqrt(x)` (`Lower < 0`) | grids crossing 0 | `expected`; `domain_error` for a value < 0 | |
| `cbrt(x)` | all | — | exact integer cube root |
| `hypot(x, y)` | all | — | `√(x²+y²)` formed exactly |

## Trigonometric (radians)

| Function | Domain | Deduced output | Errors |
|---|---|---|---|
| `sin(x)` / `cos(x)` | all | `[-1, 1]` | — |
| `tan(x)` | all | `[-1024, 1024]` | `expected`; `overflow` past `Out` (saturates when `Out` carries `clamp`) |
| `atan(x)` | all | `(-π/2, π/2)` rounded out | — |
| `asin(x)` / `acos(x)` | `[-1, 1]` | `[-π/2, π/2]` / `[0, π]` rounded out | — |
| `atan2(y, x)` | all | `[-π, π]` rounded out | — (`atan2(0, 0) = 0`) |

## Hyperbolic

| Function | Domain | Errors |
|---|---|---|
| `sinh(x)` / `cosh(x)` / `tanh(x)` | all (deduced forms: `\|x\| ≤ 4096`) | — |
| `asinh(x)` | all | — |
| `acosh(x)` | `x ≥ 1` | — |
| `atanh(x)` | `-1 < x < 1` | — |

## Exponential & logarithmic

| Function | Domain | Errors | Notes |
|---|---|---|---|
| `exp(x)` / `exp2(x)` | all (deduced forms: `\|x\| ≤ 4096`) | — | |
| `log(x)` / `log2(x)` / `log10(x)` | `x > 0` | — | `log2` / `log10` of an exact power are exact |
| `pow_base<B>(x)` | integer `B ≥ 2` | — | `B` compile-time |
| `pow(base, exp)` | deduced form: `Lower(base) > 0` | `expected`; `domain_error` for a base ≤ 0, `overflow` past `Out` (a `clamp` `Out` saturates) | integer exponents give exact powers |

## Constants

`math::pi` and `math::two_pi` are point insides (`just<…>`), so they compose
directly in inside-space: `angle * math::two_pi`.

## Angles over many turns

No rational notch divides 2π, so how an angle survives many revolutions depends
on how it is carried:

- **Don't `wrap` a radians angle.** `wrap` folds modulo the grid's period,
  `span + notch`, which is never exactly 2π. `inside<{{0, 6.28125}, per<64>}, wrap>`
  folds every 6.296875 rad, about 0.014 rad more than a turn, so each revolution
  shifts the angle.
- **Unwrapped radians are exact.** Accumulated on its grid (`x += step`), the angle
  stays an exact grid value, and `sin`/`cos`/`tan` reduce any argument exactly
  enough, so the result carries only the output grid's rounding — no drift,
  however many turns.
- **Wrapping phase: carry it in turns.** A phase in turns has the exact period 1,
  so it wraps without drift. Convert at the call; the conversion error is the
  snap onto the angle grid plus `math::two_pi`'s own error (it is the rational
  convergent 2·1068966896/340262731, |error| ≈ 6·10⁻¹⁸), and it never accumulates:

```cpp
using turn_t  = inside<{{0, 1 - 1.0 / 4096}, per<4096>}, wrap | round_nearest>;
using angle_t = inside<{{0, 8}, per<16384>}, round_nearest>;

turn_t phase{0};
phase += turn_t{0.25};                                   // a quarter turn, exactly
auto s = math::sin_into<math::amp<16384>>(angle_t{phase * math::two_pi});
```

## Using `expected` results

```cpp
auto t = math::tan(angle{1});          // expected<inside, errc>
if (t) use(*t);

auto r = math::sqrt(signed_in{v});     // mixed-sign → expected
auto p = math::pow(base, exponent);    // expected
```

Expected results compose with arithmetic directly — the chain stays an
`expected` and the first error wins:

```cpp
auto r = math::sqrt(signed_in{v}) * gain + offset;   // expected<inside, errc>
```

See [arithmetic.md](arithmetic.md) for the chaining rules and
[internals.md](internals.md#7-error-vocabulary) for the error vocabulary.

## Storage

Any storage works for inputs and outputs: integer index or value raws, `f64` /
`f32`, `exact` rationals, and wide raws past 64 bits. An output with `f64`
storage receives the correctly rounded grid point as its double. `f64` is no
longer needed for speed: the double tier reads any input as a double and
stores integer outputs as raws.

## Speed

Against the double engine this library shipped before (ns per call, x86-64,
`-O2`/`-O3 -mfma`, one core pinned). The old engine was not correctly rounded;
on grids finer than about 2^-36 it missed notches routinely.

| Inputs → outputs | New / old time |
|---|---|
| `f64` grids on both sides, notch 2^-14 (`bench.cpp`, the old engine's best case: its store was the raw double) | 1.5–3.9× (`sin` 4.8 → 9.9 ns, `exp` 7.3 → 12.5 ns, `sqrt` 1.9 → 6.3 ns) |
| integer-backed outputs, notch 2^-20 | 0.12–0.59× (faster: the old engine's double → grid store was the slow part) |
| decimal outputs (notch 10^-6) | 0.06–0.25× (faster) |
| outputs past the double tier: 2^-40 | 1.1–4.3× (acosh the slowest) |
| 2^-52 | 1.5–4.7× |
| `pow_base<10>` onto a 44-bit output (10^9 on a 2^-14 grid) | about 21× (15.7 → 328 ns): the worst case |

Inputs of up to 256 slots use the table path and cost one load. Each table adds
about 0.13 s of compile time on GCC; `BEMAN_INSIDE_MATH_TABLE_SLOTS=0` turns
tables off. The tables in [performance.md](performance.md) are the current
`bench.cpp` numbers.

## Compiling without floating point (`BEMAN_INSIDE_MATH_NO_FP`)

On a target with no hardware FPU and no `<cmath>`, define
**`BEMAN_INSIDE_MATH_NO_FP`** (or configure CMake with
`-DBEMAN_INSIDE_MATH_NO_FP=ON`). The double tier — and its `#include <cmath>` —
compiles out; the integer path computes every result, and **results do not
change**. `f64` / `f32` storage falls back to integers.

- No `<cmath>` is referenced anywhere in the library when the macro is set, in
  the modular headers and the single header. A CI smoke
  (`single_header_nofp_smoke`) compiles the single header with a *poison*
  `<cmath>` first on the include path.
- **Auto-enabled** when `__STDC_HOSTED__ == 0` (i.e. `-ffreestanding`).

```bash
g++ -std=c++23 -ffreestanding -I single_include my_app.cpp              # NO_FP auto-on
g++ -std=c++23 -DBEMAN_INSIDE_MATH_NO_FP -I single_include my_app.cpp   # or force it
```
