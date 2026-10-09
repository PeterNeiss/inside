// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_NUMERIC_LIMITS_HPP
#define BEMAN_INSIDE_NUMERIC_LIMITS_HPP

#include <beman/inside/inside.hpp>

#include <limits>
#include <functional>

//---------------------------------------------------------------------------
// numeric_limits / hash — std:: specialisations for inside<G, P>.
// numeric_limits reports the *grid* bounds (Lower/Upper), not the raw type's
// limits. std::hash hashes the Raw member (rational raw: Numerator+Denominator,
// boost-style combine). Both are the same with or without `f64` / `f32`. (std::common_type lives in arithmetic.hpp,
// always on.)
//---------------------------------------------------------------------------

namespace beman::inside::detail {
// The same inside without `f64` / `f32`: those flags only pick the raw, so
// limits and hashes are the ones the type has without them.
template <grid G, policy_flag P>
using without_fp_storage = inside<G, P & ~(f64 | f32)>;
} // namespace beman::inside::detail

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::numeric_limits<beman::inside::inside<G, P>> {
    using B = beman::inside::inside<G, P>;

    static constexpr bool is_specialized = true;
    static constexpr bool is_signed      = (G.Interval.Lower < beman::inside::detail::rational{0});
    // Every value is an integer: a non-zero integer notch over an integer Lower.
    static constexpr bool is_integer        = beman::inside::detail::is_integer_aligned<B> && G.Notch != 0;
    static constexpr bool is_exact          = true; // rational + integer raw are both exact
    static constexpr bool is_bounded        = true;
    static constexpr bool is_modulo         = (P & beman::inside::wrap) != 0;
    static constexpr bool has_infinity      = false;
    static constexpr bool has_quiet_NaN     = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr bool traps             = beman::inside::is_checked(P);
    static constexpr bool is_iec559         = false;
    static constexpr int  radix             = 2;
    // The mode stores round by (rounding_of, the one precedence every path uses).
    static constexpr std::float_round_style round_style = [] {
        using enum beman::inside::detail::round_mode;
        switch (beman::inside::detail::rounding_of(P)) {
        case floor:
            return std::round_toward_neg_infinity;
        case ceil:
            return std::round_toward_infinity;
        case nearest:
        case half_even:
            return std::round_to_nearest;
        default:
            return std::round_toward_zero;
        }
    }();

    // digits / digits10 forward to the deduced raw type (as without `f64` /
    // `f32`) so generic algorithms see the storage size, not the rational
    // interval count.
    using deduced_raw             = beman::inside::detail::raw_t<beman::inside::detail::without_fp_storage<G, P>>;
    static constexpr int digits   = std::numeric_limits<deduced_raw>::digits;
    static constexpr int digits10 = std::numeric_limits<deduced_raw>::digits10;

    static constexpr B min() noexcept { return B{::beman::inside::detail::lower64<B>}; }
    static constexpr B max() noexcept { return B{::beman::inside::detail::upper64<B>}; }
    static constexpr B lowest() noexcept { return B{::beman::inside::detail::lower64<B>}; }
    // Exact types have no rounding noise — epsilon and round_error are 0 when
    // 0 is on the grid (it always is when 0 ∈ interval, since the grid is
    // validated such that Lower is an integer multiple of Notch). When 0 is
    // outside the interval, fall back to the grid minimum — the closest
    // representable stand-in for "no error" the type can express.
    static constexpr B epsilon() noexcept {
        if constexpr (G.Interval.Lower <= beman::inside::detail::rational{0} &&
                      beman::inside::detail::rational{0} <= G.Interval.Upper)
            return B{beman::inside::detail::rational{0}};
        else
            return B{::beman::inside::detail::lower64<B>};
    }
    static constexpr B round_error() noexcept { return epsilon(); }
};

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::hash<beman::inside::inside<G, P>> {
    using B = beman::inside::inside<G, P>;

    constexpr std::size_t operator()(const B& b) const noexcept {
        if constexpr (beman::inside::detail::fp_raw<B>) {
            // The hash of the same value without fp storage.
            using twin = beman::inside::detail::without_fp_storage<G, P>;
            return std::hash<twin>{}(twin{b});
        } else if constexpr (beman::inside::detail::rational_raw<B>) {
            // Boost-style hash combine over (Numerator, Denominator).
            auto h1 = std::hash<beman::inside::umax>{}(b.raw().Numerator);
            auto h2 = std::hash<beman::inside::imax>{}(b.raw().Denominator);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        } else if constexpr (beman::inside::detail::wide_raw<B>) {
            // Same combine over the limbs of a wide index.
            std::size_t h = 0;
            for (auto w : b.raw().Word)
                h ^= std::hash<beman::inside::umax>{}(w) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return h;
        } else
            return std::hash<beman::inside::detail::raw_t<B>>{}(b.raw());
    }
};

#endif // BEMAN_INSIDE_NUMERIC_LIMITS_HPP
