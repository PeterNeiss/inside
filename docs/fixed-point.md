# inside for fixed-point users

If you already think in Qm.n, `inside` will feel familiar: it *is* fixed-point,
with the scale and range lifted into the type so the compiler tracks them for
you. This page maps your mental model onto `inside` and tells you which grids run
at native speed.

## The bridge: a grid is a fixed-point format

An `inside`'s type is a **grid** `{interval, notch}`:

- **notch** = the resolution (the value of 1 LSB). `per<2^N>` is a fraction
  `1/2^N` — i.e. **N fractional bits**.
- **interval** = `[Lower, Upper]`, the representable range.
- A value is `Lower + index · notch`; with the `direct` policy the raw storage
  *is* the value, exactly like a plain fixed-point register.

So a Q8.8 unsigned register — 8 integer bits, 8 fractional bits, range `[0, 255]`
step `1/256` — is:

```cpp
using q8_8 = inside<{{0, 255}, per<256>}, round_nearest>;  // == beman::inside::q8_8
```

`#include <beman/inside/formats.hpp>` for the curated aliases: `q4_4`, `q8_8`, `q16_16`
(uint8/16/32), `byte`…`sqword`, and `unorm8/16/32` (`[0,1]` at N-bit resolution).

Two grid shapes take the integer fast paths: **integer-aligned** (notch and
Lower are whole numbers) and **Q-format** (notch `1/N`, `N ≥ 2`, Lower 0).

## What `inside` adds over a raw integer Qm.n

- **The scale lives in the type.** `q8_8{1.5}` stores `384`; you never hand-track
  the radix point or shift amounts.
- **Result grids widen automatically.** `a * b` produces a *new* grid whose
  interval and notch are computed at compile time — no manual headroom analysis
  to avoid overflow. `Q8.8 × Q8.8 → Q16.16`-shaped, exactly. Scaling by a
  constant point (`x * just<c>`) keeps the lattice (notch `N·|c|`) and the
  integer storage.
- **Narrowing is a policy, not a hope.** Storing back onto a coarser grid runs
  `snap` (truncate), `round_nearest`, `clamp`, `wrap`, or `checked` — you
  choose per type or per operation.
- **Out-of-range is explicit.** `clamp`/`wrap` saturate/fold; `checked`
  reports (`std::expected` / `beman::inside::errc`) instead of silently
  wrapping.
- **Exactness on tap.** Need no rounding at all? The `exact` policy stores a
  rational and never loses a bit (slower — see below).

## Storage representations and their cost

| Representation | Raw holds | Cost | Use for |
|---|---|---|---|
| `direct` | the value, as a plain integer (Notch 1) | cheapest — one int | integer ranges, interop (`raw()` == wire value) |
| deduced / `indexed` | 0-based notch index | one int (+ a shift/offset to read the value) | Q-format, dense serialization |
| `f64` | the value as IEEE-754 `double` | one double; FPU | math operands (sin/cos/…) |
| `f32` | the value as IEEE-754 `float` | one float; single-precision FPU is enough | values on float-only FPUs (Cortex-M4F) |
| `exact` | exact fraction (`rational`) | **gcd/lcm per op** | when rounding is unacceptable |

Storage is deduced from the grid unless a representation flag overrides it (see
[storage.md](storage.md#choosing-the-representation)). Rule of thumb: integer-raw
(direct/indexed) and `f64`/`f32` are cheap; **rational is the slow one** — avoid it
in hot loops.

## Which grids are fast

1. **Integer-aligned grids (notch 1)** — `inside<{0,N}>`, `inside<{a,b}>`. `+ − × ÷`
   run on the raw integer at native parity (multiplication multiplies the value
   indices, one `imul`; division with `snap` is a native `a/b`). Byte-
   wide unchecked loops vectorize at native lane count.
2. **Q-format grids (notch `1/N`, Lower 0)** — a power-of-two `N` makes scaling a
   shift. Division of two same-notch Q-format operands takes the fast path
   `(a · N) / b` (`(a << log2 N) / b` for power-of-two `N`) (`qformat_codec_fits` / `q_format_encode` in
   `generic.hpp`; the Q-format divide in `detail/division.hpp`). Construction is
   ~native (Q8.8 / Q16.16 measure at ~0.97×).
3. **Avoid in hot loops:** continuous (Notch 0) grids and `exact` use rational
   storage (gcd/lcm every op). For bulk reductions use
   `beman::inside::sum<Target>`, which checks the total once and keeps
   vectorization (`add_all` / `mul_all` are plain pairwise folds).

## Performance

The generated per-operation tables — Q8.8/Q16.16 construct/add/mul/div,
accumulation, and the math engines, each with a native baseline and hardware
counters — are in [performance.md](performance.md) (`beman.inside.perf_report` target).
The short version: unchecked Q-format arithmetic sits at native parity;
`checked` on a tight loop pays a compare-and-branch per element (9 vs 4
instructions per element on the reference machine), which also stands in the way
of autovectorisation — use `unsafe` inside proven-safe inner loops, or
`beman::inside::sum` for a single deferred check.

## Choosing your grid

- **Hot integer/fixed-point math:** integer-aligned or Q-format grids; `unsafe`
  in the proven-safe inner loop, then assign the result into a checked type.
- **Transcendentals:** any grid; integer and `f64` outputs are equally fast
  ([math.md](math.md#storage)).
- **No rounding allowed:** `exact` — accept the rational cost.
- **SIMD byte/halfword loops:** keep the range within the native type (the
  `formats.hpp` aliases use the full range, e.g. `byte` is `[0, 255]`) so the raw
  stays at native width.

## Where to go next

| You want to… | Read |
|---|---|
| the full storage / deduction rules | [storage.md](storage.md) |
| result-grid rules for `+ − × ÷` | [arithmetic.md](arithmetic.md) |
| out-of-range policies & errors | [policies.md](policies.md) |
| reproducibility guarantees | [determinism.md](determinism.md) |
| call sin/cos/sqrt/… | [math.md](math.md) |
