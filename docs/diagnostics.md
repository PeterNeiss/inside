# Reading a `bound<>` in a compiler error

A bound is spelled `bound<grid G, policy_flag P>`. Compilers print both template
arguments structurally, so an error mentions the *whole* type — which looks noisy until
you know the three things it is showing you.

## Decoding the type

```
bound<grid{interval{rational{3,1}, rational{3,1}}, rational{0,1}}, 17179869184>
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
| `1<<2` | 4 | `ignore_domain` |
| `1<<4` | 16 | `snap` |
| `1<<5` | 32 | `round_nearest` (+ snap) |
| `1<<32` | 4294967296 | `clamp` |
| `1<<33` | 8589934592 | `wrap` |
| `1<<34` | 17179869184 | `checked` (the default `P`) |
| `1<<35` | 34359738368 | `sentinel` |
| `1<<37` | … | `real` |
| `1<<38` | … | `exact` |

So `17179869184` is simply `checked`, the default policy every `bound` carries unless you
choose another. (Full table: `include/bound/policy_flag.hpp`.)

## The common surprise: it's the interval, not the notch

```cpp
auto square(bound<> x) { return x * x; }   // bound<> defaults to grid {[0,0], 0}
square(3_b);                               // 3_b is the point [3,3]
```

`bound<>` defaults to the **empty grid `[0,0]`** — it can represent only `0`. Passing `3`
is out of range, so the conversion is rejected. The fix is in *your* signature: give
`square` a grid wide enough, or make it generic:

```cpp
template <grid G, policy_flag P>
auto square(bound<G, P> x) { return x * x; }
```

## Getting the *reason* at compile time

By default `bound` turns an impossible assignment/conversion into a **named** message
instead of the compiler's bare "could not convert":

```
error: static assertion failed: bound_assignable: rhs interval lies entirely outside
       lhs interval and the policy (not wrap/clamp) cannot bring it into range …
```

Two clauses you'll see: *interval lies entirely outside* (value can't fit the range) and
*incompatible notches* (value is off the grid step — opt into rounding with
`policy<snap>()` / `with_snap()`).

You can also ask explicitly, anywhere:

```cpp
static_assert(bnd::why_assignable<DstBound, decltype(src)>);  // prints the named reasons
```

## `BOUND_STRICT_SFINAE` — turning the hints off

The automatic hints work by giving `bound` a diagnostic overload for incompatible types,
which makes `std::is_constructible` / `std::convertible_to` report `true` for them (the
error surfaces on *use*, not on the trait). If you embed `bound` in `std::variant`,
`std::optional`, or other trait-driven generic code that *probes* convertibility, configure
with `-DBOUND_STRICT_SFINAE=ON` (defines `BND_STRICT_SFINAE`). That drops the diagnostic
overloads — `bound` becomes SFINAE-pure with honest traits, at the cost of the bare
"could not convert" message. `bnd::why_assignable` still works in strict builds.
