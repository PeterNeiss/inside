// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Complex numbers as the even part of the geometric algebra G(2,0):
// two basis vectors of your own square to +1 and anticommute, and their product is the unit i.
// That i·i = -1 is stated nowhere: it follows.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

namespace
{
  inline constexpr auto e1 = basis_vector<"e₁", geometric<+1>, {.ascii = "e1"}>; // e₁e₁ = 1
  inline constexpr auto e2 = basis_vector<"e₂", geometric<+1>, {.ascii = "e2"}>; // e₂e₂ = 1
  inline constexpr auto i  = unit<"i", e1 * e2>;
} // namespace

int main()
{
  static_assert(i * i == -one);

  // 1. (3 + 4i)(1 - 2i) = 11 - 2i, the conjugate is the reverse, |z|² = z z̃ = 25,
  // and z / w is exact.
  const auto z{just<3> + just<4> * i};
  const auto w{just<1> - just<2> * i};
  std::cout << "z = " << z << ", w = " << w << ", z w = " << z * w << ", conj z = " << reverse(z)
            << ", |z|² = " << squared_norm(z) << ", z / w = " << z / w << '\n';
  if (to_string(z * w) != "11 - 2 i" || to_string(reverse(z)) != "3 - 4 i" || !(squared_norm(z) == 25)
      || !(z / w == just<-1> + just<2> * i))
    return 1;

  // 2. A vector of the plane in metres: v·i turns it by +90°,
  // and e₁·v is v as a complex number, in metres.
  const auto v{just<2> * si::metre * e1 + si::metre * e2};
  std::cout << "v = " << v << ", v i = " << v * i << ", e₁ v = " << e1 * v << '\n';
  if (to_string(v * i) != "-1 m·e₁ + 2 m·e₂" || !(part(e1 * v, i) == si::metre))
    return 1;
  const auto one_plus_i{just<1> + i};
  const auto fourth    {one_plus_i * one_plus_i * one_plus_i * one_plus_i};
  std::cout << "(1 + i)⁴ = " << fourth << '\n';
  if (!(fourth == just<-4> * one))
    return 1;

  // 3. Gaussian integers with parts in [-10, 10]:
  // a product's real part ac - bd is in [-200, 200], by its type.
  using gaussian = multivector<id<"[-10..10]">, id<"[-10..10] [i]">>;
  const gaussian a{just<2> + i};
  const gaussian b{just<2> - i};
  const auto     p{a * b};
  static_assert(std::same_as<decltype(part(p, one)), id<"[-200..200]">>);
  std::cout << "(" << a << ")(" << b << ") = " << p << '\n';
  if (!(p == just<5> * one))
    return 1;

  // 4. An impedance Z = R + X i: I = V / Z = V Z̃ / |Z|², exactly, in A.
  const auto Z{(just<3> + just<4> * i) * si::ohm};
  const auto V{just<10> * si::volt};
  const auto I{(V / Z).in(si::ampere)};
  std::cout << "Z = " << Z << ", I = V / Z = " << I << '\n';
  if (to_string(I) != "1.2 A - 1.6 A·i")
    return 1;

  // 5. Euler's identity: exp rounds each part once onto 2^-20,
  // and with π on that grid e^(iπ) is -1 exactly.
  using fine_angle = id<"[-4..4] 1/1048576 round_half_away">;
  const fine_angle pi = math::pi; // π rounded onto 2^-20
  const auto       euler{math::exp(pi * i)};
  std::cout << "e^(iπ) = " << euler << '\n';
  if (!(euler == -one))
    return 1;
  return 0;
}
