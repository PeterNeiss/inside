// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: uniform sampling over a grid. `uniform<B>(rng)` returns a B drawn
// uniformly from the grid's slots (Lower, Lower + Notch, …, Upper) — exact, any
// storage. Kept out of the umbrella because <random> is heavy, hosted-only and
// pulls <cmath> (so the single header drops it under BEMAN_INSIDE_MATH_NO_FP).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_RANDOM_HPP
#define BEMAN_INSIDE_RANDOM_HPP

#include <beman/inside/inside.hpp>

#include <random>

namespace beman::inside {
template <insidable B, std::uniform_random_bit_generator G>
[[nodiscard]] B uniform(G& g) {
    static_assert(detail::notched<B> || detail::point_grid<B>,
                  "uniform<B>: a continuous grid (notch 0) has no slots to choose from");
    if constexpr (detail::wide_index_storage<B>) {
        // More than 2^64 slots: draw limbs uniformly, masked to the slot count's
        // bit width, and reject draws past the count (accepts > 1/2 of draws).
        using W                = detail::raw_t<B>;
        constexpr W   count    = static_cast<W>(grid_of<B>.slot_count());
        constexpr int top_bits = grid_of<B>.slot_bits() - 64 * (static_cast<int>(sizeof(W) / 8) - 1);
        std::uniform_int_distribution<umax> limb;
        for (;;) {
            W k;
            for (auto& w : k.Word)
                w = limb(g);
            if constexpr (top_bits < 64)
                k.Word[sizeof(W) / 8 - 1] &= (umax{1} << top_bits) - 1;
            if (!(k > count))
                return B::from_raw(k);
        }
    } else {
        std::uniform_int_distribution<umax> pick(0, detail::max_index_v<B>);
        const umax                          k = pick(g);
        if constexpr (detail::fp_storage<B> || detail::rational_storage<B>) {
            const detail::rational v =
                (detail::lower64<B> + (detail::rational{k} * detail::notch64<B>).value()).value();
            if constexpr (detail::fp_storage<B>)
                return B::from_raw(static_cast<detail::raw_t<B>>(static_cast<double>(v))); // exact: fp-exact grid
            else
                return B::from_raw(v);
        } else
            return B::from_raw(detail::raw_from_offset<B>(k)); // index or value storage
    }
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_RANDOM_HPP
