# The math engine

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

How `beman::inside::math` rounds every result correctly, what exactly is
guaranteed, the domains and output ranges of each function, its speed, and
angles over many turns. [Math](../guide/math.md) has the everyday view.

Every transcendental returns the **correctly rounded point of its output
grid**, under the output's rounding mode, wherever the engine can decide it,
at whatever precision that grid needs. A result it cannot decide within its
precision cap is a deterministic guess; [what is guaranteed](#what-is-guaranteed)
says when that can happen. So:

- **A result does not depend on how it was computed.** The same value comes
  out on every platform and compiler, at compile time and at run time, with or
  without an FPU, under the build rules of [determinism](determinism.md).
- **There is no precision ceiling.** A 2⁻¹⁰⁰ output grid gets a correctly
  rounded result, and inputs and outputs may be grids past 64 bits.

## How it works

A result is computed in fixed point together with an error bound, and both
ends of that interval are mapped onto the output grid. When they land on the
same point, that point is the correctly rounded result. When they straddle a
rounding boundary, the engine computes again at twice the precision (Ziv's
strategy). A transcendental value lies exactly on a boundary only at a few
exact inputs (`exp(0)`, `log(1)`, `sqrt(9/4)`, `log2(2^k)`, integer powers);
the engine recognises those and stores them exactly. Every other value is
irrational, so the doubling ends, but only up to a cap
([below](#what-is-guaranteed)).

The precision follows the **output** grid: it starts at the bits that resolve
the output notch, plus 8 guard bits. A coarse output is cheap and a fine one
costs more, with no fixed limit.

Each call takes the first of four paths that applies. All four give the same
grid point:

| Path | When | Cost |
|---|---|---|
| table | the input has at most 256 points, the output has an integer raw, and every result is in range (none in an instrumented build) | one load; the table is computed at compile time |
| double tier | an FPU is present, the output's value indices stay within ±2⁵², and it needs at most 46–48 bits (by function) | the library's own `double` kernels with a proved error bound |
| double-double tier | an FPU is present, the output needs more than 36 bits, indices within ±2⁶² | kernels of about 106 bits, behind a lean kernel with a proved bound of about 2⁻⁷⁰ |
| integer path | always; the only path at compile time and without an FPU | Taylor series on compile-time coefficient tables, in wide fixed point |

How the floating-point tiers prove their bounds, their minimax polynomials,
and the speed each guard costs, are in
[internals.md](../internals.md#10-the-math-engine).

## What is guaranteed

- **The same point in every build.** Under the build rules of
  [determinism](determinism.md), every path returns the point the integer
  path computes. The `double` tier's and the lean double-double kernels'
  bounds are proved; the full double-double kernels' are measured, with a
  margin of about 2⁹.
- **Exact values are exact.** The rational results — `exp(0)`, `2^−1`,
  `log10(1000)`, `sqrt(9/4)`, `8^(2/3)`, `hypot(3/2, 2)`, sin 30° — are stored
  exactly, so a tie between two points or a point under directed rounding
  rounds correctly. The theorems that make the list complete are named in
  `math/detail/cores.hpp`, and `tests/beman/inside/math_exact.test.cpp`
  checks each kind.
- **Correctly rounded, except within about 2^−4W of a boundary.** W is the
  start precision (the output's bits plus 8, at least 16). The engine doubles
  it twice, and a function that can come close to a grid point (tanh x → 1,
  sin x → x) runs once more at up to 2048 extra bits. A result still
  undecided then is stored as the point nearest its last approximation: the
  same in every build, but possibly the wrong neighbour. Nobody knows a bound
  that rules this out for an arbitrary grid: the table-maker's dilemma.
- **Certified by exhaustion, per type pair.** The certify tool computes every
  input of a pair through the public call and through the integer path
  alone, certifies it when no result differs and none was guessed, and
  hashes all its results. [Certifying math](certify.md) shows how to find and
  certify the pairs your own program uses; [certified.md](../certified.md)
  lists the library's and the examples', with their hashes.
  `BEMAN_INSIDE_MATH_INSTRUMENT=1` gives a program of your own the counters
  (`math::driver`); such a build has no table path, so the counters see every
  call.
- **Inverse trigonometry into degrees rounds twice.** `asin`, `acos`, `atan`
  and `atan2` return radians; `value_cast` to degrees then rounds a second
  time, so the angle in degrees is not correctly rounded
  ([roadmap](../roadmap.md#units)).
- **Measured accuracy.** [accuracy.md](../accuracy.md) checks every function
  on dyadic, decimal and 2⁻⁴⁰ grids against a `long double` reference. Every
  maximum error is 0.5 notch, up to the reference's own error: a result
  within about 2⁻²⁰ notch of a boundary can read a hair past 0.5.

## Output grids

- **`fn(x)` deduces its output** from the input: the input's notch and policy
  (plus `round_nearest`), and the function's range over the input's range,
  computed at compile time and rounded outward. The exceptions are fixed
  ranges: `sin`/`cos` [−1, 1], `tan` [−1024, 1024], `atan2` [−π, π]; `hypot`
  uses the gcd of both notches, `pow` the base's notch, `floor` & co. notch 1.
  A fine input gives a fine, slower output; name a coarser one with `fn_into`.
- **`fn_into<Out>(x)` names the output.** The result rounds under `Out`'s
  mode, and a result past `Out`'s range goes through `Out`'s policy (`clamp`
  saturates, `wrap` wraps the true value, the default reports). `tan` cannot
  have a `wrap` output: near a pole its value has no bound.
- **`fn<mode>(x)`** deduces the output as `fn(x)` does and rounds it by
  `mode`, whatever `x`'s type says (`pow_base<B, mode>(x)`; on quantities
  `units::sqrt<mode>(q)` & co.).
- **Grids off the notch's multiples** (`{{0.5, 10.5}, 1}`) are correctly
  rounded too, at about 1.5× the time; such an output cannot use `wrap`.
- **Domains are mathematical.** `sin` takes any finite argument, `exp` any
  whose result fits the output, `log` any positive one. Only the deduced forms
  of `exp`, `exp2`, `sinh` and `cosh` need |x| ≤ 4096, and a deduced `pow`
  output must stay below 2⁶⁵⁵³⁶; past that, name an output.
- **`constexpr`** in every build.

## The functions

### Exact grid operations

| Function | Output | Notes |
|---|---|---|
| `abs(x)` | `[min \|x\|, max \|x\|]` | exact |
| `floor`, `ceil`, `round`, `trunc` | notch 1 | exact; `round` sends a half away from zero |
| `sign(x)` | −1, 0 or 1 | by exact comparison |
| `copysign(mag, sgn)` | `±\|mag\|` for the signs `sgn` can take | `sgn == 0` counts as positive |
| `fmod(x, y)` | the sign of `x` | truncated division, exact; `expected` when `y`'s grid holds 0 |
| `pown<E>(x)` | widened at every multiply; an even power from the least \|x\|^E, never below 0 | `E ≥ 0`, known at compile time: repeated squaring |

These work on grids past 64 bits too, and need no rounding mode.

### Roots

| Function | Domain | Errors | Notes |
|---|---|---|---|
| `sqrt(x)` on a grid with Lower ≥ 0 | x ≥ 0 | none | exact integer square root |
| `sqrt(x)` on a grid that crosses 0 | all | `expected`: `domain_error` for x < 0 | |
| `cbrt(x)` | all | none | exact integer cube root |
| `hypot(x, y)` | all | none | √(x² + y²), the sum formed exactly |

### Trigonometric (radians)

| Function | Domain | Deduced output | Errors |
|---|---|---|---|
| `sin`, `cos` | all | [−1, 1] | none |
| `tan` | all | [−1024, 1024] | `expected`: `overflow` past `Out` (saturates when `Out` has `clamp`) |
| `atan` | all | (−π/2, π/2), rounded out | none |
| `asin`, `acos` | [−1, 1] | [−π/2, π/2], [0, π], rounded out | none |
| `atan2(y, x)` | all; `atan2(0, 0)` is 0 | [−π, π], rounded out | none |

### Hyperbolic

| Function | Domain |
|---|---|
| `sinh`, `cosh`, `tanh` | all (deduced forms: \|x\| ≤ 4096) |
| `asinh` | all |
| `acosh` | x ≥ 1 |
| `atanh` | −1 < x < 1 |

### Exponential and logarithmic

| Function | Domain | Errors | Notes |
|---|---|---|---|
| `exp`, `exp2` | all (deduced forms: \|x\| ≤ 4096) | none | |
| `log`, `log2`, `log10` | x > 0 | none | `log2`, `log10` of an exact power are exact |
| `pow_base<B>(x)` | `B` an integer ≥ 2, known at compile time | none | |
| `pow(base, e)` | deduced form: Lower(base) > 0 | `expected`: `domain_error` for a negative base, `division_by_zero` for 0 to a negative power, `overflow` past `Out` | exact for integer exponents, and for p/q when the base is a perfect q-th power |

A domain limit visible in the input's *type* is a compile error; a failure
that depends on the *value* comes back in the `std::expected`.

## Storage

Any storage works for inputs and outputs: integer index or value raws, the
exact fractions of continuous grids, and wide raws past 64 bits, all with the
same results. The double tier reads an integer input as its index times the
notch (exact on a dyadic grid) and stores integer outputs as raws.

## Speed

Onto the grid deduced from the input, the functions run from about 2× slower
to 3× faster than `<cmath>` (the `math:` tables in
[performance.md](../performance.md)). Inputs of up to 256 points use the table
path and cost one load; each table adds about 0.13 s of compile time on GCC.

Onto grids as fine as a `double`, correct rounding costs 1.2–7× `<cmath>`'s
`double` functions. Measured on the maintainer's machine (x86-64,
`-O3 -mfma`, GCC 16, a loop over 2·10⁶ inputs on a 2⁻²⁰ grid): `sin` onto
2⁻⁵² in about 14 ns, `exp` onto 2⁻⁴⁰ in about 6 ns, `log` onto 2⁻⁴⁸ in about
17 ns.

## Angles over many turns

No rational notch divides 2π, so how an angle survives many revolutions
depends on how it is carried.

**Best: an angle in a unit with a factor of π.** With units, a phase can be in
turns, degrees, or slots of 2π/65536 rad. `units::sin_into` and `cos_into`
reduce such an angle exactly: x·r half turns, for a rational r, minus whole
quarter turns. Only the rest, at most π/4, is approximated. The result rounds
once, and Niven's rational values (0, ±1/2, ±1) are exact under any rounding
mode:

```cpp
#include <beman/inside/units/math.hpp>
using namespace beman::inside::units;

inline constexpr named_unit slot_def{"slot", mag<rational{1, 32768}> * mag_pi * si::radian};
inline constexpr auto       slot = unit_c<slot_def>;           // 2π/65536 rad

quantity<slot, inside<{0, 65535}, wrap>> phase{0};             // wraps at exactly one turn
phase += quantity<slot, inside<{0, 65535}>>{16384};            // a quarter turn
units::sin_into<math::unit_amplitude<16384>>(phase);           // exactly 1
units::cos_into<inside<{{-1, 1}, per<16384>}, round_floor>>(
    quantity<si::degree, inside<{0, 360}>>{120});              // exactly −1/2, even under round_floor
```

[`oscillator.cpp`](../../examples/oscillator.cpp) runs a phase counter this
way, and [`audio_mixer.cpp`](../../examples/audio_mixer.cpp) mixes tones whose
phase offsets are exact fractions of a turn.

**Unwrapped radians are exact.** An angle accumulated on its own grid
(`x += step`) stays an exact grid value, and `sin`/`cos`/`tan` reduce any
argument exactly enough, so only the output's rounding remains: no drift,
however many turns. [`huge_angles.cpp`](../../examples/huge_angles.cpp) takes
the sine of 10¹⁸ rad, correctly rounded.

**Don't `wrap` a radians angle.** `wrap` folds modulo the grid's period,
span + notch, which is never exactly 2π: `inside<{{0, 6.28125}, per<64>}, wrap>`
folds every 6.296875 rad, so each revolution shifts the angle by about
0.014 rad.

Without units, carry a wrapping phase in turns (period exactly 1) and convert
at the call. The conversion adds the rounding onto the angle grid and
`math::two_pi`'s own error (about 6·10⁻¹⁸), but never accumulates:

```cpp
using turn_t  = inside<{{0, 1 - 1.0 / 4096}, per<4096>}, wrap | round_nearest>;
using angle_t = inside<{{0, 8}, per<16384>}, round_nearest>;
turn_t phase{0};
phase += turn_t{0.25};
auto s = math::sin_into<math::unit_amplitude<16384>>(angle_t{phase * math::two_pi});   // 1
```

## Without floating point

Under `BEMAN_INSIDE_MATH_NO_FP` (automatic with `-ffreestanding`) the
floating-point tiers compile out and the integer path computes every result.
**The results do not change**
([freestanding](freestanding.md#math-without-cmath-beman_inside_math_no_fp)).
