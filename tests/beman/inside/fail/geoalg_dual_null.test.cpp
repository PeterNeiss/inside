// EXPECT: use complement
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The dual divides by the pseudoscalar, which a null vector makes 0: the
// projective algebra takes the complement instead.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>

inline constexpr beman::inside::units::basis_vector p0_def{"p0", 0};
inline constexpr beman::inside::units::basis_vector p1_def{"p1", 1};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    constexpr auto p0 = unit_c<p0_def>;
    constexpr auto p1 = unit_c<p1_def>;
    auto           d  = dual(just<1> * p1, p0 * p1);
    (void)d;
}
