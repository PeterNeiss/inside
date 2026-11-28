// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The geometric algebra of units: a basis vector is a unit of your own whose square you state (+1, -1 or 0),
// and two different ones anticommute.
// Every product, sum and dual below follows, with units and exact numbers.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  inline constexpr auto e1  = basis_vector<"e₁", geometric<+1>, {.ascii = "e1"}>;
  inline constexpr auto e2  = basis_vector<"e₂", geometric<+1>, {.ascii = "e2"}>;
  inline constexpr auto e3  = basis_vector<"e₃", geometric<+1>, {.ascii = "e3"}>;
  inline constexpr auto eps = basis_vector<"ε" , geometric<0> , {.ascii = "eps"}>; // ε ε = 0
  inline constexpr auto m   = si::metre;

  template <class X>
  constexpr auto f(const X& x)
  {
    return x * x * x - 2_q * x;
  }
} // namespace

int main()
{
  // 1. In G(3,0) with lengths: u v = u⌋v + u∧v,
  // a scalar in m² (the dot product) plus an oriented area.
  const auto u{m * e1 + 2_q * m * e2 + 3_q * m * e3};
  const auto v{4_q * m * e1 + 5_q * m * e2 + 6_q * m * e3};
  std::cout << "u v = " << u * v << '\n';
  std::cout << "u⌋v = " << left_contraction(u, v) << ", u∧v = " << wedge(u, v) << '\n';
  if (!(u * v == left_contraction(u, v) + wedge(u, v)) || !(left_contraction(u, v) == 32_q * square(m))
      || !(wedge(u, v) == -wedge(v, u)))
    return 1;

  // 2. Reflection in the plane orthogonal to n = e₁ + e₂: sandwich(n, v) is -n v n⁻¹,
  // exact (n⁻¹ = n / 2), without its grade-3 part, which is 0.
  const auto n{e1 + e2};
  const auto w{sandwich(n, v)};
  std::cout << "n⁻¹ = " << inverse(n) << ", -n v n⁻¹ = " << w << '\n';
  if (to_string(w) != "-5 m·e₁ - 4 m·e₂ + 6 m·e₃")
    return 1;

  // 3. u∧v∧x is the signed volume, and dual(u∧v, e₁e₂e₃) is u × v.
  const auto x     {7_q * m * e1 + 8_q * m * e2 + 10_q * m * e3};
  const auto volume{wedge(wedge(u, v), x)};
  const auto cross {dual(wedge(u, v), e1 * e2 * e3)};
  std::cout << "u∧v∧x = " << volume << ", u × v = " << cross << '\n';
  if (to_string(volume) != "-3 m³·e₁·e₂·e₃" || to_string(cross) != "-3 m²·e₁ + 6 m²·e₂ - 3 m²·e₃")
    return 1;

  // 4. Dual numbers a + b ε: ε ε drops out of products,
  // so f(3 + ε) = f(3) + f′(3) ε, the exact derivative.
  static_assert(std::same_as<decltype(1_q * eps * (1_q * eps)), multivector<>>);
  const auto fx{f(3_q + eps)};
  std::cout << "f(3 + ε) = " << fx << " (f(3) = 21, f′(3) = 25)\n";
  if (!(fx == 21_q + 25_q * eps))
    return 1;
  return 0;
}
