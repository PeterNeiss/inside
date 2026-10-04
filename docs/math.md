# `beman::inside::math` — reproducible math, one API, three engines

`beman/inside/cmath.hpp` provides a `<cmath>`-shaped function set that operates on
`inside` values instead of `float`/`double`. There is **one public API** and
**three engines** — interchangeable at the source/API level, but pairwise **not**
value-for-value (see the engine caveat below). One is picked as the build
default; all three stay callable by namespace in the same binary:

| Engine | Default when | Reproducibility | constexpr | Speed |
|---|---|---|---|---|
| **double** (binary64, default) | — | bit-identical on every IEEE-754 binary64 platform compiled without `-ffast-math` (round-to-nearest) | no | fastest — ~10–15× the CORDIC engine with `-mfma` (`sin` 5.8 vs 81 ns, `exp` 8.9 vs 121 ns) |
| **float** (binary32) | CMake `-DBEMAN_INSIDE_MATH_FLOAT=ON` (macro `BEMAN_INSIDE_MATH_FLOAT`) | bit-identical on every IEEE-754 binary32 platform (same contract as double) | no | single-precision FPUs (Cortex-M4F) |
| **integer / CORDIC** | CMake `-DBEMAN_INSIDE_MATH_CORDIC=ON` (macro `BEMAN_INSIDE_MATH_CORDIC`) | bit-identical **unconditionally** — any platform, any flags, no FPU required | yes | embedded-friendly |

> The double engine's "constexpr: no" lifts automatically on C++26 toolchains
> with constexpr `<cmath>` (P1383, `__cpp_lib_constexpr_cmath`) — the gate is
> already in place.

The double engine evaluates its own fixed polynomials (`std::fma` Horner,
hex-float coefficients, Cody-Waite range reduction) plus the correctly-rounded
`std::sqrt` — no `<cmath>` transcendentals anywhere. The float engine runs the
same polynomial shapes in single precision. The integer engine runs
fixed-point CORDIC/Newton cores at a working precision chosen per output grid
(see Precision below). All three snap results onto the same
auto-deduced output grid, so the engines are **feature- and
signature-identical**: the same source compiles against any of them.

On x86-64 the FP engines' speed depends on hardware FMA: without `-mfma`,
`std::fma` is a software-emulated libm call and each polynomial pays for it
(`math::sin` 20.8 ns → 5.1 ns with `-mfma`, bit-identical results). The CMake
option `BEMAN_INSIDE_FMA` (default `ON`) adds `-mfma` for x86-64 GCC/Clang —
see the README for the CPU requirement it implies.

