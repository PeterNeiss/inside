# The units model

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

How `<beman/inside/units.hpp>` represents units, dimensions, kinds and
origins, the full list of operations on quantities, the catalog, and what the
header costs to compile. [Units](../guide/units.md) has the everyday view.

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
functions, not type lists that the compiler has to sort and simplify. The
names you write (`si::metre`, `isq::height`) are empty tags whose type carries
the value. `si::metre * si::second / si::second` is `si::metre`, and m·s is
the same unit as s·m.

The number is always an inside. A raw scalar has no grid, so `42 * si::metre`
and `q * 2` do not compile, and the message says to write `just<42>`.

An operation that cannot happen is a constraint failure: `requires { a + b; }`
is false for a length and a duration, for Hz and Bq, for `q * 2`, for
`q.in(si::second)` on a length, and for any number operation inside rejects. A
call says why: each reason is a deleted overload with a message
([diagnostics](diagnostics.md#named-reasons-for-a-unit-mismatch)).

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
refuses it with a message. `value_cast<si::radian, Out>(q)` (units/math.hpp)
computes the exact x·π/180 with π to as many bits as `Out` needs, and rounds
once onto `Out`, which needs a rounding mode. Only factors of π and 1/π are
supported.

π can scale a quantity too: `mag_pi * q` multiplies the **unit**, not the
number. `mag_pi * just<2> * si::second` is the number 2 in the unit [π s], so
π is never approximated, and a later `value_cast<si::hour, Out>(…)` rounds
once, with the true π. That is how to write π·a·√(a/μ) or 1/(2πRC):
`math::pi` is a rational within 2⁻⁵⁸, and would make the grid numbers grow
with every product.

### Dimensionless quantities are numbers

A quantity of dimension one converts to an inside: implicitly when its number
does, explicitly when the target must round. The number is its value in unit
one: (21 kg) / (6 kg) is 3.5, (3 km) / (2 m) is 1500, and 8 % is 2/25;
`q.as<T>()` and `q.to<T>()` read it as a built-in, as on an inside. An angle
stays a quantity: radians and steradians are `is_kind` quantities, and so is
a π-scaled ratio; read their number with `numerical_value()`. A dimensionless
kind of your own works the same way: a level in dB is no plain number, but
L / (20 dB) is one.

## Sums run in the common unit

`+`, `-`, `%` and the comparisons convert both sides to the **common unit**,
whose magnitude is the gcd of the two. km + m runs in m; ft + m runs in
1/1250 m, printed `[1/381 ft]`, and m + ft is the same type. Two spellings of
one size (J and N·m) add in the shorter one. Both conversions are exact
rescales, then inside's `+` runs, so the result is exact and its grid is the
tight hull of the operands. (mp-units forms the sum from the unscaled
numbers; here a valid mixed-unit sum cannot overflow.)

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
bool same = k == room;                                          // true: one absolute origin
```

| Spelling | Gives |
|---|---|
| `point<U>(v)` | a point counted from U's origin (K, ℃ and ℉ have one), else the dimension's zero |
| `delta<U>(v)` | a difference in U, to add to a point |
| `p.point_in(u)` | the same point in `u`, counted from `u`'s origin: 350 ℉ → 530/3 ℃ |
| `p.in(u)` | the same point in `u`, keeping the origin |
| `p.point_for(o)` | the same point, counted from origin `o` |
| `p.quantity_from(o)`, `p.quantity_from_zero()` | the quantity from origin `o`, or from the absolute zero: 20 ℃ → 293.15 ℃ |
| `value_cast<Rep2>(p)`, `value_cast<u, Rep2>(p)` | the point with its quantity cast, keeping the origin: an oven onto whole degrees |
| point ± quantity, point − point | a point; a quantity |
| `si::absolute_zero + q` | a point |

Points of two unrelated absolute origins have no difference and do not
compare. A point has no `+` with another point, so a mean of points is the
affine one: ref + Σ w·(p − ref) / Σ w
([`sensor_fusion.cpp`](../../examples/sensor_fusion.cpp)).

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
to four in one dimension). Its base quantity and units follow as usual.
Money then never adds to a length or a plain number, EUR/kg is a dimension of
its own, and EUR + ct runs in ct. The same shape gives hit points, pixels,
packets, gas, or ADC counts. Two currencies whose rate changes are two base
dimensions, and the rate is a quantity of USD/ETH
([`wei.cpp`](../../examples/wei.cpp)).

For ADC counts, a gain in cts/V is not a 1/V. With a gain type whose range
excludes 0, (raw − offset) / gain is a plain voltage; checking a two-point
calibration's gain into that type once with `try_value_cast` means no later
division can fail ([`adc_calibration.cpp`](../../examples/adc_calibration.cpp)).

### Identity and limits

An entry is identified by its content, not its address: a spec by its name,
dimension, recipe, kind boundary and parent chain; a unit by its symbols,
dimension, magnitude, kind and origin. A unit holds at most 6 named units, a
recipe at most 6 specs, and an exponent stays within ±127; past that is a
compile error.

## inside's algorithms on quantities

The rest of inside's vocabulary works on quantities. Each one converts its
operands exactly into the target's unit, then calls the inside function on
the numbers, so it still rounds once and checks once.

| Spelling | Does |
|---|---|
| `q.on_wrap(f) += d`, `on_clamp`, `on_error` | a store with a hook: `d` is a quantity, converted as `+=` does; `f` gets the quantity, then the carry (a count of periods), the overshoot (a quantity in Q's unit) or the errc |
| `q.policy<F>() = d`, `q.policy(ec) += d` | a store under the number's policy plus `F`, or setting `ec` instead of raising; `=`, `+=` and `-=` take quantities, `*=` and `/=` numbers |
| `Q{x, clamp \| Round}`, `Q{x, wrap}` | a number in Q's unit, or a quantity converted to it first |
| `checked_cast<Q>(x)` | the same, stored exactly or reported |
| `Q::try_make<F>(v)` | the number's `try_make`, of a number in Q's unit |
| `min(a, b)`, `max(a, b)`, `midpoint(a, b)` | in the common unit, as `+`; `midpoint` is exact |
| `q.as<T>()`, `q.to<T>()` | a dimensionless quantity's number as a built-in |
| `++q`, `q--` | one notch of the number up or down, under its policy |
| `mul_into<Out, F>(a, b)`, `div_into<Out, F>(a, b)` | a·b or a/b rounded once into `Out`, a quantity type (or a plain inside for a dimensionless result); the factor between units folds into the exact product |
| `add_all(…)`, `mul_all(…)`, `sum<Q>(range)` | the folds; `sum<Q>` takes quantities in any unit that converts to Q's |
| `add(a, b, args…)`, `sub`, `mod`, `mul`, `div` | inside's free arithmetic with a per-call flag or `errc&`: sums in the common unit, products and quotients with the units multiplied |
| `divmod(a, b, args…)` | in the common unit: the quotient a number, the remainder a quantity |
| `dot`, `cross`, `lerp` | on quantity components: `dot` of two px vectors is px²; `lerp`'s t is a number |
| `units::parse<Q, F>(text)`, `units::parse_exact<Q>(text)` | the number, read in Q's unit |
| `conversion_overflows<Q>(v)`, `conversion_rounds<Q>`, `conversion_loses<Q>` | a number in Q's unit against Q's grid |
| `values<Q>`, `cursor<Q, Step>` | the number's grid, as quantities; `Step` is a number in Q's unit |
| `units::uniform<Q>(rng)` | units/random.hpp: a number drawn from Q's grid, in Q's unit |
| `std::hash<Q>`, `std::numeric_limits<Q>` | the number's, with the limits as quantities |

```cpp
quantity<si::second, inside<{0, 86399}>> t{3725};
auto [h, rest] = divmod(t, just<1> * si::hour);   // 1, 125 s: the remainder keeps its unit
```

An `expected` quantity, from a division by a grid that holds 0, chains
through `+ - * / %` as an `expected` inside does; the first error wins.

## The catalog

| Namespace | Holds |
|---|---|
| `isq` | about 85 specs: base quantities, space and time, mechanics, thermodynamics, electromagnetism, light and radiation; `time` and `work` are aliases |
| `si` | the base units, the 22 named derived units, the prefixes quecto … quetta, the accepted non-SI units min, h, d, au, °, ′, ″, a, ha, L, t; the eV and the Da; the origins `absolute_zero` and `ice_point` |
| `si2019` | the defining constants as units (Δν_Cs, c, K_cd, h, e, k, N_A) and g₀: `just<1> * si2019::speed_of_light_in_vacuum` is c exactly |
| `international` | yd, ft, in, mi, nmi, lb, oz, lbf, kn, ℉ |
| `units` | `percent` (%) and `per_mille` (‰): dimensionless; `one`, the unit of a number |

[Constants](constants.md) adds the constants that follow from the SI ones
(ħ, R, F, K_J, R_K, G₀, Φ₀, σ, c₁, c₂), the standard atmosphere and the
CODATA 2022 measured values, as quantities of their kind. Grid numbers have
any size, so the prefixes past 10^±18, the eV, the Da and the SI constants h,
e, k and N_A are exact; E = h·ν comes out exact.

## Headers

`units.hpp` is `units/core.hpp` (the machinery) plus `units/systems.hpp` (the
catalog); units of your own need only `units/core.hpp`. `units/math.hpp` adds
the functions that need the math engine, `units/io.hpp` the text, and
`units/random.hpp` `uniform<Q>(rng)`. They are separate headers so that
`units.hpp` alone compiles fast.

`units.hpp` needs no text support; `units/io.hpp` includes `inside/io.hpp`,
`<ostream>` and `<string>`, so printing needs a hosted standard library
([freestanding](freestanding.md)). `units/random.hpp` needs a hosted build
without `BEMAN_INSIDE_MATH_NO_FP`, as `inside/random.hpp` does. Under
`BEMAN_INSIDE_MATH_NO_FP` on a hosted build everything else works: no units
header uses floating point.

## Compile time

`tests/beman/inside/units_compile_time.sh <cxx> include [<std>]` times four
translation units (best of three). On the maintainer's machine:

| Compiler | inside | units.hpp | 200 inside expressions | as quantities |
|---|---:|---:|---:|---:|
| GCC 16, C++26 | 0.53 s | 0.72 s | 4.16 s | 5.83 s |

The header adds about 0.2 s; printing (`units/io.hpp`, through
`inside/io.hpp`) adds another 0.4 s. Units of your own need only
`units/core.hpp`, without the catalog, which saves about 0.14 s of it. Each of
the 200 expressions uses its own grid, so most of the time is inside's
arithmetic on those grids; units add about 7 ms per expression.

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
- The kind rules are a simplified form of mp-units': a recipe matches factor
  by factor, and other recipes need `quantity_cast`.
- inside's policies, hooks and algorithms (`on_wrap`, `sum`, `mul_into`,
  `parse`) work on quantities.
