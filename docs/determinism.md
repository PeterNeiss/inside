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
| conversions to and from `double` and `float` | **yes** | IEEE-754, round to nearest; fast-math and x87 excess precision are rejected at compile time |
| conversions to and from `long double` | **per format** | one rounding to the platform's `long double` (x87 80-bit, IEEE quad, or `double`) |
| `beman::inside::math`, `units_math.hpp`, `value_cast` across π | **always** | none: every result is the correctly rounded grid point |
| compile-time constants and coefficient tables | **always** | `constexpr`, no external generators |
| text: `to_string`, `parse`, `<<`, `>>`, `std::format` | **always** | none: integer digits, no locale |
| `uniform<B>(rng)` | **always** | the same generator and seed |
| `std::hash` values | **per standard library** | built on `std::hash` of the raw's integers |

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
  `__FINITE_MATH_ONLY__`), or MSVC's `/fp:fast` (`_M_FP_FAST`), stops with an
  `#error` at the first include.
- Clang's bare `-fassociative-math` and `-funsafe-math-optimizations` define no
  macro, so they cannot be detected: don't pass them.
- Excess precision stops with an `#error` too: a build whose
  `__FLT_EVAL_METHOD__` is not 0, or 32-bit MSVC below `/arch:SSE2`. That is
  32-bit x86 on the x87 unit, which works at 80 bits and rounds when a value
  leaves a register. Compile for SSE2 (`-msse2 -mfpmath=sse`).
- On MSVC keep `/fp:precise` (the default) or `/fp:strict`. MSVC has no
  association barrier (below), so `/fp:contract` is outside the guarantee.
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
- error-free sums fenced with `__builtin_assoc_barrier` (GCC) or
  `__arithmetic_fence` (Clang), so `-ffp-contract=fast` cannot fuse them.

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

## Other surfaces

- **`float`.** A store from a `float` reads its exact value, like a
  `double`. `to<float>()` rounds the exact value once: where its `double` is
  a tie between two floats, the tie is decided from the value, not from the
  `double`.
- **`long double`.** A `long double` source is exact everywhere, including the
  bits past a `double`'s. `to<long double>()` rounds the exact value once to
  the platform's `long double`. That type is 80-bit x87 on x86 Linux, IEEE
  quad on AArch64 Linux, and `double` on MSVC and Apple, so the bits match
  between machines with the same format, not across formats.
- **Text.** `to_string`, `parse`, `from_chars`, `operator<<`/`>>` and the
  `std::formatter`s write and read the digits themselves: no `printf`,
  `to_chars`, `strtod` or locale. Only an integer spec that the formatter
  forwards to `std::formatter<std::int64_t>` and that asks for the locale
  (`{:L}`) depends on it.
- **Random sampling.** `uniform<B>(rng)` draws from the generator's raw
  outputs by a fixed recipe, not through `std::uniform_int_distribution`,
  whose algorithm each standard library picks for itself. A seeded
  `std::mt19937_64` gives the same B on libstdc++, libc++ and the MSVC STL
  (pinned in `sign_uniform.test.cpp` and `wide_grid.test.cpp`).
- **Hashing.** `std::hash<inside>` hashes its raw with `std::hash` of the
  raw's integers (and combines them for a fraction or wide raw). The integer
  hash is the identity on libstdc++ and libc++ and FNV-1a on the MSVC STL.
  Hash values, and so the iteration order of unordered containers, are
  stable within one standard library but not across them. Don't store them.

## How it is checked

- CI runs the same exact-value pins on x86-64 Linux (GCC 14–16, Clang 18–22,
  libstdc++ and libc++), arm64 macOS (AppleClang) and x64 Windows (MSVC). It
  also runs a `BEMAN_INSIDE_MATH_NO_FP=ON` cell, where the integer path
  computes every result alone.
- The differential oracle (`tests/beman/inside/differential`) recomputes
  arithmetic, rounding stores, `to<float>`/`to<double>` and π conversions
  exactly in Python. ctest runs a fixed seed, and a scheduled CI run takes a
  random seed and twenty times the cases.
- The FP-free smoke test puts a poisoned `<cmath>`
  (`tests/beman/inside/nofp_poison`) first on the include path, so a
  `BEMAN_INSIDE_MATH_NO_FP` build that pulls in floating-point math fails to
  compile.

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
  no Clang `-fassociative-math` or `-funsafe-math-optimizations` (fast-math,
  `/fp:fast` and x87 excess precision are rejected).
- On MSVC: `/fp:precise` or `/fp:strict`, not `/fp:contract`.
- `-mfma` is safe.
- `long double` results match only between platforms with the same
  `long double` format, and `std::hash` values only within one standard
  library.
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
