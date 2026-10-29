// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_MATH_CONSTANTS_HPP
#define BEMAN_INSIDE_DETAIL_MATH_CONSTANTS_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>

//---------------------------------------------------------------------------
// Mathematical constants as point insides: for each, the best rational whose
// numerator and denominator stay below 2^31, so products of a few of them
// keep 64-bit grid numbers (the fast paths). The comment gives
// |rational − value|. They compose directly in inside-space
// (`angle * math::pi`); for a correctly rounded value on a grid of your
// choice, use the functions (math::sqrt_into<Out>(just<2>)). Public from
// cmath.hpp and constants.hpp.
//---------------------------------------------------------------------------
namespace beman::inside::math {
inline constexpr auto pi          = just<rational{1068966896, 340262731}>;  // 2^-58.2
inline constexpr auto two_pi      = just<rational{2137933792, 340262731}>;  // 2^-57.2
inline constexpr auto half_pi     = just<rational{2015008331, 1282794145}>; // 2^-60.2
inline constexpr auto inv_pi      = just<rational{340262731, 1068966896}>;  // 2^-61.5
inline constexpr auto inv_sqrt_pi = just<rational{1196609286, 2120934737}>; // 2^-58.0
inline constexpr auto e           = just<rational{848456353, 312129649}>;   // 2^-60.5
inline constexpr auto log2e       = just<rational{717140287, 497083768}>;   // 2^-61.6
inline constexpr auto log10e      = just<rational{774923109, 1784326399}>;  // 2^-66.1
inline constexpr auto ln2         = just<rational{497083768, 717140287}>;   // 2^-62.6
inline constexpr auto ln10        = just<rational{1784326399, 774923109}>;  // 2^-63.7
inline constexpr auto sqrt2       = just<rational{1855077841, 1311738121}>; // 2^-62.1
inline constexpr auto sqrt3       = just<rational{1934726305, 1117014753}>; // 2^-60.9
inline constexpr auto inv_sqrt2   = just<rational{1311738121, 1855077841}>; // 2^-63.1
inline constexpr auto inv_sqrt3   = just<rational{1117014753, 1934726305}>; // 2^-62.5
inline constexpr auto phi         = just<rational{1836311903, 1134903170}>; // golden ratio, 2^-61.3
inline constexpr auto egamma      = just<rational{376566901, 652385103}>;   // Euler–Mascheroni γ, 2^-62.2
inline constexpr auto catalan     = just<rational{150820265, 164657129}>;   // Catalan's G, 2^-59.4
inline constexpr auto zeta3       = just<rational{461424925, 383862797}>;   // Apéry's ζ(3), 2^-60.6
} // namespace beman::inside::math

#endif // BEMAN_INSIDE_DETAIL_MATH_CONSTANTS_HPP
