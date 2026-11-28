// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Division: exact by default, integer division on request,
// and a std::expected only when the divisor's grid holds zero.
// A quotient's type bounds its denominators, so arithmetic on it stays exact and plain.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>

using namespace ins;

int main()
{
  using number = id<"[0..100]">;
  const number a{7}, b{3};

  // a / b is the exact rational quotient; b's grid holds 0, so it is an expected.
  const auto exact{a / b};
  std::cout << "7 / 3 (exact)          = " << *exact << "\n";
  if (*exact != "7/3"_q)
    return 1;

  // Integer division, per call: div truncates like C++ `/`, or rounds the way its flag says.
  const auto trunc  {div(a, b, round_toward_zero)};
  const auto nearest{div(number{22}, number{7}, round_half_away)};
  const auto ceil   {div(number{22}, number{7}, round_ceil)};
  std::cout << "7 / 3 (toward zero)    = " << *trunc << "\n"
            << "22 / 7 (nearest, ceil) = " << *nearest << ", " << *ceil << "\n";
  if (*trunc != 2 || *nearest != 3 || *ceil != 4)
    return 1;

  // divmod: the remainder follows the rounded quotient, so q * b + r == a under every mode.
  // A type that rounds divides the same way when stored into: /= is a store.
  using signed_number = id<"[-100..100]">;
  using floored       = id<"[-100..100] round_floor">;
  using positive      = id<"[1..10]">;
  const auto [fq, fr] = *divmod(signed_number{-8}, signed_number{3}, round_floor);
  std::cout << "-8 / 3, -8 % 3 (floor) = " << fq << ", " << fr << "\n";
  floored x{-8};
  x /= positive{3}; // -8/3 floored: -3
  if (fq != -3 || fr != 1 || x != fq)
    return 1;

  // Division by zero is a value, not a crash.
  const auto by_zero{number{10} / number{0}};
  std::cout << "10 / 0                 = " << error_message(by_zero.error()) << "\n";
  if (by_zero || by_zero.error() != error::division_by_zero)
    return 1;

  // A divisor grid without 0 cannot fail: a plain inside, nothing to unwrap.
  const auto plain{number{42} / positive{4}};
  static_assert(insidable<decltype(plain)>);
  std::cout << "42 / 4 (zero-free)     = " << plain << "\n";
  if (plain != 10.5_q)
    return 1;

  // The quotient's denominators are at most 1·max|positive| = 10,
  // so what is computed from it knows its own bound: exact, and still nothing to unwrap.
  using quotient = decltype(number{} / positive{});
  const quotient q    {number{20} / positive{3}}; // 20/3
  const auto     total{q * positive{7} + q / positive{2}};
  static_assert(grid_of<quotient>.MaxDenominator == 10);
  static_assert(insidable<decltype(total)>);
  std::cout << "20/3 * 7 + 20/3 / 2    = " << total << "\n"; // 140/3 + 10/3
  if (total != 50)
    return 1;
  return 0;
}
