// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Complex numbers are the even part of the geometric algebra G(2,0). Two
// basis vectors of your own, e₁ and e₂, each square to +1 and anticommute;
// their product names the unit i = e₁e₂. That i·i = -1 is stated nowhere:
// e₁e₂e₁e₂ = -e₁e₁e₂e₂ = -1 follows from the two squares.
//   1. z·w, the conjugate (the reverse), |z|² and z / w, exactly.
//   2. Vectors of the plane: v·i turns v by 90°, e₁·v is v as a complex
//      number, and (1 + i)⁴ = -4.
//   3. Gaussian integers on grids: each part's range is proved in the type,
//      and 5 = (2 + i)(2 - i).
//   4. An impedance: 10 V across 3 Ω + 4 Ω i draws 1.2 A - 1.6 A i.
//   5. e^(iπ) = -1: math::exp of a complex number rounds each part once,
//      here onto a grid of 2^-20, and with π on that grid the result is
//      -1 + 0 i exactly.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr basis_vector e1_def{"e₁", +1, {.ascii = "e1"}}; // e₁e₁ = 1
inline constexpr basis_vector e2_def{"e₂", +1, {.ascii = "e2"}}; // e₂e₂ = 1
inline constexpr auto         e1 = unit_c<e1_def>;
inline constexpr auto         e2 = unit_c<e2_def>;
inline constexpr named_unit   i_def{"i", e1 * e2};
inline constexpr auto         i = unit_c<i_def>;
} // namespace

int main() {
    static_assert(just<1> * i * (just<1> * i) == just<-1>);

    // 1. Arithmetic: (3 + 4i)(1 - 2i) = 11 - 2i, |z|² = z z̃ = 25, and the
    //    quotient is exact.
    const auto z = just<3> + just<4> * i;
    const auto w = just<1> - just<2> * i;
    std::cout << "z = " << z << ", w = " << w << ", z w = " << z * w << ", conj z = " << reverse(z)
              << ", |z|² = " << squared_norm(z) << ", z / w = " << z / w << '\n';
    if (to_string(z * w) != "11 - 2 i" || to_string(reverse(z)) != "3 - 4 i" || !(squared_norm(z) == just<25>) ||
        !(z / w == just<-1> + just<2> * i))
        return 1;

    // 2. A vector of the plane in metres. v·i turns it by +90°; e₁·v is a
    //    scalar plus a bivector: v's complex number, in metres.
    const auto v = just<2> * si::metre * e1 + just<1> * si::metre * e2;
    std::cout << "v = " << v << ", v i = " << v * i << ", e₁ v = " << just<1> * e1 * v << '\n';
    if (to_string(v * i) != "-1 m e₁ + 2 m e₂" || !(part(just<1> * e1 * v, i) == just<1> * si::metre))
        return 1;
    const auto one_plus_i = just<1> + just<1> * i;
    const auto fourth     = one_plus_i * one_plus_i * one_plus_i * one_plus_i;
    std::cout << "(1 + i)⁴ = " << fourth << '\n';
    if (!(fourth == just<-4>))
        return 1;

    // 3. Gaussian integers with parts in [-10, 10]: the real part of a product
    //    is ac - bd, in [-200, 200], and the type says so.
    using gaussian = multivector<inside<{-10, 10}>, quantity<i, inside<{-10, 10}>>>;
    const gaussian a{just<2> + just<1> * i};
    const gaussian b{just<2> - just<1> * i};
    const auto     p = a * b;
    static_assert(std::same_as<decltype(part(p, one)), inside<{-200, 200}>>);
    std::cout << "(" << a << ")(" << b << ") = " << p << '\n';
    if (!(p == just<5>))
        return 1;

    // 4. Impedance Z = R + X i: I = V / Z = V Z̃ / |Z|², exactly, in A.
    const auto Z = (just<3> + just<4> * i) * si::ohm;
    const auto V = just<10> * si::volt;
    const auto I = (V / Z).in(si::ampere);
    std::cout << "Z = " << Z << ", I = V / Z = " << I << '\n';
    if (to_string(I) != "1.2 A - 1.6 A i")
        return 1;

    // 5. Euler's identity, onto a grid of 2^-20.
    using fine    = inside<{{-4, 4}, per<1 << 20>}, round_nearest>;
    using complex = multivector<fine, quantity<i, fine>>;
    const fine    pi{rational{314159265358979, 100000000000000}};
    const complex euler = math::exp<complex>(complex{pi * i});
    std::cout << "e^(iπ) = " << euler << '\n';
    if (!(euler == just<-1>))
        return 1;
    return 0;
}
