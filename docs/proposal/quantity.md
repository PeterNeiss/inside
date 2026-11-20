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

# Abstract

We propose `std::units::quantity<reference, inside>`: physical quantities whose
number is always an `inside`, the bounded number with an exact rational step
proposed in the companion paper [*`inside`: numbers whose range and step size
are part of the
type*](https://github.com/PeterNeiss/inside/blob/v0.10.0/docs/proposal/inside.md)
(*the number paper* below). The model is that of mp-units, proposed as
[@P3045R10]{.title}:
dimensions, units with exact magnitudes, the ISQ tree of quantity kinds, and
quantity points with origins.

What changes is that the number is a grid. A unit conversion rescales the
grid, its interval and its notch, and keeps the stored integer, so it is exact,
it cannot fail, and at run time it is a copy. A sum of two units runs in their
common unit, exactly, and cannot overflow. A conversion with a factor of π
(degree to radian) has no exact rescale; it is ill-formed until it is rounded
once onto a declared grid, which the third companion paper, [*Correctly
rounded math for `inside` and
quantities*](https://github.com/PeterNeiss/inside/blob/v0.10.0/docs/proposal/math.md),
provides.

This paper depends on the number paper. The reference implementation is
[inside v0.10.0](https://github.com/PeterNeiss/inside/tree/v0.10.0).

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

The listings use the reference implementation's namespace `ins`, which
holds the number and the quantities (this paper's `std::units`). Each one is
a region of a program that compiles and checks its own results.

**A conversion rescales the grid and keeps the raw; a mixed-unit sum is
exact.**

<!-- listing: rescale -->
```cpp
constexpr auto km = si::kilo(si::metre);
quantity<km, inside<{{0, 500}, per<1000>}>> trip{12.5, km};    // 12.5 km, to the metre
auto m = trip.in(si::metre);                                   // 12500 m: the same raw, grid × 1000
static_assert(grid_of<decltype(m)::rep> == grid{0, 500'000});

auto total = trip + just<800> * si::metre;                     // 13300 m: the sum runs in metres
auto speed = total / quantity<si::minute, inside<{1, 600}>>{20, si::minute};
auto kmh   = speed.in(km / si::hour);                          // 39.9 km/h, exactly
inside<{0, 1000}, round_half_away | clamp> laps = trip / (just<400> * si::metre); // 31: km / m is a number
static_assert(!addable<decltype(trip), decltype(just<1> * si::second)>); // another dimension
```

**Quantity kinds separate quantities of one dimension.**

<!-- listing: kinds -->
```cpp
quantity<si::hertz, inside<{0, 1000}>>     f{50, si::hertz};
quantity<si::becquerel, inside<{0, 1000}>> a{50, si::becquerel};
static_assert(!addable<decltype(f), decltype(a)>);        // both 1/s, but not one kind

quantity<isq::width[si::metre], inside<{1, 10}>>  w{3, isq::width[si::metre]};
quantity<isq::height[si::metre], inside<{1, 10}>> h{4, isq::height[si::metre]};
quantity<isq::area[square(si::metre)], inside<{1, 100}>> area = w * h; // width · height
auto around = w + h;                                      // 7 m, a length: the common kind
auto tall   = kind_cast<isq::height>(w);              // a width read as a height, explicitly
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
inline constexpr auto euro = base_unit<"EUR", "currency">;
inline constexpr auto           cent = unit<"ct", 0.01_ins * euro>;

using price = quantity<euro / si::kilogram, inside<{{0, 100}, per<100>}>>;
using mass  = quantity<si::kilogram, inside<{{0, 10}, per<1000>}>>;

constexpr auto cost  = price{3.49_ins, euro / si::kilogram} * mass{1.25_ins, si::kilogram}; // 4.3625 EUR, exactly
constexpr auto cents = cost.in(cent);                                                       // 436.25 ct: the same raw
static_assert(!addable<decltype(cost), mass>);                                              // money never adds to a mass
```

# Design

## The model

`quantity<R, Rep>` is a number `Rep`, always an `inside`, of a *reference*
`R`: a unit, optionally with a quantity kind (`isq::height[si::metre]`). As in
mp-units [@P3045R10]:

- a *dimension* is a product of powers of base dimensions: the seven ISQ
  ones and any a program declares, all declared alike;
- a *unit* is a product of named units to integer powers, with prefixes and an
  exact *magnitude*;
- a *kind* is a node in the tree of quantity kinds (height under length, area
  as length²), and a kind marked distinct never mixes with its siblings
  (frequency and activity are both s⁻¹);
- a *quantity point* is a quantity counted from an *origin*.

## A conversion rescales the grid

`q.in(u)` multiplies the interval and the notch of `Rep`'s grid by the
conversion factor, and keeps the stored integer. km to m is a grid 1000 times
wider and finer, holding the same raw. A conversion is therefore exact and
total, and at run time it is a copy; the reference implementation compiles it
to one `mov`. A conversion to a unit of another dimension is ill-formed.

A conversion into a quantity of a given `Rep` (`quantity<m, inside<…>>{q}`) is
the conversion followed by a store, which `Rep`'s policy checks, clamps, wraps
or rounds as in the number paper. It exists when the source's kind converts to
the target's by an exact factor (otherwise `kind_cast` or `value_cast` names
the step), and it is implicit when the converted number converts implicitly
into `Rep`.

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
activity are kinds of their own. `kind_cast<S>(q)` reads a quantity as
another kind of its dimension, explicitly; it never crosses a distinct kind's boundary.

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
`42` has no grid. A number never becomes a quantity, or a quantity a number,
without its unit named where it happens. A quantity is made from a number and
its unit, `quantity<si::metre, inside<{0, 100}>>{42, si::metre}`, or from an
`inside` times a unit, `just<42> * si::metre`; its number is read in a unit,
`q.numerical_value_in(si::metre)`. `quantity<R, Rep>{42}` and
`q.numerical_value()` are ill-formed. That is [@P3045R10]'s rule, without its
exception for a dimensionless quantity in unit one: `{42, one}` names the unit
too. A number in another unit of the same quantity is scaled
(`minutes{120, si::second}` is 2 min): an `inside` converts exactly first, and
a plain number is stored once onto `Q`'s grid in the unit named, by the rules
of every store.

## Structural values

Dimensions, magnitudes, units, kinds and references are structural
values, combined by `consteval` functions. They are not type lists that the
compiler must sort, so m·s and s·m are one unit. The names a program writes
(`si::metre`, `isq::height`) are empty tag objects whose type carries the
value. An entry is identified by its content, not by its address.

## Your own

A program defines its own units, kinds, origins and base dimensions
(currency, information, pixels), each in one declaration whose name is a
template argument: `unit<"ftm", 1.8288_ins * si::metre>`. A unit is defined by
the quantity it equals. From then on they behave like the catalog's: prefixes
apply to a named unit, a kind under length is a length, and money never adds
to a mass.

## Diagnostics

A unit operation that cannot happen is a constraint failure, so
`requires { a + b; }` is false for a length and a duration. A call written
anyway names its reason as a deleted overload, for example `units: adding or
comparing quantities of different kinds`.

## The policy of the number

A quantity's stores are its number's stores: `++q`, `q += r` and the
construction from a number and its unit apply `Rep`'s policy, and
`q.on_wrap(f)`, `q.on_clamp(f)` and `q.on_error(f)` attach the number paper's
actions. `q.with<F>()` and `q.on_error(ec)` store under `Rep`'s policy plus
`F`. `Q::try_make(v, u)`, the flag constructors `Q{q, clamp}` and
`Q{v, u, clamp}` (a flag or `make_policy<F>(ec)` for one store, `q` a quantity
converted exactly), `exact_cast<Q>(q)` and `value_cast` convert into a
quantity type, each returning `std::expected` exactly where the grids do not
prove the store, and `conversion_overflows<Q>(v, u)` and its
siblings answer for a number `v` in unit `u`. `min`, `max` and `midpoint` run in the common unit,
as `+` does, and the free arithmetic takes a per-call policy as in the number
paper (`div(d, t, round_floor)`). Every function of the number paper has a
quantity form.

# Proposed interface

The names are the reference implementation's, except for the few types it
keeps internal, which this paper names ([Implementation
experience](#implementation-experience) lists them). Dimensions, units, kinds,
origins and references are structural values; the names a program writes
(`si::metre`, `isq::height`, `si::absolute_zero`) are empty tag objects
(`unit_t`, `kind_t`, `origin_t`) whose type carries the value. `grid_number`,
`policy_flag`, `checked`, `call_policy`, `policy_ref` and `insidable` are the
number paper's.

```cpp
namespace std::units {
  // names: a string held by value, so it is a template argument
  struct symbol_text {                                        // at most 63 bytes of UTF-8
    constexpr symbol_text() = default;
    template<size_t N> consteval symbol_text(const char (&s)[N]);
    constexpr string_view view() const;
    constexpr bool empty() const;
    friend constexpr bool operator==(const symbol_text&, const symbol_text&) = default;
  };

  // dimensions
  struct dimension;                                           // base dimensions to integer powers
  template<symbol_text Name> constexpr dimension base_dimension;  // "length", "currency"

  // units
  struct unit_value;                                          // named units to integer powers, prefixes, and an
                                                              // exact magnitude (a rational times a power of pi)
  template<unit_value U> struct unit_t {                      // si::metre, si::kilo(si::metre) / si::hour
    static constexpr unit_value value = U;
  };
  consteval dimension dimension_of(const unit_value&);
  struct kind_ref;                                            // a named kind, or none
  struct origin_ref;                                          // an origin, or none
  struct unit_options {
    symbol_text ascii;                                        // the ASCII symbol ("ohm" for the ohm)
    kind_ref    kind;                                         // the kind it measures, else its definition's
    origin_ref  origin;                                       // the default origin of its points
    bool        space_before = true;                          // false for 90°
  };
  template<symbol_text Symbol, auto Definition, unit_options = {}>
    constexpr auto unit;                                      // a unit_t: m, N, °C, by the quantity it equals
                                                              // (1.8288_ins * si::metre), a unit, or a kind
  template<symbol_text Symbol, symbol_text Dimension, unit_options = {}>
    constexpr auto base_unit;                                 // a unit_t: a new dimension, its kind, its unit
  inline constexpr unit_t<unit_value{}> one;                  // the unit 1
  inline constexpr auto pi;                                   // a unit_t: the number pi, exactly
  template<unit_value A, unit_value B>
    consteval auto operator*(unit_t<A>, unit_t<B>);           // also /; pow<N>(u), square(u), cubic(u), inverse(u)
  template<int P> struct prefix_t {                           // si::kilo, si::milli: 10^P, once, on a named unit
    template<unit_value U> consteval auto operator()(unit_t<U>) const;
  };

  // quantity kinds
  struct kind_expr;                                           // kinds to integer powers: a recipe
  struct kind_options {
    bool      distinct = false;                               // never mixes with its siblings (frequency, activity)
    kind_expr recipe;                                         // what it is made of (velocity: displacement / duration)
  };
  template<symbol_text Name, auto Parent, kind_options = {}>
    constexpr auto kind;                                      // a kind_t: isq::length, isq::height; under a kind,
                                                              // a product of kinds, or a dimension
  template<kind_ref K> struct kind_t {                        // k * k, k / k: a kind_expr
    template<unit_value U> consteval auto operator[](unit_t<U>) const;  // isq::height[si::metre]: a reference
  };

  // references and quantities
  struct reference {                                          // structural: a unit, and its kind or recipe
    unit_value U;
    template<unit_value V> constexpr reference(unit_t<V>);    // the unit, of the kind it measures
  };

  template<reference R, insidable Rep>
  class quantity {
  public:
    using rep = Rep;
    static constexpr units::reference reference = R;
    static constexpr unit_value       unit      = R.U;
    static constexpr units::dimension dimension = dimension_of(R.U);
    static constexpr auto             kind;                   // a named kind's kind_t, else its kind_expr

    constexpr quantity() = default;
    template<class V, class U> constexpr quantity(V&& number, U unit);  // number in unit U of R's quantity
    template<class V, class U, class Pol> constexpr quantity(V&& number, U unit, Pol p);  // p: a flag or a
                                                                                          // call_policy
    template<class Q2, class Pol> constexpr quantity(const Q2& q, Pol p);  // q a quantity, converted exactly
    template<reference R2, insidable V2>                      // R2's kind converts to R's, by an exact factor
      constexpr explicit(!convertible_to<rescaled_t<R2, R, V2>, Rep>) quantity(const quantity<R2, V2>& q);
    template<unit_value U> constexpr auto in(unit_t<U>) const;               // Rep's grid rescaled; same raw
    template<unit_value U> constexpr auto numerical_value_in(unit_t<U>) const;  // the number in unit U
    template<insidable T>                                     // dimension one, an exact factor to one
      constexpr explicit(!convertible_to<rescaled_t<R, one, Rep>, T>) operator T() const;
    template<class T, policy_flag Round = checked>
      constexpr auto to() const;                              // dimension one: T, or expected<T, error>
    template<policy_flag F = checked, class V, class U>
      static constexpr auto try_make(const V& number, U unit);  // expected<quantity, error>

    // a policy or an action for one store into the number
    template<policy_flag F = checked>
      constexpr quantity_policy_ref<quantity, policy_ref<Rep, call_policy<policy_of<Rep> | F>>> with() &;
    template<policy_flag F = checked>
      constexpr quantity_policy_ref<quantity,
                                    policy_ref<Rep, call_policy<policy_of<Rep> | F, error_ref>>>
        on_error(error& ec);
    template<class F> constexpr auto on_clamp(F f);           // also on_wrap, on_error: a quantity_policy_ref
    // ++, --, +=, -=, *=, /=: a store by Rep's policy
  };
  template<class Q, class Ref> struct quantity_policy_ref;    // the number paper's policy_ref on Q's number;
                                                              // its stores take quantities
  template<reference From, reference To, insidable V>         // V's number in To's unit: its grid rescaled
    using rescaled_t = typename decltype(declval<const quantity<From, V>&>().template in<To.U>())::rep;

  // quantity + - % <=> == quantity, min, max, midpoint: in the common unit
  // quantity * / quantity, insidable * unit_t, insidable / unit_t, quantity * unit_t: units and kinds multiply
  // quantity * / insidable: the number scales; pi * quantity, quantity / pi; pow<N>(q)
  template<auto Kind, reference R, class V> constexpr auto kind_cast(const quantity<R, V>&);  // a named kind
  template<insidable Rep2, reference R, class V>
    constexpr auto value_cast(const quantity<R, V>&);         // quantity<R, Rep2>, or expected of it
  template<auto U, insidable Rep2, reference R, class V>
    constexpr auto value_cast(const quantity<R, V>&);         // in unit U (an exact factor), on Rep2
  template<class Q, reference R, class V>
    constexpr auto value_cast(const quantity<R, V>&);         // Q, or expected<Q, error>
  template<class Q, class X> constexpr auto exact_cast(const X& q);  // q a quantity: Q, or expected<Q, error>
  template<class Q, class A, class U>
    constexpr bool conversion_overflows(A v, U unit) noexcept;  // v in unit; also conversion_rounds,
                                                                // conversion_loses
  template<reference A, class VA, reference B, class VB, class... Args>
    constexpr auto divmod(const quantity<A, VA>&, const quantity<B, VB>&, Args&&...);  // in the common unit
  // add, sub, mod (common unit), mul, div (units multiply; or a quantity and a number):
  // the number paper's free arithmetic, Args a flag, a call_policy or an error&

  // points
  template<origin_ref O> struct origin_t {};                  // si::absolute_zero, si::ice_point
  template<symbol_text Name, auto Where>
    constexpr auto origin;                                    // an origin_t: absolute (a kind or a dimension),
                                                              // or at a point of another:
                                                              // si::absolute_zero + 273.15_ins * si::kelvin
  template<reference R, origin_ref O, insidable Rep>
  class quantity_point {
  public:
    constexpr explicit quantity_point(const quantity<R, Rep>&);
    constexpr const quantity<R, Rep>& quantity_from_origin() const noexcept;
    template<origin_ref O2> constexpr auto quantity_from(origin_t<O2>) const;
    constexpr auto quantity_from_zero() const;                // from the absolute origin
    template<origin_ref O2> constexpr auto point_for(origin_t<O2>) const;  // the same point, another origin
    template<unit_value U> constexpr auto in(unit_t<U>) const;        // keeps the origin
    template<unit_value U> constexpr auto point_in(unit_t<U>) const;  // from U's origin
    // += and -= a quantity: a store by Rep's policy
  };
  template<auto U, insidable V> constexpr auto point(const V&);  // a point from U's origin
  template<auto U, insidable V> constexpr auto delta(const V&);  // a difference in U
  // point ± quantity: a point; point − point: a quantity; origin + quantity: a point;
  // value_cast<Rep2>(p), value_cast<U, Rep2>(p): as for quantities

  // the catalog: namespaces isq, si (with the prefixes), international (contents in R1)
}
namespace std {
  template<units::reference R, class Rep> struct hash<units::quantity<R, Rep>>;           // the number's
  template<units::reference R, class Rep> struct numeric_limits<units::quantity<R, Rep>>; // limits as quantities
}
```

## Semantics

| Expression | Result | Notes |
|---|---|---|
| `quantity<R, Rep>{v, u}` | `v` stored in `Rep`, `v` in unit `u` | `u` R's unit, or another unit of its quantity: an inside `v` converts exactly, a plain number is stored once onto `Rep`'s grid in unit `u`; `{v}` alone is ill-formed |
| `n * u` (`n` an inside) | `quantity<u, decltype(n)>` | `42 * u` is ill-formed |
| `q.in(u)` | `q` in `u`: `Rep`'s interval and notch times the conversion factor | the same raw; ill-formed for another dimension or a factor of π |
| `quantity<R2, Rep2>{q}` | `q` in `R2`'s unit, stored by `Rep2` | exists when `R`'s kind converts to `R2`'s by an exact factor; implicit when the rescaled number converts implicitly into `Rep2` |
| `a + b`, `a - b`, `a % b`, `a <=> b` | in the common unit (the gcd of the magnitudes); kind: the nearest common ancestor | ill-formed for different dimensions or kinds |
| `a * b`, `a / b` | units, kinds and numbers multiply or divide | `expected` iff `b`'s grid holds 0; a result of dimension one converts to its `inside` |
| `kind_cast<K>(q)` | the same number, kind `K` (a named kind) | down or sideways within one kind; never across a distinct kind's boundary |
| `value_cast<Rep2>(q)`, `value_cast<u, Rep2>(q)`, `value_cast<Q>(q)` | the exact value, stored by `Rep2` (in unit `u`) | an `expected` where the grids do not prove the store; `u` by an exact factor |
| `exact_cast<Q>(q)` | `q` in `Q`'s unit, stored exactly | `Q`, or `expected<Q, error>` where it can fail; a bare number is ill-formed |
| `Q{q, clamp}`, `Q{v, u, wrap}` | `q` or `v` in `Q`'s unit, saturated or folded into `Q`'s number | `q` a quantity whose kind converts to `Q`'s by an exact factor, `v` a number in unit `u` |
| `q.numerical_value_in(u)` | `q`'s number in unit `u` | exact; `q.numerical_value()` is ill-formed |
| `Q::try_make<F>(v, u)`, `q.on_wrap(f) += r` etc. | as for `Q::rep` | the number paper's policies and actions |
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
   `mul<Out>` / `div<Out>` and `lerp`, as for `inside`.
3. **The catalog**: about 80 ISQ kinds, the SI units and prefixes (quecto …
   quetta, exact past 64 bits), the international units, and `percent` and
   `per_mille`.
4. **Physical constants** as quantities of their kind: the SI 2019 defining
   constants and what follows from them exactly, and the CODATA 2022 values.
5. **Vectors as multivectors.** The reference implementation has geometric
   algebra on quantities: basis vectors of your own (`basis_vector<"e1", 1>`),
   sums of quantities on them, the geometric product and the exponential of a
   multivector. It takes the place of mp-units' vector and tensor characters.
6. **Logarithmic units**, which mp-units has and this layer does not; a level
   such as the decibel is a dimensionless kind of your own.

# Implementation experience

The layer is `units.hpp` and the `units/` folder. Its machinery
(`units/core.hpp` and `units/detail/`, without the multivector `exp` of
`geoalg_exp.hpp`) is about 3500 lines, 2900 without blank lines and comments;
the ISQ and SI catalog (`units/systems.hpp`) is about 290, and the text
(`units/io.hpp`) about 240. A translation unit that includes `units.hpp` parses
in 1.08 s with GCC 16, against 0.56 s for `inside` alone, and units add about
17 ms to each of 200 distinct quantity expressions (the repository's
`docs/advanced/units-model.md`, "Compile time").

A unit conversion compiles to one `mov`. 36 of the repository's 44 examples use
quantities, among them a clock whose seconds carry into minutes through
`on_wrap`, a PID controller in kelvin and percent, sensor fusion of
temperature points, a rocket's delta-v in km/s, and money and cryptocurrency
amounts as base dimensions of their own.

# Prior art

- **mp-units** (Mateusz Pusz) and its proposal [@P3045R10]: the model of this
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
   `inside`: (a) the `std::quantity` of [@P3045R10], with `inside` as one more
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
