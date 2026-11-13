# Math

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`<beman/inside/math.hpp>` has the functions of `<cmath>` for insides, in
`beman::inside::math`. Each result is the **correctly rounded** point of its
output grid, so it is the same on every platform, at compile time and
without an FPU.

```cpp
#include <beman/inside/math.hpp>
using namespace beman::inside;

using angle = inside<{{-8, 8}, per<16384>}, round_nearest>;
auto s = math::sin(angle{1});              // 0.84149169921875, on the input's notch
auto c = math::cos<per<256>>(angle{1});    // 0.5390625: the notch you name
auto h = math::hypot(s, s);                // √(s² + s²)
```

## The rounding mode

A result such as sin 1 lies between two points of any grid, so it is
rounded once, by one rounding mode:

- the call's: `math::exp<round_floor>(x)`;
- a named output's: `math::sin<Out>(x)`, where `Out` must have a rounding
  mode (it is your type, and its policy forbids rounding without one);
- otherwise the input type's mode, or `round_nearest` when it names none.
  The deduced output carries that mode.

The rounding mode decides the result: `round_nearest` gives the nearest
point, `round_floor` the one below, and so on.

## Choosing the output grid

`fn(x)` **deduces** its output from the input: the input's notch and policy,
and the function's range over the input's range, computed at compile time.
`sin` and `cos` give [−1, 1], `exp` of [0, 3] gives [1, e³] rounded outward.

Name only what you choose; the rest is still deduced:

| Call | Output |
|---|---|
| `fn(x)` | deduced: the function's range over x, on x's notch, with x's policy |
| `fn<per<1024>>(x)` | the same range, computed on the notch you name |
| `fn<round_floor>(x)`, `fn<per<1000>, round_floor, clamp>(x)` | the deduced output with your notch and policy flags, in any order |
| `fn<Out>(x)` | `Out`, a type you name: a coarser result, which is cheaper, or a range you know |
| `fn<Out, In>(x)` | `Out`, with x converted to `In` first |

A value past a named `Out`'s range goes through `Out`'s policy, like any
store. `math::unit_amplitude<K>` is [−1, 1] in steps of 1/K, what
`sin<per<K>>(x)` deduces.

## The functions

| Group | Functions |
|---|---|
| exact | `abs`, `sign`, `copysign`, `floor`, `ceil`, `round`, `trunc`, `fmod`, `pown<E>` |
| roots | `sqrt`, `cbrt`, `hypot` |
| trigonometric (radians) | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` |
| hyperbolic | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh` |
| exponential | `exp`, `exp2`, `log`, `log2`, `log10`, `pow`, `pow_base<B>` |
| counting | `factorial`, `binomial`, `falling_factorial`, `rising_factorial`, `multinomial` |
| number theory | `gcd`, `lcm` |

