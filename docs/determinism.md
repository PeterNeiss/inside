# Determinism & reproducibility

`inside` is built for results you can reproduce: same inputs → same bits, across
compilers, optimisation levels, and machines. This matters for fuzzing corpora,
record-and-replay, deterministic simulation, lockstep networking, and regression
baselines. This page states exactly what is guaranteed and under which
conditions.

## TL;DR

| Layer | Reproducible? | Condition |
|---|---|---|
| Integer & rational storage / `+ − × ÷` | **Always** | none — fixed-width `int64`, exact rational |
| `f64` / `f32` (float-backed) storage & arithmetic | **Yes** | IEEE-754 binary64/binary32, round-to-nearest (`-ffast-math` is rejected at compile time) |
| `beman::inside::math` transcendentals | **Always** | none — every result is the correctly rounded grid point, the same at compile time, at runtime, with or without an FPU |
| Compile-time constants & coefficients | **Always** | `constexpr`, no external codegen |

A transcendental has exactly one correct answer on a given output grid, and the
engine returns it. Nothing about the build — compiler, flags, FPU, constant
evaluation — can change it, and an x86 host and a soft-float core agree bit for
bit.

## The integer & rational core is deterministic by construction

Every inside without `f64`/`f32` storage holds a fixed-width integer index/value, and
all of its arithmetic is integer or exact-rational. There is no floating point on
these paths, so there is nothing for the platform to round differently:

- Widths are fixed: `using umax = std::uint64_t; using imax = std::int64_t;`
  (`include/beman/inside/math.hpp`). The value path is `imax` everywhere.
- Exact fractions use `beman::inside::rational` (`detail/rational.hpp`); every
  checked operation routes through overflow-detecting `add/sub/mul`
  (`detail/overflow.hpp`), so an overflow becomes a reported `errc::overflow`
  rather than a platform-dependent wrap.
- `rational(double)` decomposes the value straight from its IEEE-754 bits — a
  finite `double` is exactly `significand · 2^exp2` — with "no `<cmath>`, no FPU
  rounding, bit-identical across platforms" (`math.hpp`, `abs_fraction`).

Two builds on two architectures that take an integer/rational path produce the
same bits, period.

## The `f64` (double-backed) path

An `f64` inside holds its value as an
IEEE-754 `double`. The flag is storage only: every result is the one the same
type gives without it (a test compares the two for every operation). It is
only ever selected on a **`double_exact`** grid —
dyadic *and* every on-grid value within the 53-bit significand (see
[math.md](math.md#storage) and
[storage.md](storage.md#choosing-the-representation)). The same reasoning
applies to `f32` with binary32's 24-bit significand. Consequences for
determinism:

- On-grid values are *exactly* representable, so storing/loading is lossless.
- `detail::snap_double` (`include/beman/inside/grid.hpp`) rounds by the policy's
  mode (ties of `round_nearest` half away from zero), stays `constexpr` and
  `<cmath>`-free, and narrows to `imax` only when provably safe — the same
  rounding rule as integer and rational storage.
- On-grid `+ − ×` whose exact result still fits the result grid are computed
  exactly; an operation whose result `double` *cannot* represent — a finer
  grid, or a continuous quotient — **drops the `f64` flag** and stores the
  result in exact (rational/integer) storage, so `f64` never changes a result.

**Condition.** IEEE-754 correctly-rounded `+ − × ÷` are deterministic given:
round-to-nearest-even (the default), IEEE-754 binary64, and no value-changing
rewrites. A build with `-ffast-math`, GCC's `-fassociative-math` or
`-ffinite-math-only` (which define `__FAST_MATH__`, `__ASSOCIATIVE_MATH__` or
`__FINITE_MATH_ONLY__`) stops with an `#error` at the first include. On 32-bit x86, compile for SSE2 —
the legacy x87 stack evaluates at 80-bit extended precision and will not match.

## `beman::inside::math`: correctly rounded, so reproducible

`beman::inside::math` (`sin`/`cos`/`tan`/`exp`/`log`/`sqrt`/…) returns the
**correctly rounded point of the output grid** under the output's rounding mode
(see [math.md](math.md#how-it-works)). A correctly rounded result is a function
of the input value and the output grid alone, so it cannot depend on how it was
computed: the fast tiers (tables, double and dd kernels) return a slot only when
their error bound proves it is the one the integer path returns, and the integer
path — the only one at compile time and without an FPU — uses no `<cmath>`, no
runtime tables and no generated code.

The FP tiers assume IEEE-754 binary64 in round-to-nearest (`fesetround` is
outside the guarantee). Explicit `std::fma` and a 1.5× bound margin make them
independent of FMA contraction; the dd tier's error-free sums are fenced with
`__builtin_assoc_barrier`. Fast-math builds are rejected (above); Clang's bare
`-fassociative-math` (no macro), x87 and flush-to-zero builds are outside the
guarantee. `BEMAN_INSIDE_MATH_NO_FP` removes both tiers, with the same results.
[math.md](math.md#where-correctness-comes-first) lists each place where the
engine chose correctness over speed.

A rounding boundary can hold a transcendental value only at exact rational
inputs (`exp(0)`, `sqrt(9/4)`, …), which the engine stores exactly. For every
other input it raises the precision until the result is decided, up to a cap of
four times the start precision. Past the cap it takes the nearest slot; that
needs an irrational value within 2^-4W of a boundary, where W is the start
precision, and no test has ever reached it.

## Value stability over time

Determinism across platforms is half of the promise; the other half is that a
given input keeps producing the same bits **from one release to the next**, so
stored results, golden files and replay logs stay valid after an upgrade.

- The transcendental values are pinned by exact-value tests
  (`tests/beman/inside/determinism.test.cpp`, `math_adaptive.test.cpp`,
  `constexpr.test.cpp`). They are the contract: a change that alters any of them
  does not merge silently.
- Values change only deliberately, in a documented release that re-pins the
  affected tests and lists the change here by function. Algebraic results
  (`+ − × ÷`, conversions, rounding) never change. No value has changed since
  0.1.0.

## Compile-time determinism

Coefficients and constants (π, ln 2, ln 10, the series tables) are derived at
compile time from exact integer series with `constexpr` arithmetic, with **no
external code generators**. Nothing is baked by a separate tool that could drift
from the source. Every math function is `constexpr`, and a result computed at
compile time equals the runtime one.

## Checklist for reproducible builds

- Transcendentals need nothing: they are correctly rounded in every build.
- For `f64` / `f32` storage and arithmetic: keep round-to-nearest-even and
  target IEEE-754 binary64 (on 32-bit x86 use SSE2, not x87). `-ffast-math`
  and the flags that announce themselves are rejected; don't pass Clang's
  `-fassociative-math` or `-funsafe-math-optimizations`, which do not.
- FMA contraction is handled internally (explicit `std::fma`), so `-mfma` is safe;
  `BEMAN_INSIDE_FMA` (default `ON`) adds it on x86-64 and the results are identical.
- Toolchain: GCC 14+, Clang 19+, or Clang 18 with libc++, in C++23 mode (MSVC is not supported).
- To keep floating point out of the picture entirely, build with
  `-DBEMAN_INSIDE_MATH_NO_FP=ON`.

## Where to go next

| You want to… | Read |
|---|---|
| call sin/cos/sqrt/… | [math.md](math.md) |
| understand `f64` / double-backed storage | [storage.md](storage.md) |
| pick fast grids (fixed-point) | [fixed-point.md](fixed-point.md) |
| know *why* it's shaped this way | [internals.md](internals.md) |
