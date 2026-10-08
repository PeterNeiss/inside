# Policies

The second template parameter of `inside<G, P>` controls what happens on
out-of-range assignment, on rounding mismatch, and on the various opt-in
runtime checks. Policies are **flag bits**; combine them with bitwise `|`.

```cpp
// Default: checked — runtime range validation (throws beman::inside::inside_error)
using safe = inside<{0, 100}>;
safe x = 150;      // throws beman::inside::inside_error at runtime

// Unsafe: opt out of all runtime checks (compile-time checks always apply)
using fast = inside<{0, 100}, unsafe>;
fast f = 150;      // silently stores out-of-range value; UB on read

// Clamp: saturates to the nearest boundary
using clamped = inside<{0, 100}, clamp>;
clamped x = 150;   // x == 100
clamped y = -5;    // y == 0

// Wrap: modular arithmetic
using angle = inside<{0, 359}, wrap>;
angle a = 370;     // a == 10
angle b = -10;     // b == 350

// try_make: out-of-range is a value you test, not a throw
using index = inside<{0, 9}>;
auto i = index::try_make(10);  // !i, i.error() == errc::overflow
```

Every policy runs the runtime checks unless it carries `unsafe`:
`inside<G>`, `inside<G, round_nearest>` and `inside<G, f64>` all report an
out-of-range or off-notch value (by default, by throwing). `clamp` / `wrap`
handle the range instead, and `snap` / `round_*` handle the notch. Use `unsafe`
to drop runtime checks for maximum performance when correctness is proven
elsewhere; spelling `checked` next to `unsafe` turns them back on. `clamp` and
`wrap` are mutually exclusive (enforced by `static_assert`).

A rounding policy rounds **before** the range check: with
`inside<{0, 9}, round_floor>`, assigning 9.5 stores 9, while 10.0 is out of
range. The conversion predicates follow the same order
(`conversion_overflows<B>(9.5)` is false for that type, `conversion_rounds` is
true). A policy without a rounding mode rounds nothing, so 9.5 into
`inside<{0, 9}>` is out of range.

## Policy flags

| Flag | Effect |
|---|---|
| `checked` | runtime range / notch / overflow checks — on for every policy without `unsafe`; spelled explicitly only to override `unsafe` |
| `unsafe` | opt out of all runtime checks |
| `clamp` | saturate to boundary on out-of-range (mutually exclusive with `wrap`) |
| `wrap` | modular arithmetic on out-of-range |
| `snap` | rounding mismatches truncate toward zero (no error) |
| `round_nearest` | round to nearest notch, half away from zero (implies `snap`) |
| `round_floor` | round toward −∞ (implies `snap`) |
| `round_ceil` | round toward +∞ (implies `snap`) |
| `round_half_even` | banker's rounding — half to even (implies `snap`) |
| `ignore_zero` | skip the divide-by-zero check — `a / 0` / `a % 0` is UB (binary `div`/`mod`); compound `/= 0` / `%= 0` no-op |
| `ignore_range` | suppress the runtime range check |
| `f64` / `f32` / `exact` / `direct` / `indexed` / `i8`…`u64` | **representation flags** — select how the raw value is stored; see the next section |

## Representation flags

