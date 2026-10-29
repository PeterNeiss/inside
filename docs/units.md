# Physical units

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`<beman/inside/units.hpp>` adds physical quantities on top of inside: units,
dimensions, the ISQ quantity kinds and quantity points, with an inside as the
number. You can add units, kinds, origins and dimensions of your own. The
design follows [mp-units](https://github.com/mpusz/mp-units), without depending
on it.

A grid is a compile-time value, so a unit conversion only rescales the grid.
It is exact, it cannot fail, and it costs nothing at run time.

```cpp
#include <beman/inside/units.hpp>
using namespace beman::inside;
using namespace beman::inside::units;

constexpr auto km = si::kilo(si::metre);

quantity<km, inside<{{0, 500}, per<1000>}>> a{12.5};   // 12.5 km, to the metre
auto b     = just<800> * si::metre;                      // 800 m, a one-point grid
auto total = a + b;                                      // 13300 m: exact
auto mm    = total.in(si::milli(si::metre));             // 13300000 mm: the same raw
auto v     = total / quantity<si::minute, inside<{1, 600}>>{20};
std::cout << v.in(km / si::hour);                        // 39.9 km/h
```

`<beman/inside/units_math.hpp>` adds the functions that need the math engine
(cmath.hpp). It is a separate header so that units.hpp alone compiles fast.

## Examples

Every example checks its own output.

| Theme | Examples |
|---|---|
| the catalog | [`units.cpp`](../examples/units.cpp) (a tour: conversions, kinds, temperatures), [`kitchen.cpp`](../examples/kitchen.cpp) (lb + g, ℉ ovens), [`circuit.cpp`](../examples/circuit.cpp) (Ohm's law, an RC filter), [`moon_rocket.cpp`](../examples/moon_rocket.cpp) (Δv, orbits, a transfer time with the true π) |
| units of your own | [`own_units.cpp`](../examples/own_units.cpp) (fathoms, °R, EUR, bytes), [`money.cpp`](../examples/money.cpp) (USD, ¢, a tax in %), [`wei.cpp`](../examples/wei.cpp) (ETH, gwei, wei, gas, USD/ETH), [`json_io.cpp`](../examples/json_io.cpp) (USD/item), [`adc_calibration.cpp`](../examples/adc_calibration.cpp) (ADC counts) |
| hooks and wrap | [`clock.cpp`](../examples/clock.cpp) (s → min → h, months, weeks), [`game_hp.cpp`](../examples/game_hp.cpp) (HP), [`rifle.cpp`](../examples/rifle.cpp) (rounds), [`torus_map.cpp`](../examples/torus_map.cpp) (px), [`sequence_number.cpp`](../examples/sequence_number.cpp) and [`jitter_buffer.cpp`](../examples/jitter_buffer.cpp) (packets, ms) |
| angles and math | [`oscillator.cpp`](../examples/oscillator.cpp) (a phase in slots of 2π/65536 rad), [`audio_mixer.cpp`](../examples/audio_mixer.cpp) (dB gains, phases in turns), [`decibels.cpp`](../examples/decibels.cpp) (dB as a level), [`navigation.cpp`](../examples/navigation.cpp) (range and bearing), [`huge_angles.cpp`](../examples/huge_angles.cpp) (sin of 10¹⁸ rad), [`audio_math.cpp`](../examples/audio_math.cpp) (s and Hz) |
| points and control | [`sensor_fusion.cpp`](../examples/sensor_fusion.cpp) (a weighted mean of ℃ points), [`pid_controller.cpp`](../examples/pid_controller.cpp) (K in, % out), [`fixed_point.cpp`](../examples/fixed_point.cpp) (a 0.5 ℃ grid), [`expected_pipeline.cpp`](../examples/expected_pipeline.cpp) (volts) |
| big grids | [`solar_system.cpp`](../examples/solar_system.cpp) (nm across ±10¹³ m), [`planck_to_cosmos.cpp`](../examples/planck_to_cosmos.cpp) (10⁻⁴¹ m to 10²⁷ m) |

## The model

| Concept | What it is | Spelled |
|---|---|---|
| dimension | exponents of L M T I Θ N J, plus up to four of your own | `dimension_of(si::newton.value)` |
| magnitude | an exact rational × π^k, relative to the coherent SI unit | `mag<1000>`, `mag_pi` |
| unit | named units to integer powers, with prefixes and a magnitude | `si::kilo(si::metre) / si::hour` |
| quantity spec | a kind in the ISQ tree: `height` → `altitude` → `length` | `isq::height` |
| reference | a unit and a spec | `isq::height[si::metre]`, or a bare unit |
| quantity | a number (an inside) of a reference | `quantity<si::metre, inside<{0, 10}>>` |
| quantity point | a quantity counted from an origin | `point<si::degree_Celsius>(t)` |

Units, specs and references are **values**, combined by `consteval`
functions: not type lists that the compiler has to sort and simplify. The
names you write (`si::metre`, `isq::height`) are empty tags whose type carries
the value. `si::metre * si::second / si::second` is `si::metre`, and m·s is
the same unit as s·m.

The number is always an inside. A raw scalar has no grid, so `42 * si::metre`
and `q * 2` do not compile, and the message says to write `just<42>`. A
quantity constructs from a number in its unit:
`quantity<si::metre, inside<{0, 100}>>{42}`.

An operation that cannot happen is a constraint failure:
`requires { a + b; }` is false for a length and a duration, for Hz and Bq,
for `q * 2`, for `q.in(si::second)` on a length, and for any number operation
inside rejects. A call says why: each reason is a deleted overload with a
message (`= delete("…")`,
[diagnostics.md](diagnostics.md#named-reasons-for-a-unit-mismatch)).

## Conversions are grid rescales

`q.in(u)` multiplies the grid's interval and notch by the factor between the
two units and keeps the raw. km → m on `inside<{0, 10}>` gives
`inside<{0, 10000}>` with notch 1000 and the same raw; km → mm compiles to a
single `mov` (pinned by a codegen test). m → km gives notch 1/1000, and
inch → mm is ×25.4. All exact. The rescaled number keeps its policy.

| Function | Gives |
|---|---|
| `q.in(u)`, `q.in<u>()` | the same quantity in unit `u`, exact |
| `q.numerical_value()`, `q.numerical_value_in(u)` | the inside |
| `value_cast<Rep2>(q)`, `try_value_cast<Rep2, F>(q)` | another number type, through Rep2's constructor or `try_make<F>` |
| `value_cast<u, Rep2>(q)`, `try_value_cast<u, Rep2, F>(q)` | in unit `u`, into Rep2 |
| `value_cast<Q>(q)`, `try_value_cast<Q, F>(q)` | into the quantity type Q, whose spec must take q's |
| `quantity_cast<isq::height>(q)` | another spec: down the tree, or across one kind |
| `q.quantity_spec` | the spec as a value: `q.quantity_spec == isq::length` |

A magnitude scales a unit from either side: `mag<2> * si::metre`,
`si::metre * mag<2>`, `si::metre / mag_pi`. A conversion is implicit when the
spec converts implicitly and the rescaled number converts implicitly into the
target's number: `quantity<si::joule, R>` takes an N·m quantity.

### Factors of π round once

A degree is π/180 rad, so no grid can represent that conversion, and `.in()`
refuses it with a message. `value_cast<si::radian, Out>(q)` (units_math.hpp)
computes the exact x·π/180 with π to as many bits as `Out` needs, and rounds
once onto `Out`, which needs a rounding mode. Only factors of π and 1/π are
supported.

π can scale a quantity too: `mag_pi * q` multiplies the **unit**, not the
number. `mag_pi * just<2> * si::second` is the number 2 in the unit [π s], so π
is never approximated, and a later `value_cast<si::hour, Out>(…)` rounds once,
with the true π. That is how to write π·a·√(a/μ) or 1/(2πRC): `math::pi` is a
rational within 2⁻⁵⁸, and would make the grid numbers grow with every
product.

### Dimensionless quantities are numbers

A quantity of dimension one converts to an inside: implicitly when its number
does, explicitly when the target must round. The number is its value in unit
one: (21 kg) / (6 kg) is 3.5, (3 km) / (2 m) is 1500, and 8 % is 2/25. An
angle stays a quantity: radians and steradians are `is_kind` quantities, and
so is a π-scaled ratio; read their number with `numerical_value()`. A
dimensionless kind of your own works the same way: a level in dB is no plain
number, but L / (20 dB) is one.

## Sums run in the common unit

`+`, `-`, `%` and the comparisons convert both sides to the **common unit**,
whose magnitude is the gcd of the two. km + m runs in m; ft + m runs in
1/1250 m, printed `[1/381 ft]`, and m + ft is the same type. Two spellings of
one size (J and N·m) add in the shorter one. Both conversions are exact
rescales, then inside's `+` runs, so the result is exact and its grid is the
tight hull of the operands. (mp-units forms the sum from the unscaled numbers;
here a valid mixed-unit sum cannot overflow.)

`*` and `/` multiply the units and specs, and inside's `*` and `/` the
numbers. A divisor whose grid holds 0 gives `std::expected<quantity, errc>`.
`pow<N>(q)` is a repeated exact product.

## Quantity kinds

The ISQ tree follows mp-units: length → width → radius, length → altitude →
height, energy → mechanical work → kinetic energy, and so on.

- **Implicit up the tree**: a height is a length.
- **Implicit from a matching recipe**: width·height → area, mass·speed² →
  kinetic energy, m/s (no kind) → speed. A recipe that reaches a named root by
  dimension converts too (force·length → energy); a named child with another
  recipe does not (force·length ↛ torque).
- **Explicit down or sideways** within one kind:
  `quantity_cast<isq::height>(length)`.
- **Never across an `is_kind` boundary.** Angular measure is dimensionless but
  no plain ratio; phase angle and solid angle are kinds of their own. Separate
  roots of one dimension never mix: Hz (frequency) + Bq (activity), and Gy +
  Sv, do not compile.

A unit names the kind it measures: `si::hertz` a frequency, `si::metre` any
length. A quantity of a bare unit takes every spec of that kind, and
`isq::activity[si::hertz]` is an error. Adding two specs gives their nearest
common ancestor: width + height is a length.

## Points and origins

`quantity_point<R, O, Rep>` counts a quantity from an origin. An origin is
absolute (`si::absolute_zero`) or at an exact offset from one: `si::ice_point`
is absolute zero + 273.15 K, `international::zeroth_degree_Fahrenheit` is
45967/180 K. Changing the origin shifts the grid by the offset in the point's
unit, and inside's offset grids keep that exact:

```cpp
auto room = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
auto k    = room.point_for(si::absolute_zero).in(si::kelvin);   // 293.15 K
room.point_in(international::degree_Fahrenheit);                // 68.00 ℉
k == room;                                                      // true: one absolute origin
```

| Spelling | Gives |
|---|---|
| `point<U>(v)` | a point counted from U's origin (K, ℃ and ℉ have one), else the dimension's zero |
| `p.point_in(u)` | the same point in `u`, counted from `u`'s origin: 350 ℉ → 530/3 ℃ |
| `p.in(u)` | the same point in `u`, keeping the origin |
| `p.quantity_from(o)`, `p.quantity_from_zero()` | the quantity from origin `o`, or from the absolute zero: 20 ℃ → 293.15 ℃ |
| `value_cast<Rep2>(p)`, `value_cast<u, Rep2>(p)` | the point with its quantity cast, keeping the origin: an oven onto whole degrees |
| point ± quantity, point − point | a point; a quantity |
| `si::absolute_zero + q` | a point |

Points of two unrelated absolute origins have no difference and do not
compare. A point has no `+` with another point, so a mean of points is the
affine one: ref + Σ w·(p − ref) / Σ w ([`sensor_fusion.cpp`](../examples/sensor_fusion.cpp)).

## Units of your own

A `quantity_spec`, `named_unit` or `point_origin` is an `inline constexpr`
object, and a tag made from it is used like the catalog's:

```cpp
inline constexpr quantity_spec depth_def{"depth", isq::length};
inline constexpr auto          depth = spec_c<depth_def>;
inline constexpr named_unit    fathom_def{"ftm", mag<rational{18288, 10000}> * si::metre};
inline constexpr auto          fathom = unit_c<fathom_def>;
inline constexpr point_origin  sea_level_def{"sea_level", dim_length};
inline constexpr auto          sea_level = origin_c<sea_level_def>;
```

A `named_unit` is either the base unit of a kind (`{"EUR", money}`, or with a
magnitude `{"g", mag<rational{1, 1000}>, isq::mass}`) or a unit defined by
others (`{"ftm", mag<…> * si::metre}`, or with a kind
`{"Hz", one / si::second, isq::frequency}`). A last `unit_options` argument
sets the rest, each optional, as designated initializers in this order:

| Option | Means | Default |
|---|---|---|
| `.ascii` | the ASCII symbol | the symbol itself |
| `.kind` | the kind it measures | its definition's |
| `.origin` | the default origin of its points | none |
| `.space_before = false` | no space before the symbol (`90°`) | a space |

```cpp
inline constexpr named_unit rankine_def{"°R", mag<rational{5, 9}> * si::kelvin,
                                        {.ascii = "R", .origin = si::absolute_zero}};
inline constexpr named_unit rpm_def{"rpm", one / si::minute, {.kind = isq::frequency}};
inline constexpr named_unit turn_def{"tr", mag<2> * mag_pi * si::radian, {.space_before = false}};
inline constexpr quantity_spec level_def{"level", isq::dimensionless, is_kind};   // dB: no plain number
inline constexpr named_unit    decibel_def{"dB", one, spec_c<level_def>};
```

### Base dimensions of your own

Next to the seven SI base dimensions, a `base_dimension` adds one of yours (up
to four in one dimension). Its base quantity and units follow as usual:

```cpp
inline constexpr base_dimension currency_def{"currency"};
inline constexpr quantity_spec  money_def{"money", currency_def};
inline constexpr named_unit     euro_def{"EUR", spec_c<money_def>};
inline constexpr named_unit     cent_def{"ct", mag<rational{1, 100}> * unit_c<euro_def>};
```

Money then never adds to a length or a plain number, EUR/kg is a dimension of
its own, and EUR + ct runs in ct. The same shape gives hit points, pixels,
packets, gas, or ADC counts. Two currencies whose rate changes are two base
dimensions, and the rate is a quantity of USD/ETH ([`wei.cpp`](../examples/wei.cpp)).

For ADC counts, a gain in cts/V is not a 1/V. With a gain type whose range
excludes 0, (raw − offset) / gain is a plain voltage; checking a two-point
calibration's gain into that type once with `try_value_cast` means no later
division can fail ([`adc_calibration.cpp`](../examples/adc_calibration.cpp)).

An entry is identified by its content, not its address: a spec by its name,
dimension, recipe, kind boundary and parent chain; a unit by its symbols,
dimension, magnitude, kind and origin. A unit holds at most 6 named units, a
recipe at most 6 specs, and an exponent stays within ±127; past that is a
compile error.

## Math (units_math.hpp)

| Function | Takes | Gives |
|---|---|---|
| `abs`, `floor`, `ceil`, `round`, `trunc` | any quantity | the same unit |
| `sqrt`, `cbrt` | a unit with that root | the root of the unit and spec: m² → m, area → length |
| `sin`, `cos`, `tan` (and `tan_into`) | an angle in rad | a plain inside |
| `sin_into`, `cos_into` | an angle in rad, or in a unit with a factor of π | a plain inside |
| `asin`, `acos`, `atan` (and `_into`) | an inside or a dimensionless quantity | rad |
| `atan2(y, x)` | two quantities of one dimension | rad |
| `hypot(x, y)` | two quantities of one dimension | their common unit |
| `exp_into`, `exp2_into`, `log_into`, `log2_into`, `log10_into`, `sinh_into` … `atanh_into` | an inside or a dimensionless quantity | a plain inside |
| `pow_base_into<Out, B>`, `pow_into<Out>` | the same | a plain inside |
| `value_cast<si::radian, Out>(q)` | an angle in a unit with a factor of π | rad, rounded once |

Each rounds the way its inside function does. A deduced form takes its
input's grid, which needs a rounding mode. An `_into<Out>` form names the
result's number: `Out` rounds, and the input needs no rounding mode, so an
arithmetic result goes straight in: `sqrt_into<Out>(mu / r)`. As with inside,
a grid that reaches below 0 makes `sqrt_into` return an `expected`.

**Angles with a factor of π.** `sin_into` and `cos_into` take an angle in a
degree, a turn (`mag<2> * mag_pi * si::radian`) or a phase slot of
2π/65536 rad: any unit whose magnitude is r·π for a rational r. The angle is
x·r half turns, so it is reduced by quarter turns exactly, and only the rest,
at most π/4, is approximated. The result rounds once, and Niven's rational
values (0, ±1/2, ±1) are exact under any rounding mode: `cos_into<Out>` of
120° is −1/2 even with `round_floor`. The deduced `sin` and `cos`, and `tan`,
take radians only.

**Numbers.** 10^(L / 20 dB) is
`units::pow_base_into<gain_t, 10>(level / (just<20> * dB))`: the ratio of two
levels is a number ([`decibels.cpp`](../examples/decibels.cpp)).

## inside's algorithms on quantities

The rest of inside's vocabulary works on quantities. Each one converts its
operands exactly into the target's unit, then calls the inside function on the
numbers, so it still rounds once and checks once.

| Spelling | Does |
|---|---|
| `q.on_wrap(f) += d`, `on_clamp`, `on_error`, `on_overflow` | a store with a hook: `d` is a quantity, converted as `+=` does; `f` gets the quantity, then the carry, overshoot or errc |
| `++q`, `q--` | one unit up or down, under the number's policy |
| `Q::try_make<F>(v)` | the number's `try_make`, of a number in Q's unit |
| `mul_into<Out, F>(a, b)`, `div_into<Out, F>(a, b)` | a·b or a/b rounded once into `Out`, a quantity type (or a plain inside for a dimensionless result); the factor between units folds into the exact product |
| `add_all(…)`, `mul_all(…)`, `sum<Q>(range)` | the folds; `sum<Q>` takes quantities in any unit that converts to Q's |
| `divmod(a, b, args…)` | in the common unit: the quotient a number, the remainder a quantity |
| `parse<Q, F>(text)`, `parse_exact<Q>(text)` | the number, read in Q's unit |
| `conversion_overflows<Q>(v)`, `conversion_rounds<Q>`, `conversion_loses<Q>` | a number in Q's unit against Q's grid |
| `clamp_cast<Q, Round>(x)`, `wrap_cast<Q>(x)` | a number in Q's unit, or a quantity converted to it first |
| `dot`, `cross`, `lerp` | on quantity components: `dot` of two px vectors is px²; `lerp`'s t is a number |

```cpp
quantity<si::second, inside<{0, 60, bounds::half_open}, wrap>> s{50};
s.on_wrap([&](auto&, auto carry) { /* carry periods of 60 s */ }) += just<2> * si::minute;   // 50 s, carry 2
auto [h, rest] = divmod(quantity<si::second, inside<{0, 86399}>>{3725}, just<1> * si::hour);  // 1, 125 s
```

An `expected` quantity, from a division by a grid that holds 0, chains through
`+ - * / %` as an `expected` inside does; the first error wins.

`values<T>` and `cursor<T, Step>` take insides only: loop over the number's
grid and attach the unit ([storage.md](storage.md#iterating-quantities)). A
quantity has no `std::hash` or `std::numeric_limits` of its own; use its
`numerical_value()`.

## Text

`to_string(q)`, `operator<<` and `std::format` write the number and then the
symbol: `5 km`, `10.5 m/s`, `kg m²/s²`, `90°`, `20 ℃`. A unit with only a
denominator shows negative exponents: `2 s⁻¹`, `1 m⁻¹ s⁻¹`. A format spec
applies to the number (`{:.2f}`, `{:>8}`). `unit_symbol(u, text_encoding::ascii)`
gives the ASCII spelling: `kg m^2/s^2`, `um`, `ohm`.

## The catalog

| Namespace | Holds |
|---|---|
| `isq` | about 80 specs: base quantities, space and time, mechanics, thermodynamics, electromagnetism, light and radiation; `time` and `work` are aliases |
| `si` | the base units, the 22 named derived units, the prefixes quecto … quetta, the accepted non-SI units min, h, d, au, °, ′, ″, a, ha, L, t; the eV and the Da |
| `si2019` | the defining constants as units (Δν_Cs, c, K_cd, h, e, k, N_A) and g₀: `just<1> * si2019::speed_of_light_in_vacuum` is c exactly |
| `international` | yd, ft, in, mi, nmi, lb, oz, lbf, kn, ℉ |
| `units` | `percent` (%) and `per_mille` (‰): dimensionless |

[constants.md](constants.md) adds the constants that follow from the SI ones
(ħ, R, F, K_J, R_K, G₀, Φ₀, σ, c₁, c₂), the standard atmosphere and the CODATA
2022 measured values, as quantities of their kind.

## Big numbers and the standard library

Grid numbers have any size, so the prefixes past 10^±18, the eV, the Da and
the SI constants h, e, k and N_A are exact; E = h·ν comes out exact.

units.hpp includes io.hpp, `<ostream>` and `<string>`, so it needs a hosted
standard library ([freestanding.md](freestanding.md)). In the single header
the units sit inside the string guard: `BEMAN_INSIDE_NO_STRING` drops them
along with io.hpp, and `BEMAN_INSIDE_NO_UNITS` drops only units.hpp,
units_math.hpp and the physical constants. Under `BEMAN_INSIDE_MATH_NO_FP` on a hosted build, everything
here works: neither header uses floating point.

## Compile time

`tests/beman/inside/units_compile_time.sh <cxx> include [<std>]` times four
translation units (best of three). On the maintainer's machine:

| Compiler | inside + io | units.hpp | 200 inside expressions | as quantities |
|---|---:|---:|---:|---:|
| GCC 16, C++26 | 0.89 s | 1.13 s | 4.37 s | 6.70 s |

The header adds about 0.25 s. Each of the 200 expressions uses its own grid,
so most of the time is inside's arithmetic on those grids; units add about
12 ms per expression.

A one-off comparison with mp-units (master 5df0314, 2026-10-16, C++23 builds,
a `double` representation, the same five expression shapes, one fixed grid on
the inside side):

| Compiler | mp-units si + isq | units.hpp | 200 expressions: mp-units | 200 expressions: units.hpp |
|---|---:|---:|---:|---:|
| GCC 15 | 1.90 s | 0.61 s | 2.07 s | 0.81 s |
| Clang 20 | 3.39 s | 0.92 s | 3.59 s | 1.24 s |

Most of the difference is parsing the headers. Per expression the two are
about the same on GCC; on Clang units.hpp is somewhat slower.

## Differences from mp-units

- The number is always an inside: no `double` or `int` representations, no
  vector or tensor characters.
- No logarithmic units. A level such as dB is a dimensionless kind of your
  own, which adds and subtracts; its conversion to a ratio is an explicit
  `pow_base_into`.
- Conversions and mixed-unit sums are exact, and the result grids are
  inferred.
- A unit's symbol is part of its identity: J and N·m are different units of
  one magnitude, and they convert implicitly and for free.
- A factor of π rounds once through `value_cast`, never implicitly; trig of a
  π-scaled angle reduces it exactly.
- The kind rules are a simplified form of mp-units': a recipe matches factor by
  factor, and other recipes need `quantity_cast`.
- inside's policies, hooks and algorithms (`on_wrap`, `sum`, `mul_into`,
  `parse`) work on quantities.
