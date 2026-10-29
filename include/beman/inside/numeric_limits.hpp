// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_NUMERIC_LIMITS_HPP
#define BEMAN_INSIDE_NUMERIC_LIMITS_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>

#include <limits>
#include <functional>

//---------------------------------------------------------------------------
// numeric_limits / hash — std:: specializations for inside<G, P>.
// numeric_limits reports the *grid* bounds (Lower/Upper), not the raw type's
// limits. std::hash hashes the Raw member (rational raw: Numerator+Denominator,
// boost-style combine). (std::common_type lives in arithmetic.hpp, always on.)
//---------------------------------------------------------------------------

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::numeric_limits<beman::inside::inside<G, P>> {
    using B = beman::inside::inside<G, P>;

    static constexpr bool is_specialized = true;
    static constexpr bool is_signed      = (G.Interval.Lower < beman::inside::detail::rational{0});
    // Every value is an integer: a non-zero integer notch over an integer Lower.
    static constexpr bool is_integer        = beman::inside::detail::integer_lattice<B>;
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

    // digits / digits10 forward to the raw type so generic algorithms see the
    // storage size, not the rational interval count.
    using deduced_raw             = beman::inside::detail::raw_t<B>;
    static constexpr int digits   = std::numeric_limits<deduced_raw>::digits;
    static constexpr int digits10 = std::numeric_limits<deduced_raw>::digits10;

  private:
    // The value v of B (on its grid). Past 64 bits through the exact store:
    // no 64-bit rational holds a big grid's limits.
    static constexpr B at(const beman::inside::detail::grid_rational& v) noexcept {
        namespace d = ::beman::inside::detail;
        if constexpr (d::wide_valued<B>) {
            B                   b{};
            beman::inside::errc ec{};
            d::assign_exact<d::rational>(b,
                                         d::exact_of_grid<d::exact_limbs<B>>(v),
                                         beman::inside::make_policy<P>(ec),
                                         beman::inside::no_action{});
            return b;
        } else
            return B{d::to_rational(v)};
    }

  public:
    static constexpr B min() noexcept { return at(G.Interval.Lower); }
    static constexpr B max() noexcept { return at(G.Interval.Upper); }
    static constexpr B lowest() noexcept { return at(G.Interval.Lower); }
    // Exact types have no rounding noise — epsilon and round_error are 0 when
    // 0 is on the grid. A lattice that misses 0 but spans it gives its first
    // point above 0; when 0 is outside the interval, the grid minimum — the
    // closest representable stand-in for "no error" the type can express.
    static constexpr B epsilon() noexcept {
        namespace d = ::beman::inside::detail;
        if constexpr (G.representable(d::grid_rational{0}))
            return at(d::grid_rational{0});
        else if constexpr (G.Interval.Lower < d::grid_rational{0} && d::grid_rational{0} < G.Interval.Upper) {
            // Lower + ⌈−Lower/Notch⌉·Notch
            constexpr d::grid_rational k = d::grid_to_int<d::round_mode::ceil>(-G.Interval.Lower / G.Notch);
            return at(G.Interval.Lower + k * G.Notch);
        } else
            return at(G.Interval.Lower);
    }
    static constexpr B round_error() noexcept { return epsilon(); }
};

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::hash<beman::inside::inside<G, P>> {
    using B = beman::inside::inside<G, P>;

    // Boost-style combine of h with v, and with the limbs of a wide integer.
    template <class V>
    static constexpr std::size_t mix(std::size_t h, V v) noexcept {
        return h ^ (std::hash<V>{}(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
    }
    static constexpr std::size_t combine(std::size_t h, const auto& w) noexcept {
        for (beman::inside::umax limb : w.Word)
            h = mix(h, limb);
        return h;
    }

    constexpr std::size_t operator()(const B& b) const noexcept {
        if constexpr (beman::inside::detail::point_storage<B>)
            return 0; // one value: the type
        else if constexpr (beman::inside::detail::fraction_storage<B>)
            return combine(combine(0, b.raw().Num), b.raw().Den); // a reduced fraction
        else if constexpr (beman::inside::detail::rational_storage<B>)
            return mix(std::hash<beman::inside::umax>{}(b.raw().Numerator), b.raw().Denominator);
        else if constexpr (beman::inside::detail::wide_index_storage<B>)
            return combine(0, b.raw());
        else
            return std::hash<beman::inside::detail::raw_t<B>>{}(b.raw());
    }
};

#endif // BEMAN_INSIDE_NUMERIC_LIMITS_HPP
