// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Quaternions as the even part of the geometric algebra G(3,0): with three basis vectors of your own that square
// to +1, the bivectors i = e₁e₂, j = e₃e₁ and k = e₂e₃ satisfy Hamilton's rules; nothing else is stated.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

namespace {
inline constexpr auto e1 = basis_vector<"e₁", +1, {.ascii = "e1"}>;
inline constexpr auto e2 = basis_vector<"e₂", +1, {.ascii = "e2"}>;
inline constexpr auto e3 = basis_vector<"e₃", +1, {.ascii = "e3"}>;

inline constexpr auto i = unit<"i", e1 * e2>;
inline constexpr auto j = unit<"j", e3 * e1>; // -e₁e₃: the unit tracks the sign
inline constexpr auto k = unit<"k", e2 * e3>;

inline constexpr auto one_i = just<1> * i;
inline constexpr auto one_j = just<1> * j;
inline constexpr auto one_k = just<1> * k;
} // namespace

int main() {
    // 1. Hamilton's rules, i² = j² = k² = ijk = -1 and ij = k = -ji, at compile time.
    static_assert(one_i * one_i == just<-1> && one_j * one_j == just<-1> && one_k * one_k == just<-1>);
    static_assert(one_i * one_j * one_k == just<-1>);
    static_assert(one_i * one_j == one_k && one_j * one_i == -one_k);
    static_assert(one_j * one_k == one_i && one_k * one_i == one_j);

    // 2. p q ≠ q p, and |pq|² = |p|²|q|² (Euler's four-square identity), exactly.
    const auto p = just<1> + just<2> * i + just<3> * j + just<4> * k;
    const auto q = just<1, 2> + just<1, 2> * i + just<1, 2> * j + just<1, 2> * k;
    std::cout << "p = " << p << ", q = " << q << '\n';
    std::cout << "p q = " << p * q << ", q p = " << q * p << '\n';
    std::cout << "|p q|² = " << squared_norm(p * q) << " = |p|² |q|² = " << squared_norm(p) << " · "
              << squared_norm(q) << '\n';
    if (p * q == q * p || !(squared_norm(p * q) == squared_norm(p) * squared_norm(q))
        || to_string(p * q) != "-4 + 1 i + 3 j + 2 k")
        return 1;

    // 3. q turns a vector in metres a third of a turn about (1, 1, 1): e₁ → e₃ → e₂ → e₁, exactly, and the grade-3
    // part of q v q⁻¹ is 0.
    const auto v = just<1> * si::metre * e1 + just<2> * si::metre * e2 + just<3> * si::metre * e3;
    const auto r = q * v * inverse(q);
    std::cout << "q⁻¹ = " << inverse(q) << '\n';
    std::cout << "v = " << v << ", q v q⁻¹ = " << grade<1>(r) << " (and " << part(r, e1 * e2 * e3) << " e₁e₂e₃)\n";
    if (to_string(grade<1>(r)) != "2 m e₁ + 3 m e₂ + 1 m e₃" || !(grade<3>(r) == multivector<>{}))
        return 1;

    // 4. q³ = -1: three turns of 120° make a full turn, which a rotor represents by -1.
    std::cout << "q³ = " << q * q * q << '\n';
    if (!(q * q * q == just<-1>))
        return 1;

    // 5. q from its angle, exp(θ/2 · n) with θ/2 = π/3 about n = (i + j + k)/√3: each bivector coefficient is
    // π/(3√3) = 0.6045997880780726… on 2^-20, and exp rounds each part once onto 2^-16, exactly ½: it is q.
    using coefficient = inside<{{-1, 1}, per<(1 << 20)>}, round_half_away>;
    using rotor_part = inside<{{-1, 1}, per<(1 << 16)>}, round_half_away>;
    using rotor = multivector<rotor_part, quantity<i, rotor_part>, quantity<j, rotor_part>, quantity<k, rotor_part>>;
    const coefficient c{just<6045997880780726, 10000000000000000>};
    const rotor from_angle = math::exp<rotor>(c * i + c * j + c * k);
    std::cout << "exp(π/3 · (i + j + k)/√3) = " << from_angle << '\n';
    if (!(from_angle == q))
        return 1;
    return 0;
}
