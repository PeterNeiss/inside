// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The SI defining constants as quantities, exact: h, e, N_A, k, the photon
// energy E = h·ν, and the constants that follow from them. Needs C++26 big
// grids:
//
//   g++-16 -std=c++26 -freflection -O2 -I ../../include b14_constants.cpp

#include <format>
#include <iostream>

#include <beman/inside/units/constants.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/units/math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;
namespace pc = units::constants;

int main()
{
  // Each constant is a quantity of its kind: h is an action, e a charge.
  static_assert(pc::planck_constant.quantity_spec == isq::action);
  static_assert(pc::elementary_charge.quantity_spec == isq::electric_charge);

  std::cout << std::format("h    = {:.8e}\n", pc::planck_constant.in(si::joule * si::second));
  std::cout << std::format("e    = {:.9e}\n", pc::elementary_charge.in(si::coulomb));
  std::cout << std::format("N_A  = {:.8e}\n", pc::avogadro_constant.in(one / si::mole));
  std::cout << std::format("k    = {:.6e}\n", pc::boltzmann_constant.in(si::joule / si::kelvin));

  // E = h·nu for green light, 540 THz: an exact product.
  const quantity<si::tera(si::hertz), inside<{0, 1000}>> nu{540};
  std::cout << std::format("h nu = {:.9e}\n", (pc::planck_constant * nu).in(si::joule));

  // R = N_A·k and F = N_A·e: every digit.
  std::cout << "R    = " << pc::molar_gas_constant.in(si::joule / (si::mole * si::kelvin)) << "\n";
  std::cout << "F    = " << pc::faraday_constant.in(si::coulomb / si::mole) << "\n";

  // hbar = h/2pi: the factor of pi rounds once, onto a 10^-45 grid.
  using fine = inside<{{0, 1}, 1e-45_g}, round_nearest>;
  std::cout << std::format("hbar = {:.9e}\n", value_cast<si::joule * si::second, fine>(pc::reduced_planck_constant));
  return 0;
}
