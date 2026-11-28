# Reading compiler errors

An inside is `id<key K>`, its grid and policy as text (`id<key{G, P}>` names
the same type), and so is a quantity, its unit and kind in the text too. Compilers
print template arguments structurally, so an error shows the *whole* type.
This page decodes the types, lists the named messages the library gives
instead of bare "could not convert" errors, and shows how to probe an
operation without an error at all.

## Decoding an inside

An inside prints as the text that spells it ([spelling a type as
text](../guide/id.md)). The point `3_ins`, the sum of two digits, and a
heading:

```text
ins::id<ins::key{"[3]"}>
ins::id<ins::key{"[2..18]"}>
ins::id<ins::key{"[0..359] wrap"}>
```

The text is canonical: one per grid and policy, however the type was written,
so `id<"[0..360) wrap">`, `id<key{grid{0, 360, bounds::half_open}, wrap}>`
and the text above are one type.

| Text | Means |
|---|---|
| `[0..9]` | the limits, both included, and the step the limits derive: 1 here |
| `[0..10) 1/4` | the limits written with the smallest common denominator (an open limit moves in by one step: the last value is 39/4), then the step |
| `(0..1) 1/1000` | both limits open: 1/1000 … 999/1000 |
| `[0..1] <1000>` | a continuous grid: every fraction with a denominator of at most 1000 ([exact arithmetic](exact-arithmetic.md#bounded-continuous-grids)) |
| `[3]` | a point: one value, held in the type (`[3..3]` is a grid of step 1 with one value) |
| `0x1p70`, `0x3fffffffffffffffff` | a number past 64 bits, in hexadecimal, as m·2^e where shorter |
| `clamp`, `wrap`, `round_half_away`, `round_floor`, `round_ceil`, `round_half_even`, `round_toward_zero`, `unsafe` | the policy, joined by `\|`; no word is `checked`, the default |
| `cursor` | a `cursor<T, Step>` |
| `bits_N` | policy bits no flag names: a type the library rejects |

## Decoding a quantity

A quantity prints as its text, the unit in brackets after the number's, a
kind that is not the unit's own after it in braces, and a point's origin
after `@`
([quantities](../guide/id.md#quantities)):

```text
ins::id<ins::key{"[0..100] [km/h]"}>
ins::id<ins::key{"[0..100] [m]{height}"}>
ins::id<ins::key{"[1..81] [m*s]{any duration*length}"}>
ins::id<ins::key{"[-40..125] [deg_C] @ice_point"}>
```

| Text | Means |
|---|---|
| `[km/h]` | the unit: symbols with powers, `*` and `/`, in ASCII (`us` for µs, `deg_C` for ℃, `ohm` for Ω); a scaled unit in brackets (`[[1/381*ft]]`); `[1]` the unit one |
| `{height}` | exactly that kind (`isq::height[si::metre]`); no braces for the unit's own kind, any of it |
| `{any duration*length}` | the kinds a product or quotient is made of, `any` when any kind of each will do |
| `{none}`, `{any}` | no kind; any kind of the dimension |
| `@ice_point`, `@0` | a point: the quantity counted from that origin; `@0` from the implicit zero of the dimension |

Where a reference itself is a template argument (a quantity whose unit has no
text, which does not compile, or a trait's parts), it prints as the structure
behind it. A unit and its own kind holds that unit alone, here km:

```text
reference{ref_form{unit_atom{(& si::metre_def), 3, 1}}, false}
```

| Part | Means |
|---|---|
| `unit_atom{(& si::metre_def), 3, 1}` | a named unit (`si::metre`, by its definition object), its decimal prefix as a power of 10 (3: kilo), and its exponent (1). Its kind is the unit's own (any length) |
| `false` | the reference holds the unit alone |

Any other reference holds its parts: a kind, a unit of several factors or a
scale, or a product of kinds. A velocity in m/s:

```text
reference{ref_form{ref_parts{(& isq::velocity_def),
                             unit_value{unit_atom [6]{unit_atom{(& si::metre_def), 0, 1},
                                                      unit_atom{(& si::second_def), 0, -1}},
                                        magnitude{grid_number{1, 1, 0}, 0}},
                             false, recipe{kind_factor [6](), 0}}},
          true}
```

| Part | Means |
|---|---|
| `(& isq::velocity_def)` | the quantity kind, `isq::velocity`, by its definition object; `(& kind_def::none)` for none (any quantity of the dimension, as `si::metre / si::second` gives) |
| `unit_value{unit_atom [6]{…}, …}` | the unit's factors, each a named unit with its prefix and exponent |
| `magnitude{grid_number{1, 1, 0}, 0}` | the unit's extra factor as a grid number and a power of π: here 1·π⁰ |
| `false` | whether the kind is a unit's own (any kind of it) rather than exactly that kind |
| `recipe{…}` | the kinds a product is made of (`isq::width * isq::height`), empty otherwise |
| `true` | the reference holds its parts |

A unit or kind of your own declared in one line is spelled by its content
instead: `(& definition<unit_def, symbol_text{"ftm", 3}, …>::value)`, with
what it is defined by, and `(& kind_holder<symbol_text{"depth", 5}, …>::value)`.
Declare it as a named definition, as the catalog does, and it prints by name
([units of your own](../guide/units.md#units-of-your-own)):

```cpp
inline constexpr unit_def fathom_def{"ftm", 1.8288_ins * si::metre};
inline constexpr auto     fathom = defined_by<fathom_def>;   // unit_atom{(& fathom_def), 0, 1}
```

## Values in the message

A `static_assert` message can be computed (P2741, C++26), and the grid and
assignment checks print the numbers that failed:

```text
error: static assertion failed: grid: the notch must divide Upper − Lower evenly
       (notch 3, Upper − Lower 10)
error: static assertion failed: inside_assignable: incompatible notches — use
       `with<round_toward_zero>()` or a rounding policy to allow rounding (lhs notch 1, rhs notch 1/2)
error: static assertion failed: inside_assignable: rhs interval lies entirely outside
       lhs interval … (lhs [0, 3], rhs [5, 10])
```

## The common surprise: it is the interval, not the notch

```cpp
auto square(id<"[0..9]"> x) { return x * x; }
square(42_ins);                              // 42_ins is the point [42]: rejected
```

`id<"[0..9]">` holds 0 … 9, so 42 does not fit, though both are integers.
The fix is in *your* signature: give it a wide enough grid, or make it
generic:

```cpp
template <key K>
auto square(id<K> x) { return x * x; }
```

## Named reasons for a store

An impossible store or conversion is a deleted overload whose message names
the failing clause, instead of the compiler's bare "could not convert":

```text
error: use of deleted function '…': inside: the source's interval lies entirely
       outside this grid's, and the policy has no clamp or wrap - widen the grid, …
```

The two clauses are *interval lies entirely outside* (the value cannot fit
the range) and *incompatible notches* (the value is off the grid's step:
allow rounding with `with<round_toward_zero>()` or a rounding policy). Because the overload
is deleted, `std::is_constructible`, `std::convertible_to` and `requires`
probes answer `false`, so insides work in `std::variant`, `std::optional` and
other code that probes convertibility. For the two grids' numbers, ask:

```cpp
static_assert(ins::explain_assignable<Dst, decltype(src)>);   // prints the intervals and notches
```

## A store that names no channel

A store the grids cannot prove (some values of the source fit, some may not)
names what happens, or does not compile. For an inside, a compound
assignment, `++` or a flag proxy, the deleted overload says so:

```text
error: use of deleted function '…': inside: the source's values may fall outside
       this grid, or off its notch, at run time - store with clamp, wrap or a
       rounding mode (x.with<clamp>() = v, T{v, clamp}), report with
       T::try_make(v), x.on_error(ec) = v or x.on_error(f) = v, or opt out with unsafe
```

A plain number (an `int`, a `double`) goes through a
`consteval` constructor: a constant is checked while compiling, and a
run-time value is the compiler's own message, with the variable named:

```text
error: call to consteval function 'x.ins::id<ins::key{"[0..9]"}>::id<int>(n)' is not a constant expression
error: 'n' is not a constant expression
```

Read it as "this run-time number needs a channel": `T::try_make(n)`,
`T{n, clamp}`, `x.on_error(ec) = n`. Unlike a deleted overload, the
constructor exists, so `std::is_constructible_v<T, int>` is `true`; a generic
`emplace_back(5)` constructs at run time and does not compile (write
`emplace_back(5_ins)`). A constant that does not fit fails with the text:

```text
error: uncaught exception of type 'ins::detail::inside_error'; 'what()': 'inside: a checked
       store in a constant expression is outside the grid's interval - widen the interval, …'
```

(`inside_error` is thrown only during constant evaluation. Without
exceptions the same text is the template argument of `constexpr_error`.)

## Named reasons for a unit mismatch

Each units mistake is a deleted overload whose message says what to write
instead (`= delete("…")`):

| Code | Message |
|---|---|
| `just<1> * si::metre + just<1> * si::second` | `units: adding or comparing quantities of different dimensions` |
| `42 * si::metre` | `units: a raw number has no grid - write just<42> * unit, or Q{42, unit} into a quantity type Q` |
| `id<"[0..100] [m]">{42}` | `units: a number alone has no unit - write Q{42, si::metre}, or just<42> * si::metre` |
| `q.numerical_value()` | `units: a number alone has no unit - read q.numerical_value_in(si::metre)` |
| `q * 2` | `units: a raw number has no grid - scale by just<2>, not 2` |
| `length.in(si::second)` | `units: in() - the unit has another dimension` |
| `degrees.in(si::radian)` | `units: this conversion multiplies by a power of pi, which has no exact grid - round it with value_cast<unit, Rep>(q) from <inside/units/math.hpp>, Rep declared with a rounding mode` |
| `id<"[0..7]"> x = angle;` | `units: an angle (a distinct kind) or a pi-scaled ratio is no plain number - read numerical_value_in(si::radian), or value_cast it first` |
| `math::sin(degrees)` (the deduced form) | `units: sin and cos of an angle with a factor of pi (°, a turn) - name the result's notch: sin<per<K>>(q)` |

Kinds that never mix (Hz and Bq, Gy and Sv), and the kind boundaries of
angles, give messages of the same form. The `tests/fail/units_*`
cases pin each message.

## Probing operations: honest `requires`

Every operator, `add`, `sub`, `mul`, `div`, `mod`, `divmod`, the compound
assignments, the stores and `try_make` are constrained on whether the
operation has a result, and so are the quantity operations. An inside with a
raw scalar (`x + 1`) and a free function given a hook are deleted too. A requires-expression or
`if constexpr` probe answers truthfully:

```cpp
template <class A, class B>
constexpr bool can_divide = requires(A a, B b) { a / b; };
static_assert(!can_divide<id<"[0..9]">, id<"[0]">>);            // the point 0

template <class A, class B>
constexpr bool can_add = requires(A a, B b) { a + b; };
static_assert(!can_add<decltype(just<1> * si::metre), decltype(just<1> * si::second)>);
```

An impossible operator is a deleted overload whose message names the reason
(`use of deleted function …: division: no result type - …`).
