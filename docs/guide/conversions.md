# Conversions and text

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

How values cross the boundary of an inside: native numbers in and out,
casts, comparisons, and text.

```cpp
inside<{0, 1000}> b{42};                   // in: from an int
auto n = b.as<int>();                      // out: 42, as an int
auto m = b.to<std::uint8_t>();             // out: a std::expected, since 1000 does not fit a byte
std::cout << b;                            // text: 42
auto p = parse<inside<{0, 1000}>>("42");   // text in: a std::expected
```

## Into an inside

An inside is built from an integer, a `double`, a `float`, a `rational` or
another inside. The store follows the type's [policy](policies.md): a value
out of range or between notches is an error unless the policy clamps, wraps
or rounds.

| Write | For |
|---|---|
| `T x{v};`, `x = v;` | a store under T's policy |
| `T{v, F}` | one store with the flags `F`: `T{v, clamp}`, `T{v, clamp \| round_nearest}`, `T{v, wrap}` |
| `checked_cast<T>(v)` | a store that reports any loss, whatever T's policy |
| `T::try_make(v)` | a `std::expected` instead of a throw |
| `T::from_raw(r)` | trusted raw transfer, without checks ([Storage](../advanced/storage.md#raw-access)) |

To test a value without storing it:

```cpp
using pct = inside<{0, 100}>;
conversion_overflows<pct>(150);   // true: outside [0, 100]
conversion_rounds<pct>(3.5);      // true: not on notch 1
conversion_loses<pct>(150);       // true: either of the two
```

## Out of an inside

| Write | Gives |
|---|---|
| `x.as<T>()` | a `T`; a value that does not fit is reported like a failed store |
| `x.to<T>()` | a `std::expected<T, errc>`: `overflow` when it does not fit, `domain_error` for a negative value into an unsigned type |
| `x.to<int, round_floor>()` | the same, rounding a fraction by the given mode |
| `double(x)`, `x.as<double>()` | a `double`, on a type with a rounding mode |
| `x.to<double>()` | a `double`, on any type |
| `x.numerator()`, `x.denominator()` | the exact value in lowest terms: `3` and `16` for 0.1875 |
| `rational r = x;` | the exact value as a 64-bit fraction, where it fits one |

An inside has no implicit conversion to an integer: `int i = x;` would narrow
silently. Where `T` holds the whole grid, `as<T>()` has nothing left to check,
so an index costs nothing: `v[x.as<std::size_t>()]`.

A fraction into an integer type needs permission to round, as a store does:
`x.to<int>()` of 2.75 is `errc::rounding_error`, `x.to<int, round_floor>()`
is 2. A `float` or `double` result is rounded once from the exact value.

`as` and `to` are free functions too, `as<T>(x)` and `to<T>(x)`, which spares
generic code the `x.template as<T>()` spelling.

## Comparing

An inside compares with other insides, integers and rationals, exactly, and
without narrowing either side:

```cpp
inside<{0, 100}> a{42}, c{58};
a == 42;          // true
a < 50.5_r;       // true
a + c == 100;     // true
// a < 50.5;      // does not compile: no comparison with a double
```

An inside does not compare with a `double`, because 0.1 is not 1/10 in
binary: an inside holding 1/10 would compare unequal to 0.1. Compare with
`0.1_r` or `just<1, 10>`.

## Printing

With `<beman/inside/inside/io.hpp>`, `to_string(x)`, `std::cout << x` and
`std::format("{}", x)` print the **exact** value:

| Grid | Prints |
|---|---|
| integer | the integer: `42` |
| a decimal notch such as `per<100>` | that many decimals, always: `19.90` |
| any other | the shortest exact form: `0.1875`, or a fraction such as `7/3` |

A format spec rounds once, from the exact value, by the type's rounding mode:

```cpp
using cents = inside<{{0, 1'000'000}, per<100>}>;
cents price{19.9_r};
std::println("{} {:.1f} {:>8}", price, price, price);   // 19.90 19.9    19.90
std::println("{:#04x}", inside<{0, 100}>{42});          // 0x2a
```

## Parsing

`parse<T>(text)` reads one number exactly, with no `double` in between, and
stores it through `T::try_make`:

```cpp
parse<pct>("42");                          // 42
parse<pct>("0.1");                         // errc::rounding_error: not on the grid
parse<pct>("150");                         // errc::overflow
parse<pct>("4x");                          // errc::invalid_format
parse<inside<{{0, 1}, per<10>}>>("0.1");   // exactly 1/10
std::cin >> a;                             // one token; failbit on an error
```

It reads every form `to_string` prints: `-1'000`, `1.25`, `1.5e2`, `0xff`,
`7/3`.

## Quantities

A quantity converts like its number, with the unit spelled out: `q.in(u)`
changes the unit exactly, `q.numerical_value()` gives the inside, and text
prints the number and the unit (`5 km`). See [Units](units.md#conversions).

## Details

- [Text in detail](../advanced/text.md): every printed form, the format specs
  and how they round, the parse grammar, `parse_exact`.
- [Determinism](../advanced/determinism.md#conversions-to-and-from-double):
  how a `double` is read and written.
