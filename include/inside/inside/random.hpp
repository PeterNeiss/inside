// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: uniform sampling over a grid.
// `uniform<B>(rng)` returns a B drawn uniformly from the grid's slots (Lower, Lower + Notch, …, Upper):
// exact, any storage.
// The draw depends only on the generator's outputs, so a seed gives the same B on every standard library
// (the std::*_distribution algorithms are each library's own).
// Kept out of the umbrella because <random> is heavy, hosted-only and pulls <cmath>
// (so the single header drops it under INSIDE_MATH_NO_FP).
#ifndef INSIDE_INSIDE_RANDOM_HPP
#define INSIDE_INSIDE_RANDOM_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside.hpp>

#include <limits>
#include <random>

namespace ins
{
  namespace detail
  {
    // 64 uniform bits from g by a fixed recipe:
    // b bits from each output, b the widest power of two G covers
    // (outputs past 2^b − 1 above G::min() are drawn again), high bits first;
    // one output when b ≥ 64, its low 64 bits.
    // A generator wider than 64 bits (unsigned __int128) works in its own type.
    template <std::uniform_random_bit_generator G>
    constexpr std::uint64_t random_word(G& g)
    {
      using R = std::invoke_result_t<G&>;
      using U = std::conditional_t<wide_builtin<R>, std::make_unsigned_t<R>, std::uint64_t>;
      constexpr U   span{static_cast<U>(G::max()) - static_cast<U>(G::min())};
      constexpr int digits{std::numeric_limits<U>::digits};
      constexpr int bits = []
      {
        if (span == std::numeric_limits<U>::max())
          return digits;
        int b{-1};
        for (U s = span + 1; s != 0; s >>= 1)
          ++b;
        return b;
      }();
      auto draw = [&]
      {
        U d{static_cast<U>(g()) - static_cast<U>(G::min())};
        if constexpr (bits < digits)
          while (d > (U{1} << bits) - 1)
            d = static_cast<U>(g()) - static_cast<U>(G::min());
        return d;
      };
      if constexpr (bits >= 64)
        return static_cast<std::uint64_t>(draw());
      else
      {
        std::uint64_t word{0};
        for (int got = 0; got < 64; got += bits)
          word = word << bits | static_cast<std::uint64_t>(draw());
        return word;
      }
    }

    // An index in [0, N] by Lemire's multiply: the high word of w·(N + 1) for a random word w,
    // drawn again while the low word is below 2^64 mod (N + 1),
    // which leaves every index the same number of words
    // (a draw is repeated with probability below (N + 1)/2^64).
    // N = 0 draws nothing; N = 2^64 − 1 takes the word.
    template <std::uint64_t N, std::uniform_random_bit_generator G>
    constexpr std::uint64_t random_index(G& g)
    {
      if constexpr (N == 0)
        return 0;
      else if constexpr (N == std::numeric_limits<std::uint64_t>::max())
        return random_word(g);
      else
      {
        constexpr std::uint64_t s{N + 1};
        constexpr std::uint64_t t{(std::uint64_t{0} - s) % s}; // 2^64 mod s
        for (;;)
          if (const auto m = limb::mul(random_word(g), s); m.Lo >= t)
            return m.Hi;
      }
    }
  } // namespace detail

  template <insidable B, std::uniform_random_bit_generator G>
  [[nodiscard]] B uniform(G& g)
  {
    static_assert(detail::notched<B> || detail::point_grid<B>,
                  "uniform<B>: a continuous grid (notch 0) has no slots to choose from");
    if constexpr (detail::wide_index_storage<B>)
    {
      // More than 2^64 slots: the same recipe limb by limb, low limb first,
      // the top limb masked to the step count's bit width.
      using W = detail::raw_t<B>;
      constexpr W   count{static_cast<W>(grid_of<B>.step_count())};
      constexpr int top_bits{grid_of<B>.slot_bits() - 64 * (static_cast<int>(sizeof(W) / 8) - 1)};
      for (;;)
      {
        W k;
        for (auto& w : k.Word)
          w = detail::random_word(g);
        if constexpr (top_bits < 64)
          k.Word[sizeof(W) / 8 - 1] &= (std::uint64_t{1} << top_bits) - 1;
        if (!(k > count))
          return B::from_raw(k, unsafe);
      }
    }
    else
      return B::from_raw(detail::raw_from_offset<B>(detail::random_index<detail::max_index_of<B>>(g)), unsafe);
  }
} // namespace ins

#endif // INSIDE_INSIDE_RANDOM_HPP