> Engine = speed/representation; grid = precision. The result **type** does not
> depend on the engine, and each engine is bit-reproducible across platforms.
> The grid-snapped **value**, however, can differ between engines by up to
> a notch or two on rare rounding ties (the table-maker's dilemma): the engines are
> independent approximations, so **switching engines is not value-preserving**.
> Don't mix or compare outputs from different engines — see
> [determinism.md](determinism.md) ("The engines are not value-identical").
> (Algebraic ops — `+ − × ÷`, conversions, rounding — *are* identical across
> engines; only the transcendentals can differ.)

For the full reproducibility story across the whole library (not just
`beman::inside::math`), see [determinism.md](determinism.md).

```cpp
#include <beman/inside/cmath.hpp>
using namespace beman::inside;

// Math operands here carry the `f64` storage flag (optional — see below).
using angle = inside<{{-8, 8}, per<16384>}, round_nearest | f64>;
auto s = math::sin(angle{1});       // amplitude inside in [-1, 1]
auto h = math::hypot(s, s);         // √(s²+s²), output grid auto-deduced
```

## The `snap` requirement (and `f64` as a fast storage option)

A transcendental result is irrational and must be **rounded onto the operand's
grid**, so every transcendental operand must carry a policy that **permits
rounding** — i.e. the **`snap`** bit (`snap`, any `round_*` mode, or `f64`,
which implies `round_nearest`). Omitting it is a compile error. This is the only
hard requirement: `math::sin` etc. work on **any snap-capable grid**, including
plain integer grids and non-dyadic ones (e.g. a `per<100>` money grid) —
the value is computed by the engine and snapped to the grid via exact rational
rounding.

`f64` is **not** required — it is an optional **storage** flag that buys speed:

- Under the default engine `f64` selects **double-backed storage** on the
  inside's grid — the raw *is* the value, so input marshalling into the engine is
  free (the large speedup over integer-index I/O). Values still obey the grid:
  they snap to the notch on store. Out-of-range stores run the usual policy
  cascade (clamp / wrap / checked report).
- Without `f64`, a snap-capable grid still works — the engine's `double`/integer
  result is snapped to the grid through the assignment path (a touch slower; no
  double fast path). Use `f64` when the grid is dyadic and you want the speed.
- Under `BEMAN_INSIDE_MATH_CORDIC` `f64` is an ordinary `round_nearest` integer-backed
  inside — the source compiles unchanged.
- `f64` requires a grid that is **exactly representable in `double`**: dyadic
  (power-of-two notch and Lower) **and** within the 53-bit significand — writing
  a value as `N·2^(−f)` with `f = log2(notch denominator)`, every on-grid value
  must satisfy `|N| < 2^53` (and `f ≤ 1022`, so no value is subnormal). That is
  exactly "the ULP at the largest value ≤ the notch", so the snap is lossless. A
  grid that is non-dyadic **or** too fine for `double` is a compile error
  (*"grid exceeds double's 53-bit significand — coarsen the notch/range or use
  `exact`"*).
- An operation whose **result** grid would exceed that inside automatically drops
  `f64` and stores the result exactly (rational/integer), so `f64` math never
  silently diverges from the exact grid arithmetic — it trades the double fast
  path for exactness only where `double` cannot represent the result.

Pure grid operations — `abs` / `floor` / `ceil` / `round` / `trunc` /
`fmod` — do **not** require `f64`: they have no engine and act on any inside.

See [policies.md](policies.md#representation-flags) for `f64` among the
other representation flags.

## Conventions

- **Angles are radians**, everywhere — `sin`/`cos`/`tan` take radians;
  `asin`/`acos`/`atan`/`atan2` return radians. (There is no turns-valued
  public API.)
- **Output grids are auto-deduced.** Calling `f(x)` with no explicit template
  argument deduces the result `inside` from the input's interval and notch:
  the interval is the function's true range over the input, rounded *outward*
  to the input's notch; the notch and policy are inherited (with
  `round_nearest` added, since transcendental results carry sub-notch drift).
- **Explicit output: `fn_into<Out>(x)`.** Every function has an `_into` form
  that takes the output type instead of deducing it — `math::sin_into<amp_t>(a)`,
  `math::pow_into<Out>(b, e)`, `math::pow_base_into<Out, 10>(x)` — in every
  engine namespace. `Out`'s own policy then does the final rounding, and a
  `clamp` on `Out` saturates instead of erroring.
- **Error model.** A domain limit that is knowable from the *type* is a
  `static_assert` (compile error). A failure that depends on the *runtime
  value* is reported through `std::expected<Out, errc>`. Total functions
  return the inside directly. When an explicit `Out` carries `clamp`, a
  result that merely leaves `Out`'s interval **saturates** instead of
  erroring (poles and domain errors still error).
- **Precision.**
  - *Double engine:* the cores are accurate to ~1 ULP of `double`, then the
    result is quantized onto the output grid — so the stored value is within
    one notch of the true value.
  - *Integer engine:* the transcendental cores run in fixed point at a scale
    2^W chosen from the output grid: W = notch bits + integer bits of the largest
    output + 6 guard bits (at least 12, at most 31). The composed functions
    (asin, acos, sinh, cosh, tanh, log10, cbrt, hypot) add 4 more guard bits,
    capped at 30. Coarse output grids therefore run fewer CORDIC/Newton steps.
    The result is then quantized onto the output grid. The compile-time
    deduction of the auto output grids always uses 30 bits.
  - Algebraically-exact results (e.g. `cbrt(8)`, `hypot(3,4)`, `pow(2,10)`)
    land exactly under every engine.
  - Measured per-function error tables for all three engines (max/mean in
    output-notch units, against a long-double reference) are in
    [accuracy.md](accuracy.md), regenerated by the `beman.inside.accuracy_report` build
    target.
- **Input-range limits** below are engine-shared `static_assert` envelopes,
  kept identical across all engines so the same programs compile everywhere.
  The trig/root/atan limits (±2^20) come from the integer engine's working
  scale; the `exp`/`exp2`/`pow` limits are **output representability** —
  e.g. e^44's exact numerator exceeds any grid's integer range — and cannot
  widen without coupling them to the output grid.
- **constexpr.** The math functions are `constexpr` only under
  `BEMAN_INSIDE_MATH_CORDIC` (the double engine's `std::fma`/`std::sqrt` are runtime).
  The compile-time output-grid deduction uses the integer cores in **every**
  build, so grids and types never depend on the engine.

## Algebraic tier (exact, no polynomials, no `f64` needed)

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `abs(x)` | all | `[0, max\|·\|]` | — | exact |
| `floor(x)` / `ceil(x)` / `round(x)` / `trunc(x)` | all | integer notch | — | exact; `round` is half-away-from-zero |
| `sign(x)` | all | `{sign(Lower), sign(Upper)}`, notch 1 | — | −1 / 0 / 1 by exact comparison (no decode) |
| `copysign(mag, sgn)` | all | `±\|mag\|` for the signs `sgn` can take, `mag`'s notch | — | `sgn == 0` counts as positive |
| `fmod(x, y)` | all | sign of `x` | `expected` (`division_by_zero`) when `y`'s grid holds 0; plain inside otherwise | truncated-division convention, exact. Integer-backed operands on commensurable notches take a single-integer-remainder fast path (faster than `std::fmod`). |
| `pown<E>(x)` | all, `E ≥ 0` compile-time | corner-widened per multiply | `expected` per the checked-exact rules | repeated squaring in inside-space — exact, negative bases fine, no `f64` needed |

## Roots

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sqrt(x)` (`Lower == 0`) | any non-negative grid (any notch) | `[0, ≈√Upper]` | — | correctly-rounded core, grid-snapped |
| `sqrt(x)` (`Lower < 0`) | any grid crossing 0 | `[0, ≈√Upper]` | `expected`; `domain_error` if value < 0 | mixed-sign overload |
| `cbrt(x)` | `\|x\| ≤ 2^20` | monotone range | — | `sign(x)·2^(log2\|x\|/3)` |
| `hypot(x, y)` | `\|x\|,\|y\| ≤ 2^20` | `[0, √(maxX²+maxY²)]` | — | no internal overflow inside the domain |

## Trigonometric (radians)

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sin(x)` / `cos(x)` | `\|x\| ≤ 2^20` rad | `[-1, 1]` | — | grids beyond ±1024 rad use a two-term 1/2π reduction (integer engine) |
| `tan(x)` | `\|x\| ≤ 2^20` rad | `[-1024, 1024]` | `expected`; `division_by_zero` at a pole, `overflow` past `Out` (saturates instead when `Out` carries `clamp`) | one range reduction (FP engines) / one CORDIC rotation (integer engine) for both sin and cos, then their ratio; poles are exact |
| `atan(x)` | `\|x\| ≤ 2^20` | `(-π/2, π/2)` | — | reciprocal reduction for \|x\| > 1 |
| `asin(x)` | `[-1, 1]` | `[-π/2, π/2]` | — | `atan2(x, √(1-x²))` |
| `acos(x)` | `[-1, 1]` | `[0, π]` | — | `π/2 - asin(x)` |
| `atan2(y, x)` | `\|y\|,\|x\| ≤ 2^20` | `[-π, π]` | — | quadrant-correct; normalized by max magnitude internally |

## Hyperbolic

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sinh(x)` / `cosh(x)` / `tanh(x)` | `[-10, 10]` | monotone (cosh: even, min 1) | — | from `e^x` via the exp core |
| `asinh(x)` | `\|x\| ≤ 2^20` | monotone | — | `sign·(ln\|x\| + ln(1 + √(1 + 1/x²)))` for \|x\| > 1 — no x² overflow |
| `acosh(x)` | `[1, 2^20]` | `[0, …]` | — | `ln x + ln(1 + √(1 − 1/x²))` |
| `atanh(x)` | `(-1, 1)`, at least 2^-30 from ±1 | monotone | — | `½·ln((1+\|x\|)/(1−\|x\|))` with the ratio formed exactly, so precision holds next to the poles |

## Exponential & logarithmic

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `exp(x)` / `exp2(x)` | `exp`: `[-20, 20]`, `exp2`: `[-30, 30]` | `≥ 0` | — | `exp = exp2(x·log2 e)` |
| `log(x)` / `log2(x)` / `log10(x)` | `x > 0` | monotone | — | |
| `pow_base<B>(x)` | integer `B ≥ 2` | `≥ 0` | — | `exp2(x·log2 B)`, `B` compile-time |
| `pow(base, exp)` | auto form: `lower_of<base> > 0` | corner-deduced | `expected`; `overflow` if `exp·log2 base` leaves `[-30,30]` or the result leaves `Out` (a `clamp` `Out` saturates); `pow_into` with a base grid reaching ≤ 0 also reports `domain_error` for a base ≤ 0 | runtime base |

## Constants

`math::pi` and `math::two_pi` are point insides (`just<…>`), so they compose
directly in inside-space: `angle * math::two_pi`.

## Periodic trig on a degree circle: `circle<M>` / `amp<K>`

Radians have no rational period, so a radians angle with `wrap` drifts. A
`circle<M>` is one revolution split into `M` equal slots, valued in degrees
(period 360), so `wrap` is exact; its raw is the slot index `0..M-1`. `amp<K>`
is the matching amplitude grid, `[-1, 1]` at resolution `1/K`:

```cpp
using angle_t = math::circle<4096>;   // f64 | wrap, notch 360/4096 degrees
using amp_t   = math::amp<32768>;     // [-1, 1], notch 1/32768, f64

angle_t phase{0};
auto s = math::sin(phase);                   // amp<4096>: the angle's resolution
auto c = math::cos_into<amp_t>(phase);       // explicit output grid
auto t = math::tan_into<amp_t>(phase);       // expected<amp_t, errc>
phase += angle_t{90};                        // a quarter turn, exactly; wraps at 360
```

The same names as the radians functions take a circle angle and dispatch on its
shape (`Lower` 0, `Upper + Notch == 360`, `wrap`): `sin` / `cos` / `tan` and their
`_into<Out>` forms. The auto output is `amp` at the angle's resolution, rounded up
to a power of two (`circle<360>` → `amp<512>`; an `f64` grid must be dyadic); `tan`'s
auto output is the radians `tan` range. Like the radians `tan`, `tan` returns
`expected`: `division_by_zero` at a pole, `overflow` past `Out` (a `clamp` `Out`
saturates). Only the integer engine detects the exact 90° / 270° poles: the FP
engines evaluate on the angle converted to radians, where `tan(90°)` is a large
finite value (then `overflow` for any amplitude grid). `M` must be divisible by 4
(a power of two is fastest), and a custom angle type must carry `wrap | f64`.
Under the integer engine the call is a lookup into a first-quadrant table built
at compile time. The engine namespaces (`cordic::`, `dbl::`, `flt::`) take radians
only.

## Using `expected` results

```cpp
auto t = math::tan(angle{1});          // expected<inside, errc>
if (t) use(*t);
else if (t.error() == errc::division_by_zero) /* at a pole */;

auto r = math::sqrt(signed_in{v});     // mixed-sign → expected
auto p = math::pow(base, exponent);    // expected
```

Expected results compose with arithmetic directly — the chain stays an
`expected` and the first error wins:

```cpp
auto r = math::sqrt(signed_in{v}) * gain + offset;   // expected<inside, errc>
```

Division and checked exact arithmetic use the same `expected` vocabulary, so
math results and arithmetic results chain together:

```cpp
auto q = math::tan(angle{x}) / divisor;              // expected<inside, errc>
```

See [arithmetic.md](arithmetic.md) for the chaining rules (error precedence) and
[internals.md](internals.md#7-error-vocabulary) for the full error
vocabulary.

## Selecting the integer engine

```bash
cmake --preset gcc-release -B build/gcc-cordic -DBEMAN_INSIDE_MATH_CORDIC=ON
cmake --build build/gcc-cordic
```

Use it when you need bit-identical results across heterogeneous targets
(e.g. an x86 host and a soft-float embedded core), `constexpr` math, or an
FPU-free build. The default double engine is the right choice everywhere
else: it carries the same grid guarantees and is reproducible across IEEE-754
platforms compiled without `-ffast-math`.

## Choosing an engine per call (`cordic::` / `dbl::` / `flt::`)

The unqualified `beman::inside::math::fn` uses the build's default engine. All three engines
are also reachable by name, **callable side-by-side in the same binary**:

| Namespace | Engine | Availability |
|---|---|---|
| `beman::inside::math::cordic::fn` | integer / CORDIC | **always** (constexpr, FPU-free) |
| `beman::inside::math::dbl::fn` | `double` (binary64) | unless `BEMAN_INSIDE_MATH_NO_FP` |
| `beman::inside::math::flt::fn` | `float` (binary32) | unless `BEMAN_INSIDE_MATH_NO_FP` |
| `beman::inside::math::fn` | the default | `cordic` under `BEMAN_INSIDE_MATH_CORDIC`/`BEMAN_INSIDE_MATH_NO_FP`; `flt` under `BEMAN_INSIDE_MATH_FLOAT`; else `dbl` |

`beman::inside::math::default_engine` is a namespace alias for the selected
engine; the unqualified functions are using-declarations of it.

Select the unqualified default at build time: `-DBEMAN_INSIDE_MATH_CORDIC=ON` (integer),
`-DBEMAN_INSIDE_MATH_FLOAT=ON` (binary32), or neither (binary64). The macro only changes
what the bare `beman::inside::math::fn` name means — `cordic::`/`dbl::`/`flt::` stay
individually reachable regardless.

The qualified entry points have the **same signatures, domains, auto-deduced
output grids, and domain `static_assert`s** as the unqualified one — only the
compute backend differs. This lets one program pick per call site:

```cpp
using A = inside<{{-8, 8}, per<16384>}, round_nearest | f64>;

auto a = math::cordic::sin(A{1});   // bit-exact across every target — replay/sim
auto b = math::dbl::sin(A{1});      // ~14× faster with -mfma — hot paths
auto f = math::flt::sin(A{1});      // binary32 — single-precision FPUs (Cortex-M4F)
auto c = math::sin(A{1});           // whichever the build selected
```

Because the engines are independent approximations, they can disagree by a notch
or two on rounding ties (the table-maker's dilemma — see
[determinism.md](determinism.md)); algebraically-exact inputs (e.g. `sqrt(4)`,
`pow(2,4)`) land identically on all three. Under `BEMAN_INSIDE_MATH_NO_FP` neither `dbl::`
nor `flt::` is defined, so a call to either there is a compile error; `cordic::`
always works.

### The `flt` (binary32) engine

`flt::` evaluates the same fixed polynomials as `dbl::` but in single precision,
with its own compile-time-derived Cody-Waite range-reduction constants and the
correctly-rounded `std::fma(float)`/`std::sqrt(float)` — so it is **bit-identical
on every IEEE-754 binary32 platform** (same determinism contract as `dbl`). It
exists for **single-precision-only FPUs** (Cortex-M4F and similar) and for
size/speed where double-grade precision isn't needed.

- It is a **third value set**: `float ≠ double ≠ cordic`. Snapped results differ
  from the double engine by up to a few notches on fine grids; on coarse grids
  (notch ≫ binary32 ULP) they typically coincide.
- It keeps the **shared input domain** (e.g. `sin`/`cos` over `|x| ≤ 2²⁰`): the
  constexpr split holds float reduction across that range (precision degrades
  toward the edge but stays float-grade), so the same programs compile on every
  engine.
- Precision: trig ≈ 1 ULP of `float`, `exp`/compositions a handful of ULP, then
  quantized onto the output grid. Ships its own golden pins
  (`tests/beman/inside/math_engines.test.cpp`).

**Pair `flt` with `f32` storage.** An `f32`-backed operand holds a binary32 raw,
so `flt` reads it, computes, and stores the result straight in `float` — no
`double` round-trip. On a single-precision-only FPU that keeps the whole path in
hardware float; with `f64`/rational storage the boundary marshalling goes through
`double` (soft-float on such targets). Because binary32 has only a 24-bit
significand, an `f32` grid too fine for `float` but representable in `double`
**auto-widens its storage to `f64`** (the value stays exact) — so a deduced `f32`
output whose grid overflows binary32 (e.g. `exp` of a large argument on a fine
grid) stores its result in `double` instead of failing to compile. Only a grid
too fine for `double` as well is a hard error (`exact` is the escape hatch).

## Compiling without floating point (`BEMAN_INSIDE_MATH_NO_FP`)

On a target with no hardware FPU and no `<cmath>`, define **`BEMAN_INSIDE_MATH_NO_FP`**
(any value). It compiles the double engine — and its `#include <cmath>` — out
**entirely**, leaving the always-present integer/CORDIC engine to serve the full
`beman::inside::math` API. The public surface, output grids, and types are unchanged: only
the compute backend differs.

- **No `<cmath>`, no `std::fma`/`std::sqrt`** are referenced anywhere in the
  library when the macro is set — this holds for the modular headers **and** the
  amalgamated single header (the `<cmath>` include is emitted under the same
  guard). A CI smoke (`single_header_nofp_smoke`) compiles the single header with
  a *poison* `<cmath>` shim first on the include path, so the build fails if any
  `<cmath>` is pulled in.
- **Auto-enabled** when `__STDC_HOSTED__ == 0` (i.e. `-ffreestanding`) and
  **implied by `BEMAN_INSIDE_MATH_CORDIC`** — selecting the integer engine is itself an
  FP-free build.
- All transcendentals are `constexpr` under `BEMAN_INSIDE_MATH_NO_FP` (the integer engine),
  so they evaluate at compile time as well as runtime.

```bash
# bare-metal: integer engine, no <cmath>, single header
g++ -std=c++23 -ffreestanding -I single_include my_app.cpp     # NO_FP auto-on
g++ -std=c++23 -DBEMAN_INSIDE_MATH_NO_FP -I single_include my_app.cpp   # or force it
```