Every function has every form above. `pow_base<10>(x)` is 10ˣ: its base
comes first, `pow_base<10, per<1024>>(x)`, `pow_base<10, Out>(x)`; so does
`pown<3>(x)`'s exponent. `exp` also takes a multivector, every part of its
output correctly rounded ([geometric algebra](../advanced/geometric-algebra.md#exp)).
The exact functions (the first row, the counting and number-theory ones)
need no rounding mode for their deduced output, which is exact; a notch you
name rounds by the input's mode, else to nearest.

## Counting

`factorial`, `binomial`, `falling_factorial` (the k-permutations),
`rising_factorial` and `multinomial` take counts: insides on the integers
from 0, of any size (`binomial` of an n past 2⁶⁴ and a small k works). Their
results are exact, and each result type is the range the inputs can produce,
worked out while compiling:

```cpp
inside<{0, 20}> n = 20;
auto f = math::factorial(n);                    // inside<{1, 2432902008176640000}>: 8 bytes
using small = inside<{0, 60}>;
auto c = math::binomial(small{49}, small{6});   // 13983816, on [0, C(60, 30)]
auto p = math::falling_factorial(just<8>, just<3>);        // 336: podium orders
auto m = math::multinomial(1_ins, 4_ins, 4_ins, 2_ins);    // 34650: MISSISSIPPI
auto big = math::factorial(inside<{0, 100}>{100});         // 100!, on a wide raw
```

`binomial(n, k)` and `falling_factorial(n, k)` are 0 for k > n, and
`rising_factorial(n, 0)` is 1. A result whose limit would pass 16384 bits
(past 1754!) does not compile: its limit is computed while compiling.
`gcd(a, b)` and `lcm(a, b)` take lattices of any sign and size, integer or
fractional, and follow `std::gcd` and `std::lcm`: never negative, `gcd(0, 0)`
and `lcm(0, b)` are 0. Their result types keep the lattice the operands share
(the gcd of two even grids is even; of halves and thirds a multiple of 1/6:
`gcd(3/2, 4/3)` is 1/6, `lcm(3/2, 4/3)` is 12). `fn<Out>(…)` stores a
result into `Out`;
[`combinatorics.cpp`](../../examples/combinatorics.cpp) uses them for
odds as exact fractions.

## Errors

A domain limit that the input's *type* shows is a compile error: `log` of a
grid that reaches 0, `asin` of a grid past [−1, 1], `sqrt` of a grid that
reaches below 0 (take `abs(x)`, or store `x` into a non-negative grid first),
`tan` of an interval that holds a pole (an odd multiple of π/2), `pow` of a
base grid below 0 (`pown<E>` takes a negative base). Only a zero divisor
depends on the *value*, and returns a `std::expected`:

| Function | Returns `std::expected` when |
|---|---|
| `pow` | the base's grid holds 0 and the exponent's a negative: 0 to a negative power |
| `fmod` | the divisor's grid holds 0 |

Everything else returns a plain inside. A result past a named output
(`fn<Out>`) is a store into `Out`: its policy clamps, wraps or reports it,
as any store does.

## Angles

Angles are in radians, as in `<cmath>`. With `<beman/inside/units/math.hpp>`
an angle can be a quantity in degrees, in turns, or in any unit that is a
rational multiple of π. Then the reduction is exact, and only the remainder,
at most π/4, is approximated, so exact values come out exact:

```cpp
#include <beman/inside/units/math.hpp>
using namespace beman::inside::units;

quantity<si::degree, inside<{0, 360}>> deg{120};
math::cos<per<16384>, round_floor>(deg);   // exactly −1/2, even rounding down
```

An angle's notch in degrees says nothing about its sine's, so a call on one
names the result's notch (or the whole output).

Do not `wrap` an angle in radians: a grid's period is never exactly 2π, so
each turn would shift the angle. Carry a wrapping phase in degrees, turns or
steps of 2π/2ⁿ, as [`oscillator.cpp`](../../examples/oscillator.cpp) does.

## Quantities

`<beman/inside/units/math.hpp>` gives each function the unit it should have,
in every form above:

| Function | Takes | Gives |
|---|---|---|
| `abs`, `floor`, `ceil`, `round`, `trunc` | any quantity | the same unit |
| `sqrt`, `cbrt` | a unit with that root | the root of the unit: m² → m |
| `sin`, `cos`, `tan` | an angle in rad (`sin`, `cos`: or degrees, or any unit with a factor of π, reduced exactly) | a plain inside |
| `asin`, `acos`, `atan` | a dimensionless quantity | rad |
| `atan2(y, x)`, `hypot(x, y)` | two quantities of one dimension | rad; their common unit |
| `exp`, `exp2`, `log`, …, `atanh`, `pow_base<B>`, `pow` | dimensionless quantities | a plain inside |
| `exp` | a multivector, or a quantity of one blade, whose non-scalar parts anticommute | a multivector: each part rounded once |
| `value_cast<si::radian, Out>(q)` | an angle in degrees or turns | rad, rounded once |

`fn<L>(q)`, with `L` an inside, names only the result's number: the unit
follows (`math::sqrt<L>(area)` is a length on `L`). `fn<Q>(q)` names the
whole quantity, whose unit must be the result's. Of a plain inside, `asin`,
`acos` and `atan` give a plain inside, as in `<cmath>`.

A level in dB converts to a gain with
`math::pow_base<10, gain_t>(level / (just<20> * dB))`
([`decibels.cpp`](../../examples/decibels.cpp)).

## Constants

`math::pi`, `math::two_pi`, `math::e`, `math::ln2`, `math::sqrt2` and 13 more
are computed while compiling at the precision each use needs: `Out o =
math::pi` rounds π correctly onto `Out`, `radius * math::two_pi` takes 2π to
`radius`'s resolution, and `x < math::pi` compares exactly. The physical
constants are quantities in `units::constants`
([Constants](../advanced/constants.md)).

## Speed

Onto the grid deduced from the input, the functions run between about 3×
slower and 3× faster than `<cmath>`; an input of at most 256 points is a
table lookup. Onto grids as fine as a `double`, correct rounding costs a few
times `<cmath>`. The [performance page](../performance.md) has the numbers.

## Details

- [The math engine](../advanced/math-engine.md): how a result is rounded
  correctly, what is guaranteed, the paths, angles over many turns, building
  without floating point.
- [Certifying math](../advanced/certify.md): proving the results for the grids
  your program uses.
- [Determinism](../advanced/determinism.md): the build rules behind "the same
  bits everywhere".
