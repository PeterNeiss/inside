# Resources: prior art and talks

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`inside` did not appear in a vacuum. The idea that a number's range belongs in its
*type*, that arithmetic should *widen* that range automatically, and that
fixed-point and overflow safety deserve first-class library support has been
explored by several excellent projects. This page collects the prior art that
shaped `inside`'s thinking, plus talks worth watching if these ideas are new to
you.

## Feature matrix

How `inside` compares to the projects below. This is meant to be *fair*, not
flattering — see the caveat after the tables. Legend: **●** full · **◐**
partial / related · **○** no · **—** not applicable.

**Numeric model & capabilities**

| Library | Domain | Range in type `[lo,hi]` | Auto-widening result | Out-of-range handling | Fixed-point | Rational / exact | Transcendental math | Determinism / FPU-free | Freestanding / `-fno-exceptions` | Physical units |
|---|---|---|---|---|---|---|---|---|---|---|
| **inside** | bounded rational grids | ● | ● | clamp / wrap / round / snap / throw / `errc` / `std::expected` | ● | ● | ● (correctly rounded on any grid where decided) | ● | ◐ ¹⁰ | ● ²¹ |
| **bounded::integer** | integers | ● | ● | policy on narrowing (clamp / modulo / throw / assume) | ○ | ○ | ○ | — | ○ ¹¹ | ○ |
| **Intel safe-arithmetic** | integers | ● ¹⁹ | ● | compile-time proof (reject) / runtime `safe::function` | ○ | ○ | ○ | — | ○ ²⁰ | ○ |
| **Boost.SafeNumerics** | integers | ◐ ¹ | ◐ ² | detect → exception (custom exception / trap policy) | ○ | ○ | ○ | — | ◐ ¹² | ○ |
| **CNL** | fixed-point + elastic ints | ◐ ³ | ● | composable: native / saturated / throwing / undefined | ● | ◐ ⁴ | ○ | ● | ◐ ¹³ | ○ |
| **fpm** | fixed-point | ○ | ○ | ○ (wraps, no checks) | ● | ○ | ● (full `<cmath>`) | ● | ◐ ¹⁴ | ○ |
| **type_safe** | strong typedefs / vocab types | ◐ ⁵ | ○ | ◐ (arith. policy `ub`/`checked`; clamped/constrained) | ○ | ○ | ○ | — | ◐ ¹⁵ | ○ |
| **SafeInt** | integers | ○ | ○ | detect → throw / custom handler | ○ | ○ | ○ | — | ◐ ¹⁶ | ○ |
| **google/integers** | integers | ◐ ⁶ | ○ | trapping / wrapping / clamping ⁶ | ○ | ○ | ○ | — | ◐ ¹⁷ | ○ |
| **PSsst** | strong-typedef framework | ○ | ○ | ○ | ○ | ○ | ◐ ⁷ | — | ○ ¹⁸ | ○ |

**Practical**

| Library | Header-only | Single header | Min C++ | License | Maturity / status |
|---|---|---|---|---|---|
| **inside** | ● | ● ⁹ | C++23 | Apache-2.0 WITH LLVM-exception | **under development (v0.4.0) · single-author · not yet battle-tested** |
| **bounded::integer** | ○ (C++ modules) ⁸ | ○ | C++20+ (clang 22+) | BSL-1.0 | mature · active |
| **Intel safe-arithmetic** | ● | ○ | C++20 | BSL-1.0 | **pre-release · WIP (not for production)** |
| **Boost.SafeNumerics** | ● | ○ | C++14 | BSL-1.0 | mature (Boost) |
| **CNL** | ● | ○ | C++20 (v1.x: C++11) | BSL-1.0 | mature · standards-track (P0828) |
| **fpm** | ● | ○ | C++11 | MIT | mature · stable |
| **type_safe** | ● | ○ | C++11 | MIT | mature · maintenance mode |
| **SafeInt** | ● | ● | C++11 | MIT | mature · battle-tested (since 2003) |
| **google/integers** | ● | ○ | C++17 | Apache-2.0 | **archived (Apr 2026) · partial** ⁶ |
| **PSsst** | ● | ● | C++17 | BSL-1.0 | active |

Notes:

