# Determinism and reproducibility

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

inside is built for results you can reproduce: the same inputs give the same
bits, across compilers, optimisation levels and machines. That matters for
fuzzing corpora, record and replay, deterministic simulation, lockstep
networking and regression baselines. This page says exactly what is guaranteed,
and under which build conditions. It is also the home of the floating-point
build rules that the other pages link to.

## In short

| Layer | Reproducible | Condition |
|---|---|---|
| integer and fraction storage, `+ − × ÷`, policies | **always** | none: fixed-width integers and exact fractions |
| units: conversions, mixed-unit sums, points | **always** | none: a conversion is an exact grid rescale |
| conversions to and from `double` | **yes** | IEEE-754 binary64, round to nearest; fast-math is rejected at compile time |
| `beman::inside::math`, `units_math.hpp`, `value_cast` across π | **always** | none: every result is the correctly rounded grid point |
| compile-time constants and coefficient tables | **always** | `constexpr`, no external generators |

A transcendental has exactly one correct answer on a given output grid, and
the engine returns it. Nothing about the build — compiler, flags, FPU,
constant evaluation — can change it, and an x86 host and a soft-float core
agree bit for bit.

## The integer and fraction core

Every inside holds a fixed-width integer (an index or the value) or an exact
fraction, and all of its arithmetic is integer or exact fraction arithmetic.
There is no floating point on these paths, so nothing for a platform to round
differently:

- The widths are fixed: `umax` is `std::uint64_t` and `imax` is `std::int64_t`
  (`numbers.hpp`); the value path is `imax` throughout, and wide raws are
  arrays of 64-bit limbs.
- A fraction is a `beman::inside::rational` (`detail/rational.hpp`). Every
  checked operation goes through overflow-detecting add, sub and mul
  (`detail/overflow.hpp`), so an overflow is a reported `errc::overflow`, not
  a platform-dependent wrap.
- `rational(double)` takes the value straight from its IEEE-754 bits: a finite
  `double` is exactly significand · 2^exponent, with no `<cmath>` and no FPU
  rounding (`detail/float_bits.hpp`).

Quantities add nothing that could differ: a unit conversion multiplies the
grid by an exact rational at compile time, and a mixed-unit sum adds two exact
rescales. Two builds on two architectures produce the same bits.

## Conversions to and from `double`

A `double` stored into an inside is rounded exactly. On a grid whose values
are doubles with a power-of-two notch, v / notch is exact and is rounded by
the policy's mode (`detail::snap_double_index`, `grid.hpp`); on any other grid
the `double` is first turned into its exact fraction. Reading an inside as a
`double` is one correctly rounded IEEE operation where the grid allows it
(`detail::as_double`), else a decode of the fraction.

**The build rules.** IEEE-754 `+ − × ÷` are deterministic given round to
nearest even (the default), binary64, and no rewrites that change values:

- A build with `-ffast-math`, GCC's `-fassociative-math` or `-ffinite-math-only`
  (which define `__FAST_MATH__`, `__ASSOCIATIVE_MATH__` or
  `__FINITE_MATH_ONLY__`) stops with an `#error` at the first include.
- Clang's bare `-fassociative-math` and `-funsafe-math-optimizations` define no
  macro, so they cannot be detected: don't pass them.
- On 32-bit x86, compile for SSE2. The x87 stack works at 80 bits and does not
  match.
- Flush-to-zero modes and `fesetround` are outside the guarantee.

## The math engine

`beman::inside::math` and the units math return the **correctly rounded point
of the output grid**, under the output's rounding mode
([math.md](math.md#how-it-works)). Such a result depends only on the input
value and the output grid, so it cannot depend on how it was computed. The fast
paths (tables, `double` and double-double kernels) return a point only when
their error bound proves it is the one the integer path returns. The integer
path, the only one at compile time and without an FPU, uses no `<cmath>`, no
run-time tables and no generated code.

The floating-point tiers rely on the build rules above, plus:

- an explicit `std::fma` and a 1.5× margin on every bound, so FMA contraction
  cannot change a result (`-mfma` is safe; `BEMAN_INSIDE_FMA`, default `ON`,
  adds it on x86-64);
- error-free sums fenced with `__builtin_assoc_barrier`.

`BEMAN_INSIDE_MATH_NO_FP` removes both tiers, with the same results.
[internals.md](internals.md#10-the-math-engine) lists each place where the
engine chose correctness over speed.

**When is a result decided?** A transcendental value can sit exactly on a
rounding boundary only at exact inputs (`exp(0)`, `sqrt(9/4)`, …), and the
engine stores those exactly. For every other input it doubles the precision
until the result is decided, up to a cap of four times the start precision W.
A function whose results can come close to a grid point by an amount the
input bounds (tanh x → 1, sin x → x, eˣ → 0) then runs once more, at up to
2048 extra bits. Only after that does the engine take the nearest point. That
needs an irrational value within 2^−4W of a boundary, and no test has ever
reached it.

## Value stability across releases

The other half of the promise: an input keeps producing the same bits from one
release to the next, so stored results, golden files and replay logs survive
an upgrade.

- The transcendental values are pinned by exact-value tests
  (`determinism.test.cpp`, `math_adaptive.test.cpp`, `constexpr.test.cpp`).
  They are the contract: a change to any of them does not merge silently.
- Values change only on purpose, in a release that re-pins the tests and lists
  the change by function. Algebraic results (`+ − × ÷`, conversions, rounding)
  change only where a result was wrong, and each fix is listed in the
  changelog under Fixed. No math value has changed since 0.1.0.

## Compile-time determinism

Coefficients and constants (π, ln 2, ln 10, the series tables) come from exact
integer series in `constexpr` code, with **no external code generator**:
nothing is baked by a tool that could drift from the source. Every math
function is `constexpr`, and a result computed at compile time equals the run
time one.

## Checklist

- Transcendentals need nothing: they are correctly rounded in every build.
- For conversions with `double`: round to nearest even, IEEE-754 binary64,
  SSE2 on 32-bit x86, no Clang `-fassociative-math` or
  `-funsafe-math-optimizations` (fast-math itself is rejected).
- `-mfma` is safe.
- Toolchain: GCC 14+, Clang 19+, Clang 18 with libc++, or MSVC 19.51+, in
  C++23 mode or later.
- To keep floating point out entirely, build with `-DBEMAN_INSIDE_MATH_NO_FP=ON`
  ([freestanding.md](freestanding.md#math-without-cmath-beman_inside_math_no_fp)).

## Where to go next

| You want | Read |
|---|---|
| sin, cos, sqrt, … | [math.md](math.md) |
| the storage rules | [storage.md](storage.md) |
| fast grids for fixed point | [fixed-point.md](fixed-point.md) |
| *why* it is shaped this way | [internals.md](internals.md) |
