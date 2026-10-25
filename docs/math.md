# `beman::inside::math` — math on any grid, rounded once

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`<beman/inside/cmath.hpp>` is a `<cmath>`-shaped set of functions on insides.
Every transcendental returns the **correctly rounded point of its output
grid**, under the output's rounding mode, wherever the engine can decide it,
and it works at whatever precision that grid needs. A result it cannot decide
within its precision cap is a deterministic guess; [what is
guaranteed](#what-is-guaranteed) says when that can happen. So:

- **A result does not depend on how it was computed.** The same value comes
  out on every platform and compiler, at compile time and at run time, with or
  without an FPU, under the build rules of [determinism.md](determinism.md).
- **There is no precision ceiling.** A 2⁻¹⁰⁰ output grid gets a correctly
  rounded result, and inputs and outputs may be grids past 64 bits (C++26,
  [storage.md](storage.md#grids-past-64-bits-c26)).

```cpp
#include <beman/inside/cmath.hpp>
using namespace beman::inside;

using angle = inside<{{-8, 8}, per<16384>}, round_nearest>;
auto s = math::sin(angle{1});                              // 0.84149169921875: [-1, 1], notch 1/16384
auto c = math::cos_into<math::unit_amplitude<256>>(angle{1});   // 0.5390625: a coarser output, named
auto h = math::hypot(s, s);                                // √(s² + s²), output grid deduced
```

With units, `<beman/inside/units_math.hpp>` gives the same functions on
quantities, and trig on degrees and turns that is reduced exactly
([below](#quantities)).

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
| table | the input has at most `BEMAN_INSIDE_MATH_TABLE_SLOTS` points (default 256), the output has an integer raw, and every result is in range | one load; the table is computed at compile time |
| double tier | an FPU is present, the output's value indices stay within ±2⁵², and it needs at most 45–49 bits (by function) | the library's own `double` kernels with a proved error bound |
| double-double tier | an FPU is present, the output needs more than 36 bits, indices within ±2⁶² | kernels of about 106 bits, behind a lean kernel with a proved bound of about 2⁻⁷⁰ |
| integer path | always; the only path at compile time and without an FPU | Taylor series on compile-time coefficient tables, in wide fixed point |

How the floating-point tiers prove their bounds, and the speed each guard
costs, is in [internals.md](internals.md#10-the-math-engine).

## What is guaranteed

- **The same point in every build.** Under the build rules of
  [determinism.md](determinism.md), every path returns the point the integer
  path computes. The `double` tier's and the lean double-double kernels'
  bounds are proved; the full double-double kernels' are measured, with a
  margin of about 2⁹.
- **Exact values are exact.** The rational results — `exp(0)`, `2^−1`,
  `log10(1000)`, `sqrt(9/4)`, `8^(2/3)`, `hypot(3/2, 2)`, sin 30° — are stored
  exactly, so a tie between two points or a point under directed rounding
  rounds correctly. The theorems that make the list complete are named in
  `detail/math_cores.hpp`, and `tests/beman/inside/math_exact.test.cpp`
  checks each kind.
- **Correctly rounded, except within about 2^−4W of a boundary.** W is the
  start precision (the output's bits plus 8, at least 16). The engine doubles
  it twice, and a function that can come close to a grid point (tanh x → 1,
  sin x → x) runs once more at up to 2048 extra bits. A result still
  undecided then is stored as the point nearest its last approximation: the
  same in every build, but possibly the wrong neighbour. Nobody knows a bound
  that rules this out for an arbitrary grid, the table-maker's dilemma.
- **Certified by exhaustion, per type pair.** The certify tool computes every
  input of a pair through the public call and through the integer path
  alone, certifies it when no result differs and none was guessed, and
  hashes all its results. [certify.md](certify.md) shows how to find and
  certify the pairs your own program uses; [certified.md](certified.md)
  lists the library's, with their hashes. `BEMAN_INSIDE_MATH_INSTRUMENT=1`
  gives a program of your own the counters (`math::driver`).
- **Inverse trigonometry into degrees rounds twice.** `asin`, `acos`, `atan`
  and `atan2` return radians; `value_cast` to degrees then rounds a second
  time, so the angle in degrees is not correctly rounded
  ([roadmap.md](roadmap.md#units)).

## Conventions

- **Angles are radians**, as in `<cmath>`. `math::unit_amplitude<K>` ([−1, 1]
  at resolution 1/K) is a ready-made output: `math::sin_into<math::unit_amplitude<32768>>(x)`.
- **Rounding needs permission.** The input of a deduced form, or the `Out` of
  an `_into` form, must have a rounding mode (`round_nearest`, `round_floor`,
  `round_ceil`, `round_half_even` or `snap`). Without one it does not compile.
- **`fn(x)` deduces its output** from the input: the input's notch and policy
  (plus `round_nearest`), and the function's range over the input's range,
  computed at compile time and rounded outward. The exceptions are fixed
  ranges: `sin`/`cos` [−1, 1], `tan` [−1024, 1024], `atan2` [−π, π]; `hypot`
  uses the gcd of both notches, `pow` the base's notch, `floor` & co. notch 1.
  A fine input gives a fine, slower output; name a coarser one with `fn_into`.
- **`fn_into<Out>(x)` names the output.** The result rounds under `Out`'s mode,
  as [above](#what-is-guaranteed), and a result past `Out`'s range goes through `Out`'s policy
  (`clamp` saturates, `wrap` wraps the true value, the default reports). `tan`
  cannot have a `wrap` output: near a pole its value has no bound.
- **Grids off the notch's multiples** (`{{0.5, 10.5}, 1}`) are correctly
  rounded too, at about 1.5× the time; such an output cannot use `wrap`.
- **Errors.** A domain limit visible in the *type* is a `static_assert`. A
  failure that depends on the *value* comes back as `std::expected<Out, errc>`
  (`tan`, `pow`, `sqrt` of a grid that crosses 0). Total functions return the
  inside directly.
- **Domains are mathematical.** `sin` takes any finite argument, `exp` any
  whose result fits the output, `log` any positive one. Only the deduced forms
  of `exp`, `exp2`, `sinh` and `cosh` need |x| ≤ 4096, and a deduced `pow`
  output must stay below 2⁶⁵⁵³⁶; past that, name an output.
- **`constexpr`** in every build.
- **Measured accuracy.** [accuracy.md](accuracy.md) checks every function on
  dyadic, decimal and 2⁻⁴⁰ grids against a `long double` reference. Every
  maximum error is 0.5 notch, up to the reference's own error: a result within
  about 2⁻²⁰ notch of a boundary can read a hair past 0.5.

## The functions

### Exact grid operations

| Function | Output | Notes |
|---|---|---|
| `abs(x)` | `[0, max |x|]` | exact |
| `floor`, `ceil`, `round`, `trunc` | notch 1 | exact; `round` sends a half away from zero |
| `sign(x)` | −1, 0 or 1 | by exact comparison |
| `copysign(mag, sgn)` | `±|mag|` for the signs `sgn` can take | `sgn == 0` counts as positive |
| `fmod(x, y)` | the sign of `x` | truncated division, exact; `expected` when `y`'s grid holds 0 |
| `pown<E>(x)` | widened at every multiply | `E ≥ 0`, known at compile time: repeated squaring |

These work on grids past 64 bits too.

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

Every function also has its `_into<Out>` form: `math::sqrt_into<Out>(x)`,
`math::pow_into<Out>(b, e)`, `math::pow_base_into<Out, 10>(x)`.

## Constants

`math::pi`, `math::two_pi`, `math::e`, `math::ln2`, `math::sqrt2` and 13 more
are one-point insides, within 2⁻⁵⁷ of the true value, so they compose in
arithmetic: `angle * math::two_pi`. The full list, and the physical constants,
are in [constants.md](constants.md).

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
#include <beman/inside/units_math.hpp>
using namespace beman::inside::units;

inline constexpr named_unit slot_def{"slot", mag<rational{1, 32768}> * mag_pi * si::radian};
inline constexpr auto       slot = unit_c<slot_def>;           // 2π/65536 rad

quantity<slot, inside<{0, 65535}, wrap>> phase{0};             // wraps at exactly one turn
phase += quantity<slot, inside<{0, 65535}>>{16384};            // a quarter turn
units::sin_into<math::unit_amplitude<16384>>(phase);           // exactly 1
units::cos_into<inside<{{-1, 1}, per<16384>}, round_floor>>(
    quantity<si::degree, inside<{0, 360}>>{120});              // exactly −1/2, even under round_floor
```

[`oscillator.cpp`](../examples/oscillator.cpp) runs a phase counter this way, and
[`audio_mixer.cpp`](../examples/audio_mixer.cpp) mixes tones whose phase
offsets are exact fractions of a turn.

**Unwrapped radians are exact.** An angle accumulated on its own grid
(`x += step`) stays an exact grid value, and `sin`/`cos`/`tan` reduce any
argument exactly enough, so only the output's rounding remains: no drift,
however many turns. [`huge_angles.cpp`](../examples/huge_angles.cpp) takes the
sine of 10¹⁸ rad, correctly rounded.

**Don't `wrap` a radians angle.** `wrap` folds modulo the grid's period,
span + notch, which is never exactly 2π: `inside<{{0, 6.28125}, per<64>}, wrap>`
folds every 6.296875 rad, so each revolution shifts the angle by about
0.014 rad.

Without units, carry a wrapping phase in turns (period exactly 1) and convert
at the call. The conversion adds the snap onto the angle grid and
`math::two_pi`'s own error (about 6·10⁻¹⁸), but never accumulates:

```cpp
using turn_t  = inside<{{0, 1 - 1.0 / 4096}, per<4096>}, wrap | round_nearest>;
using angle_t = inside<{{0, 8}, per<16384>}, round_nearest>;
turn_t phase{0};
phase += turn_t{0.25};
auto s = math::sin_into<math::unit_amplitude<16384>>(angle_t{phase * math::two_pi});   // 1
```

## Quantities

`<beman/inside/units_math.hpp>` gives each function the unit it should have
([units.md](units.md#math-units_mathhpp)):

| Function | Takes | Gives |
|---|---|---|
| `abs`, `floor`, `ceil`, `round`, `trunc` | any quantity | the same unit |
| `sqrt`, `cbrt` (and `_into`) | a quantity whose unit has the root | the root of the unit and kind: m² → m |
| `sin`, `cos`, `tan` | an angle in rad | a plain inside |
| `sin_into`, `cos_into` | an angle in rad, or in a unit with a factor of π | a plain inside, reduced exactly |
| `asin`, `acos`, `atan` (and `_into`) | an inside or a dimensionless quantity | rad |
| `atan2(y, x)`, `hypot(x, y)` | two quantities of one dimension | rad; the common unit |
| `exp_into`, `log_into`, `log10_into`, `sinh_into`, … `pow_base_into<Out, B>`, `pow_into` | an inside or a dimensionless quantity | a plain inside |

The number functions are what a level in dB needs: 10^(L / 20 dB) is
`units::pow_base_into<gain_t, 10>(level / (just<20> * dB))`
([`decibels.cpp`](../examples/decibels.cpp)). `value_cast<si::radian, Out>(q)`
converts degrees to radians with one rounding, through the same engine.

## `expected` results

A fallible function returns `std::expected<Out, errc>`, and the result chains
through arithmetic, the first error winning
([arithmetic.md](arithmetic.md#when-the-result-is-stdexpected)). In this
fragment `signed_in` is a grid that crosses 0:

```cpp
auto t = math::tan(angle{1});                        // expected
auto r = math::sqrt(signed_in{v}) * gain + offset;   // expected: domain_error for v < 0
```

## Storage

Any storage works for inputs and outputs: integer index or value raws, the
exact fractions of continuous grids, and wide raws past 64 bits, all with the
same results. The double tier reads an integer input as its index times the
notch (exact on a dyadic grid) and stores integer outputs as raws.

## Speed

Onto the grid deduced from the input, the functions run from about 3× slower
to 3× faster than `<cmath>` (the `math:` tables in
[performance.md](performance.md)). Inputs of up to 256 points use the table
path and cost one load; each table adds about 0.13 s of compile time on GCC,
and `BEMAN_INSIDE_MATH_TABLE_SLOTS=0` turns tables off. Onto grids as fine as a
`double`, correct rounding costs 4–9× `<cmath>`: `sin` onto 2⁻⁵² in 14.6 ns,
`exp` onto 2⁻⁴⁰ in 12.5 ns, `log` onto 2⁻⁴⁸ in 17.4 ns (x86-64, `-O3 -mfma`).

## Without floating point

Under `BEMAN_INSIDE_MATH_NO_FP` (automatic with `-ffreestanding`) the
floating-point tiers compile out and the integer path computes every result.
**The results do not change**
([freestanding.md](freestanding.md#math-without-cmath-beman_inside_math_no_fp)).
