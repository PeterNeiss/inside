# Constants

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

`<beman/inside/constants.hpp>` gives the common mathematical constants, as
one-point insides, and the physical constants, each a [quantity](units.md) of
its ISQ kind. Products of constants are exact.

```cpp
#include <beman/inside/constants.hpp>
using namespace beman::inside;
using namespace beman::inside::units;
namespace pc = units::constants;

using angle_t = inside<{{0, 4}, per<1024>}>;
auto x = angle_t{1.5} * math::pi;                                   // an exact product with π's rational
auto d = pc::speed_of_light_in_vacuum * quantity<si::second, inside<{0, 10}>>{2};
d.in(si::metre);                                                    // 599584916 m, exact
auto w = pc::standard_gravity * quantity<si::kilogram, inside<{0, 100}>>{70};
w.in(si::newton);                                                   // 686.46550 N, exact

// h, e, k and N_A are grid numbers past 64 bits.
auto R = pc::avogadro_constant * pc::boltzmann_constant;            // exact
std::format("{:.14e}", R.in(si::joule / (si::mole * si::kelvin)));  // 8.31446261815324e+00 J/(K mol)
auto E = pc::planck_constant * quantity<si::tera(si::hertz), inside<{0, 1000}>>{540};
std::format("{:.9e}", E.in(si::joule));                             // 3.578077881e-19 J
```

## Mathematical constants: `math::`

Each constant is a point `inside`, `just<r>`, where `r` is the best rational
whose numerator and denominator stay below 2³¹. The numbers stay small, so
products of a few constants keep 64-bit grid numbers. `cmath.hpp` defines them
too.

| Name | Value | Error |
|---|---|---|
| `pi`, `two_pi`, `half_pi` | π, 2π, π/2 | 2^-58.2, 2^-57.2, 2^-60.2 |
| `inv_pi`, `inv_sqrt_pi` | 1/π, 1/√π | 2^-61.5, 2^-58.0 |
| `e`, `log2e`, `log10e` | e, log₂e, log₁₀e | 2^-60.5, 2^-61.6, 2^-66.1 |
| `ln2`, `ln10` | ln 2, ln 10 | 2^-62.6, 2^-63.7 |
| `sqrt2`, `sqrt3`, `inv_sqrt2`, `inv_sqrt3` | √2, √3, 1/√2, 1/√3 | 2^-62.1, 2^-60.9, 2^-63.1, 2^-62.5 |
| `phi` | golden ratio φ | 2^-61.3 |
| `egamma` | Euler–Mascheroni γ | 2^-62.2 |
| `catalan` | Catalan's constant G | 2^-59.4 |
| `zeta3` | Apéry's constant ζ(3) | 2^-60.6 |

For a correctly rounded value on a grid you choose, call the function:
`math::sqrt_into<Out>(just<2>)`, `math::exp_into<Out>(just<1>)`. Two ways
avoid approximating π at all: scale a unit with `mag_pi`
([units.md](units.md#factors-of-π-round-once)), and take the sine of an angle
in degrees or turns, which `units::sin_into` reduces exactly
([math.md](math.md#angles-over-many-turns)).

## Physical constants: `units::constants::`

Each constant is the number 1 of a named unit whose magnitude is the
constant, under the constant's ISQ kind where one fits. Products therefore
stay exact: `R`, `F` and `E = h·ν` are exact rationals. `.in(u)` gives the
number in unit `u`, and `std::format` with `{:e}` prints it compactly. A
factor of π, as in ħ or σ, rounds once, through `value_cast` (units_math.hpp).

The constant's unit is in `si2019::` or `codata2022::`, under the same name.

### Exact: the SI 2019 definitions and what follows

| Name | Symbol | Kind | Value |
|---|---|---|---|
| `hyperfine_structure_transition_frequency_of_cs` | Δν_Cs | frequency | 9 192 631 770 Hz |
| `speed_of_light_in_vacuum` | c | speed | 299 792 458 m/s |
| `luminous_efficacy` | K_cd | — | 683 lm/W |
| `planck_constant` | h | action | 6.626 070 15 × 10⁻³⁴ J s |
| `elementary_charge` | e | electric charge | 1.602 176 634 × 10⁻¹⁹ C |
| `boltzmann_constant` | k | — | 1.380 649 × 10⁻²³ J/K |
| `avogadro_constant` | N_A | — | 6.022 140 76 × 10²³ mol⁻¹ |
| `reduced_planck_constant` | ħ = h/2π | action | 1.054 571 817… × 10⁻³⁴ J s |
| `molar_gas_constant` | R = N_A k | — | 8.314 462 618 153 24 J/(mol K) |
| `faraday_constant` | F = N_A e | — | 96 485.332 123 310 0184 C/mol |
| `josephson_constant` | K_J = 2e/h | — | 4.835 978 484… × 10¹⁴ Hz/V |
| `von_klitzing_constant` | R_K = h/e² | resistance | 25 812.807 45… Ω |
| `conductance_quantum` | G₀ = 2e²/h | conductance | 7.748 091 729… × 10⁻⁵ S |
| `magnetic_flux_quantum` | Φ₀ = h/2e | magnetic flux | 2.067 833 848… × 10⁻¹⁵ Wb |
| `stefan_boltzmann_constant` | σ = 2π⁵k⁴/15h³c² | — | 5.670 374 419… × 10⁻⁸ W/(m² K⁴) |
| `first_radiation_constant` | c₁ = 2πhc² | — | 3.741 771 852… × 10⁻¹⁶ W m² |
| `second_radiation_constant` | c₂ = hc/k | — | 1.438 776 877… × 10⁻² m K |
| `standard_gravity` | g₀ | acceleration of free fall | 9.806 65 m/s² |
| `standard_atmosphere` | atm | pressure | 101 325 Pa |

### Measured: CODATA 2022

The CODATA 2022 recommended values (Mohr et al., *Rev. Mod. Phys.* 97,
025002, 2025) are stored exactly as published. Their standard uncertainty is
not represented, so a result is only as good as the constant behind it.

| Name | Symbol | Kind | Value |
|---|---|---|---|
| `gravitational_constant` | G | — | 6.674 30 × 10⁻¹¹ m³/(kg s²) |
| `fine_structure_constant` | α | — | 7.297 352 5643 × 10⁻³ |
| `rydberg_constant` | R_∞ | — | 10 973 731.568 157 m⁻¹ |
| `vacuum_magnetic_permeability` | μ₀ | — | 1.256 637 061 27 × 10⁻⁶ N/A² |
| `characteristic_impedance_of_vacuum` | Z₀ | impedance | 376.730 313 412 Ω |
| `electron_mass` | m_e | mass | 9.109 383 7139 × 10⁻³¹ kg |
| `proton_mass` | m_p | mass | 1.672 621 925 95 × 10⁻²⁷ kg |
| `neutron_mass` | m_n | mass | 1.674 927 500 56 × 10⁻²⁷ kg |
| `atomic_mass_constant` | m_u | mass | 1.660 539 068 92 × 10⁻²⁷ kg |
| `bohr_radius` | a₀ | radius | 5.291 772 105 44 × 10⁻¹¹ m |
| `vacuum_electric_permittivity` | ε₀ | — | 8.854 187 8188 × 10⁻¹² F/m |

`si::dalton` uses the same 2022 value as `atomic_mass_constant`.

## In the single header

The math constants come with `cmath.hpp`, also freestanding. The physical
constants sit inside the units guard, which is inside the string guard: both
`BEMAN_INSIDE_NO_UNITS` and `BEMAN_INSIDE_NO_STRING` drop them
([single-header.md](single-header.md)).
