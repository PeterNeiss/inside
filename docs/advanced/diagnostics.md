# Reading compiler errors

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

An inside is `inside<grid G, policy_flag P>`, and a quantity is
`quantity<reference R, Rep>`. Compilers print template arguments
structurally, so an error shows the *whole* type. That looks noisy until you
know what each part is. This page decodes the types, lists the named messages
the library gives instead of bare "could not convert" errors, and shows how to
probe an operation without an error at all.

## Decoding an inside

GCC spells every name out. The point `3_ins`:

```text
inside<grid{interval{grid_number{3, 1, 0}, grid_number{3, 1, 0}},
            grid_number{0, 1, 0}, grid_number{0, 1, 0}}, 0>
```

GCC also prefixes `ins::`, shortened here. The `grid_number` pair
is the interval, the next one the notch, the last the bound:

| Part | Means |
|---|---|
| `grid_number{num, den, big}` | a grid number, an exact fraction in lowest terms: `num` the numerator's magnitude, `den` the denominator with the sign, so −3 is `grid_number{3, -1, 0}` and 1/100 is `grid_number{1, 100, 0}`. A value past 64 bits has `den` 0 and `big` pointing at the interned value |
| interval | the range, both limits included |
| notch | the step. 0 is a *continuous* grid or a point, not a missing value |
| bound | `MaxDenominator`, the denominator bound of a bounded continuous grid ([exact arithmetic](exact-arithmetic.md#bounded-continuous-grids)); 0 otherwise |
| the policy `P` | a bitset, printed as an integer: decode it with the table below |

| Bit | Decimal | Flag |
|---|---:|---|
| `1<<4` | 16 | `round_toward_zero`: the rounding bit, which every rounding mode carries |
| `1<<5` | 32 | `round_half_away` (with the rounding bit: 48) |
| `1<<6` | 64 | `round_floor` (with the rounding bit: 80) |
| `1<<7` | 128 | `round_ceil` (with the rounding bit: 144) |
| `1<<8` | 256 | `round_half_even` (with the rounding bit: 272) |
| `1<<32` | 4294967296 | `clamp` |
| `1<<33` | 8589934592 | `wrap` |
| `1<<36` | 68719476736 | the `unsafe` marker (`unsafe` adds the rounding bit: 68719476752); the only bit that turns runtime checks off |
| `1<<50` | 1125899906842624 | a `cursor<T, Step>` |

The other bits are unused, and an inside rejects a policy that holds one or
names two rounding modes. `checked`, the default, has no bits: `0` is
`inside<G>`, and `48` is `round_half_away`. Every policy without the `unsafe`
bit is checked. (The definitions are in `include/inside/inside/policy_flag.hpp`.)

## Decoding a quantity

GCC prints a quantity's reference as the structure behind it:

```text
quantity<reference{kind_ref{(& kind_holder<symbol_text{"length", 6}, …>::value), true}, true, recipe{…},
                   unit_value{unit_atom [6]{unit_atom{(& definition<unit_def, symbol_text{"m", 1}, …>::value), 3, 1}},
                              1, magnitude{grid_number{1, 1, 0}, 0}}},
         inside<…>>
```

| Part | Means |
|---|---|
| `kind_ref{(& kind_holder<symbol_text{"length", 6}, …>::value), true}` | the quantity kind: `isq::length` |
| `unit_atom{(& definition<unit_def, symbol_text{"m", 1}, …>::value), 3, 1}` | a named unit (`m`, and after it what it is defined by), its decimal prefix as a power of 10 (3: kilo), and its exponent (1) |
| `magnitude{grid_number{1, 1, 0}, 0}` | the unit's extra factor as a grid number and a power of π: here 1·π⁰ |
| `inside<…>` | the number, decoded as above |

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
auto square(inside<> x) { return x * x; }   // inside<> is the grid {[0, 0], 0}
square(3_ins);                               // 3_ins is the point [3, 3]: rejected
```

`inside<>` defaults to the grid `[0, 0]`, which holds only 0, so 3 does not
fit. The fix is in *your* signature: give it a wide enough grid, or make it
generic:

```cpp
template <grid G, policy_flag P>
auto square(inside<G, P> x) { return x * x; }
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
error: call to consteval function 'inside<…>(n)' is not a constant expression
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
| `quantity<si::metre, inside<{0, 100}>>{42}` | `units: a number alone has no unit - write Q{42, si::metre}, or just<42> * si::metre` |
| `q.numerical_value()` | `units: a number alone has no unit - read q.numerical_value_in(si::metre)` |
| `q * 2` | `units: a raw number has no grid - scale by just<2>, not 2` |
| `length.in(si::second)` | `units: in() - the unit has another dimension` |
| `degrees.in(si::radian)` | `units: this conversion multiplies by a power of pi, which has no exact grid - round it with value_cast<unit, Rep>(q) from <inside/units/math.hpp>, Rep declared with a rounding mode` |
| `inside<{0, 7}> x = angle;` | `units: an angle (a distinct kind) or a pi-scaled ratio is no plain number - read numerical_value_in(si::radian), or value_cast it first` |
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
static_assert(!can_divide<inside<{0, 9}>, inside<{0}>>);            // the point 0

template <class A, class B>
constexpr bool can_add = requires(A a, B b) { a + b; };
static_assert(!can_add<decltype(just<1> * si::metre), decltype(just<1> * si::second)>);
```

An impossible operator is a deleted overload whose message names the reason
(`use of deleted function …: division: no result type - …`).
