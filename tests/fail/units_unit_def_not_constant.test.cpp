// EXPECT: a unit is defined by a constant
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A unit_def is defined by a constant quantity, not by a range of them.
#include <inside/units.hpp>

inline constexpr ins::unit_def span_def{"span", ins::quantity<ins::si::metre, ins::inside<{0, 2}>>{1, ins::si::metre}};

int main() {}
