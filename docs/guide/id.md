# Spelling a type as text

An inside type can be written as one string, its interval, step and policy,
and a quantity's type adds its unit:

```cpp
using digit    = ins::id<"[0..9]">;
using quarters = ins::id<"[0..10) 1/4">;              // 0, 1/4, …, 39/4
using heading  = ins::id<"[0..360) wrap">;            // 0 … 359, modular
using ratio    = ins::id<"[0..1] 1/1000 clamp|round_half_away">;
using speed    = ins::id<"[0..300] [km/h] clamp">;      // a quantity
```

The type prints as it is written, `ins::id<ins::key{"[0..10) 1/4"}>`, and so
do the results of the operators:

```cpp
static_assert(std::is_same_v<decltype(digit{} + digit{}), ins::id<"[0..18]">>);
```

A string names the same type as the grid and policy written as values:
`id<"[0..9] clamp">` is `inside<{0, 9}, clamp>`.

## The text

| Text | Means |
|---|---|
| `[0..9]` | the limits, both included. Without a step, the step is the one the limits derive, as `inside<{0, 9}>` derives it: 1 here, 1/2 for `[0..1/2]` |
| `[0..10)`, `(0..10]`, `(0..10)` | an open limit: it moves in by one step, so `[0..10)` is `[0..9]` |
| `[0..10) 1/4` | a step: any positive number that divides the interval as written |
| `[0..1] <1000>` | a continuous grid: every fraction in the interval with a denominator of at most 1000 ([exact arithmetic](../advanced/exact-arithmetic.md#bounded-continuous-grids)). Its limits are closed |
| `[3]` | a point: one value, held in the type (`3_ins`) |
| `[km/h]` | a unit: the type is a quantity ([quantities](#quantities)) |
| `clamp`, `wrap`, `round_half_away`, `round_floor`, `round_ceil`, `round_half_even`, `round_toward_zero`, `unsafe` | the policy ([policies](policies.md)), joined by `\|`: at most one rounding mode, not both `clamp` and `wrap`. No word is `checked`, the default |

Numbers read as the `_ins` literal reads them: `-5`, `2/5`, `0.25`, `1e9`,
`0x1p-4`, `1'000`, of any size. Spaces between the parts are free.

## One type, one text

Every spelling of one grid and policy is one type, and the type holds one
text for it, the *canonical* text, which diagnostics print:

- the limits written with the smallest common denominator, and closed when
  that is a tie, so an integer grid prints closed: `(0..10)` is `[1..9]`,
  while `[0..10) 1/4` stays as it is, since `[0..39/4] 1/4` needs quarters;
- the step, unless the written limits derive it;
- the flags in a fixed order: `clamp` or `wrap`, the rounding mode, `unsafe`;
- a number past 64 bits in hexadecimal, as m·2^e where shorter: `0x1p70`.

```cpp
static_assert(std::is_same_v<ins::id<"(0..10)">, ins::id<"[1..9]">>);
static_assert(std::is_same_v<ins::id<"[0..360) wrap">, ins::id<"[0..359] wrap">>);
```

A text that spells no grid fails where it stops:

```text
error: static assertion failed: inside: id<"[0..10] 3">: expected a step that divides upper - lower at 8 ("[0..10] " read)
```

A type's text has room for 2048 characters; a grid whose numbers need more
(limits of thousands of digits) has no name, and says so.

## Quantities

A unit in brackets, after the step, makes the type a quantity:
`id<"[0..300] [km/h]">` is `quantity<si::kilo(si::metre) / si::hour, id<"[0..300]">>`,
the same type. The unit is read as a quantity's text reads one, by symbols
with powers (`km/h`, `kg*m^2/s^2`, `µs`, `[1/381 ft]` for a scaled unit),
among the unit tags visible where the type is named: the catalog's and your
own (`base_unit<"cts", "count">`, `defined_by<cent_def>`, `unit<"ftm", …>`).
The canonical text is ASCII (`us`, not `µs`).

A symbol is a whole one: a unit's own, or a prefixed unit's that a tag names.
The catalog names the prefixed units in wide use (`km`, `mm`, `ms`, `us`,
`kHz`, `MW`, `mA`, `kPa`, `keV`, …); no prefix is read into another symbol
(`rd` is no ronto-day). A prefixed unit of your own is a tag of your own:

```cpp
inline constexpr auto fathom     = ins::unit<"ftm", 1.8288_ins * ins::si::metre>;
inline constexpr auto kilofathom = ins::si::kilo(fathom);   // [kftm]
```

Results read the same way:

```cpp
using metres = ins::id<"[1..9] [m]">;
static_assert(std::is_same_v<decltype(metres{} + metres{}), ins::id<"[2..18] [m]">>);
static_assert(std::is_same_v<decltype(ins::id<"[0..1] [km]">{} + metres{}), ins::id<"[1..1009] [m]">>);
```

A kind that is not the unit's own follows the unit, in braces:

| Text | Kind | C++ spelling |
|---|---|---|
| `[m]` | the unit's own kind, any length | `si::metre` |
| `[m]{height}` | height | `isq::height[si::metre]` |
| `[m*s]{any duration*length}` | any length times any duration | a quantity in m times one in s |
| `[m^2]{height*width}` | a height times a width | `isq::height[m] * isq::width[m]` |
| `[J]{none}` | no kind | |

A product or quotient of quantities remembers the kinds it is made of, so that
they come back when the units cancel, and its text says so:

```cpp
static_assert(std::is_same_v<decltype(metres{} * ins::id<"[1..9] [s]">{}),
                             ins::id<"[1..81] [m*s]{any duration*length}">>);
```

Kind names are identifiers (letters, digits and `_`), as a key spells them.

### Which unit a symbol names

A symbol is looked up in the catalog first, every table of it (the constants'
too, where `<inside/units/constants.hpp>` is included): a symbol the catalog
reads (`km`, `Pa`, `ms`, `R`) names the catalog's unit, wherever the type is
first named. Only a symbol the catalog does not read is looked up among the
other units, where the type is first named.

A unit is read by its ASCII symbol, and by its symbol where that is not ASCII
(`µs`, `Ω`, `℃`). A unit whose plain symbol has an ASCII one of its own is
read only by that one, which keeps the plain symbol free: the speed of light
`c` is `c_0`, Boltzmann's `k` is `k_B`, Planck's `h` is `h_P` (`h` is the
hour), Faraday's `F` is `F_Faraday` (`F` is the farad), so a cent's `c` and a
quaternion's `k` are yours.

So give your own units symbols of their own. A unit whose symbol the catalog
reads, or two of your own with one symbol and different definitions, have no
text, and a quantity of one does not compile:

```text
error: units: this quantity's unit or kind has no text that names it here - give a unit of your own a symbol of its own ...
```

A symbol that names one unit in one file and another in a second file makes
one type mean two units (the program is ill-formed, and no compiler is
required to say so). Two units equal in every respect, such as a unit declared
twice, are one type.

## A value with its type

A string `_ins` literal is a number and then the text of its type:

```cpp
constexpr auto n{"42 [1..100)"_ins};                  // 42, of type id<"[1..99]">
constexpr auto q{"0.25 [0..1) 1/4"_ins};              // 1/4
constexpr auto c{"200 [0..99] clamp"_ins};            // 99: stored as the type stores it
constexpr auto r{"0.3 [0..1] 1/4 round_half_away"_ins}; // 1/4
```

The number is stored while compiling, as `T{just<v>}` stores it: the type's
policy decides, and a value the type cannot hold (`"200 [0..99]"_ins`) does
not compile. Without a type, `"42"_ins` is the point `42_ins`.

With a unit the value is a quantity, its number in the type's unit, and a
unit alone makes the point:

```cpp
constexpr auto v{"42 [1..100) [km/h]"_ins};           // 42 km/h, of type id<"[1..99] [km/h]">
constexpr auto p{"42 [km/h]"_ins};                    // the point 42 km/h, id<"[42] [km/h]">
```

## Generic code

An inside type is `id<K>` for a `key K`, so a function deduces the key:

```cpp
template <ins::key K>
auto square(ins::id<K> x) { return x * x; }
```

`grid_of<T>` and `policy_of<T>` give the grid and the policy as values.
(`inside<G, P>` is an alias, so `template <grid G, policy_flag P>
f(inside<G, P>)` deduces nothing.)

Every quantity is an `id` too, so a function over quantities takes one and
reads its reference and number type:

```cpp
template <ins::quantity_like Q>
auto twice(const Q& q) { return q + q; }   // Q::reference, Q::rep
```
