// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The geometric algebra of units. A basis vector is a unit of your own whose
// square you state (+1, -1 or 0); two different ones anticommute. Every
// product, sum and dual below follows from that, with physical units and
// exact numbers.
//   1. In G(3,0) with lengths: u v = u⌋v + u∧v, a scalar in m² (the dot
//      product) plus an oriented area.
//   2. A reflection of v in the plane through 0 orthogonal to n: -n v n⁻¹,
//      whose grade-1 part is the reflected vector (its grade-3 part is 0).
//   3. u∧v∧w is the signed volume, and dual(u∧v, e₁e₂e₃) is u × v.
//   4. A null vector: the dual numbers a + b ε with ε² = 0. f(3 + ε) =
//      f(3) + f′(3) ε gives the exact derivative of f(x) = x³ - 2x.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr basis_vector e1_def{"e₁", +1, {.ascii = "e1"}};
inline constexpr basis_vector e2_def{"e₂", +1, {.ascii = "e2"}};
inline constexpr basis_vector e3_def{"e₃", +1, {.ascii = "e3"}};
inline constexpr basis_vector eps_def{"ε", 0, {.ascii = "eps"}}; // ε ε = 0
inline constexpr auto         e1  = unit_c<e1_def>;
inline constexpr auto         e2  = unit_c<e2_def>;
inline constexpr auto         e3  = unit_c<e3_def>;
inline constexpr auto         eps = unit_c<eps_def>;
inline constexpr auto         m   = si::metre;

template <class X>
constexpr auto f(const X& x) {
    return x * x * x - just<2> * x;
}
} // namespace

int main() {
    // 1. The geometric product of two vectors.
    const auto u = just<1> * m * e1 + just<2> * m * e2 + just<3> * m * e3;
    const auto v = just<4> * m * e1 + just<5> * m * e2 + just<6> * m * e3;
    std::cout << "u v = " << u * v << '\n';
    std::cout << "u⌋v = " << left_contraction(u, v) << ", u∧v = " << wedge(u, v) << '\n';
    if (!(u * v == left_contraction(u, v) + wedge(u, v)) || !(left_contraction(u, v) == just<32> * square(m)) ||
        !(wedge(u, v) == -wedge(v, u)))
        return 1;

    // 2. Reflection in the plane orthogonal to n = e₁ + e₂ (|n|² = 2): exact,
    //    through n⁻¹ = n / 2.
    const auto n = just<1> * e1 + just<1> * e2;
    const auto w = grade<1>(-n * v * inverse(n));
    std::cout << "n⁻¹ = " << inverse(n) << ", -n v n⁻¹ = " << w << '\n';
    if (to_string(w) != "-5 m e₁ - 4 m e₂ + 6 m e₃")
        return 1;

    // 3. Volume and the cross product.
    const auto x      = just<7> * m * e1 + just<8> * m * e2 + just<10> * m * e3;
    const auto volume = wedge(wedge(u, v), x);
    const auto cross  = dual(wedge(u, v), e1 * e2 * e3);
    std::cout << "u∧v∧x = " << volume << ", u × v = " << cross << '\n';
    if (to_string(volume) != "-3 m³ e₁ e₂ e₃" || to_string(cross) != "-3 m² e₁ + 6 m² e₂ - 3 m² e₃")
        return 1;

    // 4. Dual numbers: ε ε is 0 and drops out of every product.
    static_assert(std::same_as<decltype(just<1> * eps * (just<1> * eps)), multivector<>>);
    const auto fx = f(just<3> + just<1> * eps);
    std::cout << "f(3 + ε) = " << fx << " (f(3) = 21, f′(3) = 25)\n";
    if (!(fx == just<21> + just<25> * eps))
        return 1;
    return 0;
}
