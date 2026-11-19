// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: uniform sampling of quantities, as inside/random.hpp is for
// insides. `uniform<Q>(rng)` draws Q's number uniformly from its grid's
// slots, in Q's unit. Brings <random> through inside/random.hpp; units.hpp
// alone does not.
//---------------------------------------------------------------------------
#ifndef INSIDE_UNITS_RANDOM_HPP
#define INSIDE_UNITS_RANDOM_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/random.hpp>
#include <inside/units/core.hpp>

#include <random>

namespace ins {
template <class Q, std::uniform_random_bit_generator G>
    requires quantity_like<Q>
[[nodiscard]] Q uniform(G& g) {
    return detail::quantity_access::make<Q>(ins::uniform<typename Q::rep>(g));
}
} // namespace ins

#endif // INSIDE_UNITS_RANDOM_HPP
