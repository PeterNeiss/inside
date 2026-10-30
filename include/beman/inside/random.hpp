// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: uniform sampling over a grid. `uniform<B>(rng)` returns a B drawn
// uniformly from the grid's slots (Lower, Lower + Notch, …, Upper) — exact, any
// storage. The draw depends only on the generator's outputs, so a seed gives
// the same B on every standard library (the std::*_distribution algorithms are
// each library's own). Kept out of the umbrella because <random> is heavy,
// hosted-only and pulls <cmath> (so the single header drops it under
// BEMAN_INSIDE_MATH_NO_FP).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_RANDOM_HPP
#define BEMAN_INSIDE_RANDOM_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>

#include <bit>
#include <limits>
#include <random>

namespace beman::inside {
namespace detail {
// 64 uniform bits from g by a fixed recipe: one output when G spans 64 bits,
// else b bits from each output, b the widest power of two G covers (outputs
// past 2^b − 1 above G::min() are drawn again), high bits first.
template <std::uniform_random_bit_generator G>
constexpr umax random_word(G& g) {
    static_assert(std::numeric_limits<std::invoke_result_t<G&>>::digits <= 64,
                  "uniform<B>: a generator wider than 64 bits");
    constexpr umax span = static_cast<umax>(G::max()) - static_cast<umax>(G::min());
    if constexpr (span == std::numeric_limits<umax>::max())
        return static_cast<umax>(g());
    else {
        constexpr int  bits = std::bit_width(span + 1) - 1;
        constexpr umax mask = (umax{1} << bits) - 1;
        umax           word = 0;
        for (int got = 0; got < 64; got += bits) {
            umax d;
            do
                d = static_cast<umax>(g()) - static_cast<umax>(G::min());
            while (d > mask);
            word = word << bits | d;
        }
        return word;
    }
}

// An index in [0, N]: a word masked to N's bit width, drawn again past N
// (accepts more than half of the draws). N = 0 draws nothing.
template <umax N, std::uniform_random_bit_generator G>
constexpr umax random_index(G& g) {
    if constexpr (N == 0)
        return 0;
    else {
        constexpr umax mask = std::numeric_limits<umax>::max() >> std::countl_zero(N);
        for (;;)
            if (const umax k = random_word(g) & mask; k <= N)
                return k;
    }
}
} // namespace detail

template <insidable B, std::uniform_random_bit_generator G>
[[nodiscard]] B uniform(G& g) {
    static_assert(detail::notched<B> || detail::point_grid<B>,
                  "uniform<B>: a continuous grid (notch 0) has no slots to choose from");
    if constexpr (detail::wide_index_storage<B>) {
        // More than 2^64 slots: the same recipe limb by limb, low limb first,
        // the top limb masked to the step count's bit width.
        using W                = detail::raw_t<B>;
        constexpr W   count    = static_cast<W>(grid_of<B>.step_count());
        constexpr int top_bits = grid_of<B>.slot_bits() - 64 * (static_cast<int>(sizeof(W) / 8) - 1);
        for (;;) {
            W k;
            for (auto& w : k.Word)
                w = detail::random_word(g);
            if constexpr (top_bits < 64)
                k.Word[sizeof(W) / 8 - 1] &= (umax{1} << top_bits) - 1;
            if (!(k > count))
                return B::from_raw(k);
        }
    } else
        return B::from_raw(detail::raw_from_offset<B>(detail::random_index<detail::max_index_of<B>>(g)));
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_RANDOM_HPP
