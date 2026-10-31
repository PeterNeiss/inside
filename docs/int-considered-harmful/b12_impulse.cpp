// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Answer to hazard 9, with dimensions: the Mars Climate Orbiter mismatch.
// One side reports a thruster impulse in pound-force seconds, the other
// expects newton seconds. With units.hpp the unit is part of the type, so
// the sum converts exactly, and an impulse cannot be added to a length.

#include <iostream>

#include <beman/inside/inside/io.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

constexpr auto lbf_s = international::pound_force * si::second;
constexpr auto n_s   = si::newton * si::second;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

int main()
{
  quantity<lbf_s, inside<{0, 100}>> reported{10};   // ground software: lbf·s
  quantity<n_s, inside<{0, 1000}>>  budget{100};    // navigation: N·s

  // The sum runs in a common unit; both conversions are exact rescales.
  auto total = budget + reported;
  std::cout << "100 N·s + 10 lbf·s = " << total.in(n_s) << "\n";

  // An impulse and a length do not add: the expression is ill-formed.
  quantity<si::metre, inside<{0, 1000}>> d{5};
  static_assert(!addable<decltype(budget), decltype(d)>);
  std::cout << "N·s + m            : does not compile\n";
  return 0;
}
