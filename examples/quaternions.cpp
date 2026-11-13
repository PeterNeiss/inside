// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Quaternions are the even part of the geometric algebra G(3,0). With three
// basis vectors of your own that square to +1, the bivectors i = e₁e₂,
// j = e₃e₁ and k = e₂e₃ satisfy Hamilton's rules; nothing else is stated.
// j is spelled e₃e₁, which is -e₁e₃: the unit tracks the sign.
//   1. i² = j² = k² = ijk = -1 and ij = k = -ji, at compile time.
//   2. p·q ≠ q·p, and |pq|² = |p|²|q|² (Euler's four-square identity),
//      exactly.
//   3. q = (1 + i + j + k)/2 turns a vector in metres a third of a turn
//      about (1, 1, 1): q v q⁻¹ carries e₁ to e₃, e₃ to e₂ and e₂ to e₁,
//      exactly, and the grade-3 part of the product is 0.
//   4. q³ = -1: three turns of 120° make a full turn, which a rotor
//      represents by -1.
//   5. The rotor from the angle: q = exp(θ/2 · n), θ = 120° and n the unit
//      bivector (i + j + k)/√3. math::exp rounds each part of the rotor
//      once, onto a grid of 2^-16: every part is exactly ½, so it is q.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr basis_vector e1_def{"e₁", +1, {.ascii = "e1"}};
inline constexpr basis_vector e2_def{"e₂", +1, {.ascii = "e2"}};
inline constexpr basis_vector e3_def{"e₃", +1, {.ascii = "e3"}};
inline constexpr auto         e1 = unit_c<e1_def>;
inline constexpr auto         e2 = unit_c<e2_def>;
inline constexpr auto         e3 = unit_c<e3_def>;

inline constexpr named_unit i_def{"i", e1 * e2};
inline constexpr named_unit j_def{"j", e3 * e1};
inline constexpr named_unit k_def{"k", e2 * e3};
inline constexpr auto       i = unit_c<i_def>;
inline constexpr auto       j = unit_c<j_def>;
inline constexpr auto       k = unit_c<k_def>;

inline constexpr auto one_i = just<1> * i;
inline constexpr auto one_j = just<1> * j;
inline constexpr auto one_k = just<1> * k;
} // namespace

int main() {
    // 1. Hamilton's rules.
    static_assert(one_i * one_i == just<-1> && one_j * one_j == just<-1> && one_k * one_k == just<-1>);
    static_assert(one_i * one_j * one_k == just<-1>);
    static_assert(one_i * one_j == one_k && one_j * one_i == -one_k);
    static_assert(one_j * one_k == one_i && one_k * one_i == one_j);

    // 2. Products do not commute; norms multiply.
    const auto p = just<1> + just<2> * i + just<3> * j + just<4> * k;
    const auto q = just<1, 2> + just<1, 2> * i + just<1, 2> * j + just<1, 2> * k;
    std::cout << "p = " << p << ", q = " << q << '\n';
    std::cout << "p q = " << p * q << ", q p = " << q * p << '\n';
    std::cout << "|p q|² = " << squared_norm(p * q) << " = |p|² |q|² = " << squared_norm(p) << " · " << squared_norm(q)
              << '\n';
    if (p * q == q * p || !(squared_norm(p * q) == squared_norm(p) * squared_norm(q)) ||
        to_string(p * q) != "-4 + 1 i + 3 j + 2 k")
        return 1;

    // 3. A rotation of a vector (grade 1, in metres) by the rotor q.
    const auto v = just<1> * si::metre * e1 + just<2> * si::metre * e2 + just<3> * si::metre * e3;
    const auto r = q * v * inverse(q);
    std::cout << "q⁻¹ = " << inverse(q) << '\n';
    std::cout << "v = " << v << ", q v q⁻¹ = " << grade<1>(r) << " (and " << part(r, e1 * e2 * e3) << " e₁e₂e₃)\n";
    if (to_string(grade<1>(r)) != "2 m e₁ + 3 m e₂ + 1 m e₃" || !(grade<3>(r) == multivector<>{}))
        return 1;

    // 4. Three turns of 120°.
    std::cout << "q³ = " << q * q * q << '\n';
    if (!(q * q * q == just<-1>))
        return 1;

    // 5. q from its angle: θ/2 = π/3 about n = (i + j + k)/√3, so each
    //    coefficient of the bivector is π/(3√3) = 0.6045997880780726…,
    //    stored onto a grid of 2^-20.
    using coef_t = inside<{{-1, 1}, per<(1 << 20)>}, round_nearest>;
    using part_t = inside<{{-1, 1}, per<(1 << 16)>}, round_nearest>;
    using rotor  = multivector<part_t, quantity<i, part_t>, quantity<j, part_t>, quantity<k, part_t>>;
    const coef_t c{just<6045997880780726, 10000000000000000>};
    const rotor  from_angle = math::exp<rotor>(c * i + c * j + c * k);
    std::cout << "exp(π/3 · (i + j + k)/√3) = " << from_angle << '\n';
    if (!(from_angle == q))
        return 1;
    return 0;
}
