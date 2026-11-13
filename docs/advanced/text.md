# Text in detail

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

Every printed form, the format specs and how they round, and the grammar
`parse` reads. [Conversions and text](../guide/conversions.md) has the
everyday view. Text needs `<beman/inside/inside/io.hpp>` (and
`<beman/inside/units/io.hpp>` for quantities); the single header includes
both unless `BEMAN_INSIDE_NO_STRING` is defined.

## Printed forms

`to_string(x)`, `std::cout << x` and `std::format("{}", x)` print the same
text: the **exact** value, never rounded. Its form follows the grid:

| Grid | Prints | Examples |
|---|---|---|
| integer notch | the integer | `42`, `-7` |
| decimal notch (a denominator 2ᵃ·5ᵇ with a 5, like `per<100>` or `0.05_r`) | that notch's decimals, for every value | `19.90`, `2.00` on a cents grid |
| any other notch, or continuous | the shortest exact form: a decimal when there is one, else `N/D` | `0.5`, `0.0000152587890625` on `per<(1 << 16)>`; `7/3` |

A column of prices lines up (`19.90`, not `19.9`), a binary fixed-point value
is not padded to 16 decimals, and a value with no finite decimal stays exact.
Every form reads back through `parse`.

- Wide and big values print every digit.
- A `rational` prints its shortest exact form, an `interval` `[lo..hi]`, a
  grid `{[lo..hi], notch}`.
- `to_string_debug(x)` adds the raw, its type and the grid:
  `0.5 {128[uint16_t Max:512] {[0..2], 0.00390625}}`.

## Format specs

A non-empty spec formats the value:

| Type | Takes |
|---|---|
| an integer grid whose values fit `int64` | the integer specs: `{:>4}`, `{:#x}`, `{:b}` |
| an integer grid past `int64` (`uint64`, a grid past 64 bits) | `d`, `b`, `B`, `o`, `x`, `X` with fill, align, sign, `#` (the base prefix), `0` and width, of any size; and `f`, `e`, `g` |
| every other inside, and `rational` | `f`, `e`, `g` with fill, align, sign, `#`, `0`, width and precision |

The digits are **rounded once from the exact value by the type's own
rounding mode**, the digits a store at that precision would give. A type
without a rounding mode, and `rational`, round to nearest with ties to even,
as `printf` does with a `double`. A width alone keeps the exact text.

| 0.125 and −0.129 with `{:.2f}` | no mode | `round_nearest` | `round_floor` | `round_ceil` | `round_toward_zero` |
|---|---|---|---|---|---|
| prints | `0.12`, `-0.13` | `0.13`, `-0.13` | `0.12`, `-0.13` | `0.13`, `-0.12` | `0.12`, `-0.12` |

```cpp
inside<{0, 100}> hp{42};
std::println("HP = {}", hp);               // HP = 42
std::println("HP = {:>5}", hp);            // HP =    42
std::println("HP = {:#04x}", hp);          // HP = 0x2a

inside<{{0, 1}, per<(1 << 10)>}> g{0.125};
std::println("{} {:.2f} {:e}", g, g, g);   // 0.125 0.12 1.250000e-01
```

Only an integer spec that asks for the locale (`{:L}`) depends on it; every
other form is written by the library itself, with no `printf`, `to_chars` or
locale.

## Parsing

`parse<B>(text)` reads one number exactly, with no `double` in between, and
returns `std::expected<B, errc>`. The text is an optional sign, then a
literal or a fraction `N/D`: `1'000`, `1.25`, `1.5e2`, `0xff`, `0b1010`,
`0x1.8p3`, `7/3`, the forms `to_string` prints. Long decimals past 64 bits are
read exactly too. The value then goes through `B::try_make`, so B's policy
applies, and flags after the type add to it for the call. `parse_exact<B>`
accepts only a value B holds exactly, whatever B's policy:

```cpp
using cents = inside<{{0, 1'000'000}, per<100>}>;
parse<cents>("1.005");                     // errc::rounding_error
parse<cents, round_nearest>("1.005");      // 1.01: the tie goes away from zero

using price = inside<{{0, 1'000'000}, per<100>}, round_nearest | clamp>;
parse<price>("1.005");                     // 1.01: price rounds
parse_exact<price>("1.005");               // errc::rounding_error
parse_exact<price>("2000000");             // errc::overflow: not clamped
```

The core header has the `constexpr` pointer-pair form,
`parse<B>(first, last)`; `io.hpp` adds the `std::string_view` form and
`operator>>`, which reads one token and sets `failbit` on an error, leaving
the value unchanged. [`json_io.cpp`](../../examples/json_io.cpp) reads and
writes a JSON document byte for byte this way.

## Quantities

`to_string(q)`, `<<` and `std::format` write the number and then the unit's
symbol: `5 km`, `10.5 m/s`, `kg m²/s²`, `90°`, `20 ℃`. A unit with only a
denominator shows negative exponents: `2 s⁻¹`, `1 m⁻¹ s⁻¹`. A format spec
applies to the number (`{:.2f}`, `{:>8}`), and
`unit_symbol(u, text_encoding::ascii)` gives the ASCII spelling:
`kg m^2/s^2`, `um`, `ohm`.

`units::parse<Q, F>(text)` and `units::parse_exact<Q>(text)` read a number in
Q's unit; there is no `>>` for quantities. A JSON or wire format holds the
bare number: write `q.numerical_value_in(u)`, which also documents the unit.
