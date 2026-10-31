---
title: "Quantities on `inside`: units whose conversions are exact"
document: DxxxxR0
date: today
audience:
  - SG6 (Numerics)
  - LEWG Incubator
author:
  - name: Peter Neiss
    email: <https://github.com/PeterNeiss>
toc: true
toc-depth: 2
---

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

# Abstract

We propose `std::units::quantity<reference, inside>`: physical quantities whose
number is always an `inside`, the bounded number with an exact rational step
proposed in the companion paper [*`inside`: numbers whose range and step size
are part of the
type*](https://github.com/PeterNeiss/inside/blob/v0.6.0/docs/proposal/inside.md)
(*the number paper* below). The model is that of mp-units [@P3045R9]:
dimensions, units with exact magnitudes, the ISQ tree of quantity kinds, and
quantity points with origins.

What changes is that the number is a grid. A unit conversion rescales the
grid, its interval and its notch, and keeps the stored integer, so it is exact,
it cannot fail, and at run time it is a copy. A sum of two units runs in their
common unit, exactly, and cannot overflow. A conversion with a factor of π
(degree to radian) has no exact rescale; it is ill-formed until it is rounded
once onto a declared grid, which the third companion paper, [*Correctly
rounded math for `inside` and
quantities*](https://github.com/PeterNeiss/inside/blob/v0.6.0/docs/proposal/math.md),
provides.

This paper depends on the number paper. The reference implementation is
[beman.inside v0.6.0](https://github.com/PeterNeiss/inside/tree/v0.6.0).

# Motivation

A unit error can be as costly as an overflow: the Mars Climate Orbiter was lost
to pound-force seconds read as newton seconds. Units libraries catch that error
at compile time, by carrying the dimension and the unit in the type.

They do not carry the range or the step of the number, so a conversion
multiplies a number whose range the type does not know. With an `int`
representation, km → mm multiplies by 10⁶ and can overflow, and ft → m (×
0.3048) has no integer result; with a `double` it rounds. Libraries guard the
conversions they can judge from the representation type alone: mp-units makes
a value-truncating conversion explicit, and Au rejects an integer conversion
that would overflow for small values. Neither can know that *this* quantity
holds at most 500 km, to the metre. A sum of two units must first convert one
side, so it inherits the same problems.

With an `inside` as the number, the type knows the range and the step. A
conversion computes the converted grid at compile time and keeps the stored
integer: 12.5 km to the metre is 12 500 m to the metre, on a grid 1000 times
wider with a notch 1000 times larger, from the same bits. Nothing is
multiplied at run time, so nothing can overflow or round. A sum runs in the
common unit, whose grid holds every result, by the number paper's widening
arithmetic.

# A tour

The listings use the reference implementation's namespaces `beman::inside` and
`beman::inside::units`. Each one is a region of a program that compiles and
checks its own results.

**A conversion rescales the grid and keeps the raw; a mixed-unit sum is
exact.**

<!-- listing: rescale -->
```cpp
constexpr auto km = si::kilo(si::metre);
quantity<km, inside<{{0, 500}, per<1000>}>> trip{12.5};    // 12.5 km, to the metre
auto m = trip.in(si::metre);                               // 12500 m: the same raw, grid × 1000
static_assert(grid_of<decltype(m)::rep> == grid{0, 500'000});

auto total = trip + just<800> * si::metre;                 // 13300 m: the sum runs in metres
auto speed = total / quantity<si::minute, inside<{1, 600}>>{20};
auto kmh   = speed.in(km / si::hour);                      // 39.9 km/h, exactly
inside<{0, 1000}, round_nearest> laps = trip / (just<400> * si::metre); // 31: km / m is a number
static_assert(!addable<decltype(trip), decltype(just<1> * si::second)>); // another dimension
```

**Quantity kinds separate quantities of one dimension.**

<!-- listing: kinds -->
```cpp
quantity<si::hertz, inside<{0, 1000}>>     f{50};
quantity<si::becquerel, inside<{0, 1000}>> a{50};
static_assert(!addable<decltype(f), decltype(a)>);        // both 1/s, but not one kind

quantity<isq::width[si::metre], inside<{1, 10}>>  w{3};
quantity<isq::height[si::metre], inside<{1, 10}>> h{4};
quantity<isq::area[square(si::metre)], inside<{1, 100}>> area = w * h; // width · height
auto around = w + h;                                      // 7 m, a length: the common kind
auto tall   = quantity_cast<isq::height>(w);              // a width read as a height, explicitly
```

**Points count from an origin, and change origin exactly.**

<!-- listing: points -->
```cpp
auto room = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
auto k    = room.point_for(si::absolute_zero).in(si::kelvin);      // 293.15 K, exactly
auto f    = room.point_in(international::degree_Fahrenheit);       // 68 ℉, exactly
auto warm = room + just<5> * si::kelvin;                           // 25 ℃: a point plus a difference
auto rise = warm - room;                                           // 5 ℃, a quantity: a difference
static_assert(!addable<decltype(room), decltype(room)>);           // points do not add
```

**A base dimension, a kind and units of your own.**

<!-- listing: own -->
```cpp
inline constexpr base_dimension currency_def{"currency"};
inline constexpr quantity_spec  money_def{"money", currency_def};
inline constexpr named_unit     euro_def{"EUR", spec_c<money_def>};
inline constexpr named_unit     cent_def{"ct", mag<rational{1, 100}> * unit_c<euro_def>};
inline constexpr auto           euro = unit_c<euro_def>;
inline constexpr auto           cent = unit_c<cent_def>;

using price = quantity<euro / si::kilogram, inside<{{0, 100}, per<100>}>>;
using mass  = quantity<si::kilogram, inside<{{0, 10}, per<1000>}>>;

constexpr auto cost  = price{3.49_ins} * mass{1.25_ins};     // 4.3625 EUR, exactly
constexpr auto cents = cost.in(cent);                        // 436.25 ct: the same raw
static_assert(!addable<decltype(cost), mass>);               // money never adds to a mass
```

# Design

## The model

`quantity<R, Rep>` is a number `Rep`, always an `inside`, of a *reference*
`R`: a unit, optionally with a quantity kind (`isq::height[si::metre]`). As in
mp-units [@P3045R9]:

- a *dimension* is a product of powers of the seven ISQ base dimensions;
- a *unit* is a product of named units to integer powers, with prefixes and an
  exact *magnitude*;
- a *quantity spec* is a node in the tree of quantity kinds (height under
  length, area as length²), and a spec marked `is_kind` starts a kind of its
  own (frequency and activity are both s⁻¹);
- a *quantity point* is a quantity counted from an *origin*.

## A conversion rescales the grid

`q.in(u)` multiplies the interval and the notch of `Rep`'s grid by the
conversion factor, and keeps the stored integer. km to m is a grid 1000 times
wider and finer, holding the same raw. A conversion is therefore exact and
total, and at run time it is a copy; the reference implementation compiles it
to one `mov`. A conversion to a unit of another dimension is ill-formed.

A conversion into a quantity of a given `Rep` (`quantity<m, inside<…>>{q}`) is
the conversion followed by a store, which `Rep`'s policy checks, clamps, wraps
or rounds as in the number paper. It is implicit when the kinds allow it and
the converted number converts implicitly into `Rep`.

## A sum runs in the common unit

`a + b`, `a - b`, `a % b` and `a <=> b` convert both sides to their *common
unit*, the unit whose magnitude is the gcd of the two magnitudes, and then use
the number paper's operation. Both conversions are exact, and the sum's grid
holds every result, so a valid mixed-unit sum cannot overflow. 12.5 km + 800 m
is 13 300 m, computed in metres; its number lies on `[800, 500 800]` in steps
of 1, the sum of `[0, 500 000]` and the point 800.

`*` and `/` multiply and divide the units and the kinds, and the numbers take
the number paper's result grids, including `std::expected` for a divisor whose
grid holds 0. A quotient of dimension one (km / m) is a number: it converts to
an `inside`, implicitly when that conversion is implicit.

## Kinds

Quantities of one dimension are not always interchangeable. A height is a
length, so a height and a width add to a length, and width · height is an area.
But hertz and becquerel are both s⁻¹ and do not add, because frequency and
activity are kinds of their own. `quantity_cast<S>(q)` reads a quantity as
another spec of its kind, explicitly; it never crosses an `is_kind` boundary.

## Points and origins

A temperature is a point, counted from an origin, while a temperature
difference is a quantity. `point<si::degree_Celsius>(v)` is counted from the
ice point, and `point_for(si::absolute_zero)` counts the same point from
absolute zero: an exact shift of the grid, so 20 ℃ is exactly 293.15 K.
`point ± quantity` is a point, `point − point` a quantity, and two points do not
add. An origin is either absolute or at an exact offset from another.

## π is never approximated

A magnitude is an exact rational times a power of π. A conversion whose factor
contains π (degree to radian) has no exact rescale, so `q.in(si::radian)` on a
quantity in degrees is ill-formed, and the diagnostic names the fix: a
conversion that rounds once onto a declared grid. That conversion belongs to
the math paper (`value_cast<si::radian, Out>(q)`). Its trigonometric functions
also reduce an angle in degrees or turns exactly, so `cos` of 120° is exactly
−1/2.

## No bare scalars

`42 * si::metre` and `q * 2` are ill-formed, as `a + 1` is in the number paper:
`42` has no grid. A quantity is made from a number in its unit
(`quantity<si::metre, inside<{0, 100}>>{42}`) or from an `inside` times a unit
(`just<42> * si::metre`).

## Structural values

Dimensions, magnitudes, units, quantity specs and references are structural
values, combined by `consteval` functions. They are not type lists that the
compiler must sort, so m·s and s·m are one unit. The names a program writes
(`si::metre`, `isq::height`) are empty tag objects whose type carries the
value. An entry is identified by its content, not by its address.

## Your own

A program defines its own units, quantity specs, origins and up to four base
dimensions of its own (currency, information, pixels) as `inline constexpr`
objects. From then on they behave like the catalog's: prefixes apply to a
named unit, a spec under length is a length, and money never adds to a mass.

## Diagnostics

A unit operation that cannot happen is a constraint failure, so
`requires { a + b; }` is false for a length and a duration. A call written
anyway names its reason as a deleted overload, for example `units: adding or
comparing quantities of different kinds`.

## The policy of the number

A quantity's stores are its number's stores: `++q`, `q += r` and the
construction from a number apply `Rep`'s policy, and `q.on_wrap(f)`,
`q.on_clamp(f)` and `q.on_error(f)` attach the number paper's actions.
`Q::try_make(v)`, `clamp_cast<Q>`, `wrap_cast<Q>`, `value_cast` and
`try_value_cast` convert into a quantity type, and
`conversion_overflows<Q>(v)` and its siblings answer for a number `v` in `Q`'s
unit.

# Proposed interface

The names are the reference implementation's. `@*unit*@`, `@*spec*@` and
`@*origin*@` are exposition-only tag types: empty objects whose type carries a
`unit`, a `quantity_spec` or a `point_origin` as a value. `@*grid-number*@` is
the number paper's.

```cpp
namespace std::units {
  // dimensions and magnitudes
  struct base_dimension { string_view Name; };                 // a dimension of your own
  struct dimension;                                           // exponents of L M T I Θ N J and of base dimensions
  struct magnitude;                                           // an exact rational times a power of π
  template<@*grid-number*@ R> constexpr @*magnitude*@ mag;          // R
  inline constexpr @*magnitude*@ mag_pi;                          // π

  // units
  struct unit;                                                // named units to integer powers, prefixes, a magnitude
  struct unit_options { string_view ascii; @*spec*@ kind; @*origin*@ origin; bool space_before = true; };
  struct named_unit {                                         // m, N, °C: a static object
    constexpr named_unit(string_view symbol, @*spec*@ kind, unit_options = {});
    constexpr named_unit(string_view symbol, @*magnitude*@, @*spec*@ kind, unit_options = {});
    constexpr named_unit(string_view symbol, @*unit*@ definition, unit_options = {});
    constexpr named_unit(string_view symbol, @*unit*@ definition, @*spec*@ kind, unit_options = {});
  };
  template<const named_unit& U> constexpr @*unit*@ unit_c;
  inline constexpr @*unit*@ one;                                  // the unit 1
  // @*unit*@ * @*unit*@, @*unit*@ / @*unit*@, @*magnitude*@ * @*unit*@, pow<N>(u), square(u), cubic(u), inverse(u)
  // prefixes: kilo(u), milli(u), ... on a named unit

  // quantity kinds
  struct quantity_spec {                                      // isq::length, isq::height
    constexpr quantity_spec(string_view name, dimension);             // a base quantity
    constexpr quantity_spec(string_view name, const base_dimension&);
    constexpr quantity_spec(string_view name, @*spec*@ parent [, is_kind]);  // a child
    constexpr quantity_spec(string_view name, @*spec-expression*@ [, is_kind]); // a derived root
    constexpr quantity_spec(string_view name, @*spec*@ parent, @*spec-expression*@ [, is_kind]);
  };
  template<const quantity_spec& S> constexpr @*spec*@ spec_c;
  // @*spec*@ * @*spec*@, @*spec*@ / @*spec*@ (recipes); s[u] names a reference

  // references and quantities
  struct reference;                                           // a unit, a kind or recipe; a unit converts to one
  template<reference R, class Rep>                            // Rep: an inside
  class quantity {
  public:
    using rep = Rep;
    static constexpr units::reference reference = R;
    constexpr quantity() = default;
    template<class V> constexpr explicit quantity(V&& number);     // a number in the unit R
    template<reference R2, class V2>                               // implicit when R2's kind converts and
      constexpr explicit(@*see below*@) quantity(const quantity<R2, V2>&); // the rescaled number does
    constexpr const Rep& numerical_value() const noexcept;
    template<@*unit*@ U> constexpr auto in(U) const;                 // Rep's grid rescaled; same raw
    template<@*unit*@ U> constexpr auto numerical_value_in(U) const;
    template<class T> constexpr explicit(@*see below*@) operator T() const; // dimension one: its number
    template<policy_flag F = none, class V> static constexpr auto try_make(const V&);
    template<class F> constexpr auto on_clamp(F f);               // also on_wrap, on_error
    // ++, --, compound assignment: a store by Rep's policy
  };
  // quantity + - % <=> == quantity: in the common unit
  // quantity * / quantity, inside * @*unit*@, quantity * @*unit*@: units and kinds multiply
  // quantity * / inside: the number scales
  template<class Spec, reference R, class V> constexpr auto quantity_cast(const quantity<R, V>&);
  template<class Rep2, reference R, class V> constexpr quantity<R, Rep2> value_cast(const quantity<R, V>&);
  template<auto U, class Rep2, reference R, class V> constexpr auto value_cast(const quantity<R, V>&);
  template<class Q, reference R, class V> constexpr Q value_cast(const quantity<R, V>&);
  // try_value_cast<...>(q): the same through Rep2::try_make, an expected
  template<class Q, class X> constexpr Q clamp_cast(const X&);  // also wrap_cast
  template<class Q, class N> constexpr bool conversion_overflows(N v) noexcept; // v in Q's unit; also
                                                                // conversion_rounds, conversion_loses
  template<reference A, class VA, reference B, class VB, class... Args>
    constexpr auto divmod(const quantity<A, VA>&, const quantity<B, VB>&, Args&&...); // in the common unit

  // points
  struct point_origin {                                       // absolute, or at an exact offset from one
    string_view Name; dimension Dim; @*origin*@ Absolute; @*grid-number*@ Offset;
  };
  template<const point_origin& O> constexpr @*origin*@ origin_c;
  template<reference R, @*origin*@ O, class Rep>
  class quantity_point {
  public:
    constexpr explicit quantity_point(const quantity<R, Rep>&);
    constexpr const quantity<R, Rep>& quantity_from_origin() const noexcept;
    constexpr auto quantity_from(@*origin*@) const;
    constexpr auto quantity_from_zero() const;                     // from the absolute origin
    constexpr auto point_for(@*origin*@) const;                     // the same point from another origin
    template<@*unit*@ U> constexpr auto in(U) const;                 // keeps the origin
    template<@*unit*@ U> constexpr auto point_in(U) const;           // from U's origin
  };
  template<auto U, class V> constexpr auto point(const V&);      // a point from U's origin
  template<auto U, class V> constexpr auto delta(const V&);      // a difference in U
  // point ± quantity: a point; point − point: a quantity; origin + quantity: a point

  // the catalog: namespaces isq, si, international (contents in R1)
}
```

## Semantics

| Expression | Result | Notes |
|---|---|---|
| `quantity<R, Rep>{v}` | `v` stored in `Rep`, in the unit of `R` | explicit |
| `n * u` (`n` an inside) | `quantity<u, decltype(n)>` | `42 * u` is ill-formed |
| `q.in(u)` | `q` in `u`: `Rep`'s interval and notch times the conversion factor | the same raw; ill-formed for another dimension or a factor of π |
| `quantity<R2, Rep2>{q}` | `q` in `R2`'s unit, stored by `Rep2` | implicit when `R`'s kind converts to `R2`'s and the rescaled number converts implicitly |
| `a + b`, `a - b`, `a % b`, `a <=> b` | in the common unit (the gcd of the magnitudes); kind: the nearest common ancestor | ill-formed for different dimensions or kinds |
| `a * b`, `a / b` | units, kinds and numbers multiply or divide | `expected` iff `b`'s grid holds 0; a result of dimension one converts to its `inside` |
| `quantity_cast<S>(q)` | the same number, kind `S` | down or sideways within one kind; never across an `is_kind` boundary |
| `value_cast<Rep2>(q)`, `value_cast<u, Rep2>(q)` | the exact value, stored by `Rep2` | `try_value_cast` reports through `expected` |
| `clamp_cast<Q>(x)`, `wrap_cast<Q>(x)` | `x` in `Q`'s unit, saturated or folded into `Q`'s number | `x` a quantity of `Q`'s dimension, or a number in `Q`'s unit |
| `Q::try_make<F>(v)`, `q.on_wrap(f) += r` etc. | as for `Q::rep` | the number paper's policies and actions |
| `divmod(a, b, …)` | `{q, r}` in the common unit | as the number paper's `divmod` |
| `point<u>(v)` | a point counted from `u`'s origin | |
| `p.point_for(o)`, `p.quantity_from(o)` | the point or quantity from origin `o` | an exact shift of the grid |
| `p - p`, `p ± q` | a quantity; a point | no difference between unrelated absolute origins; `p + p` is ill-formed |

# Future work

The reference implementation provides more than this paper proposes. Each item
below is a candidate for a later paper or revision:

1. **Text**: `to_string`, `std::formatter` and stream operators print the
   number, then the unit symbol (`unit_symbol`: "kg m²/s²", "km/h"), in UTF-8 or
   ASCII; `parse<Q>` reads a number in `Q`'s unit.
2. **Algorithms on quantities**: `sum<Q>`, `add_all`, `mul_all`,
   `mul_into<Out>` / `div_into<Out>`, `min`, `max`, `lerp`, `dot` and `cross`,
   as for `inside`.
3. **The catalog**: about 80 ISQ kinds, the SI units and prefixes (quecto …
   quetta, exact past 64 bits), the international units, and `percent` and
   `per_mille`.
4. **Physical constants** as quantities of their kind: the SI 2019 defining
   constants and what follows from them exactly, and the CODATA 2022 values.
5. **Logarithmic units** and **vector and tensor characters**, which
   mp-units has and this layer does not.

# Implementation experience

The layer is `units.hpp` and its folder, about 2400 lines: the machinery
(`units/core.hpp`, about 2100 lines) and the ISQ and SI catalog
(`units/systems.hpp`, about 280), with its text in a separate `units/io.hpp`
(about 180 lines). A translation unit that includes it parses in
0.72 s with GCC 16, against 0.53 s for `inside` alone, and 200 distinct
quantity expressions add about 7 ms each (the repository's `docs/units.md`,
"Compile time"). Measured on v0.5.0's C++23 build, the layer parses in about a
third of the time of mp-units' `si` and `isq` headers (0.61 s against 1.90 s on
GCC 15, 0.92 s against 3.39 s on Clang 20), and per expression the two are
about the same on GCC.

A unit conversion compiles to one `mov`. 31 of the repository's 39 examples use
quantities, among them a clock whose seconds carry into minutes through
`on_wrap`, a PID controller in kelvin and percent, sensor fusion of
temperature points, a rocket's delta-v in km/s, and money and cryptocurrency
amounts as base dimensions of their own.

# Prior art

- **mp-units** (Mateusz Pusz) and its proposal [@P3045R9]: the model of this
  layer. References, the ISQ tree of quantity kinds, quantity points with
  origins, magnitudes with powers of π and `consteval` unit arithmetic all
  follow mp-units. This layer differs in that the representation is always an
  `inside`, which makes conversions and mixed-unit sums exact and gives every
  result an inferred grid. It has no logarithmic units and no vector or
  tensor characters.
- **Au** (Aurora Innovation): a units library with a strong focus on safe
  conversions of integer representations.
- **Boost.Units** (Matthias Schabel, Steven Watanabe) and **nholthaus/units**
  (Nic Holthaus): units libraries over a generic representation.

The repository's `docs/resources.md` compares them.

# Open questions

1. **This layer and P3045.** There are two ways to standardize quantities on
   `inside`: (a) the `std::quantity` of [@P3045R9], with `inside` as one more
   representation, or (b) this quantity, whose representation is always an
   `inside`. Exact, free conversions need the conversion to compute a new
   representation *type* (the rescaled grid), which a generic representation
   cannot do; under (a) that would need a customization point in P3045.
   Should the two papers be coordinated, and how?
2. **π.** Should the rounding conversion across a factor of π
   (`value_cast<si::radian, Out>(q)`) move into this paper, so that degrees and
   radians meet without the math paper?
3. **The catalog.** Which systems and how much of each belong in R1: the ISQ
   tree and SI as in P3045, the international units, the physical constants?
4. **Naming.** `std::units`, `quantity`, `quantity_point` and the catalog's
   names follow mp-units, so a standard with both would need to choose.
