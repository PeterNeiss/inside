// EXPECT: math::log: no closed form
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// e₁e₂ + e₃e₄ in G(4,0): the two bivectors commute, so the argument's square
// is no scalar and log has no closed form.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/math.hpp>

inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<1>>;
inline constexpr auto e2 = ins::basis_vector<"e2", ins::geometric<1>>;
inline constexpr auto e3 = ins::basis_vector<"e3", ins::geometric<1>>;
inline constexpr auto e4 = ins::basis_vector<"e4", ins::geometric<1>>;

int main() {
    using namespace ins;
    using I   = id<"[-1..1] 1/64 round_half_away">;
    using dom = multivector<id<"[-1..1] 1/64 [e1*e2] round_half_away">, id<"[-1..1] 1/64 [e3*e4] round_half_away">>;
    using cod = multivector<I, id<"[-1..1] 1/64 [e1*e2] round_half_away">, id<"[-1..1] 1/64 [e3*e4] round_half_away">, id<"[-1..1] 1/64 [e1*e2*e3*e4] round_half_away">>;
    const dom x{I{1} * (e1 * e2) + I{1} * (e3 * e4)};
    (void)math::log<cod>(x);
}