`f64`, `f32`, `exact`, `direct`, `indexed` and `i8`…`u64` select what the raw
storage holds instead of the deduced default; arithmetic results resolve them
widest-wins. The table and rules are in
[storage.md](storage.md#choosing-the-representation).

> **API-boundary shorthand:** the modern idiom for "saturate-and-round into
> this type" is to put `clamp | round_nearest` on the target inside's policy
> and write `T{value}`. This replaces the explicit
> `clamp_round<T>(value)` cast for typed boundaries — see
> [conversions.md](conversions.md#api-boundary-clamp--round_nearest).

## Per-operation override

Without a type-level policy, you can clamp, wrap, or pick a rounding mode on
a per-operation basis:

```cpp
inside<{0, 100}> x{50};
x.with_clamp() = 150;  // x == 100
x.with_wrap()  = 103;  // x == 2

inside<{{0, 10}, 2}> g{0};
g.with_snap<round_floor>()           = 3.0;  // g == 2
g.with_snap<round_ceil>()            = 3.0;  // g == 4
g.with_snap<round_half_even>() = 5.0;  // g == 4 (tie → even)
```

## Named policies

The free arithmetic functions take a policy object as an optional argument.
Each named policy is spelled after the flag it carries:

| Name | Flag |
|---|---|
| `snapped` | `snap` (truncate toward zero) |
| `rounded_nearest` / `rounded_floor` / `rounded_ceil` / `rounded_half_even` | `round_nearest` / `round_floor` / `round_ceil` / `round_half_even` |
| `clamped` / `wrapped` | `clamp` / `wrap` |

```cpp
auto q = div(a, b, rounded_floor);   // == div(a, b, make_policy<round_floor>())
```

## Callbacks: `on_wrap` / `on_clamp` / `on_overflow` / `on_error`

Each policy event can fire a zero-overhead callback. Unused handlers are
eliminated entirely by the compiler (`if constexpr` + `[[no_unique_address]]`).
Each handler receives the inside by mutable reference (so it can override the
stored value) plus an event-specific payload.

| Method | Path | Fires when | Callback signature |
|---|---|---|---|
| `on_clamp(λ)`    | assignment | a narrowed value leaves the grid and `clamp` saturates it | `λ(inside&, overshoot)` |
| `on_wrap(λ)`     | assignment | a narrowed value leaves the grid and `wrap` folds it (carry) | `λ(inside&, carry)` — the carry is an inside |
| `on_error(λ)`    | assignment | an out-of-range (`overflow`) or off-notch (`rounding_error`) value under `checked` (replaces the throw) | `λ(inside&, errc, const char* msg)` |
| `on_overflow(λ)` | binary arithmetic | a fractional or imax result overflows, or `div`/`mod` divides by zero | `λ(inside&, errc)` |

The first three fire on the **assignment** path — narrowing a value *into* a
inside: a direct `=`, the `.on_*()= …` / `with(…) = …` proxies, and the
compound `+= / -= / *= / /=`. `on_overflow` fires on the **binary-arithmetic**
path — the free `add` / `sub` / `mul` / `div` / `mod` and the `+ - * /`
operators, whose widened result can overflow (or, for `div`/`mod`, hit a zero
divisor).

`on_*()` returns a temporary handle whose `=`/`+=`/`-=`/etc. apply the
operation with the callback wired in. Calling `on_*` automatically OR-merges
the policy bit it implies (e.g. `on_clamp` adds `clamp`). A pack may carry
handlers for *both* paths — e.g. `with(on_overflow(…), on_clamp(…))` on a
compound `+=`, whose widened arithmetic can overflow *and* whose narrowing back
into the target can clamp —
and each handler fires only on its own path; handlers that a given operation
never reaches are accepted but simply not invoked.

```cpp
using sec = inside<{0, 59}, wrap>;
using min = inside<{0, 59}>;

sec seconds{0};
min minutes{0};

// When seconds overflows, carry into minutes.
seconds.on_wrap([&](auto& self, auto carry) {
    (void)self;
    minutes += carry;
}) = 125;
// seconds == 5, minutes == 2
```

The carry is always an inside, so it adds straight into another inside. Its
grid holds every carry the source can produce: an inside source's own range
(`125_ins` gives `[2, 2]`), an integer source's type limits (`int` into
`{0, 59}` gives about ±35.8 million), and the whole `imax` range for a `double`
or exact-fraction source. The `+=` then narrows through `minutes`' own policy.
A callback that takes `imax` instead of `auto` still works through the
implicit `operator imax()`.

The free arithmetic functions accept the same factories — useful for catching
divide-by-zero or arithmetic overflow without throwing:

```cpp
auto q = div(d, z, on_overflow([&](auto& res, errc c) {
    res = std::remove_cvref_t<decltype(res)>{0};
    log(c);
}));
```

### `policy_ref` compound assignment

`x.on_wrap(...) += rhs` (and `-=`, `*=`, `/=`, `%=`) accept another inside or a
`rational`. A raw `int` / `double` is ill-formed, as for plain compound
assignment: give it a grid with `_ins`, `just<…>`, or an inside over its range.

```cpp
using pos = inside<{{0, 64}, per<16>}, wrap | round_nearest>;
pos p{0};
int wrap_count = 0;

p.on_wrap([&](auto&, auto) { ++wrap_count; }) += 65.5_ins;
// p == 1.4375 (65.5 − 1025/16: the wrap period is span + notch), wrap_count == 1
```

See [examples/torus_map.cpp](../examples/torus_map.cpp) for a full sprite
position demo using this pattern on both axes, with a runtime delta typed as
a ranged inside.

### Combining actions: `with(...)`

`inside::with(actions...)` packs multiple `on_*` callbacks into a single
operation. Mutually exclusive combinations (e.g. `on_clamp` + `on_wrap`) are
rejected at compile time by `static_assert`.

```cpp
using c100 = inside<{0, 100}>;
c100 acc{50};
inside<{0, 1000}> big_value{900};

acc.with(
    on_overflow([&](auto& self, errc) { self = 0; /* arithmetic overflowed */ }),
    on_clamp   ([&](auto&, auto over)  { log_overshoot(over);              })
) += big_value;                         // acc == 100, overshoot 850
```

## Error code mode

Instead of throwing, errors can be reported via `beman::inside::errc` (the library has no
`std::error_code` / `<system_error>` dependency). Construction that can fail is
`try_make` (see [Non-throwing construction](#non-throwing-construction)); assignment and
free arithmetic take an `errc&`:

```cpp
beman::inside::errc ec{};   // value-initialised: errc{} == 0 means "no error"

// Per-operation with error code
inside<{0, 100}> y{50};
y.policy(ec) = 200;
// ec is set to errc::overflow, y remains 50 (a failed assignment leaves the prior value intact)

// Free arithmetic with error code
auto sum = add(y, y, ec);
// overflow / range errors captured in ec

// Combining flags with error code
inside<{{0, 10}, 2}> coarse{0};
inside<{{0, 10}, 1}> fine{3};
coarse.policy<snap>(ec) = fine;
// snap suppresses the rounding error, ec captures range errors only
```

The error code is only set on the first error (subsequent errors don't
overwrite it). A value outside the interval produces `errc::overflow`, an
off-notch value `errc::rounding_error`.

**One condition, one code.** `errc::overflow` always means "the value does not
fit its destination's range" — an inside's interval (assignment, construction,
`try_make`, casts, a math result past `Out`), a native type (`to<T>()`), or a
rational's 64-bit fields. `errc::domain_error` means "the argument is outside the
function's mathematical domain": `sqrt` of a negative, `pow` with a base ≤ 0, a
negative value into an unsigned `to<T>()`, invalid `grid::try_make` parameters.

## Replacing the throw handler (freestanding / bare-metal)

Every checked failure funnels through one replaceable handler, which by default
throws `beman::inside::inside_error` (it traps under `-fno-exceptions`).
`set_error_handler` installs your own; see
[freestanding.md](freestanding.md#1-exceptions-off--install-a-handler).

## Non-throwing construction

`try_make` returns `std::expected<inside, errc>` instead of throwing:

```cpp
auto maybe = inside<{0, 100}>::try_make(150);
if (!maybe) { /* maybe.error() == errc::overflow */ }
```

For types with a `clamp` or `wrap` policy, `try_make` applies the policy
before checking, so it will always succeed:

```cpp
auto clamped = inside<{0, 100}, clamp>::try_make(150);
// clamped has value 100 — clamp always succeeds
```