1. via `safe_signed_range<MIN,MAX>` / `safe_unsigned_range`.
2. through the promotion policy.
3. `elastic_integer` tracks digit/bit-width, not arbitrary `[lo,hi]`.
4. dyadic scaling, not arbitrary rationals.
5. `bounded_type` / `constrained_type` / `clamped_type` enforce intervals but don't propagate ranges through arithmetic.
6. `ranged<T>` constrains a range; repo archived April 2026 with only `trapping<T>` fully implemented.
7. `Trigonometric` / `ExpLog` / `Root` mix-ins delegate to `<cmath>` on the wrapped type.
8. implemented as C++20 modules, so not header-only in the classic sense.
9. a *generated* amalgamation of the multi-file source (`single_include/beman/inside/inside.hpp`, standard-library-only), rebuilt by a CMake target — SafeInt (`SafeInt.hpp`) and PSsst (`pssst.h`) are instead authored as a single file; the rest ship a multi-header tree (fpm, e.g., splits `fixed.hpp` / `math.hpp` / `ios.hpp`).
10. core compiles `-ffreestanding -fno-exceptions` (no `<system_error>`; a replaceable `error_handler`; strings/printing in the opt-in `beman/inside/io.hpp`, droppable via `BEMAN_INSIDE_NO_STRING`). `BEMAN_INSIDE_MATH_NO_FP` (auto under `-ffreestanding`) compiles the `<cmath>` dependency out of `beman::inside::math` entirely; full `-ffreestanding` still depends on the stdlib's freestanding maturity — see [freestanding.md](freestanding.md). The units (`units.hpp`) need a hosted library.
11. C++20 modules (clang-only) with throwing policies; no documented freestanding / no-exceptions path.
12. `-fno-exceptions` via the compile-time `trap` exception policy; the default policy throws.
13. non-throwing overflow tags (`native` / `saturated` / `undefined`) and no `<cmath>`, but no explicit `-ffreestanding` guarantee.
14. no overflow checks → no exception path, but the math header pulls full `<cmath>` (not FPU-free).
15. the `DEBUG_ASSERT` handler is user-overridable (no exceptions); no documented `-ffreestanding`.
16. a custom `SafeIntException` / overflow handler avoids exceptions; integer-only (no `<cmath>`).
17. `trapping<T>` traps without exceptions, but the repo is archived / partial.
18. strong-typedef only — no overflow checks, so nothing to throw, but no numeric-safety story either.
19. also models disjoint interval *unions* (e.g. `ival<-1000,-1> || ival<1,1000>` to exclude zero), not just a single `[lo,hi]`; arithmetic propagates ranges via the Cartesian product of the operand intervals.
20. no documented freestanding / `-fno-exceptions` path; integer-only (no `<cmath>`).
21. `<beman/inside/units.hpp>`, an opt-in header: quantities, kinds, points and units of your own on an inside, in the style of mp-units ([units.md](units.md)). The libraries above have no units; the dedicated units libraries are listed [below](#units-libraries).

Freestanding marks are best-effort from each project's docs as of June 2026 — corrections welcome.

A filled-in cell is **not** a verdict. The mature, widely deployed options here
(Boost.SafeNumerics, SafeInt, CNL, fpm) have years of production hardening, broad
compiler support, and wide use; `inside` is at its fourth release (0.4.0, October 2026),
under development, single-author, and not yet battle-tested. What `inside` brings that the others
don't combine is *rational* grids with arbitrary `[lower, upper]` bounds **and**
reproducible transcendental math over those grids — not dominance of every
column. Pick the tool that fits: if you need overflow-safe plain integers,
`bounded::integer` or Boost.SafeNumerics are proven; for pure fixed-point DSP,
`fpm` or CNL; `inside` is for when the *domain* of a value is part of its type.

## Implementation patterns considered and not adopted

- **CRTP operator mix-ins** (PSsst, Boost's operator generation) — `inside`'s
  operators dispatch on storage shape with `if constexpr`, which a generic
  mix-in base can't see.
- **Selectable promotion policies** (Boost.SafeNumerics `native`/`automatic`) —
  a "native" mode would bring back the silent narrowing the library prevents.
- **Constraint/verifier splitting** (type_safe) — the policy bitmask plus the
  `on_*` actions already separate what is enforced from what happens.
- **Wrapper stacking** (CNL's `overflow<rounding<scaled<int>>>`) — one flat
  policy set keeps flag interactions in one place.
- **Disjoint interval unions** (Intel safe-arithmetic) — a
  [roadmap sketch](roadmap.md).

## Related libraries

- **[bounded::integer](https://github.com/davidstone/bounded-integer)** (David
  Stone) — the closest prior art. Integer types carry a compile-time
  `[min, max]` range, and arithmetic widens the result range so that code
  without explicit casts is guaranteed not to overflow. `inside` generalises the
  same widening idea from integers to rational *grids* (lower, upper, notch).
  See also the author's writeup at <http://doublewise.net/c++/bounded/>.
- **[Intel safe-arithmetic](https://github.com/intel/safe-arithmetic)** (Intel —
  *not* an official Intel product) — a C++20 library that encodes integer ranges
  into the type as `ival_s32<MIN, MAX>` and propagates them through arithmetic,
  with the twist that it also models *disjoint* interval unions (e.g. excluding
  zero so division is provably safe). Where `bounded::integer` widens, `safe`
  goes further: it demands a compile-time *proof* that each operation stays in
  range, falling back to runtime `safe::function` validation only where the proof
  can't be discharged statically. The same range-in-the-type spirit as `inside`,
  scoped to integers; still pre-release.
- **[Boost.SafeNumerics](https://www.boost.org/libs/safe_numerics/)** (Robert
  Ramey) — drop-in replacements for the built-in integer types that detect, and
  by policy raise on, results that would be incorrect. Shares `inside`'s "make
  the unsafe operation impossible rather than merely discouraged" stance, with a
  configurable exception/error policy.
  [[github](https://github.com/boostorg/safe_numerics)]
- **[CNL — Compositional Numeric Library](https://github.com/johnmcfarlane/cnl)**
  (John McFarlane) — composable numeric components (fixed-point, elastic,
  overflow, rounding) that slot together to build safer, cheaper arithmetic
  types. CNL underpins the C++ fixed-point standardisation effort.
- **[fpm](https://github.com/MikeLankamp/fpm)** (Mike Lankamp) — header-only
  fixed-point math with a `<cmath>`-shaped API (trig, pow, log, …) implemented in
  pure integer arithmetic. A direct point of comparison for `inside`'s math
  engine, whose integer path is FPU-free and rounds correctly where it decides — see
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
  cousin of `inside`'s range-in-the-type model, focused on integers.
- **[PSsst — Peter Sommerlad's Simple Strong Typing](https://github.com/PeterSommerlad/PSsst)**
  (Peter Sommerlad) — a tiny `strong<T, Tag>` base for building safe numeric
  wrappers with only the operators you opt into. Kindred to `inside`'s view that
  a bounded quantity is a *type*, not a runtime check.

## Units libraries

`units.hpp` follows the design of mp-units, without depending on it; the
other libraries here shaped the field. What sets `units.hpp` apart is the
number: always an inside, so a conversion is an exact rescale of its grid and
a mixed-unit sum cannot overflow. The others take any arithmetic type as the
number, and are far more mature.

- **[mp-units](https://github.com/mpusz/mp-units)** (Mateusz Pusz) — the
  modern C++20 units library behind the WG21 proposal
  [P3045](https://wg21.link/p3045) "Quantities and units library": dimensions,
  units, the ISQ quantity kinds and quantity points. `units.hpp` copies its
  model (kinds, the ISQ tree, points and origins); the differences are listed
  in [units.md](units.md#differences-from-mp-units).
  [[documentation](https://mpusz.github.io/mp-units/)]
- **[Au](https://github.com/aurora-opensource/au)** (Aurora Innovation) — a
  C++14 units library with a strong focus on safe conversions of integer
  representations, and a single-header distribution.
- **[Boost.Units](https://www.boost.org/libs/units/)** (Matthias Schabel,
  Steven Watanabe) — the long-standing Boost library for dimensional analysis,
  with zero runtime overhead.
- **[nholthaus/units](https://github.com/nholthaus/units)** (Nic Holthaus) — a
  header-only, compile-time units library for C++14.

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
  directly parallels `inside`. (Overview discussion on
  [CppCast ep. 22](https://isocpp.org/blog/2015/08/cppcast-episode-22-bounded-integers-with-david-stone).)
- **["Cross-Platform Floating-Point Determinism Out of the Box"](https://www.youtube.com/watch?v=7MatbTHGG6Q)**
  — Sherry Ignatchenko, CppCon 2024. Why cross-platform floating-point
  reproducibility is hard and how to get it — directly relevant to `inside`'s
  bit-exact guarantees in [docs/determinism.md](determinism.md).
- **["Simplest Strong Typing instead of Language Proposal (P0109)"](https://www.youtube.com/watch?v=ABkxMSbejZI)**
  — Peter Sommerlad, C++Now 2021. The design behind PSsst and the case for
  strong numeric types as a library, not a language feature.
- **["int != safe && int != ℤ"](https://www.youtube.com/watch?v=YyNE6Y2mv1o)**
  — Peter Sommerlad, Meeting C++ 2025. Why the built-in `int` is neither safe
  nor a mathematical integer — the exact gap `inside` sets out to close.

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
