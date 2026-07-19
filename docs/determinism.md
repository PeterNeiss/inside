# Determinism & reproducibility

`bound` is built for results you can reproduce: same inputs → same bits, across
compilers, optimisation levels, and machines. This matters for fuzzing corpora,
record-and-replay, deterministic simulation, lockstep networking, and regression
baselines. This page states exactly what is guaranteed and under which
conditions.

(The alternative — trusting that arithmetic just works and that everyone gets
the same answer — is how you end up at [xkcd #2030, "Voting Software"](https://xkcd.com/2030/):
"our entire field is bad at what we do, and if you rely on us, everyone will
die." Reproducibility is the antidote.)

## TL;DR

| Layer | Reproducible? | Condition |
|---|---|---|
| Integer & rational storage / `+ − × ÷` | **Always** | none — fixed-width `int64`, exact rational |
| `f64` / `f32` (float-backed) storage & arithmetic | **Yes** | IEEE-754 binary64/binary32, round-to-nearest, no `-ffast-math` |
| `bnd::math` — default `double` engine | **Yes** | same as above |
| `bnd::math` — `float` engine (`-DBOUND_MATH_FLOAT=ON`) | **Yes** | IEEE-754 binary32, round-to-nearest, no `-ffast-math` |
| `bnd::math` — integer/CORDIC engine (`-DBOUND_MATH_FIXED=ON`) | **Yes, unconditionally** | any platform, any flags, no FPU |
| Compile-time constants & coefficients | **Always** | `constexpr`, no external codegen |

If you need bit-identical output on heterogeneous targets with *no* build-flag
assumptions (e.g. an x86 host replaying a soft-float embedded core), build with
`-DBOUND_MATH_FIXED=ON`. Otherwise the default engine is reproducible on every
conforming IEEE-754 platform built without `-ffast-math`.

> **"Reproducible" means per engine.** Each `bnd::math` engine is bit-identical
> within itself, but the three engines (`dbl` / `flt` / `cordic`) are **not**
> value-identical to *each other* — their transcendentals can differ by a notch
> or two, so switching engines is not value-preserving. See
> [The engines are not value-identical](#the-engines-are-not-value-identical-switching-engines-changes-results).

## The integer & rational core is deterministic by construction

Every bound without `f64`/`f32` storage holds a fixed-width integer index/value, and
all of its arithmetic is integer or exact-rational. There is no floating point on
these paths, so there is nothing for the platform to round differently:

- Widths are fixed: `using umax = std::uint64_t; using imax = std::int64_t;`
  (`include/bound/math.hpp`). The value path is `imax` everywhere.
- Exact fractions use `bnd::detail::rational` (`detail/rational.hpp`); every
  checked operation routes through overflow-detecting `add/sub/mul`
  (`detail/overflow.hpp`), so an overflow becomes a reported `errc::overflow`
  rather than a platform-dependent wrap.
- `rational(double)` decomposes the value straight from its IEEE-754 bits — a
  finite `double` is exactly `significand · 2^exp2` — with "no `<cmath>`, no FPU
  rounding, bit-identical across platforms" (`math.hpp`, `abs_fraction`).

Two builds on two architectures that take an integer/rational path produce the
same bits, period.

## The `f64` (double-backed) path

An `f64` bound (`real` is the deprecated spelling) holds its value as an
IEEE-754 `double`. It is only ever selected on a **`double_exact`** grid —
dyadic *and* every on-grid value within the 53-bit significand (see
[math.md](math.md#the-snap-requirement-and-f64-as-a-fast-storage-option) and
[storage.md](storage.md#choosing-the-representation)). The same reasoning
applies to `f32` with binary32's 24-bit significand. Consequences for
determinism:

- On-grid values are *exactly* representable, so storing/loading is lossless.
- `grid::snap_double` (`include/bound/grid.hpp`) rounds half-away-from-zero,
  stays `constexpr` and `<cmath>`-free, and narrows to `imax` only when provably
  safe — the same rounding rule as the integer engine.
- On-grid `+ − ×` whose exact result still fits the result grid are computed
  exactly; an operation whose result `double` *cannot* represent **drops the
  `f64` flag** and stores the result in exact (rational/integer) storage, so
  `f64` math never silently diverges from the exact grid arithmetic.

**Condition.** IEEE-754 correctly-rounded `+ − × ÷` are deterministic given:
round-to-nearest-even (the default), IEEE-754 binary64, and **no `-ffast-math`**
(which permits value-changing reassociation). On 32-bit x86, compile for SSE2 —
the legacy x87 stack evaluates at 80-bit extended precision and will not match.

## The three `bnd::math` engines

`bnd::math` (`sin`/`cos`/`tan`/`exp`/`log`/`sqrt`/…) ships **one API over three
engines** (`dbl` / `flt` / `cordic`, all reachable by namespace; one is the
build default) — each reproducible; they differ only in how strong the
guarantee is.

### Default — the `double` engine

> bnd::math double engine — a small, reproducible libm in `double`. Bit-identical
> on every IEEE-754 binary64 platform compiled without `-ffast-math`, via:
> * NO `<cmath>` transcendentals — sin/cos/exp/log are fixed polynomials here;
>   only `std::fma`/`sqrt`/`nearbyint` (well-defined) and the constexpr ldexp.
> * Horner evaluation with explicit `std::fma` (immune to FMA-contraction).
> * Cody-Waite range reduction for full-precision args.
>
> — `include/bound/cmath_double.hpp`

It does **not** call the platform `libm` for transcendentals (those vary
bit-to-bit between vendors); it evaluates its own fixed polynomials using only
the three IEEE-754-well-defined primitives. Explicit `std::fma` removes the
"did the compiler contract `a*b+c`?" ambiguity. Fast (~ns), needs an FPU, runs at
runtime.

### `-DBOUND_MATH_FLOAT=ON` — the `float` engine

The same construction in single precision: fixed polynomials, explicit
`std::fma(float)`, its own compile-time-derived range-reduction constants.
**Bit-identical on every IEEE-754 binary32 platform** under the same conditions
as the double engine. It exists for single-precision-only FPUs (Cortex-M4F and
similar) — see [math.md](math.md#the-flt-binary32-engine).

### `-DBOUND_MATH_FIXED=ON` — the integer/CORDIC engine

The reproducibility contract, verbatim from `include/bound/cmath.hpp`:

> Every function below produces bit-identical output for the same input across
> compiler, platform, optimisation level, and FP flags — relied on for fuzzing
> corpora, record-and-replay, deterministic simulation, regression testing.
> Requirements:
> 1. NO `<cmath>`/FPU/intrinsics; hot paths are integer-only over int64.
> 2. NO runtime-derived tables — coefficients derived at compile time from
>    `rational` literals, quantized to integer Q-format via constexpr rounding.
> 3. NO external code generators; derivation is constexpr C++.
> 4. C++20+ (well-defined signed right-shift semantics).
> 5. Each transcendental ships checked-in `static_assert` vectors pinning its
>    bit-exact output.

This engine is **unconditionally** bit-identical — any platform, any flags, no
FPU required — at the cost of speed. Use it for embedded / soft-float targets or
when you must match results across a heterogeneous fleet.

All engines write to the same auto-deduced output grids, so each is reproducible
and correct to within the grid — but that does **not** mean two engines produce
the *same* grid value for every input (see below).

### The engines are not value-identical (switching engines changes results)

Each engine is deterministic **per engine** — bit-identical for a given engine
across platform, compiler, optimisation level, and FP flags. But the engines
are **independent approximations** of the same irrational result, so for some
inputs they land on **adjacent notches**: any two engines can differ by a notch
(a couple on fine grids under `flt`) — a ULP or two of the output grid.

**Why — the table-maker's dilemma.** When the exact mathematical result falls
extremely close to the midpoint between two grid points, each engine's tiny
(sub-notch) approximation error can tip round-to-nearest to the *opposite*
neighbour. No finite working precision rules this out for every transcendental —
guaranteeing a single correctly-rounded value in all cases is the open,
unbounded-cost table-maker's dilemma, so the library does not promise it.

*Example.* `sinh(4)` on a `notch<1, 4096>` grid is `111779.5008…` — only `0.0008`
of a notch above the midpoint `111779.5`. The default `double` engine rounds up to
`111780/4096`; the integer/CORDIC engine rounds down to `111779/4096`. Both are
within the grid's resolution of the true value; they simply disagree by one notch.

**Consequence — switching engines is not value-preserving.** You **cannot** rebuild
with another engine and expect bit-identical results: toggling `-DBOUND_MATH_FIXED`
or `-DBOUND_MATH_FLOAT` can change individual transcendental values by a notch.
Golden vectors, record-and-replay corpora, lockstep peers, and any cross-build
comparison are valid **within a single engine only** — pick one engine for any
dataset that must stay bit-comparable, and never mix outputs from different
engines. (Algebraic results —
`+ − × ÷`, conversions, rounding — *are* identical across engines; this caveat is
specific to the transcendental `bnd::math` functions.)

## Compile-time determinism

Coefficients and constants are derived at compile time from `rational` literals
via `constexpr` rounding, with **no external code generators** — the derivation
is plain `constexpr` C++ (`cmath.hpp`). Nothing is baked by a separate tool that
could drift from the source.

## Checklist for reproducible builds

- Build **without** `-ffast-math` / `-funsafe-math-optimizations`.
- Keep the default rounding mode (round-to-nearest-even).
- Target IEEE-754 binary64; on 32-bit x86 use SSE2, not x87.
- FMA contraction is handled internally (explicit `std::fma`), so `-mfma` is safe.
- For a guarantee that survives *any* of the above being wrong, build with
  `-DBOUND_MATH_FIXED=ON`.

## Where to go next

| You want to… | Read |
|---|---|
| call sin/cos/sqrt/… and pick an engine | [math.md](math.md) |
| understand `f64` / double-backed storage | [storage.md](storage.md) |
| pick fast grids (fixed-point) | [fixed-point.md](fixed-point.md) |
| know *why* it's shaped this way | [internals.md](internals.md) |
