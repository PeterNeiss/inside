# Bound for fixed-point users

If you already think in Qm.n, `bound` will feel familiar: it *is* fixed-point,
with the scale and range lifted into the type so the compiler tracks them for
you. This page maps your mental model onto `bound` and tells you which grids run
at native speed.

## The bridge: a grid is a fixed-point format

A `bound`'s type is a **grid** `{interval, notch}`:

- **notch** = the resolution (the value of 1 LSB). `notch<1, 2^N>` is a fraction
  `1/2^N` — i.e. **N fractional bits**.
- **interval** = `[Lower, Upper]`, the representable range.
- A value is `Lower + index · notch`; with the `direct` policy the raw storage
  *is* the value, exactly like a plain fixed-point register.

So a Q8.8 unsigned register — 8 integer bits, 8 fractional bits, range `[0, 255]`
step `1/256` — is:

```cpp
using q8_8 = bound<{{0, 255}, notch<1, 256>}, round_nearest>;  // == bnd::q8_8
```

`#include "bound/formats.hpp"` for the curated aliases: `q4_4`, `q8_8`, `q16_16`
(uint8/16/32), `byte`…`sqword`, and `unorm8/16/32` (`[0,1]` at N-bit resolution).

The traits that classify these grids (in `include/bound/generic.hpp`):

```cpp
// notch and Lower are whole numbers (denominator 1)
template <boundable B> inline constexpr bool IsIntegerAligned =
    abs_den(Notch<B>.Denominator) == 1 && abs_den(Lower<B>.Denominator) == 1;

// Qm.N: unit-numerator power-of-two notch (1/2^N), Lower == 0
template <boundable B> inline constexpr bool IsQFormat =
       !rational_raw<B> && Notch<B>.Numerator == 1
    && abs_den(Notch<B>.Denominator) > 1
    && abs_den(Lower<B>.Denominator) == 1 && Lower<B> == 0;
```

## What `bound` adds over a raw integer Qm.n

- **The scale lives in the type.** `q8_8{1.5}` stores `384`; you never hand-track
  the radix point or shift amounts.
- **Result grids widen automatically.** `a * b` produces a *new* grid whose
  interval and notch are computed at compile time — no manual headroom analysis
  to avoid overflow. `Q8.8 × Q8.8 → Q16.16`-shaped, exactly.
- **Narrowing is a policy, not a hope.** Storing back onto a coarser grid runs
  `snap` (truncate), `round_nearest`, `clamp`, `wrap`, or `checked` — you
  choose per type or per operation.
- **Out-of-range is explicit.** `clamp`/`wrap` saturate/fold; `checked`/`sentinel`
  report (`slim::optional` / `slim::expected` / `bnd::errc`) instead of
  silently wrapping.
- **Exactness on tap.** Need no rounding at all? The `exact` policy stores a
  rational and never loses a bit (slower — see below).

## Storage representations and their cost

| Representation | Raw holds | Cost | Use for |
|---|---|---|---|
| `direct` | the value, as a plain integer (Notch 1) | cheapest — one int | integer ranges, interop (`raw()` == wire value) |
| deduced / `indexed` | 0-based notch index | one int (+ a shift/offset to read the value) | Q-format, dense serialization |
| `real` | the value as IEEE-754 `double` | one double; FPU | math operands (sin/cos/…) |
| `exact` | exact fraction (`rational`) | **gcd/lcm per op** | when rounding is unacceptable |

Storage is deduced from the grid unless a representation flag overrides it (see
[storage.md](storage.md#choosing-the-representation)). Rule of thumb: integer-raw
(direct/indexed) and `real` are cheap; **rational is the slow one** — avoid it in
hot loops.

## Which grids are fast

1. **Integer-aligned grids (notch 1)** — `bound<{0,N}>`, `bound<{a,b}>`. `+ − × ÷`
   run on the raw integer at native parity (multiplication takes a four-quadrant
   `umax·umax` integer path; division with `snap` is a native `a/b`). Byte-
   wide unchecked loops vectorize at native lane count.
2. **Q-format grids (notch `1/2^N`, Lower 0)** — power-of-two notch means scaling
   is a shift. Division of two same-notch Q-format operands takes the fast path
   `(a << log2 N) / b` (`HasQFormatFastPath` / `q_format_encode` in
   `generic.hpp`; the Q-format divide in `detail/division.hpp`). Construction is
   ~native (Q8.8 / Q16.16 measure at ~0.97×).
3. **`real` dyadic, `double_exact` grids** — the right choice for transcendental
   math: the raw *is* the `double`, so feeding `bnd::math` is free marshalling.
4. **Avoid in hot loops:** non-power-of-two notches and continuous (Notch 0) grids
   fall to rational storage (gcd/lcm every op). For bulk reductions use
   `bnd::sum` / `mul_all`, which defer the check and keep vectorization.

### The SIMD / sentinel caveat

The smallest-type selection reserves one raw slot for the zero-overhead
`slim::optional<bound>` sentinel. So `bound<{0,255}>` promotes to **uint16** (raw
255 is the sentinel), halving SIMD lanes versus native `uint8_t`. Cap one short —
`bound<{0,254}>` fits **uint8** and runs at exactly native speed. The
`formats.hpp` aliases already do this (`byte` is `[0,254]`); Q-format types have
headroom and keep full range.

## Performance

The generated per-operation tables — Q8.8/Q16.16 construct/add/mul/div,
accumulation, and the math engines, each with a native baseline and hardware
counters — are in [performance.md](performance.md) (`perf_report` target).
The short version: unchecked Q-format arithmetic sits at native parity;
`checked` on a tight loop costs ~4× because the per-element domain check
breaks autovectorisation — use `unsafe` inside proven-safe inner loops, or
`bnd::sum` for a single deferred check.

## Choosing your grid

- **Hot integer/fixed-point math:** integer-aligned or Q-format grids; `unsafe`
  or `snap` in the inner loop, convert back to `checked` after.
- **Transcendentals:** `real` on a dyadic `double_exact` grid (see
  [math.md](math.md)).
- **No rounding allowed:** `exact` — accept the rational cost.
- **SIMD byte/halfword loops:** keep the range one below the type max (use the
  `formats.hpp` aliases) so the raw stays at native width.

## Where to go next

| You want to… | Read |
|---|---|
| the full storage / deduction rules | [storage.md](storage.md) |
| result-grid rules for `+ − × ÷` | [arithmetic.md](arithmetic.md) |
| out-of-range policies & errors | [policies.md](policies.md) |
| reproducibility guarantees | [determinism.md](determinism.md) |
| call sin/cos/sqrt/… | [math.md](math.md) |
