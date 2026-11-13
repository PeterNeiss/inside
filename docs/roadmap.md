# Roadmap

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

What is deliberately *not* implemented, as of November 2026, and what would
unblock each item. Nothing here is a promise of
a date; the [changelog](../CHANGELOG.md) records what has shipped.

## Gated on the C++ standard

### A heap-backed, unbounded grid number

Grids past 64 bits already work by interning limbs in static
storage ([Storage](advanced/storage.md#grids-past-64-bits)). A *heap-backed*
bignum as a template argument stays impossible: a class template argument must
be structural, with public members only ([temp.param]/7), so `std::vector`
cannot qualify; a pointer argument must point to static storage
([temp.arg.nontype]); and non-transient `constexpr` allocation ([P1974], after
C++20's transient [P0784]) has stalled.

## Not gated on the standard

### Units

The gaps found while moving the examples to units. Each one is a library
change, not an example workaround:

- **`atan2` and `asin` straight into degrees or turns,** with one rounding. Today
  the angle rounds onto a fine radian grid, then `value_cast` rounds again.
- **`tan` of a π-scaled angle**, which would need an exact pole at 90°.
- The [differences from mp-units](advanced/units-model.md#differences-from-mp-units): no
  representation other than an inside, no logarithmic units. Vectors are
  [multivectors](advanced/geometric-algebra.md), not vector characters.

### Geometric algebra

- **log and sqrt of rotors**, rounded once (exp is there:
  [`math::exp`](advanced/geometric-algebra.md#exp)), and exp of an argument
  without the closed form (e₁e₂ + e₃e₄ in 4-D, as a product of two).
- **A sandwich product that keeps the grades:** R v R̃ of a vector is a vector,
  but the types keep a grade-3 part that is 0; today `grade<1>` drops it.

### Declined or parked

- **`dyn_inside`** (bounds known only at run time): declined on design grounds,
  since there is no space- and time-efficient implementation; not deferred.
- **Interval unions in the type.** Intel's
  [safe-arithmetic](resources.md) models disjoint unions
  (`ival<-1000,-1> || ival<1,1000>`), so a divisor that excludes zero makes
  division total at compile time. `grid::operator/` already computes the two
  zero-free halves internally, but expressing the union in the type would
  touch every operator and predicate, so it stays a sketch until a use case
  needs it.
- **Modules.** Parked; modules are C++20, not a blocker.

[P0784]: https://wg21.link/p0784
[P1974]: https://wg21.link/p1974
