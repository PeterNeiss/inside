# Resources — prior art & talks

`bound` did not appear in a vacuum. The idea that a number's range belongs in its
*type*, that arithmetic should *widen* that range automatically, and that
fixed-point and overflow safety deserve first-class library support has been
explored by several excellent projects. This page collects the prior art that
shaped `bound`'s thinking, plus talks worth watching if these ideas are new to
you.

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
