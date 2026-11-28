// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Polynomials: x is a basis vector whose powers add (x·x = x²), so a polynomial is a sum of parts, one per power,
// each with its own proved range. Values, composition, the calculus and division with a remainder are exact.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>
#include <inside/units/polynomial.hpp>

using namespace ins;

namespace
{
  inline constexpr auto x = basis_vector<"x", polynomial>;
  inline constexpr auto y = basis_vector<"y", polynomial>;
} // namespace

int main()
{
  // 1. Products multiply out: (1 + x)³ = 1 + 3 x + 3 x² + x³.
  const auto a{1_q + x};
  std::cout << "(1 + x)³ = " << a * a * a << '\n';
  if (!(a * a * a == 1_q + 3_q * x + 3_q * x * x + x * x * x))
    return 1;

  // 2. substitute gives a value, by Horner's rule: coefficients in [-10, 10] and an argument in [0, 2]
  // prove p(a) in [-70, 70]. A polynomial in its place gives the composition: p(1 + y) = y² - y.
  using coefficient = id<"[-10..10]">;
  using argument    = id<"[0..2] 1/10">;
  const auto p{coefficient{2} - coefficient{3} * x + coefficient{1} * x * x}; // (x - 1)(x - 2)
  const auto v{substitute(p, x, argument{1.5})};
  static_assert(lower_of<decltype(v)> == grid_number{-70} && upper_of<decltype(v)> == grid_number{70});
  std::cout << "p = " << p << ", p(1.5) = " << v << ", p(1 + y) = " << substitute(p, x, 1_q + y)
            << '\n';
  if (!(v == -0.25_q) || !(substitute(p, x, 1_q + y) == y * y - y))
    return 1;

  // 3. The calculus: p′ = 2 x - 3, and ∫₀² p dx = F(2) - F(0) = 2/3 from the antiderivative F.
  const auto F{antiderivative(p, x)};
  std::cout << "p′ = " << derivative(p, x) << ", F = " << F << ", ∫₀² p dx = "
            << substitute(F, x, 2_q) - substitute(F, x, 0_q) << '\n';
  if (!(derivative(p, x) == -3_q + 2_q * x)
      || !(substitute(F, x, 2_q) - substitute(F, x, 0_q) == "2/3"_q))
    return 1;

  // 4. Division with a remainder: x³ - 1 = (x - 1)(x² + x + 1), and p = q (2 x + 1) + r with exact fractions.
  const auto [q1, r1]{divmod(x * x * x - 1_q, x - 1_q)};
  std::cout << "(x³ - 1) / (x - 1) = " << q1 << ", remainder " << r1 << '\n';
  const auto d{1_q + 2_q * x};
  const auto [q2, r2]{divmod(p, d)};
  std::cout << "p / (1 + 2 x) = " << q2 << ", remainder " << r2 << '\n';
  if (!(q1 == 1_q + x + x * x) || !(r1 == 0) || !(q2 * d + r2 == p)
      || !(r2 == 3.75_q))
    return 1;
  return 0;
}
