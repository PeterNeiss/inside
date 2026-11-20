# Text in detail

Every printed form, the format specs and how they round, and the grammar
`parse` reads. [Conversions and text](../guide/conversions.md) has the
everyday view. Text needs `<inside/inside/io.hpp>` (and
`<inside/units/io.hpp>` for quantities); the single header includes
both unless `INSIDE_NO_STRING` is defined.

## Printed forms

`to_string(x)`, `std::cout << x` and `std::format("{}", x)` print the same
text: the **exact** value, never rounded. Its form follows the grid:

| Grid | Prints | Examples |
|---|---|---|
| integer notch | the integer | `42`, `-7` |
| decimal notch (a denominator 2ᵃ·5ᵇ with a 5, like `per<100>` or `0.05_ins`) | that notch's decimals, for every value | `19.90`, `2.00` on a cents grid |
| any other notch, or continuous | the shortest exact form: a decimal when there is one, else `N/D` | `0.5`, `0.0000152587890625` on `per<(1 << 16)>`; `7/3` |

A column of prices lines up (`19.90`, not `19.9`), a binary fixed-point value
is not padded to 16 decimals, and a value with no finite decimal stays exact.
Every form reads back through `parse`.

- Wide and big values print every digit.
- An `interval` prints as `[lo..hi]`, a grid as `{[lo..hi], notch}`.
- `to_string_debug(x)` adds the raw, its type and the grid:
  `0.5 {128[uint16_t Max:512] {[0..2], 0.00390625}}`.

## Format specs

A non-empty spec formats the value:

| Type | Takes |
|---|---|
| an integer grid whose values fit `int64` | the integer specs: `{:>4}`, `{:#x}`, `{:b}` |
| an integer grid past `int64` (`uint64`, a grid past 64 bits) | `d`, `b`, `B`, `o`, `x`, `X` with fill, align, sign, `#` (the base prefix), `0` and width, of any size; and `f`, `e`, `g` |
| every other inside | `f`, `e`, `g` with fill, align, sign, `#`, `0`, width and precision |

The digits are **rounded once from the exact value by the type's own
rounding mode**, the digits a store at that precision would give. A type
without a rounding mode rounds to nearest with ties to even,
as `printf` does with a `double`. A width alone keeps the exact text.

| 0.125 and −0.129 with `{:.2f}` | no mode | `round_half_away` | `round_floor` | `round_ceil` | `round_toward_zero` |
|---|---|---|---|---|---|
| prints | `0.12`, `-0.13` | `0.13`, `-0.13` | `0.12`, `-0.13` | `0.13`, `-0.12` | `0.12`, `-0.12` |

```cpp
inside<{0, 100}> n{42};
std::println("n = {}", n);                 // n = 42
std::println("n = {:>5}", n);              // n =    42
std::println("n = {:#04x}", n);            // n = 0x2a

inside<{{0, 1}, per<(1 << 10)>}> g{0.125};
std::println("{} {:.2f} {:e}", g, g, g);   // 0.125 0.12 1.250000e-01
```

Only an integer spec that asks for the locale (`{:L}`) depends on it; every
other form is written by the library itself, with no `printf`, `to_chars` or
locale.

## Parsing

`parse<B>(text)` reads one number exactly, with no `double` in between, and
returns `std::expected<B, error>`. The text is an optional sign, then a
literal or a fraction `N/D`: `1'000`, `1.25`, `1.5e2`, `0xff`, `0b1010`,
`0x1.8p3`, `7/3`, the forms `to_string` prints. Long decimals past 64 bits are
read exactly too. The value then goes through `B::try_make`, so B's policy
applies, and flags after the type add to it for the call. `parse_exact<B>`
accepts only a value B holds exactly, whatever B's policy:

```cpp
using hundredths = inside<{{0, 1'000'000}, per<100>}>;
parse<hundredths>("1.005");                    // error::rounding_error
parse<hundredths, round_half_away>("1.005");   // 1.01: the tie goes away from zero

using rounded = inside<{{0, 1'000'000}, per<100>}, round_half_away | clamp>;
parse<rounded>("1.005");                   // 1.01: rounded rounds
parse_exact<rounded>("1.005");             // error::rounding_error
parse_exact<rounded>("2000000");           // error::overflow: not clamped
```

