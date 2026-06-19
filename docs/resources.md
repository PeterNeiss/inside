# Resources — prior art & talks

`bound` did not appear in a vacuum. The idea that a number's range belongs in its
*type*, that arithmetic should *widen* that range automatically, and that
fixed-point and overflow safety deserve first-class library support has been
explored by several excellent projects. This page collects the prior art that
shaped `bound`'s thinking, plus talks worth watching if these ideas are new to
you. And yes — we're aware that adding one more safe-numerics library to the
pile is its own kind of joke ([xkcd #927, "Standards"](https://xkcd.com/927/)).

## Feature matrix

How `bound` compares to the projects below. This is meant to be *fair*, not
flattering — see the caveat after the tables. Legend: **●** full · **◐**
partial / related · **○** no · **—** not applicable.

**Numeric model & capabilities**

| Library | Domain | Range in type `[lo,hi]` | Auto-widening result | Out-of-range handling | Fixed-point | Rational / exact | Transcendental math | Determinism / FPU-free |
|---|---|---|---|---|---|---|---|---|
| **bound** | bounded rational grids | ● | ● | clamp / wrap / sentinel / round / snap / throw / `error_code` | ● | ● | ● (two engines) | ● |
| **bounded::integer** | integers | ● | ● | policy on narrowing (clamp / modulo / throw / assume) | ○ | ○ | ○ | — |
| **Boost.SafeNumerics** | integers | ◐ ¹ | ◐ ² | detect → exception (custom exception / trap policy) | ○ | ○ | ○ | — |
| **CNL** | fixed-point + elastic ints | ◐ ³ | ● | composable: native / saturated / throwing / undefined | ● | ◐ ⁴ | ○ | ● |
| **fpm** | fixed-point | ○ | ○ | ○ (wraps, no checks) | ● | ○ | ● (full `<cmath>`) | ● |
| **type_safe** | strong typedefs / vocab types | ◐ ⁵ | ○ | ◐ (arith. policy `ub`/`checked`; clamped/constrained) | ○ | ○ | ○ | — |
| **SafeInt** | integers | ○ | ○ | detect → throw / custom handler | ○ | ○ | ○ | — |
| **google/integers** | integers | ◐ ⁶ | ○ | trapping / wrapping / clamping ⁶ | ○ | ○ | ○ | — |
| **PSsst** | strong-typedef framework | ○ | ○ | ○ | ○ | ○ | ◐ ⁷ | — |

**Practical**

| Library | Header-only | Min C++ | License | Maturity / status |
|---|---|---|---|---|
| **bound** | ● | C++23 (C++20 backport) | **none yet (TBD)** | **alpha · single-author · not yet battle-tested** |
| **bounded::integer** | ○ (C++ modules) ⁸ | C++20+ (clang 22+) | BSL-1.0 | mature · active |
| **Boost.SafeNumerics** | ● | C++14 | BSL-1.0 | mature (Boost) |
| **CNL** | ● | C++20 (v1.x: C++11) | BSL-1.0 | mature · standards-track (P0828) |
| **fpm** | ● | C++11 | MIT | mature · stable |
| **type_safe** | ● | C++11 | MIT | mature · maintenance mode |
| **SafeInt** | ● | C++11 | MIT | mature · battle-tested (since 2003) |
| **google/integers** | ● | C++17 | Apache-2.0 | **archived (Apr 2026) · partial** ⁶ |
| **PSsst** | ● | C++17 | BSL-1.0 | active |

<sub>¹ via `safe_signed_range<MIN,MAX>` / `safe_unsigned_range`. ² through the
promotion policy. ³ `elastic_integer` tracks digit/bit-width, not arbitrary
`[lo,hi]`. ⁴ dyadic scaling, not arbitrary rationals. ⁵ `bounded_type` /
`constrained_type` / `clamped_type` enforce intervals but don't propagate ranges
through arithmetic. ⁶ `ranged<T>` constrains a range; repo archived April 2026
with only `trapping<T>` fully implemented. ⁷ `Trigonometric` / `ExpLog` / `Root`
mix-ins delegate to `<cmath>` on the wrapped type. ⁸ implemented as C++20
modules, so not header-only in the classic sense. Compiled from each project's
README/docs as of June 2026 — corrections welcome.</sub>

A filled-in cell is **not** a verdict. The mature, widely deployed options here
(Boost.SafeNumerics, SafeInt, CNL, fpm) have years of production hardening, broad
compiler support, and real licenses; `bound` is alpha, currently unlicensed,
single-author, and not yet battle-tested. What `bound` brings that the others
don't combine is *rational* grids with arbitrary `[lower, upper]` bounds **and**
reproducible transcendental math over those grids — not dominance of every
column. Pick the tool that fits: if you need overflow-safe plain integers,
`bounded::integer` or Boost.SafeNumerics are proven; for pure fixed-point DSP,
`fpm` or CNL; `bound` is for when the *domain* of a value is part of its type.

## Related libraries

- **[bounded::integer](https://github.com/davidstone/bounded-integer)** (David
  Stone) — the closest prior art. Integer types carry a compile-time
  `[min, max]` range, and arithmetic widens the result range so that code
  without explicit casts is guaranteed not to overflow. `bound` generalises the
  same widening idea from integers to rational *grids* (lower, upper, notch).
  See also the author's writeup at <http://doublewise.net/c++/bounded/>.
- **[Boost.SafeNumerics](https://www.boost.org/libs/safe_numerics/)** (Robert
  Ramey) — drop-in replacements for the built-in integer types that detect, and
  by policy raise on, results that would be incorrect. Shares `bound`'s "make
  the unsafe operation impossible rather than merely discouraged" stance, with a
  configurable exception/error policy.
  [[github](https://github.com/boostorg/safe_numerics)]
- **[CNL — Compositional Numeric Library](https://github.com/johnmcfarlane/cnl)**
  (John McFarlane) — composable numeric components (fixed-point, elastic,
  overflow, rounding) that slot together to build safer, cheaper arithmetic
  types. CNL underpins the C++ fixed-point standardisation effort.
- **[fpm](https://github.com/MikeLankamp/fpm)** (Mike Lankamp) — header-only
  fixed-point math with a `<cmath>`-shaped API (trig, pow, log, …) implemented in
  pure integer arithmetic. A direct point of comparison for `bound`'s
  integer/CORDIC math engine (FPU-free, deterministic) — see
  [docs/math.md](math.md).
- **[type_safe](https://github.com/foonathan/type_safe)** (Jonathan Müller) —
  zero-overhead vocabulary types that use the type system to prevent bugs,
  including constrained and bounded integer wrappers.
- **[SafeInt](https://github.com/dcleblanc/SafeInt)** (David LeBlanc /
  Microsoft) — a long-lived, battle-tested integer wrapper that checks every
  cast and arithmetic operation for overflow.
- **[google/integers](https://github.com/google/integers)** (Fuchsia Security
  Team — *not* an official Google product) — safer integer types for C++,
  including a `ranged<T>` that constrains an integer to a value range. A close
  cousin of `bound`'s range-in-the-type model, focused on integers.
- **[PSsst — Peter Sommerlad's Simple Strong Typing](https://github.com/PeterSommerlad/PSsst)**
  (Peter Sommerlad) — a tiny `strong<T, Tag>` base for building safe numeric
  wrappers with only the operators you opt into. Kindred to `bound`'s view that
  a bounded quantity is a *type*, not a runtime check.

## Talks & videos

- **["Safe Numerics Library"](https://isocpp.org/blog/2017/08/cppcon-2016-safe-numerics-library-robert-ramey)**
  — Robert Ramey, CppCon 2016 (an extended version with a real-world case study
  followed at CppCon 2018). The motivation for treating integer overflow as a
  correctness problem the library should rule out.
- **["CNL: A Compositional Numeric Library"](https://isocpp.org/blog/2018/07/cppcon-2017-cnl-a-compositional-numeric-library-john-mcfarlane)**
  — John McFarlane, CppCon 2017. How small numeric components compose into safe
  fixed-point types — "do for integers what the STL does for pointers."
- **"Removing undefined behavior from integer operations: the bounded::integer
  library"** — David Stone, C++Now 2014. The auto-widening-range model that most
  directly parallels `bound`. (Overview discussion on
  [CppCast ep. 22](https://isocpp.org/blog/2015/08/cppcast-episode-22-bounded-integers-with-david-stone).)
- **["Cross-Platform Floating-Point Determinism Out of the Box"](https://www.youtube.com/watch?v=7MatbTHGG6Q)**
  — Sherry Ignatchenko, CppCon 2024. Why cross-platform floating-point
  reproducibility is hard and how to get it — directly relevant to `bound`'s
  bit-exact guarantees in [docs/determinism.md](determinism.md).
- **["Simplest Strong Typing instead of Language Proposal (P0109)"](https://www.youtube.com/watch?v=ABkxMSbejZI)**
  — Peter Sommerlad, C++Now 2021. The design behind PSsst and the case for
  strong numeric types as a library, not a language feature.
- **["int != safe && int != ℤ"](https://www.youtube.com/watch?v=YyNE6Y2mv1o)**
  — Peter Sommerlad, Meeting C++ 2025. Why the built-in `int` is neither safe
  nor a mathematical integer — the exact gap `bound` sets out to close.

## Blog posts & articles

Longer-form writeups on the libraries above and the ideas behind them.

- **type_safe** — Jonathan Müller's announcement,
  ["Type safe — Zero overhead utilities for more type safety"](https://www.foonathan.net/2016/10/type-safe/),
  and the companion tutorial
  ["Emulating strong/opaque typedefs in C++"](https://www.foonathan.net/2016/10/strong-typedefs/);
  plus Embedded Artistry's hands-on walkthrough,
  ["Improve Type Safety in Your C++ Program With the type_safe Library"](https://embeddedartistry.com/blog/2018/05/24/improve-type-safety-in-your-c-program-with-the-type_safe-library/).
- **Boost.SafeNumerics** — Embedded Artistry,
  ["Enforce Correct Integer Arithmetic using the C++ Safe Numerics Library"](https://embeddedartistry.com/blog/2019/06/28/enforce-correct-integer-arithmetic-using-the-c-safe-numerics-library/).
- **CNL / fixed-point** — Embedded Artistry,
  ["C++11 Fixed Point Arithmetic Library"](https://embeddedartistry.com/blog/2017/08/25/c11-fixed-point-arithmetic-library/),
  covering John McFarlane's `fixed_point` (the precursor to CNL).
- **SafeInt** — Giovanni Dicanio,
  ["Protecting Your C++ Code Against Integer Overflow Made Easy by SafeInt"](https://giodicanio.com/2023/11/13/protecting-your-c-plus-plus-code-against-integer-overflow-made-easy-by-safeint/).
