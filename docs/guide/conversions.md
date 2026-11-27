# Conversions and text

How values cross the boundary of an inside: native numbers in and out,
casts, comparisons, and text.

```cpp
id<"[0..1000]"> b{42};                     // in: from an int
auto n = b.to<int>();                      // out: 42, a plain int: 1000 fits one
auto m = b.to<std::uint8_t>();             // out: a std::expected, since 1000 does not fit a byte
std::cout << b;                            // text: 42
auto p = parse<id<"[0..1000]">>("42");     // text in: a std::expected
```

## Into an inside

An inside is built from an integer, a `double`, a `float` or another
inside (a constant such as `0.1_ins` is one). The store follows the type's [policy](policies.md): a value
out of range or between notches is an error unless the policy clamps, wraps
or rounds.

| Write | For |
|---|---|
| `T x{v};`, `x = v;` | a store under T's policy: proven by the grids, or a constant checked while compiling |
| `T{v, F}` | one store with the flags `F`: `T{v, clamp}`, `T{v, clamp \| round_half_away}`, `T{v, wrap}` |
| `exact_cast<T>(v)` | a store that reports any loss, whatever T's policy: a `std::expected` where it can fail |
| `T::try_make(v)` | a `std::expected`: the store for a run-time value the grids cannot prove |
| `T::from_raw(r, unsafe)` | raw transfer without checks, which is why it names `unsafe` ([Storage](../advanced/storage.md#raw-access)) |

To test a value without storing it:

```cpp
using bounded = id<"[0..100]">;
conversion_overflows<bounded>(150);   // true: outside [0, 100]
conversion_rounds<bounded>(3.5);      // true: not on notch 1
conversion_loses<bounded>(150);       // true: either of the two
```

## Out of an inside

| Write | Gives |
|---|---|
| `x.to<T>()` | a `T` where it cannot fail (every value of the grid is an integer that fits `T`); else a `std::expected<T, error>`: `overflow` when it does not fit (a negative value into an unsigned type too), `rounding_error` for a fraction |
| `x.to<int, round_floor>()` | the same, rounding a fraction by the given mode: a plain `int` when the grid fits |
| `x.to<double>()` | a `double`, rounded once from the exact value (there is no `double(x)`) |
| `x.numerator()`, `x.denominator()` | the exact value in lowest terms, as insides on the integers: `3` and `16` for 0.1875 (`.to<int>()` reads them) |
| `x.step()` | the count of notches above Lower, a `step_t<T>`; `T::from_step(i)` goes back |

The type of `to<T>()` says whether it can fail: where `T` holds the whole
grid it is a plain `T`, with nothing to check; elsewhere it is a
`std::expected`. `T::from_step(i)` follows the same rule: a plain `T` where
every value of `i` is a step of `T`, else an expected.

A fraction into an integer type needs permission to round, as a store does:
`x.to<int>()` of 2.75 is `error::rounding_error`, `x.to<int, round_floor>()`
is 2. A `float` or `double` result is rounded once from the exact value.

`to` is a free function too, `to<T>(x)`, which spares generic code the
`x.template to<T>()` spelling.

## Indices

One conversion is implicit: an index, an inside whose every value is a
non-negative integer, converts to the builtin integers. A container takes it
as it is:

```cpp
using row = id<"[0..479]">;
std::array<int, 480> a{};
row r = 17;
a[r] = 1;       // also std::vector, std::span, std::mdspan, a builtin array
```

- The target is any builtin integer type that holds the whole grid. A
  smaller one, `bool`, a character and a floating type do not compile:
  nothing narrows, and `int n = x;` loses nothing.
- The grid need not start at 0: `id<"[3..11]">` names the positions 3 to
  11 of a larger array. A grid with a negative or fractional value is no
  index (`x.to<T>()` reads it), and neither is a cursor, whose end lies one
  past the last index.
- No builtin operator takes an index except the subscript and the pointer
  arithmetic behind it: `n + x`, `n += x`, `x << 2`, `x & 1` and `if (x)` do
  not compile, so the arithmetic stays on insides.
- The index type is the proof of the bounds, so give a container the extent
  its index reaches. A result is an index where its grid is: `a[r + 1_ins]`
  compiles, and reaches 480. Store it into the index type first where that
  matters, by its policy: `row{r + 1_ins, clamp}`.
  [mdspan](../advanced/mdspan.md) builds extents from index types and
  checks an index type against an extent.

## Comparing

An inside compares with other insides, integers and rationals, exactly, and
without narrowing either side:

```cpp
id<"[0..100]"> a{42}, c{58};
a == 42;          // true
a < 50.5_ins;       // true
a + c == 100;     // true
// a < 50.5;      // does not compile: no comparison with a double
```

An inside does not compare with a `double`, because 0.1 is not 1/10 in
binary: an inside holding 1/10 would compare unequal to 0.1. Compare with
`0.1_ins` or `just<1, 10>`.

## Printing

With `<inside/inside/io.hpp>`, `to_string(x)`, `std::cout << x` and
`std::format("{}", x)` print the **exact** value:

| Grid | Prints |
|---|---|
| integer | the integer: `42` |
| a decimal notch such as `per<100>` | that many decimals, always: `19.90` |
| any other | the shortest exact form: `0.1875`, or a fraction such as `7/3` |

A format spec rounds once, from the exact value, by the type's rounding mode:

```cpp
using hundredths = id<"[0..1'000'000] 1/100">;
hundredths x{19.9_ins};
std::println("{} {:.1f} {:>8}", x, x, x);   // 19.90 19.9    19.90
std::println("{:#04x}", id<"[0..100]">{42});          // 0x2a
```

## Parsing

`parse<T>(text)` reads one number exactly, with no `double` in between, and
stores it through `T::try_make`:

```cpp
parse<bounded>("42");                      // 42
parse<bounded>("0.1");                     // error::rounding_error: not on the grid
parse<bounded>("150");                     // error::overflow
parse<bounded>("4x");                      // error::invalid_format
parse<id<"[0..1] 1/10">>("0.1");           // exactly 1/10
std::cin >> a;                             // one token; failbit on an error
```

It reads every form `to_string` prints: `-1'000`, `1.25`, `1.5e2`, `0xff`,
`7/3`.

## Quantities

A quantity converts like its number, with the unit spelled out: `q.in(u)`
changes the unit exactly, `q.numerical_value_in(u)` gives the inside in unit
`u`, and text
prints the number and the unit (`5 km`). See [Units](units.md#conversions).

## Details

- [Text in detail](../advanced/text.md): every printed form, the format specs
  and how they round, the parse grammar, `parse_exact`.
- [Determinism](../advanced/determinism.md#conversions-to-and-from-double):
  how a `double` is read and written.
