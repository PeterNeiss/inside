# Policies

The second template parameter of `bound<G, P>` controls what happens on
out-of-range assignment, on rounding mismatch, and on the various optional
runtime checks. Policies are **flag bits**; combine them with bitwise `|`.

```cpp
// Default: checked — runtime domain validation (throws bnd::bound_error)
using safe = bound<{0, 100}>;
safe x = 150;      // throws bnd::bound_error at runtime

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
| `snap` | rounding mismatches truncate toward zero (no error) |
| `round_nearest` | round to nearest notch, half away from zero (implies `snap`) |
| `round_floor` | round toward −∞ (implies `snap`) |
| `round_ceil` | round toward +∞ (implies `snap`) |
| `round_half_even` | banker's rounding — half to even (implies `snap`) |
| `ignore_zero` | skip the divide-by-zero check — `a / 0` / `a % 0` is UB (binary `div`/`mod`); compound `/= 0` / `%= 0` no-op |
| `ignore_domain` | suppress the runtime domain check |
| `f64` / `f32` / `exact` / `direct` / `indexed` / `i8`…`u64` | **representation flags** — select how the raw value is stored; see the next section (`real` is a deprecated alias of `f64`) |

## Representation flags

Besides the *behavior* flags above, these flags select the **representation**
— what the raw storage holds. Without one, storage is deduced from the grid
(see [storage.md](storage.md#choosing-the-representation)).

| Flag | Forces | Grid requirement | Notes |
|---|---|---|---|
| `f64` | IEEE-754 `double` raw (the value itself, snapped to the grid) | dyadic **and** double-exact (every value fits `double`'s 53-bit significand) | bundles `round_nearest`; the fast math-storage flag. Arithmetic drops `f64` to an exact representation when a result grid is too fine for `double`. Under `BND_MATH_FIXED` it falls back to integer storage. **`real` is a deprecated alias of `f64`.** |
| `f32` | IEEE-754 `float` raw (the value itself, snapped to the grid) | dyadic **and** float-exact (every value fits `float`'s 24-bit significand) | the binary32 sibling of `f64`, for single-precision FPUs and the `flt` engine. Arithmetic **demotes `f32`→`f64`** when a result grid outgrows `float` (then drops to exact when it outgrows `double`). Under `BND_MATH_FIXED` it falls back to integer storage. |
| `exact` | exact-fraction raw on **any** grid | none | no notch-count limit, no `double` anywhere; arithmetic is exact — on notched grids overflow is usually provably impossible and `+ − ×` return plain bounds (no `optional`) |
| `i8 u8 i16 u16 i32 u32 i64 u64` | the named fixed-width integer raw | value storage needs `Notch == 1` and the value range to fit (add `indexed` for a notched grid) | **pins the exact backing type** (e.g. a `uint16_t` where deduction would pick `uint8_t`) for a fixed wire layout. Bare = value storage (`raw() == value`, like `direct`); `+ indexed` = 0-based index storage. **No silent widening** — a type too small for the grid is a compile error. One width flag at a time; dropped on arithmetic results. |
| `direct` | raw == value as a plain integer | `Notch == 1` | e.g. `bound<{5, 100}, direct>` stores 5..100, not index 0..95 — the raw equals the wire/debugger value |
| `indexed` | raw == 0-based notch index | `Notch != 0` | e.g. `bound<{-5, 5}, indexed>` stores 0..10 unsigned — dense layout for serialization |

```cpp
using gain   = bound<{{0, 4}, notch<1, 65536>}, round_nearest | f64>;  // math operand
using ratio  = bound<{{0, 1}, notch<1, 3>},     exact>;                 // thirds, exactly
using regval = bound<{5, 100}, direct>;       // raw() == value, interop-friendly
using slot   = bound<{-5, 5},  indexed>;      // raw() == 0..10, dense unsigned
using wire   = bound<{0, 100}, u16>;          // pin uint16_t, raw() == value
```

Binary arithmetic ORs the policies of both operands, so a result can carry
several representation flags; storage selection resolves them
**widest-wins**: `exact > f64 > f32 > {width} > direct > indexed > deduced`. An
`exact + f64` sum is therefore exact, and a `f64` math chain stays
double-backed end to end — no errors at mixed call sites.

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
g.with_snap<round_floor>()           = 3.0;  // g == 2
g.with_snap<round_ceil>()            = 3.0;  // g == 4
g.with_snap<round_half_even>() = 5.0;  // g == 4 (tie → even)
```

## Callbacks: `on_wrap` / `on_clamp` / `on_overflow` / `on_sentinel` / `on_error`

