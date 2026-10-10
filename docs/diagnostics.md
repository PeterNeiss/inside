# Reading an `inside<>` in a compiler error

An inside is spelled `inside<grid G, policy_flag P>`. Compilers print both template
arguments structurally, so an error mentions the *whole* type — which looks noisy until
you know the three things it is showing you.

## Decoding the type

```
inside<grid{interval{rational{3,1}, rational{3,1}}, rational{0,1}}, 17179869184>
       └────────── interval [lo, hi] ──────────┘  └─ notch ─┘   └── policy P ──┘
```

- **interval** `{lo, hi}` — the inclusive value range, as exact fractions.
- **notch** — the grid step, also a fraction. `rational{0,1}` (= `0`) means a *continuous*
  point/range with no discrete step; it is **not** "missing".
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
| `1<<35` | 34359738368 | (unused) |
| `1<<36` | 68719476736 | `unsafe` marker (+ ignore_range, snap, ignore_zero); the only bit that turns runtime checks off |
| `1<<37` | … | (unused) |
| `1<<38` | … | (unused) |
| `1<<39` | … | (unused) |
| `1<<40` | … | (unused) |
| `1<<41` | … | (unused) |
| `1<<42`…`1<<49` | … | (unused) |

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
static_assert(beman::inside::why_assignable<DstInside, decltype(src)>);  // prints the named reasons
```

## `BEMAN_INSIDE_STRICT_SFINAE` — turning the hints off

The automatic hints work by giving `inside` a diagnostic overload for incompatible types,
which makes `std::is_constructible` / `std::convertible_to` report `true` for them (the
error surfaces on *use*, not on the trait). If you embed `inside` in `std::variant`,
`std::optional`, or other trait-driven generic code that *probes* convertibility, configure
with `-DBEMAN_INSIDE_STRICT_SFINAE=ON` (defines `BEMAN_INSIDE_STRICT_SFINAE`). That drops the diagnostic
overloads — `inside` becomes SFINAE-pure with honest traits, at the cost of the bare
"could not convert" message. `beman::inside::why_assignable` still works in strict builds.
