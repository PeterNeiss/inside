// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_NUMERIC_LIMITS_HPP
#define BEMAN_INSIDE_INSIDE_NUMERIC_LIMITS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/core.hpp>

#include <limits>
#include <functional>

//---------------------------------------------------------------------------
// numeric_limits / hash — std:: specializations for inside<G, P>. Part of the
// umbrella: a TU without them would silently get the primary numeric_limits
// (is_specialized false, max() a zero raw), and an ODR violation beside one
// that has them.
// numeric_limits reports the *grid* bounds (Lower/Upper), not the raw type's
// limits. std::hash hashes the Raw member (rational raw: Num+Den,
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

    // digits / digits10 from the grid, not its (unspecified) storage: the bits
    // of the narrowest integer that numbers every value, less a sign bit when
    // the grid holds negatives — 8 for {0, 255}, 7 for {-128, 127}, 6 for
    // {0, 100}. A point or a continuous grid has no such count: 0.
    static constexpr int digits = [] {
        namespace d = ::beman::inside::detail;
        if constexpr (G.Notch == 0)
            return 0;
        else
            return d::bit_width_of(G.step_count() + d::grid_int{1}) - 1 -
                   (G.Interval.Lower < d::grid_rational{0} ? 1 : 0);
    }();
    static constexpr int digits10 = digits * 30103 / 100000; // ⌊digits·log10 2⌋

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
    // An exact type has no rounding error: epsilon and round_error are 0, as
    // for the integer types. They exist where 0 is a value of the grid; on
    // any other grid generic code gets a compile error, not a stand-in.
    static constexpr B epsilon() noexcept
        requires(G.representable(::beman::inside::detail::grid_rational{0}))
    {
        return at(::beman::inside::detail::grid_rational{0});
    }
    static constexpr B round_error() noexcept
        requires(G.representable(::beman::inside::detail::grid_rational{0}))
    {
        return epsilon();
    }
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
            return mix(std::hash<beman::inside::umax>{}(b.raw().Num), b.raw().Den);
        else if constexpr (beman::inside::detail::wide_index_storage<B>)
            return combine(0, b.raw());
        else
            return std::hash<beman::inside::detail::raw_t<B>>{}(b.raw());
    }
};

#endif // BEMAN_INSIDE_INSIDE_NUMERIC_LIMITS_HPP
