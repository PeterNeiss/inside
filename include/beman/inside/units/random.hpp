// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: uniform sampling of quantities, as inside/random.hpp is for
// insides. `uniform<Q>(rng)` draws Q's number uniformly from its grid's
// slots, in Q's unit. Brings <random> through inside/random.hpp; units.hpp
// alone does not.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_UNITS_RANDOM_HPP
#define BEMAN_INSIDE_UNITS_RANDOM_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/random.hpp>
#include <beman/inside/units/core.hpp>

#include <random>

namespace beman::inside::units {
template <class Q, std::uniform_random_bit_generator G>
    requires quantity_like<Q>
[[nodiscard]] Q uniform(G& g) {
    return Q{beman::inside::uniform<typename Q::rep>(g)};
}
} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_RANDOM_HPP
