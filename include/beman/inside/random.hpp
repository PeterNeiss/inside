// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
// Opt-in: uniform sampling over a grid. `uniform<B>(rng)` returns a B drawn
// uniformly from the grid's slots (Lower, Lower + Notch, …, Upper) — exact, any
// storage. Kept out of the umbrella because <random> is heavy, hosted-only and
// pulls <cmath> (so the single header drops it under BEMAN_INSIDE_MATH_NO_FP).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_RANDOM_HPP
#define BEMAN_INSIDE_RANDOM_HPP

#include <beman/inside/inside.hpp>

#include <random>

namespace beman::inside
{
  template <insidable B, std::uniform_random_bit_generator G>
  [[nodiscard]] B uniform(G& g)
  {
    static_assert(notch_of<B> != 0 || lower_of<B> == upper_of<B>,
                  "uniform<B>: a continuous grid (notch 0) has no slots to choose from");
    static_assert(grid_of<B>.max_index_representable(),
                  "uniform<B>: the grid has more slots than a 64-bit index");
    std::uniform_int_distribution<umax> pick(0, detail::max_index_v<B>);
    const umax k = pick(g);
    if constexpr (detail::fp_raw<B> || detail::rational_raw<B>)
    {
      const detail::rational v = (lower_of<B> + (detail::rational{k} * notch_of<B>).value()).value();
      if constexpr (detail::fp_raw<B>)
        return B::from_raw(static_cast<detail::raw_t<B>>(static_cast<double>(v)));   // exact: fp-exact grid
      else
        return B::from_raw(v);
    }
    else
      return B::from_raw(detail::raw_from_offset<B>(k));   // index or value storage
  }
} // namespace beman::inside

#endif // BEMAN_INSIDE_RANDOM_HPP
