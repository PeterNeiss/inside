// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_MATH_CONSTANTS_HPP
#define BEMAN_INSIDE_DETAIL_MATH_CONSTANTS_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/inside.hpp>

//---------------------------------------------------------------------------
// Mathematical constants as rationals: for each, the best approximation whose
// numerator and denominator stay below 2^31, so products of a few of them
// keep 64-bit grid numbers. The comment gives |rational − value|. They are
// public as math::pi, math::e, … (point insides), from cmath.hpp and
// constants.hpp.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
inline constexpr rational pi_rat{1068966896, 340262731};           // 2^-58.2
inline constexpr rational two_pi_rat = 2 * pi_rat;                 // 2^-57.2
inline constexpr rational half_pi_rat{2015008331, 1282794145};     // 2^-60.2
inline constexpr rational inv_pi_rat{340262731, 1068966896};       // 2^-61.5
inline constexpr rational inv_sqrt_pi_rat{1196609286, 2120934737}; // 2^-58.0
inline constexpr rational e_rat{848456353, 312129649};             // 2^-60.5
inline constexpr rational log2e_rat{717140287, 497083768};         // 2^-61.6
inline constexpr rational log10e_rat{774923109, 1784326399};       // 2^-66.1
inline constexpr rational ln2_rat{497083768, 717140287};           // 2^-62.6
inline constexpr rational ln10_rat{1784326399, 774923109};         // 2^-63.7
inline constexpr rational sqrt2_rat{1855077841, 1311738121};       // 2^-62.1
inline constexpr rational sqrt3_rat{1934726305, 1117014753};       // 2^-60.9
inline constexpr rational inv_sqrt2_rat{1311738121, 1855077841};   // 2^-63.1
inline constexpr rational inv_sqrt3_rat{1117014753, 1934726305};   // 2^-62.5
inline constexpr rational phi_rat{1836311903, 1134903170};         // 2^-61.3
inline constexpr rational egamma_rat{376566901, 652385103};        // 2^-62.2
inline constexpr rational catalan_rat{150820265, 164657129};       // 2^-59.4
inline constexpr rational zeta3_rat{461424925, 383862797};         // 2^-60.6
} // namespace beman::inside::detail

namespace beman::inside::math {
// Point insides, so they compose directly in inside-space (`angle * math::pi`)
// with no rational on the surface. For a correctly rounded value on a grid of
// your choice, use the functions (math::sqrt_into<Out>(just<2>)).
inline constexpr auto pi          = just<beman::inside::detail::pi_rat>;
inline constexpr auto two_pi      = just<beman::inside::detail::two_pi_rat>;
inline constexpr auto half_pi     = just<beman::inside::detail::half_pi_rat>;
inline constexpr auto inv_pi      = just<beman::inside::detail::inv_pi_rat>;
inline constexpr auto inv_sqrt_pi = just<beman::inside::detail::inv_sqrt_pi_rat>;
inline constexpr auto e           = just<beman::inside::detail::e_rat>;
inline constexpr auto log2e       = just<beman::inside::detail::log2e_rat>;
inline constexpr auto log10e      = just<beman::inside::detail::log10e_rat>;
inline constexpr auto ln2         = just<beman::inside::detail::ln2_rat>;
inline constexpr auto ln10        = just<beman::inside::detail::ln10_rat>;
inline constexpr auto sqrt2       = just<beman::inside::detail::sqrt2_rat>;
inline constexpr auto sqrt3       = just<beman::inside::detail::sqrt3_rat>;
inline constexpr auto inv_sqrt2   = just<beman::inside::detail::inv_sqrt2_rat>;
inline constexpr auto inv_sqrt3   = just<beman::inside::detail::inv_sqrt3_rat>;
inline constexpr auto phi         = just<beman::inside::detail::phi_rat>;     // golden ratio
inline constexpr auto egamma      = just<beman::inside::detail::egamma_rat>;  // Euler–Mascheroni γ
inline constexpr auto catalan     = just<beman::inside::detail::catalan_rat>; // Catalan's G
inline constexpr auto zeta3       = just<beman::inside::detail::zeta3_rat>;   // Apéry's ζ(3)
} // namespace beman::inside::math

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_MATH_CONSTANTS_HPP