Each policy event can fire a zero-overhead callback. Unused handlers are
eliminated entirely by the compiler (`if constexpr` + `[[no_unique_address]]`).
Each handler receives the bound by mutable reference (so it can override the
stored value) plus an event-specific payload.

| Method | Path | Fires when | Callback signature |
|---|---|---|---|
| `on_clamp(λ)`    | assignment | a narrowed value leaves the grid and `clamp` saturates it | `λ(bound&, overshoot)` |
| `on_wrap(λ)`     | assignment | a narrowed value leaves the grid and `wrap` folds it (carry) | `λ(bound&, carry)` |
| `on_sentinel(λ)` | assignment | an out-of-range write under `sentinel` stores the empty slot | `λ(bound&, original_value)` |
| `on_error(λ)`    | assignment | a domain / rounding error under `checked` (replaces the throw) | `λ(bound&, errc, std::string_view msg)` |
| `on_overflow(λ)` | binary arithmetic | a fractional or imax result overflows, or `div`/`mod` divides by zero | `λ(bound&, errc)` |

The first four fire on the **assignment** path — narrowing a value *into* a
bound: a direct `=`, the `.on_*()= …` / `with(…) = …` proxies, and the
compound `+= / -= / *= / /=`. `on_overflow` fires on the **binary-arithmetic**
path — the free `add` / `sub` / `mul` / `div` / `mod` and the `+ - * /`
operators, whose widened result can overflow (or, for `div`/`mod`, hit a zero
divisor).

`on_*()` returns a temporary handle whose `=`/`+=`/`-=`/etc. apply the
operation with the callback wired in. Calling `on_*` automatically OR-merges
the policy bit it implies (e.g. `on_clamp` adds `clamp`). A pack may carry
handlers for *both* paths — e.g. `with(on_overflow(…), on_clamp(…))` on a
compound `+=`, whose imax probe can overflow *and* whose narrowing can clamp —
and each handler fires only on its own path; handlers that a given operation
never reaches are accepted but simply not invoked.

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
divide-by-zero or arithmetic overflow without throwing:

```cpp
auto q = div(d, z, on_overflow([&](auto& res, errc c) {
    res = std::remove_cvref_t<decltype(res)>{0};
    log(c);
}));
```

### `policy_ref` compound assignment with a floating-point or bound RHS

`x.on_wrap(...) += rhs` (and `-=`, `*=`, `/=`) accept a `float` / `double` RHS
or another bound. So a runtime `double` delta flows straight through the
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

Instead of throwing, errors can be reported via `bnd::errc` (the library has no
`std::error_code` / `<system_error>` dependency). This works with construction,
direct assignment, and per-operation policies:

```cpp
bnd::errc ec{};   // value-initialised: errc{} == 0 means "no error"

// Construction with error code
bound<{0, 100}> x(150, ec);
// ec is set to errc::domain_error; on error x's value is ill-defined — check ec before reading x

// Per-operation with error code
bound<{0, 100}> y{50};
y.policy(ec) = 200;
// ec is set to errc::domain_error, y remains 50 (a failed assignment leaves the prior value intact)

// Free arithmetic with error code
auto sum = add(y, y, ec);
// overflow / range errors captured in ec

// Combining flags with error code
bound<{{0, 10}, 2}> coarse{0};
bound<{{0, 10}, 1}> fine{3};
coarse.policy<snap>(ec) = fine;
// snap suppresses the rounding error, ec captures domain errors only
```

The error code is only set on the first error (subsequent errors don't
overwrite it). Domain violations produce `errc::domain_error`, rounding
violations produce `errc::rounding_error`.

## Replacing the throw handler (freestanding / bare-metal)

The throwing default is a single replaceable hook. Every checked failure funnels
through `bnd::detail::raise`, which calls the installed handler:

```cpp
using bnd::errc;
using bnd::error_handler_t;

// Default handler throws bnd::bound_error (which carries `errc code`). When the
// program is compiled with exceptions disabled the default instead traps.
// Install your own to redirect failures (log, reset, longjmp, …):
error_handler_t prev = bnd::set_error_handler(
    [](errc code, const char* what) noexcept -> void {
        my_log(code, what);
        my_reset();            // a handler MUST NOT return; raise() traps if it does
        for (;;) {}
    });
// ... bnd::set_error_handler(prev);  // restore; nullptr also restores the default
```

This — together with not including `bound/io.hpp` (or defining `BND_NO_STRING`
in the single-header build) — lets the core run with no `<string>`,
`<system_error>`, or C++ exception-ABI dependency.

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
