# Units

`<inside/units.hpp>` adds physical quantities: a number (an inside)
with a unit. Units, dimensions and quantity kinds are checked at compile time,
and you can add your own. The design follows
[mp-units](https://github.com/mpusz/mp-units), without depending on it.

```cpp
#include <inside/units.hpp>
#include <inside/units/io.hpp>   // printing
using namespace ins;

constexpr auto km = si::kilo(si::metre);
id<"[0..500] 1/1000 [km]"> leg{12.5, km};                    // 12.5 km, to the metre
auto total = leg + 800_q * si::metre;                        // 13300 m, exact
auto speed = total / id<"[1..600] [min]">{20, si::minute};
std::cout << speed.in(km / si::hour);                        // 39.9 km/h
```

## Making a quantity

A quantity is `id<quantity_key<Unit, Number>>`, and the number is always an inside.
A number goes in with its unit named, where the value is written:

```cpp
id<"[0..100] [m]"> d{42, si::metre};                      // a number and its unit
auto c = 800_q * si::metre;                               // a constant: a one-point inside times a unit
```

`d{42}` names no unit and does not compile. Neither does `42 * si::metre`,
since a plain number has no grid, nor does `d * 2`. The error messages say
what to write: `d{42, si::metre}`, `42_q` and `2_q`. An inside in
another unit of the same quantity converts exactly first. A number comes out
with its unit named too: `d.numerical_value_in(si::metre)`.

Units combine with `*` and `/` (`km / si::hour`), take prefixes
(`si::milli(si::second)`) and powers (`square(si::metre)`). The catalog has
the SI units, the ISQ quantity kinds, and the international units: `si::`,
`isq::`, `international::` (foot, pound, ℉, …), and `percent`.

## Conversions

`q.in(u)` gives the same quantity in another unit. The grid is rescaled and
the stored integer stays, so the conversion is **exact and free**: km → mm on
`id<"[0..10]">` is the same byte, read on a grid 10⁶ times larger.

| Write | Gives |
|---|---|
| `q.in(u)` | the same quantity in unit `u`, exactly |
| `q.numerical_value_in(u)` | the number in unit `u`, an inside |
| `value_cast<Number>(q)` | the quantity on another number type, rounded by it |
| `Q{q, clamp}`, `Q{v, u, clamp}`, `Q::try_make(v, u)`, `exact_cast<Q>(q)` | the stores of [Policies](policies.md), on quantities |

A conversion that would not be exact does not compile: degrees to radians
multiply by π, which no grid holds. Round it once with
`value_cast<si::radian, Out>(q)` from `<inside/units/math.hpp>`.

## Sums in the common unit

`+`, `-` and comparisons convert both sides to their **common unit**, exactly,
and then add: km + m runs in metres, EUR + ct in cents. A valid mixed-unit sum
cannot overflow, and its result is exact. `*` and `/` multiply the units:
metres over minutes is m/min, and `.in(km / si::hour)` turns it into km/h.

A quantity compares only with a quantity it could be added to. A unit is one
of itself, in comparisons and in arithmetic: `len > si::metre` is
`len > 1 m`, `jar += international::pound` adds a pound, `divmod(t, si::hour)`
counts hours, and `-si::metre` is −1 m. Only a plain inside compares with a
number: not even a percentage or a quantity of unit `one` takes `== 5`; write
`p == 5_q * percent`.

Quantities of different dimensions never mix:

```cpp
// total + si::second;             // does not compile: a length and a duration
// total > 1000;                   // does not compile: a length is no number
```

## Kinds

Quantities of one dimension can still be different *kinds*: a height is a
length, but a frequency (Hz) and an activity (Bq) are both 1/s and never add.
A height converts to a length on its own; going the other way is explicit:
`kind_cast<isq::height>(length)`. Most programs only need the units; the
[units model](../advanced/units-model.md#quantity-kinds) has the rules.

## Temperatures and other points

A temperature is a **point** on a scale, counted from an origin, not an
amount. 20 ℃ + 5 ℃ is a point plus a difference:

```cpp
auto room   = point<si::degree_Celsius>(id<"[-50..50] 1/10">{20});
room.point_in(international::degree_Fahrenheit);                 // 68.00 ℉
room.point_in(si::kelvin);                                       // 293.15 K
auto warmer = room + delta<si::degree_Celsius>(id<"[0..10]">{5});   // 25 ℃
auto rise   = warmer - room;                                     // 5 ℃, a difference
```

`point<U>(v)` makes a point, `delta<U>(v)` a difference. Two points subtract
to a difference, a point and a difference add to a point, and two points do
not add. A point type takes a number in its own unit, as a quantity does:

```cpp
using celsius = id<"[-40..60] 0.5 [deg_C] @ice_point">;
std::vector<celsius> temps = {{21.5, si::degree_Celsius}, {-5, si::degree_Celsius}};
```

## Units of your own

A unit of your own is one declaration: its symbol, and the quantity it
equals. Then it works like the catalog's:

```cpp
inline constexpr auto fathom = unit<"ftm", 1.8288_q * si::metre>;   // a fathom is 1.8288 m

auto depth = 3_q * fathom;
depth.in(si::metre);                                       // 5.4864 m
```

(The definition is a template argument: a one-point inside such as
`1.8288_q` or `5_q`, a quantity of one and a point of one are
structural types, so your own templates can take them too.)

For something that is not a physical quantity at all, such as money, hit
points, pixels or ADC counts, declare a **base unit** with a dimension of its
own. It then never mixes with anything else:

```cpp
inline constexpr auto EUR = base_unit<"EUR", "currency">;   // a new dimension, and its unit
inline constexpr auto ct  = unit<"ct", 0.01_q * EUR>;

auto price = 5_q * EUR + 20_q * ct;                // 520 ct
```

A kind of your own is `kind<"depth", isq::length>`, an origin
`origin<"ice_point", si::absolute_zero + 273.15_q * si::kelvin>`.

A quantity's type prints as its text either way (`id<"[0..100] [ftm]">`).
Where a unit itself shows (a unit tag's type, a diagnostic that spells a
reference), a one-line unit is spelled by its whole definition; name the
definition and take its unit with `defined_by`, as the catalog does, and it
shows by name ([named definitions](../advanced/units-model.md#named-definitions-short-symbols)):

```cpp
inline constexpr unit_def fathom_def{"ftm", 1.8288_q * si::metre};
inline constexpr auto     fathom = defined_by<fathom_def>;   // printed as (& fathom_def)
```
[`own_units.cpp`](../../examples/own_units.cpp) adds a depth kind, °R with an
origin, and bytes; [`money.cpp`](../../examples/money.cpp) a currency with a
tax.

## Geometric algebra

A **basis vector** is a unit of your own whose square you state:
`geometric<+1>`, `geometric<-1>` or `geometric<0>`.
Two different ones anticommute (e₂ e₁ = −e₁ e₂), and the units track the
sign. With `<inside/units/geoalg.hpp>`, a sum of different blades is a
multivector, and complex numbers, quaternions and vectors follow:

```cpp
inline constexpr auto         e1 = basis_vector<"e₁", geometric<+1>>;
inline constexpr auto         e2 = basis_vector<"e₂", geometric<+1>>;
// i·i = -1 follows
inline constexpr auto         i = unit<"i", e1 * e2>;

auto z = (3_q + 4_q * i) * (1_q - 2_q * i);                // 11 - 2 i
auto v = (2_q * si::metre * e1 + si::metre * e2) * i;      // -1 m·e₁ + 2 m·e₂
```

Two other rules share the machinery: `basis_vector<"R", inner>` is a vector
of Rⁿ whose products are dot products, and `basis_vector<"x", polynomial>` a
polynomial's variable ([polynomials](../advanced/polynomials.md)).

[Geometric algebra](../advanced/geometric-algebra.md) has the model, the
operations and the limits; [`complex.cpp`](../../examples/complex.cpp),
[`quaternions.cpp`](../../examples/quaternions.cpp) and
[`geometric_algebra.cpp`](../../examples/geometric_algebra.cpp) use them.

## Policies and hooks on quantities

Everything an inside does, a quantity does with its number: the policy is the
number's, and the stores, hooks, `mul<C>`, `sum`, `parse` and the math take
quantities in any unit of the right kind, converted exactly first:

```cpp
id<"[0..60) [s] wrap"> s{50, si::second};
id<"[0..60) [min] wrap"> m{0, si::minute};
s.on_wrap([&](auto&, auto carry) { m += carry * si::minute; }) += 2_q * si::minute;
// s == 50 s (170 s folds to 50 s), m == 2 min
```

## Dimensionless results

A quantity of dimension one is a number: (3 km) / (2 m) converts to an
inside holding 1500, and 8 % is 2/25. An angle is the exception: radians are
a kind of their own, so read an angle's number with `numerical_value_in(si::radian)`.

## Text

With `<inside/units/io.hpp>`, `to_string(q)`, `<<` and `std::format`
print the number and the unit's symbol: `5 km`, `39.9 km/h`, `20 ℃`, `12 m²`.
A format spec applies to the number (`{:.2f}`), and
`unit_symbol(u, text_encoding::ascii)` gives an ASCII spelling (`um`, `ohm`).
`ins::parse<Q>(text)` reads it back, with its unit: `"1.5 h"` into minutes
is 90 min, in any unit that converts exactly; a bare number is
`error::missing_unit` ([reading quantities](../advanced/text.md#reading-a-quantity-with-its-unit)).

## Details

- [The units model](../advanced/units-model.md): dimensions, magnitudes and
  π, kinds, origins, unit options, the catalog, every algorithm on
  quantities, compile time, the differences from mp-units.
- [Math](math.md#quantities): roots of units, trigonometry in degrees.
- [Constants](../advanced/constants.md): c, h, k and the other physical
  constants as quantities.
- [Geometric algebra](../advanced/geometric-algebra.md): basis vectors,
  multivectors and their products.
- The examples by theme are in the [map](../README.md#examples).
