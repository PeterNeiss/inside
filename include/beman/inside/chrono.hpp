// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: std::chrono at the boundary. An inside's value is a time in
// seconds, its notch the resolution: inside<{{0, 86400}, per<1000>}> is a
// time of day to the millisecond, and a chrono period is a notch
// (`period_notch<std::milli>` == per<1000>).
//   from_duration<B, F>(d)   duration → B: the exact count·num/den seconds,
//                            stored like B::try_make<F> (B's policy plus F)
//   to_duration<D, F>(x)     B → duration D: the exact count value·den/num;
//                            off a whole number of D's periods it is a
//                            rounding_error unless F names a rounding mode,
//                            and a count outside D's rep is an overflow
// to_duration returns a plain D when B's grid proves neither can happen (every
// value a whole number of periods, the counts within the rep), and always for
// a floating rep (rounded once); else std::expected<D, errc>.
// An inside is not a duration *rep*: chrono converts periods by multiplying
// the count by a raw intmax_t and keeps the operands' rep for a sum, while an
// inside rejects raw scalars and widens its results.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CHRONO_HPP
#define BEMAN_INSIDE_CHRONO_HPP

#include <beman/inside/inside.hpp>

#include <chrono>
#include <concepts>
#include <expected>
#include <limits>
#include <ratio>

namespace beman::inside {
// A chrono period as a grid notch, in seconds: period_notch<std::milli> is 1/1000.
template <class Period>
inline constexpr rational period_notch =
    rational{static_cast<imax>(Period::num), static_cast<imax>(Period::den)};

namespace detail {
template <class T>
inline constexpr bool is_duration_v = false;
template <class Rep, class Period>
inline constexpr bool is_duration_v<std::chrono::duration<Rep, Period>> = true;

template <class T>
concept chrono_duration = is_duration_v<std::remove_cvref_t<T>>;

// Whether every value of B gives D a count within its rep — a whole number of
// periods, unless F rounds — so to_duration cannot fail.
template <insidable B, chrono_duration D, policy_flag F>
inline constexpr bool duration_total = [] {
    using Rep = typename D::rep;
    if constexpr (std::floating_point<Rep>)
        return true;
    else {
        constexpr grid_rational p{period_notch<typename D::period>};
        if constexpr (F == none && (!(notched<B> || point_grid<B>) || !grid_divides_evenly(lower_of<B>, p) ||
                                    !grid_divides_evenly(notch_of<B>, p)))
            return false;
        else {
            constexpr std::size_t K = exact_limbs<B>;
            using lim               = std::numeric_limits<Rep>;
            const auto            q = exact_of<K>(period_notch<typename D::period>);
            return exact_of_grid<K>(lower_of<B>) / q >= exact_of<K>(lim::min()) &&
                   exact_of_grid<K>(upper_of<B>) / q <= exact_of<K>(lim::max());
        }
    }
}();

// D's count for x: the exact value·den/num, rounded by F when F names a mode.
template <chrono_duration D, policy_flag F, insidable B>
constexpr std::expected<typename D::rep, errc> duration_count(const B& x) {
    using Rep = typename D::rep;
    const auto q = exact_of(x) / exact_of(period_notch<typename D::period>);
    if constexpr (std::floating_point<Rep>)
        return nearest_float<Rep>(q);
    else {
        using I = decltype(q.Num);
        auto  t = q;
        if (q.Num % q.Den != I{0}) {
            if constexpr (F == none)
                return std::unexpected{errc::rounding_error};
            else
                t = round_to_denominator<rounding_of(F)>(q, I{1});
        }
        const I n = trunc(t);
        if (n < I{std::numeric_limits<Rep>::min()} || n > I{std::numeric_limits<Rep>::max()})
            return std::unexpected{errc::overflow};
        return static_cast<Rep>(n);
    }
}
} // namespace detail

// duration → B: the exact value count·num/den seconds, then B's store with
// the flags F added (as B::try_make<F>): B's policy rounds, clamps or wraps,
// and a value it cannot take is the error. A floating count is taken as its
// binary value (to 2^-62 off a 1-second period); past 2^63 periods it is an
// overflow, and a non-finite count is not_finite.
template <insidable B, policy_flag F = none, class Rep, class Period>
[[nodiscard]] constexpr std::expected<B, errc> from_duration(std::chrono::duration<Rep, Period> d) {
    if constexpr (std::floating_point<Rep> && std::ratio_equal_v<Period, std::ratio<1>>)
        return B::template try_make<F>(d.count());
    else {
        std::expected<rational, errc> v{rational{0}};
        if constexpr (std::floating_point<Rep>) {
            const Rep c = d.count();
            if (!detail::is_finite(c))
                return std::unexpected{errc::not_finite};
            if (!(c > Rep{-0x1p63} && c < Rep{0x1p63}))
                return std::unexpected{errc::overflow};
            v = rational{static_cast<double>(c)} * period_notch<Period>;
        } else
            v = rational{d.count()} * period_notch<Period>;
        if (!v)
            return std::unexpected{v.error()};
        return B::template try_make<F>(*v);
    }
}

// B → duration D: the count value·den/num, exactly. F (none, or one rounding
// mode) rounds a count that is not a whole number of D's periods; without it
// that is errc::rounding_error. A count outside D's rep is errc::overflow.
// Returns D when the grid rules both out (see the top), else expected<D, errc>.
template <detail::chrono_duration D, policy_flag F = none, insidable B>
    requires detail::cast_rounding<F>
[[nodiscard]] constexpr auto to_duration(const B& x) {
    if constexpr (detail::duration_total<B, D, F>)
        return D{*detail::duration_count<D, F>(x)};
    else {
        const auto c = detail::duration_count<D, F>(x);
        if (!c)
            return std::expected<D, errc>{std::unexpect, c.error()};
        return std::expected<D, errc>{D{*c}};
    }
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_CHRONO_HPP
