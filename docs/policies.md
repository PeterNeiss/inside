# Policies

The second template parameter of `bound<G, P>` controls what happens on
out-of-range assignment, on rounding mismatch, and on the various optional
runtime checks. Policies are **flag bits**; combine them with bitwise `|`.

```cpp
// Default: checked — runtime domain validation (throws std::system_error)
using safe = bound<{0, 100}>;
safe x = 150;      // throws std::system_error at runtime

// Unsafe: opt out of all runtime checks (compile-time checks always apply)
using fast = bound<{0, 100}, unsafe>;
fast f = 150;      // silently stores out-of-range value; UB on read

// Clamp: saturates to the nearest boundary
using clamped = bound<{0, 100}, clamp>;
clamped x = 150;   // x == 100
clamped y = -5;    // y == 0

// Wrap: modular arithmetic
using angle = bound<{0, 359}, wrap>;
angle a = 370;     // a == 10
angle b = -10;     // b == 350

// Sentinel: out-of-range produces nullopt (via slim::optional)
using index = bound<{0, 9}, sentinel>;
slim::optional<index> i = 10;  // i == nullopt
```

The default `checked` enables runtime domain checks and throws on violations.
Use `unsafe` to drop runtime checks for maximum performance when correctness
is proven elsewhere. `clamp`, `wrap`, and `sentinel` are mutually exclusive
(enforced by `static_assert`).

## Policy flags

| Flag | Effect |
|---|---|
| `checked` | runtime domain / round / overflow checks (**default**) |
| `unsafe` | opt out of all runtime checks |
| `clamp` | saturate to boundary on out-of-range (mutually exclusive with `wrap`/`sentinel`) |
| `wrap` | modular arithmetic on out-of-range |
| `sentinel` | out-of-range yields `nullopt` via `slim::optional` |
| `ignore_round` | rounding mismatches truncate toward zero (no error) |
| `round_nearest` | round to nearest notch, half away from zero (implies `ignore_round`) |
| `round_floor` | round toward −∞ (implies `ignore_round`) |
| `round_ceil` | round toward +∞ (implies `ignore_round`) |
| `round_half_even` | banker's rounding — half to even (implies `ignore_round`) |
| `ignore_zero` | division by zero is silent |
| `ignore_domain` | suppress the runtime domain check |

> **API-boundary shorthand:** the modern idiom for "saturate-and-round into
> this type" is to put `clamp | round_nearest` on the target bound's policy
> and write `T{value}`. This replaces the explicit
> `clamp_round<T>(value)` cast for typed boundaries — see
> [conversions.md](conversions.md#api-boundary-clamp--round_nearest).

## Per-operation override

Without a type-level policy, you can clamp, wrap, or pick a rounding mode on
a per-operation basis:

```cpp
bound<{0, 100}> x{50};
x.with_clamp() = 150;  // x == 100
x.with_wrap()  = 103;  // x == 2

bound<{{0, 10}, 2}> g{0};
g.with_floor()           = 3.0;  // g == 2
g.with_ceil()            = 3.0;  // g == 4
g.with_round_half_even() = 5.0;  // g == 4 (tie → even)
```

## Callbacks: `on_wrap` / `on_clamp` / `on_overflow` / `on_sentinel` / `on_error`

Each policy event can fire a zero-overhead callback. Unused handlers are
eliminated entirely by the compiler (`if constexpr` + `[[no_unique_address]]`).
Each handler receives the bound by mutable reference (so it can override the
stored value) plus an event-specific payload.

| Method | Fires on | Callback signature |
|---|---|---|
| `on_clamp(λ)`    | clamp narrowing             | `λ(bound&, overshoot)` |
| `on_wrap(λ)`     | wrap with carry             | `λ(bound&, carry)` |
| `on_sentinel(λ)` | sentinel write              | `λ(bound&, original_value)` |
| `on_error(λ)`    | domain / round error        | `λ(bound&, errc, std::string_view msg)` |
| `on_overflow(λ)` | rational/imax arithmetic OF | `λ(bound&, errc::overflow)` |

`on_*()` returns a temporary handle whose `=`/`+=`/`-=`/etc. apply the
operation with the callback wired in. Calling `on_*` automatically OR-merges
the policy bit it implies (e.g. `on_clamp` adds `clamp`).

```cpp
using sec = bound<{0, 59}, wrap>;
using min = bound<{0, 59}>;

sec seconds{0};
min minutes{0};

// When seconds overflows, carry into minutes.
seconds.on_wrap([&](auto& self, auto carry) {
    (void)self;
    minutes += carry;
}) = 125;
// seconds == 5, minutes == 2
```

The free arithmetic functions accept the same factories — useful for catching
divide-by-zero or rational overflow without throwing:

```cpp
auto q = div(d, z, on_overflow([&](auto& res, errc c) {
    res = std::remove_cvref_t<decltype(res)>{0};
    log(c);
}));
```

### `policy_ref` compound assignment with `real` RHS

`x.on_wrap(...) += rhs` (and `-=`, `*=`, `/=`) accept `real` RHS — `rational`,
`float`, `double`. So a runtime `double` delta flows straight through the
wrap callback without an intermediate cast:

```cpp
using pos = bound<{{0, 64}, notch<1, 16>}, wrap | round_nearest>;
pos p{0};
int wrap_count = 0;

p.on_wrap([&](auto&, auto) { ++wrap_count; }) += 65.5;
// p == 1.5, wrap_count == 1
```

See [examples/torus_map.cpp](../examples/torus_map.cpp) for a full sprite
position demo using this pattern on both axes.

### Combining actions: `with(...)`

`bound::with(actions...)` packs multiple `on_*` callbacks into a single
operation. Mutually exclusive combinations (e.g. `on_clamp` + `on_wrap`) are
rejected at compile time by `static_assert`.

```cpp
using c100 = bound<{0, 100}>;
c100 acc{50};

acc.with(
    on_overflow([&](auto& self, errc) { self = 0; /* imax saturated */ }),
    on_clamp   ([&](auto&, auto over)  { log_overshoot(over);          })
) += big_value;
```

## Error code mode

Instead of throwing, errors can be reported via `std::error_code`. This works
with construction, direct assignment, and per-operation policies:

```cpp
std::error_code ec;

// Construction with error code
bound<{0, 100}> x(150, ec);
// ec is set to EDOM, x is unchanged (default-constructed)

// Per-operation with error code
bound<{0, 100}> y{50};
y.policy(ec) = 200;
// ec is set to EDOM, y remains 50

// Free arithmetic with error code
auto sum = add(x, y, ec);
// overflow / range errors captured in ec

// Combining flags with error code
bound<{{0, 10}, 2}> coarse{0};
bound<{{0, 10}, 1}> fine{3};
coarse.policy<ignore_round>(ec) = fine;
// ignore_round suppresses the rounding error, ec captures domain errors only
```

The error code is only set on the first error (subsequent errors don't
overwrite it). Domain violations produce `EDOM`, rounding violations produce
`ENOSPC`.

## Optional construction

`try_make` returns `slim::optional<bound>` instead of throwing:

```cpp
auto maybe = bound<{0, 100}>::try_make(150);
if (!maybe) { /* out of range */ }
```

For types with a `clamp` or `wrap` policy, `try_make` applies the policy
before checking, so it will always succeed:

```cpp
auto clamped = bound<{0, 100}, clamp>::try_make(150);
// clamped has value 100 — clamp always succeeds
```
