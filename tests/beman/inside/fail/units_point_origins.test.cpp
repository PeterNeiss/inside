// EXPECT: points of different absolute origins
// REQUIRES: delete-reason
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Two points counted from unrelated absolute origins have no difference.
#include <beman/inside/units.hpp>

inline constexpr beman::inside::units::point_origin other_zero{"other_zero",
                                                               beman::inside::units::dim_thermodynamic_temperature};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    const auto a = origin_c<other_zero> + just<5> * si::kelvin;
    const auto b = point<si::kelvin>(just<1>);
    auto       d = a.quantity_from(si::absolute_zero);
    (void)b;
    (void)d;
}
