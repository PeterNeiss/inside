# `bnd::math` — reproducible math, one API, two engines

`bound/cmath.hpp` provides a `<cmath>`-shaped function set that operates on
`bound` values instead of `float`/`double`. There is **one public API** and
**two engines**, selected at build time — interchangeable at the source/API level,
but **not** value-for-value (see the engine caveat below):

| Engine | Selected by | Reproducibility | constexpr | Speed |
|---|---|---|---|---|
| **double** (default) | — | bit-identical on every IEEE-754 binary64 platform compiled without `-ffast-math` (round-to-nearest) | no | ~2× faster |
| **integer / CORDIC** | CMake `-DBOUND_MATH_FIXED=ON` (macro `BND_MATH_FIXED`) | bit-identical **unconditionally** — any platform, any flags, no FPU required | yes | embedded-friendly |

> The double engine's "constexpr: no" lifts automatically on C++26 toolchains
> with constexpr `<cmath>` (P1383, `__cpp_lib_constexpr_cmath`) — the gate is
> already in place.

The double engine evaluates its own fixed polynomials (`std::fma` Horner,
hex-float coefficients, Cody-Waite range reduction) plus the correctly-rounded
`std::sqrt` — no `<cmath>` transcendentals anywhere. The integer engine runs
Q.30 fixed-point CORDIC/Newton cores. Both snap results onto the same
auto-deduced output grid, so the two engines are **feature- and
signature-identical**: the same source compiles against either.

