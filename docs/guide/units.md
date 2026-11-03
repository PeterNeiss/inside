# Units

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`<beman/inside/units.hpp>` adds physical quantities: a number (an inside)
with a unit. Units, dimensions and quantity kinds are checked at compile time,
and you can add your own. The design follows
[mp-units](https://github.com/mpusz/mp-units), without depending on it.

```cpp
#include <beman/inside/units.hpp>
#include <beman/inside/units/io.hpp>   // printing
using namespace beman::inside;
using namespace beman::inside::units;

constexpr auto km = si::kilo(si::metre);
quantity<km, inside<{{0, 500}, per<1000>}>> leg{12.5};   // 12.5 km, to the metre
auto total = leg + just<800> * si::metre;                // 13300 m, exact
auto speed = total / quantity<si::minute, inside<{1, 600}>>{20};
std::cout << speed.in(km / si::hour);                    // 39.9 km/h
```

## Making a quantity

A quantity is `quantity<Unit, Number>`, and the number is always an inside.
There are two ways to make one:

```cpp
quantity<si::metre, inside<{0, 100}>> d{42};   // a number in the quantity's unit
auto c = just<800> * si::metre;                // a constant: a one-point inside times a unit
```

A plain number has no grid, so `42 * si::metre` and `d * 2` do not compile;
the error message says to write `just<42>` and `just<2>`.

Units combine with `*` and `/` (`km / si::hour`), take prefixes
(`si::milli(si::second)`) and powers (`square(si::metre)`). The catalog has
the SI units, the ISQ quantity kinds, and the international units: `si::`,
`isq::`, `international::` (foot, pound, ℉, …), and `percent`.

## Conversions

`q.in(u)` gives the same quantity in another unit. The grid is rescaled and
the stored integer stays, so the conversion is **exact and free**: km → mm on
`inside<{0, 10}>` is the same byte, read on a grid 10⁶ times larger.

| Write | Gives |
|---|---|
| `q.in(u)` | the same quantity in unit `u`, exactly |
| `q.numerical_value()`, `q.numerical_value_in(u)` | the number, an inside |
| `value_cast<Number>(q)` | the quantity on another number type, rounded by it |
| `Q{q, clamp}`, `Q::try_make(v)`, `checked_cast<Q>(q)` | the stores of [Policies](policies.md), on quantities |

A conversion that would not be exact does not compile: degrees to radians
multiply by π, which no grid holds. Round it once with
`value_cast<si::radian, Out>(q)` from `<beman/inside/units/math.hpp>`.

## Sums in the common unit

`+`, `-` and comparisons convert both sides to their **common unit**, exactly,
and then add: km + m runs in metres, EUR + ct in cents. A valid mixed-unit sum
cannot overflow, and its result is exact. `*` and `/` multiply the units:
metres over minutes is m/min, and `.in(km / si::hour)` turns it into km/h.

Quantities of different dimensions never mix:

```cpp
// total + just<1> * si::second;   // does not compile: a length and a duration
```

## Kinds

Quantities of one dimension can still be different *kinds*: a height is a
length, but a frequency (Hz) and an activity (Bq) are both 1/s and never add.
A height converts to a length on its own; going the other way is explicit:
`quantity_cast<isq::height>(length)`. Most programs only need the units; the
[units model](../advanced/units-model.md#quantity-kinds) has the rules.

## Temperatures and other points

A temperature is a **point** on a scale, counted from an origin, not an
amount. 20 ℃ + 5 ℃ is a point plus a difference:

```cpp
auto room   = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
room.point_in(international::degree_Fahrenheit);                 // 68.00 ℉
room.point_in(si::kelvin);                                       // 293.15 K
auto warmer = room + delta<si::degree_Celsius>(inside<{0, 10}>{5});   // 25 ℃
auto rise   = warmer - room;                                     // 5 ℃, a difference
```

`point<U>(v)` makes a point, `delta<U>(v)` a difference. Two points subtract
to a difference, a point and a difference add to a point, and two points do
not add.

## Units of your own

A unit of your own is an `inline constexpr` object plus a tag made from it,
and then it works like the catalog's:

```cpp
inline constexpr named_unit fathom_def{"ftm", mag<rational{18288, 10000}> * si::metre};
inline constexpr auto       fathom = unit_c<fathom_def>;   // 1.8288 m

auto depth = just<3> * fathom;
depth.in(si::metre);                                       // 5.4864 m
```

For something that is not a physical quantity at all, such as money, hit
points, pixels or ADC counts, add a **base dimension**. It then never mixes
with anything else:

```cpp
inline constexpr base_dimension currency_def{"currency"};
inline constexpr quantity_spec  money_def{"money", currency_def};
inline constexpr named_unit     euro_def{"EUR", spec_c<money_def>};
inline constexpr auto           EUR = unit_c<euro_def>;
inline constexpr named_unit     cent_def{"ct", mag<rational{1, 100}> * EUR};
inline constexpr auto           ct = unit_c<cent_def>;

auto price = just<5> * EUR + just<20> * ct;                // 520 ct
```

[`own_units.cpp`](../../examples/own_units.cpp) adds a depth kind, °R with an
origin, and bytes; [`money.cpp`](../../examples/money.cpp) a currency with a
tax.

## Policies and hooks on quantities

Everything an inside does, a quantity does with its number: the policy is the
number's, and the stores, hooks, `mul_into`, `sum`, `parse` and the math take
quantities in any unit of the right kind, converted exactly first:

```cpp
quantity<si::second, inside<{0, 60, bounds::half_open}, wrap>> s{50};
quantity<si::minute, inside<{0, 60, bounds::half_open}, wrap>> m{0};
s.on_wrap([&](auto&, auto carry) { m += carry * si::minute; }) += just<2> * si::minute;
// s == 50 s (170 s folds to 50 s), m == 2 min
```

## Dimensionless results

A quantity of dimension one is a number: (3 km) / (2 m) converts to an
inside holding 1500, and 8 % is 2/25. An angle is the exception: radians are
a kind of their own, so read an angle's number with `numerical_value()`.

## Text

With `<beman/inside/units/io.hpp>`, `to_string(q)`, `<<` and `std::format`
print the number and the unit's symbol: `5 km`, `39.9 km/h`, `20 ℃`, `12 m²`.
A format spec applies to the number (`{:.2f}`), and
`unit_symbol(u, text_encoding::ascii)` gives an ASCII spelling (`um`, `ohm`).
`units::parse<Q>(text)` reads a number in Q's unit.

## Details

- [The units model](../advanced/units-model.md): dimensions, magnitudes and
  π, kinds, origins, unit options, the catalog, every algorithm on
  quantities, compile time, the differences from mp-units.
- [Math](math.md#quantities): roots of units, trigonometry in degrees.
- [Constants](../advanced/constants.md): c, h, k and the other physical
  constants as quantities.
- The examples by theme are in the [map](../README.md#examples).
