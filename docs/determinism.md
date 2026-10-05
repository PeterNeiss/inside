# Determinism & reproducibility

`inside` is built for results you can reproduce: same inputs → same bits, across
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
- Exact fractions use `beman::inside::detail::rational` (`detail/rational.hpp`); every
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
IEEE-754 `double`. It is only ever selected on a **`double_exact`** grid —
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
  exactly; an operation whose result `double` *cannot* represent **drops the
  `f64` flag** and stores the result in exact (rational/integer) storage, so
  `f64` math never silently diverges from the exact grid arithmetic.

**Condition.** IEEE-754 correctly-rounded `+ − × ÷` are deterministic given:
round-to-nearest-even (the default), IEEE-754 binary64, and **no `-ffast-math`**
(which permits value-changing reassociation). On 32-bit x86, compile for SSE2 —
the legacy x87 stack evaluates at 80-bit extended precision and will not match.

## `beman::inside::math`: correctly rounded, so reproducible

`beman::inside::math` (`sin`/`cos`/`tan`/`exp`/`log`/`sqrt`/…) returns the
**correctly rounded point of the output grid** under the output's rounding mode
(see [math.md](math.md#how-it-works)). A correctly rounded result is a function
of the input value and the output grid alone, so it cannot depend on how it was
computed:

- **Integer path** (always present; the only path at compile time and under
  `BEMAN_INSIDE_MATH_NO_FP`): integer-only fixed point, coefficients rounded at
  compile time from exact integer series, no `<cmath>`, no tables made at
  runtime, no external code generators. Bit-identical by construction.
- **Double tier** (when an FPU is present): the library's own double kernels —
  polynomials with explicit `std::fma` sized to the output, never the platform
  `libm` — give the value with an error bound proved at compile time from the
  kernel's coefficients, argument range and every rounding. When the bound
  places the result in one slot, that slot is the correctly rounded one;
  otherwise the dd tier or the integer path decides. So the tier only ever
  returns what the integer path would.
- **dd tier** (when an FPU is present, for outputs of more than 36 bits whose
  value indices stay within ±2^62): the same scheme with double-double
  kernels (about 106 bits), their tables and coefficients computed at compile
  time from the integer path's series, and a bound of 2^-88 of the result plus
  2^-92·max(1, |x|).
- **Tables** (small inputs): the integer path's own results, computed at
  compile time.

The double tier's bounds assume IEEE-754 binary64 arithmetic in the default
rounding mode (round to nearest; a mode set with `fesetround` is outside the
guarantee). They hold with or without FMA contraction: every multiply-add in
the kernels is an explicit `std::fma`, and the bound arithmetic has a margin
of 1.5. Reassociation is outside the proofs; a build that defines
`__ASSOCIATIVE_MATH__` or `__FAST_MATH__` widens the bounds 2^4, which every
audit passes. The dd tier's error-free sums need additions in program order: a build that defines
`__ASSOCIATIVE_MATH__` or `__FAST_MATH__` (GCC's `-fassociative-math`, either
compiler's `-ffast-math`) leaves the dd tier out, and Clang's
`-fassociative-math` on its own, which defines neither, is outside both
tiers' guarantee. FMA contraction, which GCC applies across statements by default, is
fenced off inside those sums (`__builtin_assoc_barrier`). A build whose
doubles are not IEEE (x87 80-bit evaluation, flush-to-zero in the kernels'
range, `-ffast-math` reciprocal approximations) is outside both tiers'
guarantee. Building with `BEMAN_INSIDE_MATH_NO_FP` removes both tiers —
results stay the same, and then no FPU behaviour is involved at all.
[math.md](math.md#where-correctness-comes-first) lists each place where the
engine chose correctness over speed, and what it costs.

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
- Values change only deliberately, in a documented release: the change re-pins the
  affected tests in the same commit and is listed in the table below, by
  function. Algebraic results (`+ − × ÷`, conversions, rounding) never change.
- Correctly rounded results cannot move again for an unchanged input and output
  grid: there is nothing left to improve.

| Change | Engine | Functions | Effect on values |
|---|---|---|---|
| 2026-10 (branch `adaptive-math`) | all → one | every transcendental | One correctly rounded engine replaces `dbl`, `flt` and `cordic`. A value that an engine had rounded the wrong way moves by one notch to the correctly rounded point (the old engines erred by up to 0.9 notch, `flt` up to 3). Deduced output **grids** change where the old 64-bit envelopes or 30-bit endpoint precision set them: ranges are now tight to the function's range over the input, and domains are the mathematical ones. The pinned values in `determinism.test.cpp` did not move. |
| 2026-10 (branch `perf-readability`) | CORDIC | asin, acos, sinh, cosh, tanh, log10, cbrt, hypot, sqrt | Runtime evaluation at the output grid's precision (+4 guard bits), one-exponential sinh/cosh, table-seeded sqrt. Last-bit differences in the working value; on the pinned and accuracy grids one snapped value moved (`sinh(4)` on `per<4096>`: 111779 → 111780 /4096, now correctly rounded). Accuracy unchanged within ±0.01 notch ([accuracy.md](accuracy.md)). |
| 2026-10 (same) | CORDIC, dbl, flt | tan (all), cos (dbl) | One shared range reduction / one CORDIC rotation. No pinned or accuracy-grid value moved. |
| 2026-10 (same), 8421ac5 | all (storage, not engines) | f64/f32 stores, math `store_grid` | One rounding rule: ties half away from zero, and an explicit mode is honoured on fp storage. Before, fp storage and the `store_grid` fast path rounded ties half toward +∞ (f64 −0.75 on notch ½ gave −0.5; integer storage gave −1), and `f64 \| round_floor` rounded to nearest. Negative ties and fp targets with a directional mode change value. |
| 2026-10 (same), 50144ea / 09aa483 | — | assignment, `wrap` | An off-notch integer source rounds by the policy (`{0,10}` notch 2 from 3 under `round_nearest`: 2 → 4); wrap folds modulo span + notch (`wrap_cast` of 12 onto `{0,10}` notch ½: 1 → 1.5); wrap rounds onto the lattice before folding (`{0,8}, wrap \| round_nearest` from 8.5: an out-of-grid 9 → 0). |
| 2026-10 (same), 7811134 / ca8ce56 | all | fmod, atan2, hypot | Output **grids** changed: `fmod` is bounded by min(max\|x\|, max\|y\|) on the gcd notch, `atan2` / `hypot` use the gcd notch of both inputs. Results on a different notch can snap differently; same-notch inputs keep their values. |
| 2026-10 (same), 02b272a | — | every store | Not a value change: every policy without `unsafe` is runtime-checked, so stores that used to keep an out-of-range value silently (`round_nearest`, `f64`, `indexed` types) now report it. |
| 2026-10 (same) | — | every store, conversion predicates | A rounding policy rounds before the range check: a value less than one notch outside that rounds onto an endpoint is stored (`{0, 9}, round_floor` from 9.55: domain_error → 9). Values that rounded inside the grid before are unchanged. |
| 2026-10 (same) | — | error codes | Not a value change: an out-of-range value reports `errc::overflow` everywhere (assignment, construction, `try_make`, `on_error` payloads used to say `domain_error`); `domain_error` now means only an argument outside a function's mathematical domain. The flag `ignore_domain` is renamed `ignore_range`. |
| 2026-10 (same) | — | trig API | Not a value change: the degree-circle trig (`circle<M>`, `sin(circle)` in degrees) is removed; every trig function takes radians, as in `<cmath>`. `amp<K>` stays as an output grid. |


## Compile-time determinism

Coefficients and constants (π, ln 2, ln 10, the series tables) are derived at
compile time from exact integer series with `constexpr` arithmetic, with **no
external code generators**. Nothing is baked by a separate tool that could drift
from the source. Every math function is `constexpr`, and a result computed at
compile time equals the runtime one.

## Checklist for reproducible builds

- Transcendentals need nothing: they are correctly rounded in every build.
- For `f64` / `f32` storage and arithmetic: build **without** `-ffast-math` /
  `-funsafe-math-optimizations`, keep round-to-nearest-even, target IEEE-754
  binary64 (on 32-bit x86 use SSE2, not x87).
- FMA contraction is handled internally (explicit `std::fma`), so `-mfma` is safe;
  `BEMAN_INSIDE_FMA` (default `ON`) adds it on x86-64 and the results are identical.
- Toolchain: GCC 14+ or Clang 18+ in C++23 mode (MSVC is not supported).
- To keep floating point out of the picture entirely, build with
  `-DBEMAN_INSIDE_MATH_NO_FP=ON`.

## Where to go next

| You want to… | Read |
|---|---|
| call sin/cos/sqrt/… | [math.md](math.md) |
| understand `f64` / double-backed storage | [storage.md](storage.md) |
| pick fast grids (fixed-point) | [fixed-point.md](fixed-point.md) |
| know *why* it's shaped this way | [internals.md](internals.md) |
