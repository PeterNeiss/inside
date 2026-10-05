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

Each call takes the first of four paths that applies — all four give the same
grid point:

| Path | When | Cost |
|---|---|---|
| **Table** | the input has at most `BEMAN_INSIDE_MATH_TABLE_SLOTS` slots (default 256), the output stores an integer, `f64` or `f32` raw, and every result lies in the output's range | one load; the table is computed at compile time |
| **Double tier** | an FPU is present (not `BEMAN_INSIDE_MATH_NO_FP`), the output's value indices stay within ±2^52, and it needs no more bits than the kernel's limit (42–49, by function) | the library's own double kernels, sized to the output, plus a proved error bound; decided results are stored as raws |
| **dd tier** | an FPU is present, the output needs more than 36 bits, and its value indices stay within ±2^62; where the double tier also applies, it runs second | double-double kernels (about 106 bits) from compile-time tables, plus an error bound |
| **Integer path** | always available; the only path at compile time and without an FPU | Taylor polynomials with compile-time coefficient tables in wide fixed point |

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
at the output's largest values: 48 for `sin`, `cos` and `exp`, 47 for `log`
and `atan`, 42 for `pow` and `cbrt`, whose bounds grow with |ln x|. Whenever
the bound does not place the result in a single slot, the dd tier or the
integer path decides. Tests check every kernel against its bound and that the
tiers agree slot for slot.

