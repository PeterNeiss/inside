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
inside<grid{interval{grid_rational{big_int{3, 0, 0, false}, big_int{1, 0, 0, false}},
                     grid_rational{big_int{3, 0, 0, false}, big_int{1, 0, 0, false}}},
            grid_rational{big_int{0, 0, 0, false}, big_int{1, 0, 0, false}},
            grid_rational{big_int{0, 0, 0, false}, big_int{1, 0, 0, false}}}, 17179869184>
```

GCC also prefixes `beman::inside::` and `beman::inside::detail::`, shortened
here. The first `grid_rational` pair is the interval, the next the notch, the
last the bound:

| Part | Means |
|---|---|
| `grid_rational{num, den}` | a grid number, an exact fraction: numerator and denominator as `big_int`s |
| `big_int{small, limbs, size, negative}` | an integer: `small` is the magnitude while it fits 64 bits (`limbs` 0, `size` 0); past that `limbs` points to `size` interned 64-bit limbs, least significant first. −3 is `big_int{3, 0, 0, true}` |
| interval | the range, both limits included |
| notch | the step. 0 is a *continuous* grid or a point, not a missing value |
| bound | `MaxDenominator`, the denominator bound of a bounded continuous grid ([arithmetic.md](arithmetic.md#bounded-continuous-grids)); 0 otherwise |
| the policy `P` | a bitset, printed as an integer: decode it with the table below |

| Bit | Decimal | Flag |
|---|---:|---|
| `1<<4` | 16 | `snap`, alone also `round_toward_zero` |
| `1<<5` | 32 | `round_nearest` (with `snap`: 48) |
| `1<<6` | 64 | `round_floor` (with `snap`: 80) |
| `1<<7` | 128 | `round_ceil` (with `snap`: 144) |
| `1<<8` | 256 | `round_half_even` (with `snap`: 272) |
| `1<<32` | 4294967296 | `clamp` |
| `1<<33` | 8589934592 | `wrap` |
| `1<<34` | 17179869184 | `checked`: the default policy |
| `1<<36` | 68719476736 | the `unsafe` marker (`unsafe` adds `snap`: 68719476752); the only bit that turns runtime checks off, also beside `checked` |
| `1<<37` | 137438953472 | `uninitialized` |
| `1<<50` | 1125899906842624 | a `cursor<T, Step>` |

The other bits are unused. So `17179869184` is plain `checked`, the default of
`inside<G>`, and `17179869232` is `checked | round_nearest`. Every policy
without the `unsafe` bit is checked. (The definitions are in
`include/beman/inside/policy_flag.hpp`.)

## Decoding a quantity

GCC prints a quantity's reference as the structure behind it:

```text
quantity<reference{spec_ref{(& isq_defs::length), true}, true, recipe{…},
                   unit{unit_atom[6]{unit_atom{(& si_defs::metre), 3, 1}}, 1, magnitude{grid_rational{…}, 0}}},
         inside<…>>
```

| Part | Means |
|---|---|
| `spec_ref{(& isq_defs::length), true}` | the quantity kind: `isq::length` |
| `unit_atom{(& si_defs::metre), 3, 1}` | a named unit, its decimal prefix as a power of 10 (3: kilo), and its exponent (1) |
| `magnitude{grid_rational{…}, 0}` | the unit's extra factor as a grid number and a power of π: here 1·π⁰ |
| `inside<…>` | the number, decoded as above |

## Values in the message

A `static_assert` message can be computed (P2741, C++26), and the grid and
assignment checks print the numbers that failed:

```text
error: static assertion failed: grid: the notch must divide Upper − Lower evenly
       (notch 3, Upper − Lower 10)
error: static assertion failed: inside_assignable: incompatible notches — use
       `with_snap()` or `policy<snap>()` to allow rounding (lhs notch 1, rhs notch 1/2)
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

An impossible store or conversion gives a **named** message instead of the
compiler's bare "could not convert":

```text
error: static assertion failed: inside_assignable: rhs interval lies entirely outside
       lhs interval and the policy (not wrap/clamp) cannot bring it into range …
```

The two clauses you will see are *interval lies entirely outside* (the value
cannot fit the range) and *incompatible notches* (the value is off the grid's
step: allow rounding with `policy<snap>()` or `with_snap()`). You can ask for
the reasons anywhere:

```cpp
static_assert(beman::inside::explain_assignable<Dst, decltype(src)>);   // prints the reasons
```

## Named reasons for a unit mismatch

Each units mistake is a deleted overload whose message says what to write
instead (`= delete("…")`):

| Code | Message |
|---|---|
| `just<1> * si::metre + just<1> * si::second` | `units: adding or comparing quantities of different dimensions` |
| `42 * si::metre` | `units: a raw number has no grid - write just<42> * unit, or quantity<unit, inside<…>>{42}` |
| `q * 2` | `units: a raw number has no grid - scale by just<2>, not 2` |
| `length.in(si::second)` | `units: in() - the unit has another dimension` |
| `degrees.in(si::radian)` | `units: this conversion multiplies by a power of pi, which has no exact grid - round it with value_cast<unit, Rep>(q) from <beman/inside/units_math.hpp>, Rep declared with a rounding mode` |
| `inside<{0, 7}> x = angle;` | `units: an angle (an is_kind quantity) or a pi-scaled ratio is no plain number - read numerical_value(), or value_cast it first` |
| `units::sin(degrees)` (the deduced form) | `units: sin takes an angle in radians - sin_into<Out>(q) takes a unit with a factor of pi` |

Kinds that never mix (Hz and Bq, Gy and Sv), and the kind boundaries of
angles, give messages of the same form. The `tests/beman/inside/fail/units_*`
cases pin each message.

## Probing operations: honest `requires`

Every operator, `add`, `sub`, `mul`, `div`, `mod`, `divmod`, the compound
assignments and `try_make` are constrained on whether the operation has a
result, and so are the quantity operations. A requires-expression or
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

## `BEMAN_INSIDE_STRICT_SFINAE`: turning the hints off

The named store messages come from a diagnostic overload for incompatible
types. That overload makes `std::is_constructible` and `std::convertible_to`
report `true` for them: the error appears on *use*, not in the trait. If you
put insides in `std::variant`, `std::optional` or other code that *probes*
convertibility, configure with `-DBEMAN_INSIDE_STRICT_SFINAE=ON`. That drops
the diagnostic overloads, so the traits are honest, at the cost of the bare
"could not convert" message. `explain_assignable` still works.
