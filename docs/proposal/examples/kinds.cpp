// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: quantity kinds separate quantities of one dimension.
#include <beman/inside/units.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };

int main() {
    // [listing]
    quantity<si::hertz, inside<{0, 1000}>>     f{50};
    quantity<si::becquerel, inside<{0, 1000}>> a{50};
    static_assert(!addable<decltype(f), decltype(a)>);        // both 1/s, but not one kind

    quantity<isq::width[si::metre], inside<{1, 10}>>  w{3};
    quantity<isq::height[si::metre], inside<{1, 10}>> h{4};
    quantity<isq::area[square(si::metre)], inside<{1, 100}>> area = w * h; // width · height
    auto around = w + h;                                      // 7 m, a length: the common kind
    auto tall   = quantity_cast<isq::height>(w);              // a width read as a height, explicitly
    // [/listing]
    if (area.numerical_value() != just<12> || around.numerical_value() != just<7> ||
        tall.numerical_value() != just<3>)
        return 1;
    return 0;
}