The core header has the `constexpr` pointer-pair form,
`parse<B>(first, last)`; `io.hpp` adds the `std::string_view` form and
`operator>>`, which reads one token and sets `failbit` on an error, leaving
the value unchanged. [`json_io.cpp`](../../examples/json_io.cpp) reads and
writes a JSON document byte for byte this way.

## Quantities

`to_string(q)`, `<<` and `std::format` write the number and then the unit's
symbol: `5 km`, `10.5 m/s`, `kg·m²/s²`, `90°`, `20 ℃`. A unit's factors are
joined by `·`, so a quantity's text is one number and one unit token (which
`>>` reads back). A unit with only a
denominator shows negative exponents: `2 s⁻¹`, `1 m⁻¹·s⁻¹`. A format spec
applies to the number (`{:.2f}`, `{:>8}`), and
`unit_symbol(u, text_encoding::ascii)` gives the ASCII spelling:
`kg*m^2/s^2`, `um`, `ohm`.

### Reading a quantity with its unit

Text names its unit, and reads back what `to_string` writes, in either
encoding: `parse<Q>(to_string(q)) == q` for every quantity and point.

```cpp
using minutes = quantity<si::minute, inside<{{0, 600}, per<10>}>>;
parse<minutes>("1.5 h");          // 90.0 min: any unit that converts exactly
parse<minutes>("90 s");           // 1.5 min
parse<minutes>("90");             // error::missing_unit: a number alone names no unit
parse<minutes>("90 m");           // error::incompatible_unit: a length
parse<minutes>("90 zz");          // error::unknown_unit
parse<minutes>("90", "s");        // the two apart: a JSON key "wait[s]", a CSV header
parse<minutes>("90", si::second); // a unit named in code
```

| Form | Reads |
|---|---|
| `parse<Q, F>(text)`, `parse_exact<Q>(text)` | the number, then its unit after a space or against it (`90°`, `90deg`) |
| `parse<Q, F>(number, unit)` | the two apart |
| `parse<Q, F>(number, u)` | a bare number in the unit `u`, named in code; a `u` that does not convert exactly does not compile |
| `is >> e`, `e` a `std::expected<Q, error>` | a number token and a unit token; `e` holds the value or the error, and the stream stays good on a parse error (only running out of input sets failbit). There is no `>>` into a plain quantity. |

**Which units.** Any unit visible where `parse` is called that converts
**exactly** to Q's: the factor between them is a rational (km→m ×1000,
h→min ×60, ‰→% ×1/10, ℉→℃ ×5/9 and a shift), so the value in Q's unit is
computed with no rounding, and Q's number type then stores it (by its
policy and the flags `F`; `parse_exact` only where it is on the grid). A
factor of π (° into rad) is not exact. The units are the catalog's and
every unit of your own declared at namespace scope before the call, in any
namespace (unnamed ones too), found by reflection. Each call site gets the
set it sees as a template argument, so two translation units that see
different units get different specializations.

**The unit's text.** A symbol with an optional SI prefix (`k`, `µ`, ASCII
`u`) and exponent (`²`, `⁻¹`, `^2`, `^-1`); factors joined by a space, `·`
or `*`; one `/` before a symbol or a parenthesized product (`J/(kg K)`),
or `1/s`; a scaled unit as printed (`[1/381·ft]`). An exact symbol wins
over a prefix reading (`min` is the minute, `mm` the millimetre); at most
6 symbols and exponents up to 9.

**Kinds and origins.** A single unit's kind must add to Q's: `"5 Hz"` is no
activity in Bq, `"15 %"` no level in dB. A compound unit names no kind, so
the type gives it: `"5 1/s"` reads as 5 Hz. A point reads the number from
the unit's origin and shifts it exactly into its own: `"350 ℉"` into a ℃
point is 530/3 ℃; a quantity is a difference, so `"5 ℃"` into kelvins is
5 K.

Errors: `missing_unit`, `unknown_unit`, `incompatible_unit`, then the
number's (`invalid_format`) and the store's (`rounding_error`,
`overflow`). A JSON document carries units in its keys
(`"price[USD/item]": 19.99`), as
[`json_io.cpp`](../../examples/json_io.cpp) reads and writes them.
