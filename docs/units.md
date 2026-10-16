# Physical units

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`#include <beman/inside/units.hpp>` adds physical quantities on top of
inside: units, dimensions, the ISQ quantity kinds and quantity points, with
an inside as the number. The design follows [mp-units](https://github.com/mpusz/mp-units),
but the library does not depend on it. A grid is a compile-time value, so a
unit conversion only rescales the grid: it is exact, it never fails, and it
costs nothing at runtime.

```cpp
#include <beman/inside/units.hpp>
using namespace beman::inside;
using namespace beman::inside::units;

constexpr auto km = si::kilo(si::metre);

quantity<km, inside<{{0, 500}, per<1000>}>> a{12.5};   // 12.5 km, to the metre
auto b     = just<800> * si::metre;                      // 800 m, a point grid
auto total = a + b;                                      // 13300 m — exact
auto mm    = total.in(si::milli(si::metre));             // 13300000 mm, same raw
auto v     = total / quantity<si::minute, inside<{1, 600}>>{20};
std::cout << v.in(km / si::hour);                        // 39.9 km/h
```

The examples [`units.cpp`](../examples/units.cpp) (a tour),
[`circuit.cpp`](../examples/circuit.cpp) (Ohm's law, an RC filter),
[`moon_rocket.cpp`](../examples/moon_rocket.cpp) (Δv, orbits, a transfer
time) and [`kitchen.cpp`](../examples/kitchen.cpp) (mixed kitchen units,
oven temperatures) check their own output.

`<beman/inside/units_math.hpp>` adds the functions that need the math
engine (cmath.hpp). It is a separate header so that units.hpp keeps a low
compile time.

## The model

| | What it is | Spelled |
|---|---|---|
| dimension | exponents of L M T I Θ N J | `dimension_of(si::newton.value)` |
| magnitude | an exact rational × π^k, relative to the coherent SI unit | `mag<1000>`, `mag_pi` |
| unit | named units to integer powers, with decimal prefixes and a scale | `si::kilo(si::metre) / si::hour` |
| quantity spec | a kind in the ISQ tree: `height` → `altitude` → `length` | `isq::height` |
| reference | a unit and a spec | `isq::height[si::metre]`, or a bare unit |
| quantity | a number (an inside) of a reference | `quantity<si::metre, inside<{0, 10}>>` |
| quantity point | a quantity counted from an origin | `point<si::degree_Celsius>(t)` |

Units, specs and references are **values** combined by `consteval`
functions. They are not type lists that the compiler has to sort and
simplify. The names you write (`si::metre`, `isq::height`) are empty tag
objects whose type carries the value. `si::metre * si::second / si::second`
is `si::metre`, and `m·s` is the same unit as `s·m`.

The representation is always an inside. A raw scalar has no grid, so
`42 * si::metre` and `q * 2` do not compile; the error message says to
write `just<42>`.

An operation that cannot happen is a constraint failure, as on inside:
`requires { a + b; }` is false for a length and a duration, for Hz and Bq,
for `q * 2`, for `q.in(si::second)` on a length, and for any number
operation inside rejects (an off-notch `+=`, a product past 64-bit grids).
A call says why: each unit reason is a deleted overload with a message
(`= delete("…")`, C++26; GCC 15+ and Clang 19+ accept it in C++23, older
compilers report a plain deleted function). As with insides,
`quantity<si::metre, inside<{0, 100}>>{42}` constructs from a number in the
quantity's unit.

## Conversions are grid rescales

`q.in(u)` multiplies the grid's interval and notch by the factor between
the two units and keeps the raw. km → m on `inside<{0, 10}>` gives
`inside<{0, 10000}, notch 1000>` with the same raw. The codegen guard pins
this: km → mm compiles to a single `mov`. m → km makes the notch 1/1000, and
inch → mm is ×25.4. Both are exact. The rescaled number keeps the policy
(clamp, wrap, rounding).

| Function | Gives |
|---|---|
| `q.in(u)` / `q.in<u>()` | the same quantity in unit `u` (exact) |
| `q.numerical_value()` / `q.numerical_value_in(u)` | the inside |
| `value_cast<Rep2>(q)` / `try_value_cast<Rep2, F>(q)` | another representation, through Rep2's constructor / `try_make<F>` |
| `try_value_cast<u, Rep2, F>(q)` | in unit `u`, through `Rep2::try_make<F>` |
| `value_cast<u, Rep2>(q)` | in unit `u`, stored into Rep2 |
| `value_cast<Q>(q)` / `try_value_cast<Q, F>(q)` | into the quantity type `Q`: its unit and representation; Q's spec must take q's, as for an implicit conversion |
| `quantity_cast<isq::height>(q)` | another spec (down the tree, or across one kind) |
| `q.quantity_spec` | the spec as a value: `q.quantity_spec == isq::length` |

A magnitude scales a unit from either side: `mag<2> * si::metre`,
`si::metre * mag<2>`, `si::metre / mag_pi`.

A conversion that implicitly converts the spec and gives a number Rep
takes implicitly is itself implicit. For example, `quantity<si::joule, R>`
takes an N·m quantity.

### Factors of π round once

A degree is π/180 rad, so no grid represents that conversion exactly.
`.in()` refuses it with a message.
`value_cast<si::radian, Out>(q)` from units_math.hpp computes the exact
x·π/180 with π to as many bits as Out needs, and rounds it once onto Out's
grid. That is the same correctly rounded engine the math functions use, and
Out needs a rounding mode. Only factors of π^±1 are supported.

π can also scale a quantity: `mag_pi * q` (and `mag<r> * q`, `q / mag_pi`)
multiplies the **unit**, not the number. `mag_pi * just<2> * si::second`
is the number 2 in the unit `[π s]`, so π is never approximated. A later
`value_cast<si::hour, Out>(…)` rounds it once, with the true π. This is how
to write π·a·√(a/μ) or 1/(2πRC): `math::pi` is a rational within 2^-58 and
would make the grid numbers grow with every product.

### Dimensionless quantities are numbers

A quantity of dimension one converts to an inside: implicitly when its
number does, and explicitly when the target must round. The number is its
value in unit one, so (21 kg) / (6 kg) is 3.5, and (3 km) / (2 m) is 1500.
An angle is the exception. Radians and steradians are `is_kind` quantities,
so they stay quantities, as are π-scaled ratios; read their number with
`numerical_value()`.

## Sums run in the common unit

`+`, `-`, `%` and the comparisons convert both sides to the **common
unit**. Its magnitude is the gcd of the two magnitudes: km + m runs in m,
and ft + m runs in 1/1250 m, the smaller unit scaled, printed as
`[1/381 ft]`; m + ft is the same type. Two spellings of one size (J and
N·m) add in the shorter one, in either order. Both conversions are exact rescales, and then inside's `+` runs. The result is
exact, and its grid is the tight hull of the two operands. In mp-units the
sum is formed from the unscaled representations; here mixed units cannot
overflow a valid sum.

`*` and `/` multiply the units and specs, and inside's `*` and `/` the
numbers. Division keeps inside's error story: a divisor whose grid holds 0
gives `std::expected<quantity, errc>`. `pow<N>(q)` is a repeated exact
product.

## Quantity kinds

The ISQ tree is copied from mp-units: length → width → radius, length →
altitude → height, energy → mechanical work → kinetic energy, and so on.
The conversions follow mp-units:

- **implicit up the tree**: a height is a length;
- **implicit from a matching recipe**: width·height → area,
  mass·speed² → kinetic energy, m/s (no kind) → speed. A recipe that
  reaches a named root by dimension converts implicitly too
  (force·length → energy), but a named child with another recipe does not
  (force·length ↛ torque);
- **explicit down or sideways** within one kind: `quantity_cast<isq::height>(length)`;
- **never across an `is_kind` boundary**. Angular measure is dimensionless
  but no plain ratio; phase angle and solid angle are separate kinds too.
  Separate roots of one dimension never mix: Hz (frequency) + Bq
  (activity), and Gy + Sv, are compile errors.

A unit names the kind it measures: `si::hertz` measures a frequency and
`si::metre` any length. A quantity built from a bare unit takes every spec
of that kind. `isq::activity[si::hertz]` is an error.

Adding two specs gives their nearest common ancestor: width + height is a
length.

## Points and origins

`quantity_point<R, O, Rep>` counts a quantity from an origin. An origin is
absolute (`si::absolute_zero`) or sits at an exact offset from one
(`si::ice_point` is absolute zero + 273.15 K, and
`international::zeroth_degree_Fahrenheit` is 45967/180 K). Changing the
origin shifts the grid by the offset in the point's unit. Inside's affine
grids keep that exact:

```cpp
auto room = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
auto k    = room.point_for(si::absolute_zero).in(si::kelvin);   // 293.15 K
room.quantity_from(si::absolute_zero);                          // 293.15 ℃ above absolute zero
k == room;                                                      // true: one absolute origin
```

`point<U>(v)` uses the unit's origin (K, ℃ and ℉ have one), and falls back
to the implicit zero of the dimension. `p.point_in(u)` is the same point in unit `u`,
counted from `u`'s own origin (350 ℉ → 530/3 ℃). `p.in(u)` keeps the
origin. `p.quantity_from_zero()` is the quantity from the absolute origin of
the dimension (20 ℃ → 293.15 ℃ above absolute zero). Points of two
unrelated absolute origins have no difference and do not compare. `value_cast<Rep2>(p)` and `value_cast<u, Rep2>(p)` cast a point's
quantity and keep its origin, which rounds an oven temperature onto whole
degrees. Point ± quantity gives a point, and
point − point gives a quantity. `si::absolute_zero + q` makes a point.

## Math (units_math.hpp)

| Function | Unit of the result |
|---|---|
| `abs`, `floor`, `ceil`, `round`, `trunc` | the same |
| `sqrt`, `cbrt` | the root of the unit and spec (m² → m, area → length) |
| `sin`, `cos`, `tan` | a plain inside; the angle must be in radians |
| `asin`, `acos`, `atan`, `atan2(y, x)` | radians |
| `hypot(x, y)` | the common unit |

Each one rounds the way its inside function does. The deduced forms take
the input's grid, which needs a rounding mode.

Each also has an `_into<Out>` form (`sqrt_into<Out>(q)`,
`atan2_into<Out>(y, x)`, …) that names the result's number. There `Out`
rounds and the input needs no rounding mode. An arithmetic result is
`checked` (no rounding mode), so it goes straight in: `sqrt_into<Out>(mu / r)`.
As with inside, a grid that reaches below 0 makes `sqrt_into` return an
expected (`domain_error` on a negative value). `asin_into`, `acos_into` and
`atan_into` also take a dimensionless quantity.

## Text

`to_string(q)`, `operator<<` and `std::format` write the number and then
the symbol: `"5 km"`, `"10.5 m/s"`, `"kg m²/s²"`, `"90°"`, `"20 ℃"`. A
unit with only a denominator shows negative exponents: `"2 s⁻¹"`,
`"1 m⁻¹ s⁻¹"`.
A `std::format` spec applies to the number (`{:.2f}`, `{:>8}`).
`unit_symbol(u, text_encoding::ascii)` gives `"kg m^2/s^2"`, `"um"` and
`"ohm"`.

## The catalog

- **`isq`**: base quantities, space and time, mechanics, thermodynamics,
  electromagnetism, light and radiation. About 80 specs, and `time` and
  `work` are aliases.
- **`si`**: base units, the 22 named derived units, prefixes (atto–exa, and
  quecto–quetta under C++26 big grids) and the accepted non-SI units (min,
  h, d, au, °, ′, ″, a, ha, L, t). The eV and Da need big grids.
- **`si2019`**: the defining constants as units (Δν_Cs, c, K_cd, and
  under big grids h, e, k, N_A) and g₀. `just<1> * si2019::speed_of_light_in_vacuum`
  is c exactly.
- **`international`**: yd, ft, in, mi, nmi, lb, oz, lbf, kn, ℉.

You define your own entries the same way: a `quantity_spec`, `named_unit`
or `point_origin` as an `inline constexpr` object, plus a tag made from it:

```cpp
inline constexpr quantity_spec  depth_def{"depth", isq::length};
inline constexpr auto           depth = spec_c<depth_def>;
inline constexpr named_unit     fathom_def{"ftm", mag<rational{18288, 10000}> * si::metre};
inline constexpr auto           fathom = unit_c<fathom_def>;
inline constexpr point_origin   sea_level_def{"sea_level", dim_length};
inline constexpr auto           sea_level = origin_c<sea_level_def>;
```

An entry is identified by its content, not its address: a spec by its
name, dimension, recipe, kind boundary and parent chain; a unit by its
symbols, dimension, magnitude, kind and origin. A spec that reuses a
catalog name under another parent is another spec. A unit holds at most 6
named units (atoms), a recipe at most 6 specs, and an exponent stays
within ±127; past that is a compile error.

## C++23 and C++26

Everything works under C++23 with 64-bit grid numbers. Under C++26
reflection (`BEMAN_INSIDE_BIG_GRIDS`), grid numbers have any size, which
adds the prefixes past 10^±18, the eV, the Da and the SI constants h, e, k
and N_A. With those, E = h·ν comes out exact.

In the single header, units.hpp and units_math.hpp sit inside the string
guard. `BEMAN_INSIDE_NO_UNITS` drops them.

## Compile time

`tests/beman/inside/units_compile_time.sh <cxx> include [<std>]` times four
TUs (best of three). A run on the maintainer's machine:

| Compiler | inside + io | units.hpp | 200 inside expressions | as quantities |
|---|---:|---:|---:|---:|
| GCC 15, C++23 | 0.54 s | 0.61 s | 3.69 s | 5.06 s |
| GCC 16, C++26 | 0.80 s | 0.97 s | 4.01 s | 5.67 s |
| Clang 20, C++23 | 0.83 s | 0.93 s | 6.58 s | 7.92 s |

The header adds about 0.1 s. Each of the 200 expressions uses its own grid,
and most of the time goes to inside's arithmetic on those grids. Units add
about 7 ms per expression on top.

A one-off comparison with mp-units (master 5df0314, 2026-10-16, `double`
representation, the same five expression shapes with one fixed grid on the
inside side) gave:

| Compiler | mp-units si + isq | units.hpp | 200 expressions: mp-units | 200 expressions: units.hpp |
|---|---:|---:|---:|---:|
| GCC 15 | 1.90 s | 0.61 s | 2.07 s | 0.81 s |
| Clang 20 | 3.39 s | 0.92 s | 3.59 s | 1.24 s |

Most of the difference is the cost of parsing the headers. Per expression
the two are about the same on GCC; on Clang, units.hpp is somewhat slower.

## Differences from mp-units

- The representation is always an inside. There are no `double` or `int`
  reps, no vector or tensor characters, and no logarithmic units.
- Conversions and mixed-unit sums are exact, and the result grids are
  inferred.
- A unit's symbol is part of its identity: J and N·m are different units
  with the same magnitude, and they convert implicitly and for free.
- A factor of π rounds once through `value_cast`. It is never implicit.
- The kind rules are a simplified version of mp-units': a recipe matches
  factor by factor, and other recipes need `quantity_cast`.
