# Reading an `inside<>` in a compiler error

An inside is spelled `inside<grid G, policy_flag P>`. Compilers print both template
arguments structurally, so an error mentions the *whole* type — which looks noisy until
you know the three things it is showing you.

## Decoding the type

GCC spells every name out; Clang abbreviates. The point `3_ins` reads:

```
GCC:   inside<small_grids_v2::grid{small_grids_v2::interval{grid_rational{3, 1}, grid_rational{3, 1}},
                                   grid_rational{0, 1}, grid_rational{0, 1}}, 17179869184>
Clang: inside<grid{{{3, 1}, {3, 1}}, {0, 1}, {0, 1}}>
              └─ interval ─┘  └notch┘ └bound┘
```

(GCC also prefixes `beman::inside::` and `beman::inside::detail::`, shortened
here. Clang leaves out a policy argument that equals the default.)

- **interval** `{lo, hi}` — the inclusive value range, as exact fractions. A
  fraction prints as `{numerator, denominator}` with the sign on the
  denominator: −3 is `{3, -1}`.
- **notch** — the grid step, also a fraction. `{0, 1}` (= `0`) means a
  *continuous* point/range with no discrete step; it is **not** "missing".
- **bound** — `MaxDenominator`, the denominator bound of a bounded continuous
  grid ([arithmetic.md](arithmetic.md#bounded-continuous-grids)); `{0, 1}` on
  every other grid.
- **`small_grids_v2` / `big_grids_v2`** — the grid ABI namespace: 64-bit grid
  numbers, or C++26 big grids (then each number is a `big_rational` of limbs).
- **policy `P`** — a bitset printed as a plain integer (it is an `unsigned long long`). The
  compiler cannot print the flag names, so decode the bits yourself:

| bit value | decimal | flag |
|---|---|---|
| `1<<1` | 2 | `ignore_zero` |
| `1<<2` | 4 | `ignore_range` |
| `1<<4` | 16 | `snap` |
| `1<<5` | 32 | `round_nearest` (+ snap) |
| `1<<6` | 64 | `round_floor` (+ snap) |
| `1<<7` | 128 | `round_ceil` (+ snap) |
| `1<<8` | 256 | `round_half_even` (+ snap) |
| `1<<32` | 4294967296 | `clamp` |
| `1<<33` | 8589934592 | `wrap` |
| `1<<34` | 17179869184 | `checked` (the default `P`; overrides `unsafe`) |
| `1<<36` | 68719476736 | `unsafe` marker (+ ignore_range, snap, ignore_zero); the only bit that turns runtime checks off |
| `1<<50` | 1125899906842624 | a `cursor<T, Step>` |

The other bits are unused.

So `17179869184` is simply `checked`, the default policy of `inside<G>`. Every
other policy without the `unsafe` bit is checked too. (Full table:
`include/beman/inside/policy_flag.hpp`.)

## The common surprise: it's the interval, not the notch

```cpp
auto square(inside<> x) { return x * x; }   // inside<> defaults to grid {[0,0], 0}
square(3_ins);                               // 3_ins is the point [3,3]
```

`inside<>` defaults to the **empty grid `[0,0]`** — it can represent only `0`. Passing `3`
is out of range, so the conversion is rejected. The fix is in *your* signature: give
`square` a grid wide enough, or make it generic:

```cpp
template <grid G, policy_flag P>
auto square(inside<G, P> x) { return x * x; }
```

## Getting the *reason* at compile time

By default `inside` turns an impossible assignment/conversion into a **named** message
instead of the compiler's bare "could not convert":

```
error: static assertion failed: inside_assignable: rhs interval lies entirely outside
       lhs interval and the policy (not wrap/clamp) cannot bring it into range …
```

Two clauses you'll see: *interval lies entirely outside* (value can't fit the range) and
*incompatible notches* (value is off the grid step — opt into rounding with
`policy<snap>()` / `with_snap()`).

You can also ask explicitly, anywhere:

```cpp
static_assert(beman::inside::explain_assignable<DstInside, decltype(src)>);  // prints the named reasons
```

## Probing operations: honest `requires`

Every arithmetic operator, `add` / `sub` / `mul` / `div` / `mod` / `divmod`, the
compound assignments and `try_make` are constrained on whether the operation has a
result, so a requires-expression or `if constexpr` probe answers truthfully:

```cpp
template <class A, class B>
constexpr bool can_divide = requires(A a, B b) { a / b; };
static_assert(!can_divide<inside<{0, 9}>, inside<{{-2, 2}, 0}>>); // continuous divisor reaching 0
static_assert(!can_divide<inside<{0, 9}>, inside<{0}>>);          // the point 0
```

An impossible operator is a deleted overload whose message names the reason
(`use of deleted function …: division: no result grid - …`); compilers before GCC 15 /
Clang 19 print only *use of deleted function*.

## `BEMAN_INSIDE_STRICT_SFINAE` — turning the hints off

The automatic hints work by giving `inside` a diagnostic overload for incompatible types,
which makes `std::is_constructible` / `std::convertible_to` report `true` for them (the
error surfaces on *use*, not on the trait). If you embed `inside` in `std::variant`,
`std::optional`, or other trait-driven generic code that *probes* convertibility, configure
with `-DBEMAN_INSIDE_STRICT_SFINAE=ON` (defines `BEMAN_INSIDE_STRICT_SFINAE`). That drops the diagnostic
overloads — `inside` becomes SFINAE-pure with honest traits, at the cost of the bare
"could not convert" message. `beman::inside::explain_assignable` still works in strict builds.