The dd tier works the same way with values carried as a sum of two doubles. Its
kernels use table-driven reductions (2^(i/64)·2^(j/4096) for exp, sin(jπ/128),
atan(j/32)) and Newton steps for log, sqrt and cbrt. Every table and coefficient
comes at compile time from the integer path's series, computed only in
translation units that use the tier. Its bound is 2^-88 of the result plus
2^-92·max(1, |x|), with the same condition-number terms. A test checks that the
kernels' errors against the integer path at 150 bits stay at least 2^8 below
this bound; the measured worst is about 2^-96 relative.

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
| `f64` grids on both sides, notch 2^-14 (the old engine's best case: its store was the raw double) | 0.67–1.06×, geometric mean 0.84 (`sin` 9.3 → 8.2 ns, `exp` 13.3 → 11.0 ns) |
| integer-backed outputs, notch 2^-20, dyadic or decimal inputs | 0.05–0.18× (faster: the old engine's double → grid store was the slow part) |
| decimal outputs (notch 10^-6) | 0.02–0.07× (faster) |
| integer-backed outputs, notch 2^-40 (the double tier near its limit, else the dd tier) | 0.05–1.09× (`sin` 113 → 5.9 ns, `log` 115 → 8.1 ns, `exp` 59 → 10.0 ns; `acosh`, still in the dd tier, the slowest) |
| 2^-52, value indices up to 2^62 (the dd tier) | 0.20–0.72× |
| `pow_base<10>` onto a 44-bit output (10^9 on a 2^-14 grid) | 0.47× (31.0 → 14.5 ns) |

These were measured together on one core at a reduced clock, so the absolute
times are about twice those in [performance.md](performance.md).

Against all three engines this library shipped before (`dbl`, `flt` and the
integer `cordic`, built from commit 0f68ee8): all 22 functions they had, each
from an input of the same storage onto the output named, geometric mean of
new / old time (range in parentheses). None of the three rounded correctly; `flt`
cannot resolve a 2^-40 grid at all.

| Output | vs `dbl` | vs `flt` | vs `cordic` |
|---|---|---|---|
| `f64`, notch 2^-14 | 0.84 (0.67–1.06) | 0.87 (0.75–1.02) | 0.09 (0.05–0.15) |
| `f32`, notch 2^-8 | 0.83 (0.69–1.02) | 0.93 (0.80–1.08) | 0.11 (0.06–0.19) |
| integer index, notch 2^-20 | 0.09 (0.05–0.18) | 0.16 (0.05–0.24) | 0.06 (0.02–0.10) |
| decimal, notch 10^-6 | 0.04 (0.02–0.07) | 0.13 (0.04–0.21) | 0.06 (0.02–0.11) |
| integer index, notch 2^-40 | 0.15 (0.05–1.09) | 0.18 (0.05–1.20) | 0.09 (0.02–0.46) |
| `exact` (rational), notch 10^-6 | 0.47 (0.38–0.70) | 0.71 (0.60–0.94) | 0.53 (0.35–0.75) |

The rows slower than an old engine are within 20%: `acosh` onto 2^-40, which
stays in the dd tier, `hypot` on `f64` and `f32` grids, and a few `f32` rows
against `flt`'s float polynomials.

Inputs of up to 256 slots use the table path and cost one load, onto integer
and floating-point outputs alike (`sin` of a 129-slot input onto an `f64`
grid: 6.8 → 2.2 ns, `cbrt` 14.9 → 2.2 ns). Each table adds
about 0.13 s of compile time on GCC; `BEMAN_INSIDE_MATH_TABLE_SLOTS=0` turns
tables off. The tables in [performance.md](performance.md) are the current
`bench.cpp` numbers.

## Where correctness comes first

The fast tiers exist only to return the integer path's answer sooner. Wherever
a faster choice could, under some build, input or storage, return a different
grid point, the engine takes the slower one. Each choice below gives up speed
measured on x86-64 with `-mfma`.

| Choice | What it guards against | What it costs |
|---|---|---|
| The double tier's bounds proved, not measured: every rounding of every step counted at its worst, times 1.5 | a kernel error the measurements missed (an input, a compiler, a platform) turning into a wrong slot | the proofs are worst cases, about 2–4 times the error measured, so outputs near a kernel's limit fall back to the dd tier for up to 13% of inputs |
| The dd tier's bound far wider than its kernels' measured error: 2^-88 + 2^-92·max(1, \|x\|) (measured at worst 2^-96), times 1.5 | the same, for kernels whose bounds are not proved | outputs past the double tier's limits take the dd tier, 2–9 times slower than the double tier |
| `-ffast-math`, `-fassociative-math` and `-ffinite-math-only` builds rejected with an `#error` | reassociation adding roundings the proofs did not count and breaking the error-free sums; NaN and infinity tests folded away | such builds do not compile |
| Strict decision tests: a value within the bound of a slot boundary, exact ties and exact grid points under directed rounding go to the integer path | a rounding the double arithmetic cannot settle | the integer path's time for those inputs; rare for irrational results, every time for exact ones such as `sqrt` of a perfect square under `round_floor` |
| `nearbyint` to round the value index, not adding and subtracting 1.5·2^52 | reassociation (Clang's `-fassociative-math`, which no macro announces) folding the add-subtract away; the tests then pass a non-integer and return a wrong slot | 0.1–0.4 ns per call (`sqrt` 1.65 → 2.05 ns, `hypot` 2.22 → 2.49 ns on `f64` grids) |
| Range checked in double before the index becomes an integer; no finite checks, since NaN and infinities fail every comparison | a huge or non-finite kernel value converted to an integer (undefined behaviour) | none measured |
| Inputs that are not doubles exactly (decimal notches, rational storage) add their conversion error times the function's slope to the bound | a decimal input's rounding moving a result across a boundary | slightly more fallbacks for those inputs |
| Error-free sums fenced against FMA contraction (`__builtin_assoc_barrier`), the error-free product's rounded part computed as `fma(a, b, +0)` | GCC's default `-ffp-contract=fast` fusing a rounded product into a later sum, which made the dd tier 1–2 notches wrong at `-O2` | about 2% more instructions in the dd tier, no measurable time |
| The dd tier only for value indices up to 2^62 | index arithmetic overflowing 64 bits | finer or wider outputs take the integer path |
| The dd kernels carry about 100 bits even when the output needs 50 | an undersized kernel for some output; one kernel per function, tested once | a kernel sized to the output could be cheaper for 49–80-bit outputs |
| Rational outputs store the reduced fraction j·p/q (one gcd); f32/f64 outputs store j·notch, exact on their dyadic grids | a stored value off the grid, or not in canonical form | about 60 ns per rational result |
| Every constant and table computed at compile time from the integer path's own series, never written out as literals | a constant that drifts from the series it should equal | compile time only: about 0.1–0.3 s in a translation unit that uses the dd tier, nothing in one that does not |

What the tiers never trade away: they return a result only when it provably
equals the integer path's, and the integer path is the only path at compile
time and without an FPU. A build cannot change a value; it can only change how
fast the value comes.

The remaining gaps to the old engines (`acosh` onto 2^-40, `hypot` and some
`f32` grids) are speed that can still be won without giving any of this up:
tighter proofs for `acosh`, `asinh`, `sinh`, `cbrt` and `pow`, whose bounds
grow with the input, would let them take finer outputs in the double tier.

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