> Engine = speed/representation; grid = precision. The result **type** does not
> depend on the engine, and each engine is bit-reproducible across platforms.
> The grid-snapped **value**, however, can differ between the two engines by up to
> one notch on rare rounding ties (the table-maker's dilemma): the engines are
> independent approximations, so **switching engines is not value-preserving**.
> Don't mix or compare outputs from different engines — see
> [determinism.md](determinism.md) ("The two engines are not value-identical").
> (Algebraic ops — `+ − × ÷`, conversions, rounding — *are* identical across
> engines; only the transcendentals can differ.)

For the full reproducibility story across the whole library (not just
`bnd::math`), see [determinism.md](determinism.md).

```cpp
#include "bound/cmath.hpp"
using namespace bnd;

// Math operands carry the `real` policy (see below).
using angle = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;
auto s = math::sin(angle{1});       // amplitude bound in [-1, 1]
auto h = math::hypot(s, s);         // √(s²+s²), output grid auto-deduced
```

## The `snap` requirement (and `f64` as a fast storage option)

> **Naming:** the double-backed storage flag is now spelled **`f64`**; **`real`
> is a deprecated alias** kept for one release. The two are bit-identical, so the
> examples below (and existing code) compile under either spelling. New code
> should prefer `f64`.

A transcendental result is irrational and must be **rounded onto the operand's
grid**, so every transcendental operand must carry a policy that **permits
rounding** — i.e. the **`snap`** bit (`snap`, any `round_*` mode, or `f64`,
which implies `round_nearest`). Omitting it is a compile error. This is the only
hard requirement: `math::sin` etc. work on **any snap-capable grid**, including
plain integer grids and non-dyadic ones (e.g. a `notch<1,100>` money grid) —
the value is computed by the engine and snapped to the grid via exact rational
rounding.

`f64` is **not** required — it is an optional **storage** flag that buys speed:

- Under the default engine `real` selects **double-backed storage** on the
  bound's grid — the raw *is* the value, so input marshalling into the engine is
  free (the large speedup over integer-index I/O). Values still obey the grid:
  they snap to the notch on store. Out-of-range stores run the usual policy
  cascade (clamp / wrap / sentinel / checked report).
- Without `real`, a snap-capable grid still works — the engine's `double`/integer
  result is snapped to the grid through the assignment path (a touch slower; no
  double fast path). Use `real` when the grid is dyadic and you want the speed.
- Under `BND_MATH_FIXED` `real` is an ordinary `round_nearest` integer-backed
  bound — the source compiles unchanged.
- `real` requires a grid that is **exactly representable in `double`**: dyadic
  (power-of-two notch and Lower) **and** within the 53-bit significand — writing
  a value as `N·2^(−f)` with `f = log2(notch denominator)`, every on-grid value
  must satisfy `|N| < 2^53` (and `f ≤ 1022`, so no value is subnormal). That is
  exactly "the ULP at the largest value ≤ the notch", so the snap is lossless. A
  grid that is non-dyadic **or** too fine for `double` is a compile error
  (*"grid exceeds double's 53-bit significand — coarsen the notch/range or use
  `exact`"*).
- An operation whose **result** grid would exceed that bound automatically drops
  `real` and stores the result exactly (rational/integer), so `real` math never
  silently diverges from the exact grid arithmetic — it trades the double fast
  path for exactness only where `double` cannot represent the result.

Pure grid operations — `abs` / `floor` / `ceil` / `round` / `trunc` /
`fmod` — do **not** require `real`: they have no engine and act on any bound.

See [policies.md](policies.md#representation-flags) for `real` among the
other representation flags.

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
  return the bound directly. When an explicit `Out` carries `clamp`, a
  result that merely leaves `Out`'s interval **saturates** instead of
  erroring (poles and domain errors still error).
- **Precision.**
  - *Double engine:* the cores are accurate to ~1 ULP of `double`, then the
    result is quantized onto the output grid — so the stored value is within
    one notch of the true value.
  - *Integer engine:* the transcendental cores run in Q.30 fixed point
    (~1e-6–1e-9 depending on composition depth), then quantize onto the
    output grid.
  - Algebraically-exact results (e.g. `cbrt(8)`, `hypot(3,4)`, `pow(2,10)`)
    land exactly under both engines.
- **Input-range limits** below are engine-shared `static_assert` envelopes,
  kept identical for both engines so the same programs compile everywhere.
  The trig/root/atan limits (±2^20) come from the integer engine's working
  scale; the `exp`/`exp2`/`pow` limits are **output representability** —
  e.g. e^44's exact numerator exceeds any grid's integer range — and cannot
  widen without coupling them to the output grid.
- **constexpr.** The math functions are `constexpr` only under
  `BND_MATH_FIXED` (the double engine's `std::fma`/`std::sqrt` are runtime).
  The compile-time output-grid deduction uses the integer cores in **both**
  builds, so grids and types never depend on the engine.

## Algebraic tier (exact, no polynomials, no `real` needed)

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `abs(x)` | all | `[0, max\|·\|]` | — | exact |
| `floor(x)` / `ceil(x)` / `round(x)` / `trunc(x)` | all | integer notch | — | exact; `round` is half-away-from-zero |
| `fmod(x, y)` | `y` must not span 0 | sign of `x` | — | truncated-division convention, exact. Integer-backed operands on commensurable notches take a single-integer-remainder fast path (faster than `std::fmod`). |
| `pown<E>(x)` | all, `E ≥ 0` compile-time | corner-widened per multiply | optional per the checked-exact rules | repeated squaring in bound-space — exact, negative bases fine, no `real` needed |

## Roots

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sqrt(x)` (`Lower == 0`) | `[0, 4]`, notch `1/2^K`, `K ≤ 30` | `[0, ≈√Upper]` | — | correctly-rounded core, grid-snapped |
| `sqrt(x)` (`Lower < 0`) | `max\|·\| ≤ 4`, notch `1/2^K` | — | `expected`; `domain_error` if value < 0 | mixed-sign overload |
| `cbrt(x)` | `\|x\| ≤ 2^20` | monotone range | — | `sign(x)·2^(log2\|x\|/3)` |
| `hypot(x, y)` | `\|x\|,\|y\| ≤ 2^20` | `[0, √(maxX²+maxY²)]` | — | no internal overflow inside the domain |

## Trigonometric (radians)

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sin(x)` / `cos(x)` | `\|x\| ≤ 2^20` rad | `[-1, 1]` | — | grids beyond ±1024 rad use a two-term 1/2π reduction (fixed engine) |
| `tan(x)` | `\|x\| ≤ 2^20` rad | `[-1024, 1024]` | `expected`; `division_by_zero` at a pole, `overflow` past `Out` (saturates instead when `Out` carries `clamp`) | sin/cos ratio |
| `atan(x)` | `\|x\| ≤ 2^20` | `(-π/2, π/2)` | — | reciprocal reduction for \|x\| > 1 |
| `asin(x)` | `[-1, 1]` | `[-π/2, π/2]` | — | `atan2(x, √(1-x²))` |
| `acos(x)` | `[-1, 1]` | `[0, π]` | — | `π/2 - asin(x)` |
| `atan2(y, x)` | `\|y\|,\|x\| ≤ 2^20` | `[-π, π]` | — | quadrant-correct; normalized by max magnitude internally |

## Hyperbolic

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `sinh(x)` / `cosh(x)` / `tanh(x)` | `[-10, 10]` | monotone (cosh: even, min 1) | — | from `e^x` via the exp core |

## Exponential & logarithmic

| Function | Domain | Output | Errors | Notes |
|---|---|---|---|---|
| `exp(x)` / `exp2(x)` | `exp`: `[-20, 20]`, `exp2`: `[-30, 30]` | `≥ 0` | — | `exp = exp2(x·log2 e)` |
| `log(x)` / `log2(x)` / `log10(x)` | `x > 0` | monotone | — | |
| `pow_base<B>(x)` | integer `B ≥ 2` | `≥ 0` | — | `exp2(x·log2 B)`, `B` compile-time |
| `pow(base, exp)` | `Lower<base> > 0` | corner-deduced | `expected`; `overflow` if `exp·log2 base` leaves `[-30,30]` or result leaves `Out` | runtime base |

## Constants

`math::pi` and `math::two_pi` are point-bounds (`just<…>`), so they compose
directly in bound-space: `angle * math::two_pi`.

## Using `expected` results

```cpp
auto t = math::tan(angle{1});          // expected<bound, errc>
if (t) use(*t);
else if (t.error() == errc::division_by_zero) /* at a pole */;

auto r = math::sqrt(signed_in{v});     // mixed-sign → expected
auto p = math::pow(base, exponent);    // expected
```

Expected results compose with arithmetic directly — the chain stays an
`expected` and the first error wins:

```cpp
auto r = math::sqrt(signed_in{v}) * gain + offset;   // expected<bound, errc>
```

To drop the cause and enter the zero-cost `optional` chaining world instead,
convert with `bnd::ok(...)`:

```cpp
auto o = ok(math::tan(angle{x})) * gain;             // optional<bound>
```

See [arithmetic.md](arithmetic.md) for the bridge rules (error precedence,
the no-mixing compile error) and
[internals.md](internals.md#7-error-vocabulary) for the full error
vocabulary.

## Selecting the integer engine

```bash
cmake -B build-fixed -DBOUND_CXX20=ON -DBOUND_MATH_FIXED=ON
cmake --build build-fixed
```

Use it when you need bit-identical results across heterogeneous targets
(e.g. an x86 host and a soft-float embedded core), `constexpr` math, or an
FPU-free build. The default double engine is the right choice everywhere
else: it carries the same grid guarantees and is reproducible across IEEE-754
platforms compiled without `-ffast-math`.

## Choosing an engine per call (`cordic::` / `dbl::` / `flt::`)

The unqualified `bnd::math::fn` uses the build's default engine. All three engines
are also reachable by name, **callable side-by-side in the same binary**:

| Namespace | Engine | Availability |
|---|---|---|
| `bnd::math::cordic::fn` | integer / CORDIC | **always** (constexpr, FPU-free) |
| `bnd::math::dbl::fn` | `double` (binary64) | unless `BND_MATH_NO_FP` |
| `bnd::math::flt::fn` | `float` (binary32) | unless `BND_MATH_NO_FP` |
| `bnd::math::fn` | the default | `cordic` under `BND_MATH_FIXED`/`BND_MATH_NO_FP`; `flt` under `BND_MATH_FLOAT`; else `dbl` |

Select the unqualified default at build time: `-DBOUND_MATH_FIXED=ON` (integer),
`-DBOUND_MATH_FLOAT=ON` (binary32), or neither (binary64). The macro only changes
what the bare `bnd::math::fn` name means — `cordic::`/`dbl::`/`flt::` stay
individually reachable regardless.

The qualified entry points have the **same signatures, domains, auto-deduced
output grids, and domain `static_assert`s** as the unqualified one — only the
compute backend differs. This lets one program pick per call site:

```cpp
using A = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;

auto a = math::cordic::sin(A{1});   // bit-exact across every target — replay/sim
auto b = math::dbl::sin(A{1});      // ~2× faster — hot, accuracy-insensitive path
auto f = math::flt::sin(A{1});      // binary32 — single-precision FPUs (Cortex-M4F)
auto c = math::sin(A{1});           // whichever the build selected
```

Because the engines are independent approximations, they can disagree by a notch
or two on rounding ties (the table-maker's dilemma — see
[determinism.md](determinism.md)); algebraically-exact inputs (e.g. `sqrt(4)`,
`pow(2,4)`) land identically on all three. Under `BND_MATH_NO_FP` neither `dbl::`
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
  (`tests/test_math_engines.cpp`).

**Pair `flt` with `f32` storage.** An `f32`-backed operand holds a binary32 raw,
so `flt` reads it, computes, and stores the result straight in `float` — no
`double` round-trip. On a single-precision-only FPU that keeps the whole path in
hardware float; with `f64`/rational storage the boundary marshalling goes through
`double` (soft-float on such targets). Because binary32 has only a 24-bit
significand, an `f32` result grid that a function would overflow (e.g. `exp` of a
large argument on a fine grid) is a compile error — widen the grid or use `f64`.

## Compiling without floating point (`BND_MATH_NO_FP`)

On a target with no hardware FPU and no `<cmath>`, define **`BND_MATH_NO_FP`**
(any value). It compiles the double engine — and its `#include <cmath>` — out
**entirely**, leaving the always-present integer/CORDIC engine to serve the full
`bnd::math` API. The public surface, output grids, and types are unchanged: only
the compute backend differs.

- **No `<cmath>`, no `std::fma`/`std::sqrt`** are referenced anywhere in the
  library when the macro is set — this holds for the modular headers **and** the
  amalgamated single header (the `<cmath>` include is emitted under the same
  guard). A CI smoke (`single_header_nofp_smoke`) compiles the single header with
  a *poison* `<cmath>` shim first on the include path, so the build fails if any
  `<cmath>` is pulled in.
- **Auto-enabled** when `__STDC_HOSTED__ == 0` (i.e. `-ffreestanding`) and
  **implied by `BND_MATH_FIXED`** — selecting the integer engine is itself an
  FP-free build.
- All transcendentals are `constexpr` under `BND_MATH_NO_FP` (the integer engine),
  so they evaluate at compile time as well as runtime.

```bash
# bare-metal: integer engine, no <cmath>, single header
g++ -std=c++23 -ffreestanding -I single_include my_app.cpp     # NO_FP auto-on
g++ -std=c++23 -DBND_MATH_NO_FP -I single_include my_app.cpp   # or force it
```
