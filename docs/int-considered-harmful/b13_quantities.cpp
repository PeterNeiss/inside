// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The unit layer: quantities whose number is an inside. A conversion
// rescales the grid and keeps the raw; a temperature point changes origin
// exactly; quantity kinds keep apart what one dimension cannot; a ratio of
// two lengths is a plain number.

#include <iostream>

#include <beman/inside/inside/io.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

int main()
{
  // A conversion multiplies the grid, not the number: same raw, exact.
  quantity<si::kilo(si::metre), inside<{{0, 500}, per<1000>}>> trip{12.5};
  auto metres = trip.in(si::metre);
  std::cout << "12.5 km in m       = " << metres << "   (raw "
            << trip.numerical_value().raw() << " -> " << metres.numerical_value().raw() << ")\n";

  // A temperature is a point; its origin moves by an exact offset.
  auto room = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
  std::cout << "20 C in K          = " << room.point_for(si::absolute_zero).in(si::kelvin) << "\n";

  // One dimension, two kinds: hertz and becquerel are both 1/s.
  quantity<si::hertz, inside<{0, 1000}>>     f{50};
  quantity<si::becquerel, inside<{0, 1000}>> a{50};
  static_assert(!addable<decltype(f), decltype(a)>);
  std::cout << "50 Hz + 50 Bq      : does not compile\n";

  // A width times a height is an area.
  quantity<isq::width[si::metre], inside<{0, 10}>>  w{3};
  quantity<isq::height[si::metre], inside<{0, 10}>> h{4};
  quantity<isq::area[square(si::metre)], inside<{0, 100}>> floor_area = w * h;
  std::cout << "3 m x 4 m          = " << floor_area << "\n";

  // A ratio of two lengths is dimensionless: it converts to a plain inside.
  quantity<si::kilo(si::metre), inside<{1, 10}>> run{3};
  quantity<si::metre, inside<{1, 10}>>           step{2};
  inside<{0, 10000}, round_nearest> ratio = run / step;
  std::cout << "3 km / 2 m         = " << ratio << "\n";
  return 0;
}
