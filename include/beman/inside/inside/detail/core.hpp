// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.
// The core type beman::inside::inside<G, P>: construction and try_make,
// compound assignment, ++/--, the conversions (rational, double,
// to<T>/as<T>), numerator()/denominator(), raw()/from_raw, the per-store
// overrides (policy<F>(), policy(ec)) and hooks (on_wrap, on_clamp,
// on_error), parse(first, last), just<…> and the _ins literal.
// The umbrella adds the casts, arithmetic, range and cursor layers, which
// need this complete type.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_CORE_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_CORE_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/generic.hpp>
#include <beman/inside/inside/detail/wide_value.hpp>
#include <beman/inside/inside/detail/lift.hpp>
#include <beman/inside/inside/detail/call_policy.hpp>
#include <beman/inside/inside/detail/policy_ref.hpp>
#include <beman/inside/inside/detail/addition.hpp>
#include <beman/inside/inside/detail/multiplication.hpp>
#include <beman/inside/inside/detail/division.hpp>
#include <beman/inside/inside/detail/assignment.hpp>
#include <beman/inside/inside/predicates.hpp>

#include <bit>     // std::countr_zero, std::has_single_bit
#include <cstddef> // std::ptrdiff_t
#include <optional>
#include <expected> // std::expected, std::unexpected
#include <iterator> // std::incrementable_traits, std::indirectly_readable_traits
#include <utility>  // std::pair

//---------------------------------------------------------------------------
// inside — defines `inside<G, P>` and its member operators. Free-function
// arithmetic (arithmetic.hpp), casts (casts.hpp) and `inside_range` (range.hpp)
// follow in the umbrella. Heavy lifting is delegated to addition/multiplication/division.hpp
// (per-operator code), assignment.hpp (narrowing/clamp/wrap), generic.hpp
// (inside_assignable, over inside/detail/introspection.hpp and inside/detail/storage.hpp),
// inside/detail/call_policy.hpp and inside/detail/policy_ref.hpp (the proxies of b.policy() and on_*).
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// Selects inside's raw constructor (from_raw).
struct raw_tag {};

// What `*t` yields for a cursor on G: G without its end point, no cursor — a
// value of T, which assigns into T.
template <grid G, policy_flag P>
using cursor_value_t =
    inside<grid{interval{G.Interval.Lower, G.Interval.Upper - G.Notch}, G.Notch}, P & ~cursor_marker>;

// (index_integer, the builtin integers an index converts to: policy_ref.hpp.)

// B converts implicitly to the builtin integer T: B is an index - every
// value a non-negative integer (storage.hpp index_shaped), and no cursor,
// whose end is one past the last index - and T holds its whole grid.
template <class B, class T>
inline constexpr bool index_converts = false;
template <insidable B, index_integer T>
inline constexpr bool index_converts<B, T> =
    index_shaped<grid_of<B>> && !has_flag(flags_of<B>, cursor_marker) && values_fit_int64<B> &&
    !(lower_of<B> < grid_rational{std::numeric_limits<T>::min()}) &&
    !(upper_of<B> > grid_rational{std::numeric_limits<T>::max()});

// The number a type steps over in cursor<T>, values<T> and end(t): an inside
// itself, or a quantity's number (units specializes it); rebind<N> wraps a
// number N back into the type's kind, and make(n) is that value.
template <class T>
struct stepping {};
template <insidable T>
struct stepping<T> {
    using number = T;
    template <class N>
    using rebind = N;
    template <class N>
    static constexpr N make(const N& n) {
        return n;
    }
};
template <class T>
concept steppable = requires { typename stepping<T>::number; };
template <class T>
using step_number_t = typename stepping<T>::number;
template <class T, class N>
using step_rebind_t = typename stepping<T>::template rebind<N>;

// A store into a fresh B through store(b, policy) under B's flags F with an
// errc& policy: b, or the error it reported.
template <class B, policy_flag F, class Store>
constexpr std::expected<B, errc> try_store(Store store) {
    errc ec{};
    B    b{};
    store(b, make_policy<F>(ec));
    if (ec != errc{})
        return std::unexpected{ec};
    return b;
}

// The grids of the counts and exact parts an inside reports at run time, as
// insides: a grid number past 64 bits exists only at compile time, an inside
// holds any value of its grid in a raw sized from it.
// index_grid: the notch indices 0 … step_count (a point's is the point 0).
consteval grid index_grid(const grid& g) {
    const grid_int s = g.step_count();
    if (s == grid_int{0})
        return grid{grid_rational{0}};
    return grid{interval{grid_rational{0}, grid_rational{s}}, grid_rational{1}};
}

// numerator_grid / denominator_grid: the parts of g's values in lowest
// terms. Every denominator d is at most D = den_bound(g), so a numerator
// v·d lies in [⌈Lower⌉, …] for Lower ≥ 0, else [⌈Lower·D⌉, …], and up to
// ⌊Upper⌋ for Upper ≤ 0, else ⌊Upper·D⌋. On an integer lattice (D = 1) that
// is g's interval. A point's parts are points.
consteval grid denominator_grid(const grid& g) {
    if (g.Interval.Lower == g.Interval.Upper)
        return grid{grid_rational{grid_denominator(g.Interval.Lower)}};
    const grid_int d = den_bound(g);
    if (d == grid_int{1})
        return grid{grid_rational{1}};
    return grid{interval{grid_rational{1}, grid_rational{d}}, grid_rational{1}};
}
consteval grid numerator_grid(const grid& g) {
    const grid_rational& lo = g.Interval.Lower;
    const grid_rational& hi = g.Interval.Upper;
    if (lo == hi)
        return grid{grid_rational{grid_numerator(lo)}};
    const grid_rational d{den_bound(g)};
    const grid_rational a = grid_round<round_mode::ceil>(lo >= 0 ? lo : lo * d);
    const grid_rational b = grid_round<round_mode::floor>(hi <= 0 ? hi : hi * d);
    return grid{interval{a, b}, grid_rational{1}};
}

// Whether I's raw is its value, an integer: a value raw, or an index raw
// counting from 0 in steps of 1.
template <insidable I>
inline constexpr bool raw_holds_value =
    integer_value_storage<I> || (index_storage<I> && !point_storage<I> && lower_of<I> == 0 && notch_of<I> == 1);

// The inside of N (a notch-1 integer grid) that holds the integer n, which
// the caller proves lies in N: n itself on a value raw, n − Lower on an
// index raw. (Every caller returns a point result itself.)
template <insidable N, typename I>
constexpr N integer_inside(const I& n) noexcept {
    static_assert(!point_storage<N>);
    if constexpr (value_storage<N>)
        return N::from_raw(raw_cast<N>(n));
    else if constexpr (wide_integer<I>) {
        constexpr I lo = static_cast<I>(grid_numerator(lower_of<N>));
        return N::from_raw(raw_cast<N>(n - lo));
    } else
        return N::from_raw(raw_cast<N>(static_cast<std::uint64_t>(n) - static_cast<std::uint64_t>(lower_int64<N>)));
}

// stored<T>(v): v stored into the inside T under T's policy, past the
// constructors' gate - the library's own store of a value whose fit it knows
// by other means (a math result on its output grid, an index within range,
// raw memory read by an accessor). A failure goes to T's policy: the error
// handler, as a precondition.
template <insidable T, policy_flag F = flags_of<T>, typename V>
[[nodiscard]] constexpr T stored(const V& v) {
    T r;
    assignment<T, V>::assign(r, v, make_policy<F>());
    return r;
}
} // namespace detail

//---------------------------------------------------------------------------
// inside
//---------------------------------------------------------------------------
template <grid G, policy_flag P>
struct inside {
    static_assert(grid::validate<G>());
    static_assert((P & ~detail::known_bits) == 0,
                  "inside: the policy holds bits no flag names - build it from the flags (clamp | round_floor)");
    static_assert(detail::rounding_modes(P) <= 1,
                  "inside: the policy names two rounding modes - pick one (round_nearest, round_floor, round_ceil or "
                  "round_half_even)");
    static_assert(!(P & clamp) || !(P & wrap), "clamp and wrap are mutually exclusive");

    using negated_type = inside<-G, P & ~detail::cursor_marker>; // −cursor is no cursor
    using raw_type     = detail::raw_for_t<G>;

  private:
    [[no_unique_address]] raw_type Raw; // empty for a point grid

  public:
    // raw() — access escape hatch, symmetric with `from_raw`. Read overload
    // under every policy (read-only C interop: `&std::as_const(b).raw()`). The
    // mutable overload is gated to `unsafe` — only an inside that has opted out of
    // every check can honestly hand out a writable storage handle; writing an
    // out-of-range raw elsewhere would make conversions lie, so it's a compile error.
    [[nodiscard]] constexpr const raw_type& raw() const noexcept { return Raw; }
    [[nodiscard]] constexpr raw_type&       raw() noexcept
        requires(has_flag(P, unsafe))
    {
        return Raw;
    }

    // A default-constructed inside is 0, or Lower on a grid without 0 — a
    // value of the grid, as every other inside holds.
    constexpr inside()
        requires(!has_flag(P, detail::cursor_marker))
        : Raw{detail::default_raw<inside>} {}

    // A cursor starts at Lower: slot 0.
    constexpr inside()
        requires(has_flag(P, detail::cursor_marker))
        : Raw{detail::raw_from_offset<inside>(std::uint64_t{0})} {}

  private:
    // from_raw's: the raw as given, without a default value first.
    constexpr inside(detail::raw_tag, raw_type r) noexcept : Raw{r} {}

    // The one store every constructor and assignment goes through — the same
    // for every raw kind.
    template <detail::numeric A, typename Pol>
    constexpr void store_value(const A& value, Pol&& pol) {
        detail::assignment<inside, A>::assign(*this, value, pol);
    }

    template <detail::numeric A>
    constexpr void store_value(const A& value) {
        store_value(value, make_policy<P>());
    }

  public:
    // (Raw starts at the default value: a store that fails leaves its target
    // unchanged, and a store that rounds a fine double starts from the
    // destination's value, which a constant expression may not read
    // uninitialized. The optimizer drops the dead store.)
    // A store the grids prove (detail::store_proven) runs at run time. A
    // plain number they cannot prove (an int past the interval, a double, a
    // rational) must be a constant: the store is checked while compiling, and
    // a run-time value does not compile - store it with try_make, a policy
    // with an errc&, clamp, wrap or unsafe. An inside they cannot prove is
    // deleted below (BEMAN_INSIDE_WHY_unproven).
    template <detail::numeric A>
        requires inside_assignable<inside, A, P> && detail::store_proven<inside, A, P>
    constexpr inside(A value) : Raw{detail::default_raw<inside>} {
        store_value(value);
    }
    template <detail::numeric A>
        requires inside_assignable<inside, A, P> && (!detail::store_proven<inside, A, P>) && (!insidable<A>)
    consteval inside(A value) : Raw{detail::default_raw<inside>} {
        store_value(value);
    }

    // One-shot policy: a flag or make_policy<F>(ec) for this store. Its
    // rounding mode replaces the type's and its clamp or wrap the type's range
    // handling (detail::override_flags): T{v, clamp | round_ceil} saturates
    // and rounds up whatever T declares. The store runs on the twin of this
    // type under those flags (the same grid, so the raw carries over). If it
    // reports an error (ec mode), the inside keeps its default value.
    template <detail::numeric A, typename Pol>
        requires(detail::flag_like<Pol> || detail::policy_like<Pol>) &&
                inside_assignable<inside<G, detail::override_flags(P, flags_of<Pol>)>,
                                  A,
                                  detail::override_flags(P, flags_of<Pol>)> &&
                (detail::store_proven<inside<G, detail::override_flags(P, flags_of<Pol>)>,
                                      A,
                                      detail::override_flags(P, flags_of<Pol>)> ||
                 detail::reports_into_errc<Pol>)
    constexpr inside(A value, Pol&& pol) : Raw{detail::default_raw<inside>} {
        store_with(value, pol);
    }
    template <detail::numeric A, typename Pol>
        requires(detail::flag_like<Pol> || detail::policy_like<Pol>) &&
                inside_assignable<inside<G, detail::override_flags(P, flags_of<Pol>)>,
                                  A,
                                  detail::override_flags(P, flags_of<Pol>)> &&
                (!detail::store_proven<inside<G, detail::override_flags(P, flags_of<Pol>)>,
                                       A,
                                       detail::override_flags(P, flags_of<Pol>)> &&
                 !detail::reports_into_errc<Pol> && !insidable<A>)
    consteval inside(A value, Pol&& pol) : Raw{detail::default_raw<inside>} {
        store_with(value, pol);
    }
    template <insidable A, typename Pol>
        requires(detail::flag_like<Pol> || detail::policy_like<Pol>) &&
                    inside_assignable<inside<G, detail::override_flags(P, flags_of<Pol>)>,
                                      A,
                                      detail::override_flags(P, flags_of<Pol>)> &&
                    (!detail::store_proven<inside<G, detail::override_flags(P, flags_of<Pol>)>,
                                           A,
                                           detail::override_flags(P, flags_of<Pol>)> &&
                     !detail::reports_into_errc<Pol>)
    inside(A, Pol&&) = delete ("inside: the source's values may fall outside this grid, or off its notch, at run "
                               "time - store with clamp, wrap or a rounding mode, report with T::try_make(v) or "
                               "make_policy<F>(ec), or unsafe");

  private:
    // The one-shot store of the policy constructors: on the twin of this type
    // under the call's flags (the same grid, so the raw carries over).
    template <detail::numeric A, typename Pol>
    constexpr void store_with(const A& value, Pol& pol) {
        constexpr policy_flag O = detail::override_flags(P, flags_of<Pol>);
        inside<G, O>          twin;
        detail::assignment<inside<G, O>, A>::assign(twin, value, detail::with_flags<O>(pol));
        Raw = twin.raw();
    }

  public:
    // No error-code constructor: construction that can fail is `try_make(value)`,
    // which returns expected<inside, errc>. This overload only turns
    // `inside x(value, ec)` into a readable error (an errc& would otherwise bind
    // the Pol&& constructor above).
    template <detail::numeric A>
    inside(A, errc&) = delete ("inside(value, errc&) was removed: use `auto r = B::try_make(value);` "
                               "(expected<B, errc>), or `b.policy(ec) = value` to assign with an error code");

    // A std::expected is never stored implicitly: check it and store *e, or
    // use try_make(e), which passes its error on. Construction, assignment and
    // the compound assignments from an expected are deleted with that reason.
#define BEMAN_INSIDE_WHY_expected                                                                       \
    "inside: a std::expected is not stored implicitly - check it and store *e, or use T::try_make(e), " \
    "which passes its error on"
    template <detail::numeric A>
    inside(const std::expected<A, errc>&) = delete (BEMAN_INSIDE_WHY_expected);
    template <detail::numeric A>
    inside& operator=(const std::expected<A, errc>&) = delete (BEMAN_INSIDE_WHY_expected);
    template <detail::numeric A>
    inside& operator+=(const std::expected<A, errc>&) = delete (BEMAN_INSIDE_WHY_expected);
    template <detail::numeric A>
    inside& operator-=(const std::expected<A, errc>&) = delete (BEMAN_INSIDE_WHY_expected);
    template <detail::numeric A>
    inside& operator*=(const std::expected<A, errc>&) = delete (BEMAN_INSIDE_WHY_expected);
    template <detail::numeric A>
    inside& operator/=(const std::expected<A, errc>&) = delete (BEMAN_INSIDE_WHY_expected);
    template <detail::numeric A>
    inside& operator%=(const std::expected<A, errc>&) = delete (BEMAN_INSIDE_WHY_expected);
#undef BEMAN_INSIDE_WHY_expected

    // A grid number, read at run time (`residue r = N - 1;`: N - 1 is formed
    // during compilation): checked like any store, since its interval is not
    // part of its type. (Exactly a grid number: templates, so nothing
    // converts to one.)
    template <std::same_as<detail::grid_rational> V>
    consteval inside(const V& value) : Raw{detail::default_raw<inside>} {
        detail::assign_grid_number(*this, value, make_policy<P>());
    }
    template <std::same_as<detail::grid_rational> V, class Pol>
        requires(detail::policy_like<Pol> || detail::flag_like<Pol>) && detail::reports_into_errc<Pol>
    constexpr inside(const V& value, Pol&& pol) : Raw{detail::default_raw<inside>} {
        store_grid_number_with(value, pol);
    }
    template <std::same_as<detail::grid_rational> V, class Pol>
        requires(detail::policy_like<Pol> || detail::flag_like<Pol>) && (!detail::reports_into_errc<Pol>)
    consteval inside(const V& value, Pol&& pol) : Raw{detail::default_raw<inside>} {
        store_grid_number_with(value, pol);
    }
    // (`x = N - 1` runs the constructor above, then the copy assignment.)

  private:
    template <class Pol>
    constexpr void store_grid_number_with(const detail::grid_rational& value, Pol& pol) {
        constexpr policy_flag O = detail::override_flags(P, flags_of<Pol>);
        inside<G, O>          twin;
        detail::assign_grid_number(twin, value, detail::with_flags<O>(pol));
        Raw = twin.raw();
    }

  public:
    // (A plain number the grids cannot prove assigns through the consteval
    // constructor and the copy assignment: a constant only.)
    template <detail::numeric B>
        requires inside_assignable<inside, B, P> && detail::store_proven<inside, B, P>
    constexpr inside& operator=(const B& other) {
        store_value(other);
        return *this;
    }

#define BEMAN_INSIDE_WHY_unproven                                                                                 \
    "inside: the source's values may fall outside this grid, or off its notch, at run time - store with clamp, "  \
    "wrap or a rounding mode (x.policy<clamp>() = v, T{v, clamp}), report with T::try_make(v), x.policy(ec) = v " \
    "or x.on_error(f) = v, or opt out with unsafe"
    template <insidable A>
        requires inside_assignable<inside, A, P> && (!detail::store_proven<inside, A, P>)
    inside(A) = delete (BEMAN_INSIDE_WHY_unproven);
    template <insidable B>
        requires inside_assignable<inside, B, P> && (!detail::store_proven<inside, B, P>)
    inside& operator=(const B&) = delete (BEMAN_INSIDE_WHY_unproven);

    // A store that can never succeed is deleted, with the clause that fails:
    // a trait or `requires` probe sees it as ill-formed, and the error names
    // the fix. explain_assignable<To, From> prints the two grids.
#define BEMAN_INSIDE_WHY_disjoint                                                                   \
    "inside: the source's interval lies entirely outside this grid's, and the policy has no clamp " \
    "or wrap - widen the grid, or store with clamp or wrap (explain_assignable<To, From> prints both)"
#define BEMAN_INSIDE_WHY_notch                                                                                    \
    "inside: incompatible notches - the source's values miss this grid's notch; use policy<round_toward_zero>() " \
    "or a rounding policy (explain_assignable<To, From> prints both notches)"
    template <detail::numeric A>
        requires(!detail::assign_intervals_ok<inside, A, P>)
    inside(A) = delete (BEMAN_INSIDE_WHY_disjoint);
    template <detail::numeric A>
        requires(detail::assign_intervals_ok<inside, A, P> && !detail::assign_notch_ok<inside, A, P>)
    inside(A) = delete (BEMAN_INSIDE_WHY_notch);
    template <detail::numeric B>
        requires(!detail::assign_intervals_ok<inside, B, P>)
    inside& operator=(const B&) = delete (BEMAN_INSIDE_WHY_disjoint);
    template <detail::numeric B>
        requires(detail::assign_intervals_ok<inside, B, P> && !detail::assign_notch_ok<inside, B, P>)
    inside& operator=(const B&) = delete (BEMAN_INSIDE_WHY_notch);
#undef BEMAN_INSIDE_WHY_disjoint
#undef BEMAN_INSIDE_WHY_notch

    // Trusted construction from a storage-layout raw — no validation; the caller
    // asserts `r` is a valid slot. Entry point for tests, fast paths, and same-grid
    // raw transfer.
    [[nodiscard]] static constexpr inside from_raw(raw_type r) noexcept { return inside{detail::raw_tag{}, r}; }
    // (An inside is no raw: an index would convert to its value and be read as one.)
    template <insidable X>
    static inside from_raw(const X&) = delete ("from_raw takes a raw, not an inside: store the inside (T{x}), or "
                                               "pass its raw (T::from_raw(x.raw()))");

    // Conversion summary:
    //   operator rational — implicit; lossless and exact.
    //   operator double   — explicit, and gated on a rounding flag (a double
    //                       may round the value).
    //                       A strict inside opts in via `to<double>().value()`.
    //   to<T>()           — narrowing/widening: T where the grid proves it total,
    //                       else `expected<T, errc>` (overflow / domain_error).
    //   as<T>()           — non-expected sibling; throws on error. For known-
    //                       in-range sites (array indexing: `vec[b.as<std::size_t>()]`);
    //                       no check is left where T holds the whole grid.
    //   to<T>(b)/as<T>(b) — free-function forms, for generic code.
    // No implicit integer conversion: it would narrow silently into a smaller
    // built-in (`int i = b;`, `unsigned char c = b;`), the range escaping
    // the type. The exception is an index, an inside whose every value is a
    // non-negative integer (inside<{0, 479}>, inside<{3, 11}>): it converts
    // to each builtin integer that holds its whole grid, which is what
    // std::mdspan, std::array, std::span, std::vector and builtin arrays ask
    // of an index (`m[i, j]`, `a[i]`). Never narrowing, and never to bool, a
    // character or a floating type: those are deleted, as they would
    // otherwise follow from the integer by a standard conversion.
    // (std::size_t is no template: a builtin subscript takes no other.)
    constexpr operator std::size_t() const noexcept
        requires(detail::index_converts<inside, std::size_t>)
    {
        return static_cast<std::size_t>(detail::to_value(*this));
    }
    template <detail::index_integer T>
        requires(!std::same_as<T, std::size_t> && detail::index_converts<inside, T>)
    constexpr operator T() const noexcept {
        return static_cast<T>(detail::to_value(*this));
    }
    template <class T>
        requires(std::is_arithmetic_v<T> && detail::index_converts<inside, std::size_t> &&
                 !detail::index_converts<inside, T>)
    operator T() const = delete ("inside: an index converts to the builtin integers that hold its whole grid - "
                                 "not to a smaller one, bool, a character or a floating type (x.to<T>(), x != 0)");

    constexpr explicit operator double() const
        requires(has_flag(P, detail::snap))
    {
        return detail::as_double(*this);
    }

    // Unavailable on a wide-index grid: its values outgrow the 64-bit rational
    // (compare it, or read it with to<T>()).
    constexpr operator detail::rational() const
        requires(detail::rational_readable<inside>)
    {
        if constexpr (G.Interval.Lower == G.Interval.Upper)
            return G.Interval.Lower;
        else if constexpr (detail::value_storage<inside>)
            return Raw;
        // Q-format-with-integer-Lower fast path skips the generic path's three
        // rational ops. Falls through to the rational path when a value times
        // the notch's denominator could pass int64 (e.g. a Q16.16 × Q16.16 result).
        else if constexpr (detail::q_format_fits<inside>)
            return detail::q_format_decode(*this);
        else {
            // Raw·Notch can pass 64 bits on the way to a value that fits.
            const auto scaled = detail::try_mul(rational{Raw}, detail::notch64<inside>);
            const auto value  = scaled ? detail::try_add(*scaled, detail::lower64<inside>) : scaled;
            if (value) [[likely]]
                return *value;
            if (const auto exact = detail::try_rational(detail::exact_of(*this)))
                return *exact;
            // (Not reached for a rational_readable grid; reported, as any
            // overflow, rather than a bad_expected_access.)
            detail::raise(errc::overflow, "inside: value past the 64-bit rationals");
        }
    }

  private:
    // to<T, Round>() cannot fail: the grid lies within T's limits, and every
    // value is an integer or the call may round.
    template <std::integral T, policy_flag Round>
    static constexpr bool total_to =
        !(lower_of<inside> < detail::grid_rational{std::numeric_limits<T>::min()}) &&
        !(upper_of<inside> > detail::grid_rational{std::numeric_limits<T>::max()}) &&
        (detail::integer_lattice<inside> || Round != detail::none || has_flag(P, detail::snap));

  public:
    // to<T>() — scalar extraction (mirrors rational::to<T>, extended to signed
    // and floating point). Like `/`, the type says whether it can fail: a plain
    // T where the grid proves the conversion total (total_to, and every
    // floating T), else std::expected<T, errc> with `errc::overflow` (out of
    // T's range; `errc::domain_error` for a negative value into unsigned T)
    // or `errc::rounding_error`. Rounding needs permission, as for a store: a
    // fractional value rounds by Round, else by this type's rounding mode.
    // Bounds the grid already satisfies are not checked.
    template <std::integral T, policy_flag Round = detail::none>
        requires(Round == detail::none || Round == round_toward_zero || Round == round_nearest ||
                 Round == round_floor || Round == round_ceil || Round == round_half_even)
    [[nodiscard]] constexpr std::conditional_t<total_to<T, Round>, T, std::expected<T, errc>> to() const {
        using lim                             = std::numeric_limits<T>;
        constexpr bool               check_lo = lower_of<inside> < detail::grid_rational{lim::min()};
        constexpr bool               check_hi = upper_of<inside> > detail::grid_rational{lim::max()};
        constexpr bool               rounds   = Round != detail::none || has_flag(P, detail::snap);
        constexpr detail::round_mode mode     = detail::rounding_of(Round != detail::none ? Round : P);
        if constexpr (detail::integer_lattice<inside> && !check_lo && !check_hi && detail::values_fit_int64<inside>)
            return static_cast<T>(detail::to_value(*this));
        else if constexpr (detail::wide_valued<inside>) { // round first: -0.5 → unsigned is 0 when truncating
            const auto f = detail::exact_of(*this);
            const auto q = detail::rounded_div_exact<mode>(f.Num, f.Den);
            if constexpr (!rounds && !detail::integer_lattice<inside>)
                if (!q.Exact)
                    return std::unexpected{errc::rounding_error};
            const auto t = q.Quot;
            if constexpr (check_lo)
                if (t < decltype(t){lim::min()})
                    return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
            if constexpr (check_hi)
                if (t > decltype(t){lim::max()})
                    return std::unexpected{errc::overflow};
            return static_cast<T>(t);
        } else {
            const auto v = detail::as_rational(*this);
            const auto t = detail::round_to_integral(v, mode);
            if constexpr (!rounds && !detail::integer_lattice<inside>)
                if (t != v)
                    return std::unexpected{errc::rounding_error};
            if constexpr (check_lo)
                if (t < detail::rational{lim::min()})
                    return std::unexpected{std::unsigned_integral<T> ? errc::domain_error : errc::overflow};
            if constexpr (check_hi)
                if (t > detail::rational{lim::max()})
                    return std::unexpected{errc::overflow};
            if constexpr (detail::wide_builtin<T>) // a value to 2^64 − 1: past int64
                return static_cast<T>(t);
            else
                return static_cast<T>(detail::signed_numerator(t));
        }
    }

    // Rounded once to T's digits: a floating point wider than double (long
    // double) from the exact value; a narrower one (float) through the double,
    // unless that double is a tie between two Ts. Total: a plain T.
    template <std::floating_point T>
    [[nodiscard]] constexpr T to() const {
        if constexpr (detail::wide_float<T>)
            return detail::nearest_float<T>(detail::exact_of(*this));
        else if constexpr (sizeof(T) < sizeof(double)) {
            const double d = detail::as_double(*this);
            if (detail::narrow_midpoint<T>(d)) [[unlikely]]
                return detail::nearest_float<T>(detail::exact_of(*this));
            return static_cast<T>(d);
        } else
            return static_cast<T>(detail::as_double(*this));
    }

  private:
    // to<T>() as as<T>() reads it: with the call's rounding for an integer T.
    template <typename T, policy_flag Round>
    constexpr auto to_for_as() const {
        if constexpr (std::integral<T>)
            return to<T, Round>();
        else
            return to<T>();
    }
    template <typename T, policy_flag Round>
    static constexpr bool as_total =
        !detail::expected_like<decltype(std::declval<const inside&>().template to_for_as<T, Round>())>;

  public:
    // as<T>() — to<T>() where the grid proves the conversion total: T itself,
    // for known-in-range sites (array indexing, capacity arithmetic). Where a
    // value may not convert, to<T>() returns a std::expected and as<T>() is
    // deleted. FP targets share operator double's policy gate, so a strict
    // inside rejects `b.as<double>()` too.
    template <typename T, policy_flag Round = detail::none>
        requires(std::integral<T> || (std::floating_point<T> && Round == detail::none)) &&
                (!std::floating_point<T> || has_flag(P, detail::snap)) && as_total<T, Round>
    [[nodiscard]] constexpr T as() const {
        return to_for_as<T, Round>();
    }
    template <typename T, policy_flag Round = detail::none>
        requires(std::integral<T> || (std::floating_point<T> && Round == detail::none)) &&
                    (!std::floating_point<T> || has_flag(P, detail::snap)) && (!as_total<T, Round>)
    T as() const = delete ("inside: as<T>() takes a conversion the grid proves total - here a value may not fit T "
                           "(or would need rounding): use to<T>(), which returns a std::expected, or to<T, "
                           "round_floor>() and the like");

    // numerator() / denominator() — the exact value in lowest terms (sign on
    // the numerator, denominator positive), each an inside on the integers
    // that holds every such part of this grid's values (numerator_grid,
    // denominator_grid): 3 and 16 for 0.1875 on {{0, 1}, per<16>}. On an
    // integer lattice the numerator is the value and the denominator the
    // point 1. Read a builtin with .as<T>() or .to<T>().
    [[nodiscard]] constexpr auto numerator() const {
        using N = inside<detail::numerator_grid(G)>;
        if constexpr (detail::point_storage<N>)
            return N::from_raw({});
        else if constexpr (detail::numerator_grid(G) == G)
            return N::from_raw(Raw); // the value itself, on the same grid
        else if constexpr (detail::fraction_fits_int64<inside>)
            return detail::integer_inside<N>(fraction().first);
        else
            return detail::integer_inside<N>(detail::reduced_exact(*this).Num);
    }
    [[nodiscard]] constexpr auto denominator() const {
        using D = inside<detail::denominator_grid(G)>;
        if constexpr (detail::point_storage<D>)
            return D::from_raw({});
        else if constexpr (detail::fraction_fits_int64<inside>)
            return detail::integer_inside<D>(fraction().second);
        else
            return detail::integer_inside<D>(detail::reduced_exact(*this).Den);
    }

    // index() — the notch index (value − Lower)/Notch, 0 … step_count_of<T>,
    // as an index_t<T>: run-time counting on any grid, past 2^64 points too.
    // Its raw has this raw's type, so on an index raw it is a copy; on a value
    // raw, one subtraction. T::from_index(i) goes back.
    [[nodiscard]] constexpr auto index() const noexcept
        requires(G.Notch != 0 || G.Interval.Lower == G.Interval.Upper)
    {
        using I = inside<detail::index_grid(G)>;
        if constexpr (detail::point_storage<inside>)
            return I::from_raw({});
        else if constexpr (detail::index_storage<inside>) {
            static_assert(std::same_as<typename I::raw_type, raw_type>);
            return I::from_raw(Raw);
        } else
            return I::from_raw(detail::raw_cast<I>(detail::raw_offset<inside>(Raw)));
    }

#define BEMAN_INSIDE_WHY_index "inside: index() counts notches - a continuous grid has none to count"
    auto index() const
        requires(G.Notch == 0 && G.Interval.Lower != G.Interval.Upper)
    = delete (BEMAN_INSIDE_WHY_index);
#undef BEMAN_INSIDE_WHY_index

  private:
    // Lower + i·Notch, exactly, as an inside (a point counts in steps of 1,
    // so only index 0 lands on it).
    template <insidable I>
    static constexpr auto index_value(const I& i) {
        constexpr detail::grid_rational unit  = G.Notch != 0 ? G.Notch : detail::grid_rational{1};
        constexpr auto                  lower = inside<grid{G.Interval.Lower}>::from_raw({});
        constexpr auto                  step  = inside<grid{unit}>::from_raw({});
        return lower + i * step;
    }
    // A builtin index as the inside on its type's range.
    template <std::integral T>
    static constexpr auto as_index_inside(T n) {
        return inside<grid{interval_of<T>, detail::grid_rational{1}}>{n};
    }

  public:
    // from_index(i) — the value with notch index i, Lower + i·Notch: index()'s
    // inverse. i is an inside on the integers (or a builtin integer). Plain
    // when every value of i is an index of T (0 … step_count_of<T>), a raw
    // copy; else a store checked by T's policy, or by a one-shot policy
    // (clamp saturates at the ends, wrap folds onto the indices).
    // try_from_index reports the failure instead.
    template <insidable I>
        requires detail::integer_lattice<I> && (G.Notch != 0 || G.Interval.Lower == G.Interval.Upper)
    // (An index that may fall outside 0 … step_count: a std::expected, as
    // try_from_index.)
    [[nodiscard]] static constexpr auto from_index(const I& i) {
        constexpr bool within = lower_of<I> >= 0 && upper_of<I> <= G.step_count();
        if constexpr (!within)
            return try_from_index(i);
        else if constexpr (detail::point_storage<inside>)
            return from_raw({});
        else if constexpr (detail::index_storage<inside> && detail::raw_holds_value<I>)
            return from_raw(detail::raw_cast<inside>(i.raw()));
        else if constexpr (!detail::wide_index_storage<inside> && detail::values_fit_int64<I>)
            return from_raw(detail::raw_from_offset<inside>(detail::to_value(i)));
        else
            return detail::stored<inside>(index_value(i));
    }
    template <insidable I, typename Pol>
        requires detail::integer_lattice<I> && (G.Notch != 0 || G.Interval.Lower == G.Interval.Upper) &&
                 (detail::flag_like<Pol> || detail::policy_like<Pol>)
    [[nodiscard]] static constexpr auto from_index(const I& i, Pol&& pol) {
        using V                 = decltype(index_value(i));
        constexpr policy_flag O = detail::override_flags(P, flags_of<Pol>);
        if constexpr (detail::store_proven<inside<G, O>, V, O> || detail::reports_into_errc<Pol>)
            return inside{index_value(i), std::forward<Pol>(pol)};
        else
            return detail::try_store<inside, O>([&](inside& r, auto&& ec_pol) { r = inside{index_value(i), ec_pol}; });
    }
    template <policy_flag F = detail::none, insidable I>
        requires detail::integer_lattice<I> && (G.Notch != 0 || G.Interval.Lower == G.Interval.Upper)
    [[nodiscard]] static constexpr std::expected<inside, errc> try_from_index(const I& i) {
        return try_make<F>(index_value(i));
    }
#define BEMAN_INSIDE_WHY_from_index \
    "inside: from_index(i) takes a notch index - an inside on the integers (index_t<T>, 3_ins) or a builtin integer"
    template <insidable I>
        requires(!detail::integer_lattice<I>)
    static inside from_index(const I&) = delete (BEMAN_INSIDE_WHY_from_index);
    template <insidable I, typename Pol>
        requires(!detail::integer_lattice<I>)
    static inside from_index(const I&, Pol&&) = delete (BEMAN_INSIDE_WHY_from_index);
#undef BEMAN_INSIDE_WHY_from_index
    template <std::integral T>
        requires(G.Notch != 0 || G.Interval.Lower == G.Interval.Upper)
    [[nodiscard]] static constexpr auto from_index(T i) {
        return from_index(as_index_inside(i));
    }
    template <std::integral T, typename Pol>
        requires(G.Notch != 0 || G.Interval.Lower == G.Interval.Upper) &&
                (detail::flag_like<Pol> || detail::policy_like<Pol>)
    [[nodiscard]] static constexpr auto from_index(T i, Pol&& pol) {
        return from_index(as_index_inside(i), std::forward<Pol>(pol));
    }
    template <policy_flag F = detail::none, std::integral T>
        requires(G.Notch != 0 || G.Interval.Lower == G.Interval.Upper)
    [[nodiscard]] static constexpr std::expected<inside, errc> try_from_index(T i) {
        return try_from_index<F>(as_index_inside(i));
    }

  private:
    // The reduced exact value as {numerator, positive denominator}. Integer
    // grids need no division; dyadic Q-format grids reduce by shifting out
    // common factors of two instead of a gcd.
    constexpr std::pair<std::int64_t, std::int64_t> fraction() const
        requires detail::fraction_fits_int64<inside>
    {
        if constexpr (detail::integer_storage<inside> && detail::integer_lattice<inside>)
            return {detail::to_value(*this), 1};
        else if constexpr (detail::index_storage<inside> && detail::q_format_fits<inside> &&
                           std::has_single_bit(detail::abs_den(detail::notch64<inside>.Den))) {
            constexpr std::int64_t nd  = detail::abs_den(detail::notch64<inside>.Den);
            const std::int64_t     num = detail::raw_int64(*this) + detail::lower_int64<inside> * nd;
            const int              s =
                std::countr_zero(static_cast<std::uint64_t>(num) | static_cast<std::uint64_t>(nd)); // 0 → 0/1
            return {num >> s, nd >> s};
        } else {
            auto r = detail::as_rational(*this);
            return {signed_numerator(r), static_cast<std::int64_t>(detail::abs_den(r.Den))};
        }
    }

  public:
    // Integer reductions (floor/ceil/round/trunc) and abs live as free
    // functions in `beman::inside::math` — `beman::inside::math::floor(b)` etc. (the
    // output deduced) or `beman::inside::math::floor<Out>(b)`. There is deliberately
    // no member-syntax alias: one spelling, in `<beman/inside/math.hpp>`.

    // +x is x, as for a built-in number (and a quantity's +q).
    [[nodiscard]] constexpr inside operator+() const noexcept { return *this; }

    [[nodiscard]] constexpr negated_type operator-() const {
        negated_type neg;
        if constexpr (detail::point_storage<inside>)
            neg = negated_type::from_raw({});        // −point is a point: no raw
        else if constexpr (!detail::notched<inside>) // continuous: the raw is the value
            neg = negated_type::from_raw(-(Raw));
        else {
            // Integer raws: the negated value index is −J (wide_value.hpp), in int64
            // when the bounds allow, else by wrapping. Index storage on both sides
            // counts the slot from the opposite end instead.
            using W = detail::index_work_t<negated_type, inside, G.Notch, inside, G.Notch>;
            if constexpr (detail::index_storage<inside> && detail::index_storage<negated_type>) {
                constexpr W count = static_cast<W>(G.step_count());
                neg = negated_type::from_raw(static_cast<detail::raw_t<negated_type>>(count - static_cast<W>(Raw)));
            } else
                neg = detail::from_value_index<negated_type>(W{0} - detail::value_index<W>(*this));
        }
        return neg;
    }

    // policy<F>() — per-operation policy override. On an lvalue it returns a
    // policy_ref holding `*this` by reference (needed so `b.policy<…>() = x` writes
    // back into b, and cheap for the common immediate use). On an *rvalue* receiver
    // it returns a policy_buffer that OWNS the moved-in value, so a snapped temporary
    // survives being returned/stored (`return (a*b).policy<round_toward_zero>();`) — no dangling.
    template <policy_flag F = detail::none, typename Self>
    [[nodiscard]] constexpr auto policy(this Self&& self) {
        auto pol = make_policy<P | F>();
        if constexpr (std::is_lvalue_reference_v<Self>)
            return detail::policy_ref<inside, decltype(pol)>{self, pol, {}};
        else
            return detail::policy_buffer<inside, decltype(pol)>{std::move(self), pol, {}};
    }

    template <policy_flag F = detail::none>
    [[nodiscard]] constexpr auto policy(errc& ec) {
        auto pol = make_policy<P | F>(ec);
        return detail::policy_ref<inside, decltype(pol)>{*this, pol, {}};
    }

  private:
    // Shared builder for the single-action fluent hooks below (internal). Merges
    // the tag's implied policy flag, then returns a policy_ref bound to *this
    // carrying the tagged action. Each on_* hook is a thin wrapper that fixes the tag.
    template <template <class> class Tag, typename A>
    [[nodiscard]] constexpr auto make_action_ref(A&& action) {
        using tag = Tag<std::remove_cvref_t<A>>;
        auto pol  = make_policy<P | detail::implied_flags<tag>>();
        return detail::policy_ref<inside, decltype(pol), tag>{*this, pol, tag{std::forward<A>(action)}};
    }

  public:
    template <typename A>
    [[nodiscard]] constexpr auto on_wrap(A&& a) {
        return make_action_ref<detail::on_wrap_t>(std::forward<A>(a));
    }
    template <typename A>
    [[nodiscard]] constexpr auto on_clamp(A&& a) {
        return make_action_ref<detail::on_clamp_t>(std::forward<A>(a));
    }
    template <typename A>
    [[nodiscard]] constexpr auto on_error(A&& a) {
        return make_action_ref<detail::on_error_t>(std::forward<A>(a));
    }

  private:
    // Raw-space fast paths of += and -=. Each adds a delta to the raw: a
    // compile-time constant (point rhs), or ±rhs raw plus a constant bias.
    // With equal notches, raw(v_l ± v_r) = raw_l ± (raw_r + bias): the bias is
    // Lower/Notch of an index-raw rhs (its raw is Lower-relative), else 0.
    //   point_delta<R>   — rhs is one whole number of notches: the delta.
    //   raw_add_ok<R>    — rhs raw adds or subtracts with that bias.
    // The add runs in raw_work_t, sized from the raw and delta ranges: int64 for
    // every grid within int64, a wide_int beyond — never overflowing.
    template <insidable R>
    static constexpr bool point_delta_ok =
        detail::notched<inside> && detail::point_grid<R> && detail::grid_divides_evenly(lower_of<R>, notch_of<inside>);

    // (R anchored: its Lower is a whole number of notches, the bias.)
    template <insidable R>
    static constexpr bool raw_add_ok = detail::notched<inside> && notch_of<inside> == notch_of<R> &&
                                       !detail::point_storage<R> && detail::anchored<R>; // ⇒ integer raws

    template <insidable R>
    static constexpr detail::grid_int point_delta = detail::exact_quotient(lower_of<R>, notch_of<inside>);
    template <insidable R>
    static constexpr detail::grid_int add_bias = [] {
        if constexpr (detail::index_storage<R>)
            return detail::slot_base<R>;
        else
            return detail::grid_int{0};
    }();

    // Work type of a raw-space add whose delta lies in [Dlo, Dhi]: it holds
    // every raw of this grid, the delta, the new raw (in [raw_lo + Dlo,
    // raw_hi + Dhi]), the wrap range raw_hi − raw_lo + 1 and the new raw's
    // offset from raw_lo (wrap divides it), so the add cannot overflow. int64
    // for every grid within int64.
    template <detail::grid_int Dlo, detail::grid_int Dhi>
    static constexpr int raw_work_bits = [] {
        using W        = detail::grid_int;
        constexpr W lo = detail::raw_lo_exact<inside>, hi = detail::raw_hi_exact<inside>;
        return detail::signed_value_bits({lo, hi, Dlo, Dhi, lo + Dlo, hi + Dhi, hi - lo + W{1}, hi - lo + Dhi});
    }();
    template <detail::grid_int Dlo, detail::grid_int Dhi>
    using raw_work_t = detail::work_int_t<raw_work_bits<Dlo, Dhi>>;

    // Work type of the raw delta ±(rhs.raw() + bias) itself: it holds R's raws,
    // their negations and the bias, so neither step overflows before the delta
    // (in raw_work_t's range) is formed.
    template <insidable R>
    using delta_work_t = detail::work_int_t<detail::signed_value_bits({detail::raw_lo_exact<R>,
                                                                       detail::raw_hi_exact<R>,
                                                                       -detail::raw_lo_exact<R>,
                                                                       -detail::raw_hi_exact<R>,
                                                                       add_bias<R>,
                                                                       -add_bias<R>,
                                                                       detail::raw_lo_exact<R> + add_bias<R>,
                                                                       detail::raw_hi_exact<R> + add_bias<R>,
                                                                       -detail::raw_lo_exact<R> - add_bias<R>,
                                                                       -detail::raw_hi_exact<R> - add_bias<R>})>;

    // A compound op's constraint: its binary op's result (an expected's
    // value) assigns to this type, so `requires { x op= y; }` is honest.
    template <typename Result>
    static constexpr bool compound_assignable = inside_assignable<inside, detail::unwrap_t<Result>, P>;
    // ... and the grids prove the store back (detail::store_proven): the
    // result's values fit this grid, or the policy clamps, wraps or is unsafe.
    template <typename Result>
    static constexpr bool compound_proven = detail::store_proven<inside, detail::unwrap_t<Result>, P>;
    // A divisor whose grid holds 0 under a checked policy: /= and %= need a
    // channel for it (policy(ec), on_error), or unsafe on either operand.
    template <insidable R>
    static constexpr bool zero_divisor_possible =
        detail::is_checked(P | flags_of<R>) && grid_of<R>.representable(detail::grid_rational{0});

    template <insidable R>
    using sum_t = decltype(std::declval<const inside&>() + std::declval<const R&>());
    template <insidable R>
    using difference_t = decltype(std::declval<const inside&>() - std::declval<const R&>());
    template <insidable R>
    using product_t = decltype(std::declval<const inside&>() * std::declval<const R&>());
    template <insidable R>
    using quotient_t = decltype(std::declval<const inside&>() / std::declval<const R&>());
    template <insidable R>
    using remainder_t = decltype(mod(std::declval<const inside&>(), std::declval<const R&>(), make_policy<P>()));

  public:
    template <insidable R>
        requires(point_delta_ok<R> || raw_add_ok<R> || compound_assignable<sum_t<R>>) && compound_proven<sum_t<R>>
    constexpr inside& operator+=(const R& rhs) {
        // Point-inside rhs (just<v> / 1_ins / ++) whose value is a whole number of
        // this grid's notches: the raw delta is a compile-time constant and the
        // raw encoding cancels every Lower term (raw(v+d) = raw(v) + d/Notch for
        // index and value raws alike), so this compiles to one integer add.
        if constexpr (point_delta_ok<R>) {
            using W           = raw_work_t<point_delta<R>, point_delta<R>>;
            constexpr W delta = static_cast<W>(point_delta<R>);
            return store_raw<W>(static_cast<W>(Raw) + delta);
        } else if constexpr (raw_add_ok<R>) {
            using W = raw_work_t<detail::raw_lo_exact<R> + add_bias<R>, detail::raw_hi_exact<R> + add_bias<R>>;
            using D = delta_work_t<R>;
            constexpr D bias = static_cast<D>(add_bias<R>);
            return store_raw<W>(static_cast<W>(Raw) + static_cast<W>(static_cast<D>(rhs.raw()) + bias));
        } else
            return assign_op_result(*this + rhs);
    }

  private:
    // Store a raw computed by the raw-space fast paths of += and -=, in their
    // work type W. Under clamp/wrap/checked an out-of-range raw is clamped,
    // wrapped or reported.
    template <detail::raw_integer W>
    constexpr inside& store_raw(W new_raw) {
        constexpr W lo = static_cast<W>(detail::raw_lo_exact<inside>);
        constexpr W hi = static_cast<W>(detail::raw_hi_exact<inside>);
        if constexpr (detail::range_handled(P))
            if (new_raw < lo || new_raw > hi) {
                if constexpr (P & clamp)
                    new_raw = new_raw < lo ? lo : hi;
                else if constexpr (P & wrap)
                    new_raw = lo + detail::floor_divmod(new_raw - lo, hi - lo + W{1}).Rem;
                else {
                    make_policy<P>().report(errc::overflow);
                    return *this;
                }
            }
        Raw = static_cast<raw_type>(new_raw);
        return *this;
    }

    //-----------------------------------------------------------------------
    // Compound assignment private helpers — extracted to keep each
    // `operator*=` body focused on its own arithmetic shape.
    //-----------------------------------------------------------------------
    // An expected result (/ and % by a grid that holds 0) holds a value here:
    // its only error is the zero divisor, which /= and %= report first.
    template <typename Result>
    constexpr inside& assign_op_result(const Result& r) {
        if constexpr (detail::expected_like<Result>)
            *this = *r;
        else
            *this = r;
        return *this;
    }

  public:
    // A rational has no grid, like an int or a double: a compound assign
    // takes an inside (`x += 0.5_ins`, `x += just<1, 2>`), as the binary
    // operators do (arithmetic.hpp deletes inside ⋈ rational the same way).
#define BEMAN_INSIDE_NO_RATIONAL_ASSIGN(op)                                                                     \
    template <std::same_as<detail::rational> A>                                                                 \
    inside& operator op(const A&) = delete ("an inside cannot be combined with a rational: write the constant " \
                                            "as just<1, 2> or 0.5_ins (a grid), or store the rational into an " \
                                            "inside first");
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(+=)
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(-=)
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(*=)
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(/=)
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(%=)
#undef BEMAN_INSIDE_NO_RATIONAL_ASSIGN

    template <insidable R>
        requires(raw_add_ok<R> || compound_assignable<difference_t<R>>) && compound_proven<difference_t<R>>
    constexpr inside& operator-=(const R& rhs) {
        // Raw-space fast path, the subtraction mirror of +='s.
        if constexpr (raw_add_ok<R>) {
            using W = raw_work_t<-detail::raw_hi_exact<R> - add_bias<R>, -detail::raw_lo_exact<R> - add_bias<R>>;
            using D = delta_work_t<R>;
            constexpr D bias = static_cast<D>(add_bias<R>);
            return store_raw<W>(static_cast<W>(Raw) + static_cast<W>(-static_cast<D>(rhs.raw()) - bias));
        } else
            return *this += (-rhs);
    }

    template <insidable R>
        requires compound_assignable<product_t<R>> && compound_proven<product_t<R>>
    constexpr inside& operator*=(const R& rhs) {
        return assign_op_result(*this * rhs);
    }

    // The outer zero check is semantic, not redundant: the binary `a / b`
    // yields an error value on a zero divisor (expected vocabulary), so the compound
    // form's report comes from here. (Measured perf-neutral to remove.)
    // (Under unsafe on either operand a zero divisor is a precondition: /= 0
    // leaves the value.)
    template <insidable R>
        requires compound_assignable<quotient_t<R>> && compound_proven<quotient_t<R>> && (!zero_divisor_possible<R>)
    constexpr inside& operator/=(const R& rhs) {
        if (rhs == 0)
            return *this;
        return assign_op_result(*this / rhs);
    }

    template <insidable R>
        requires compound_assignable<remainder_t<R>> && compound_proven<remainder_t<R>> && (!zero_divisor_possible<R>)
    constexpr inside& operator%=(const R& rhs) {
        if (rhs == 0)
            return *this;
        return assign_op_result(mod(*this, rhs, make_policy<P>()));
    }

    // A compound assignment the grids cannot prove is deleted with the fix.
    template <insidable R>
        requires(point_delta_ok<R> || raw_add_ok<R> || compound_assignable<sum_t<R>>) && (!compound_proven<sum_t<R>>)
    inside& operator+=(const R&) = delete (BEMAN_INSIDE_WHY_unproven);
    template <insidable R>
        requires(raw_add_ok<R> || compound_assignable<difference_t<R>>) && (!compound_proven<difference_t<R>>)
    inside& operator-=(const R&) = delete (BEMAN_INSIDE_WHY_unproven);
    template <insidable R>
        requires compound_assignable<product_t<R>> && (!compound_proven<product_t<R>>)
    inside& operator*=(const R&) = delete (BEMAN_INSIDE_WHY_unproven);
    template <insidable R>
        requires compound_assignable<quotient_t<R>> && (!compound_proven<quotient_t<R>>)
    inside& operator/=(const R&) = delete (BEMAN_INSIDE_WHY_unproven);
    template <insidable R>
        requires compound_assignable<remainder_t<R>> && (!compound_proven<remainder_t<R>>)
    inside& operator%=(const R&) = delete (BEMAN_INSIDE_WHY_unproven);
#define BEMAN_INSIDE_WHY_zero                                                                                   \
    "inside: the divisor's grid holds 0 - report it with x.policy(ec) /= d or x.on_error(f) /= d, divide with " \
    "div(x, d) (a std::expected), or give the divisor a grid without 0"
    template <insidable R>
        requires compound_assignable<quotient_t<R>> && compound_proven<quotient_t<R>> && zero_divisor_possible<R>
    inside& operator/=(const R&) = delete (BEMAN_INSIDE_WHY_zero);
    template <insidable R>
        requires compound_assignable<remainder_t<R>> && compound_proven<remainder_t<R>> && zero_divisor_possible<R>
    inside& operator%=(const R&) = delete (BEMAN_INSIDE_WHY_zero);
#undef BEMAN_INSIDE_WHY_zero

    // ++/-- move one notch: they add the point inside of ±Notch through the
    // insidable += (which has the raw-level integer fast path) instead of the
    // rational round-trip (~30× the instructions on an integer grid). On a
    // notch-1 grid that is ±1; on a cursor, one step. `just` itself is declared
    // after the class, so spell the point inside directly.
    //
    // ++ and -- store like +=: on a checked grid without clamp or wrap the
    // step may leave the interval, so they are deleted there (step with
    // x.policy(ec) += 1_ins, or iterate with a cursor). A cursor steps one
    // past its last value to its end; stepping past the end is a
    // precondition, reported to the error handler.
  private:
    using up_step_t                 = inside<grid{G.Notch}>;
    using down_step_t               = inside<grid{-G.Notch}>;
    static constexpr bool is_cursor = has_flag(P, detail::cursor_marker);
    template <class S>
    static constexpr bool step_proven = is_cursor || compound_proven<sum_t<S>>;

    // The raw-space store of one ±notch step (a cursor's precondition check).
    template <insidable S>
    constexpr inside& step_by() {
        using W           = raw_work_t<point_delta<S>, point_delta<S>>;
        constexpr W delta = static_cast<W>(point_delta<S>);
        return store_raw<W>(static_cast<W>(Raw) + delta);
    }

  public:
    constexpr inside& operator++()
        requires(G.Notch != 0 && step_proven<up_step_t>)
    {
        if constexpr (is_cursor)
            return step_by<up_step_t>();
        else {
            // constexpr local: the point inside is materialized at compile time.
            constexpr auto notch_step = up_step_t::from_raw({});
            return *this += notch_step;
        }
    }
    constexpr inside operator++(int)
        requires(G.Notch != 0 && step_proven<up_step_t>)
    {
        inside t = *this;
        ++*this;
        return t;
    }
    constexpr inside& operator--()
        requires(G.Notch != 0 && step_proven<down_step_t>)
    {
        if constexpr (is_cursor)
            return step_by<down_step_t>();
        else {
            constexpr auto notch_step = down_step_t::from_raw({});
            return *this += notch_step;
        }
    }
    constexpr inside operator--(int)
        requires(G.Notch != 0 && step_proven<down_step_t>)
    {
        inside t = *this;
        --*this;
        return t;
    }
#define BEMAN_INSIDE_WHY_step_out                                                                                   \
    "inside: ++ and -- may step out of this grid - use a clamp or wrap type, x.policy(ec) += 1_ins, x.on_error(f) " \
    "+= 1_ins, or iterate with a cursor"
    inside& operator++()
        requires(G.Notch != 0 && !step_proven<up_step_t>)
    = delete (BEMAN_INSIDE_WHY_step_out);
    inside operator++(int)
        requires(G.Notch != 0 && !step_proven<up_step_t>)
    = delete (BEMAN_INSIDE_WHY_step_out);
    inside& operator--()
        requires(G.Notch != 0 && !step_proven<down_step_t>)
    = delete (BEMAN_INSIDE_WHY_step_out);
    inside operator--(int)
        requires(G.Notch != 0 && !step_proven<down_step_t>)
    = delete (BEMAN_INSIDE_WHY_step_out);
#undef BEMAN_INSIDE_WHY_step_out
#define BEMAN_INSIDE_WHY_step "inside: ++ and -- step one notch - a grid without a notch has none to step"
    inside& operator++()
        requires(G.Notch == 0)
    = delete (BEMAN_INSIDE_WHY_step);
    inside operator++(int)
        requires(G.Notch == 0)
    = delete (BEMAN_INSIDE_WHY_step);
    inside& operator--()
        requires(G.Notch == 0)
    = delete (BEMAN_INSIDE_WHY_step);
    inside operator--(int)
        requires(G.Notch == 0)
    = delete (BEMAN_INSIDE_WHY_step);
#undef BEMAN_INSIDE_WHY_step
#undef BEMAN_INSIDE_WHY_unproven

    // *t: a cursor's value without the end, so a cursor is its own iterator
    // and end(t) its sentinel. At the end it reports out of range.
    [[nodiscard]] constexpr auto operator*() const
        requires(has_flag(P, detail::cursor_marker))
    {
        // Past the end is a precondition: the policy reports it to the handler.
        using value_t = detail::cursor_value_t<G, P>;
        value_t v;
        detail::assignment<value_t, inside>::assign(v, *this, make_policy<flags_of<value_t>>());
        return v;
    }

    // try_make<F>(value): the constructors' store under this type's policy plus
    // the flags F, with the failure as the error. An expected value passes its
    // error on, so fallible steps chain.
    template <policy_flag F = detail::none, detail::numeric A>
        requires inside_assignable<inside, A, P | F>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(A value) {
        return detail::try_store<inside, P | F>([&](inside& r, auto&& pol) { r.store_value(value, pol); });
    }
    template <policy_flag F = detail::none, detail::numeric A>
        requires inside_assignable<inside, A, P | F>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(const std::expected<A, errc>& value) {
        return value.and_then([](const A& a) { return try_make<F>(a); });
    }
    template <policy_flag F = detail::none, std::same_as<detail::grid_rational> V>
    [[nodiscard]] static constexpr std::expected<inside, errc> try_make(const V& value) {
        return detail::try_store<inside, P | F>(
            [&](inside& r, auto&& pol) { detail::assign_grid_number(r, value, pol); });
    }
};

//---------------------------------------------------------------------------
// to<T>(b) / as<T>(b) — free-function forms, for generic code that would
// otherwise need the `.template` disambiguator. Same semantics as the members.
//---------------------------------------------------------------------------
template <typename T, policy_flag Round = detail::none, insidable B>
[[nodiscard]] constexpr auto to(const B& b)
    requires(Round == detail::none ? requires { b.template to<T>(); } : requires { b.template to<T, Round>(); })
{
    if constexpr (Round == detail::none)
        return b.template to<T>();
    else
        return b.template to<T, Round>();
}

template <typename T, policy_flag Round = detail::none, insidable B>
[[nodiscard]] constexpr T as(const B& b)
    requires requires { b.template as<T, Round>(); }
{
    return b.template as<T, Round>();
}

//---------------------------------------------------------------------------
// parse<B, F>(first, last) — text → B, exactly (no double round-trip). The
// whole range must be one number: an optional sign, then the literal grammar
// (1'000, 1.25, 1.5e2, 0xff, 0b1010, 0x1.8p3) or a fraction N/D. Malformed text
// is errc::invalid_format; the value then goes through B::try_make<F>, so B's
// policy plus the flags F round, clamp or wrap it and report overflow /
// rounding_error. parse_exact<B> accepts only a value B holds exactly:
// off the grid is rounding_error and out of range overflow, whatever B's
// policy. (io.hpp adds std::string_view overloads and operator>>.)
//---------------------------------------------------------------------------
namespace detail {
// parse past the 64-bit rational (a long decimal, a wide grid's value,
// 1e-200): the text parsed exactly, or past that width as a tiny or huge
// value's stand-in. Out of line: the common parse stays small.
template <insidable B, policy_flag F>
[[gnu::cold]] [[gnu::noinline]] constexpr std::expected<B, errc> parse_wide(const char* first, const char* last) {
    constexpr std::size_t K = std::max(exact_limbs<B>, parse_limbs);
    auto                  w = parse_exact<K>(first, last);
    if (!w && w.error() == errc::overflow)
        if (const auto t = stand_in<B, K, static_cast<bool>((flags_of<B> | F) & wrap)>(first, last))
            w = *t;
    if (!w)
        return std::unexpected{w.error()};
    return try_store<B, flags_of<B> | F>([&](B& b, auto&& pol) { assign_exact<rational>(b, *w, pol, no_action{}); });
}
} // namespace detail

template <insidable B, policy_flag F = detail::none>
[[nodiscard]] constexpr std::expected<B, errc> parse(const char* first, const char* last) {
    if (detail::fraction_text(first, last))
        return detail::parse_wide<B, F>(first, last);
    const auto v = detail::parse_text(first, last);
    if (!v && v.error() == errc::overflow) [[unlikely]]
        return detail::parse_wide<B, F>(first, last);
    if (!v)
        return std::unexpected{v.error()};
    return B::template try_make<F>(*v);
}

template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> parse_exact(const char* first, const char* last) {
    if (!detail::fraction_text(first, last)) {
        const auto v = detail::parse_text(first, last);
        if (v) {
            if (conversion_overflows<B>(*v))
                return std::unexpected{errc::overflow};
            if (conversion_rounds<B>(*v))
                return std::unexpected{errc::rounding_error};
            return B::template try_make<detail::snap>(*v); // on the grid: nothing rounds
        }
        if (v.error() != errc::overflow)
            return std::unexpected{v.error()};
    }
    // A fraction, or a value past the 64-bit rational: exactly.
    constexpr std::size_t K = std::max(detail::exact_limbs<B>, detail::parse_limbs);
    auto                  w = detail::parse_exact<K>(first, last);
    if (!w && w.error() == errc::overflow)
        if (const auto t = detail::stand_in<B, K>(first, last))
            w = *t; // never on the grid: rounding_error, or overflow outside it
    if (!w)
        return std::unexpected{w.error()};
    if (*w < detail::exact_of_grid<K>(lower_of<B>) || detail::exact_of_grid<K>(upper_of<B>) < *w)
        return std::unexpected{errc::overflow};
    if constexpr (detail::notched<B>) {
        if (!detail::exact_index<B, detail::round_mode::trunc>(*w).Exact)
            return std::unexpected{errc::rounding_error};
    } else if constexpr (grid_of<B>.MaxDenominator != 0) {
        // A continuous grid holds denominators up to its bound only.
        if (static_cast<detail::wide_sint<K>>(detail::grid_numerator(grid_of<B>.MaxDenominator)) <
            detail::reduced(*w).Den)
            return std::unexpected{errc::rounding_error};
    }
    return detail::try_store<B, flags_of<B>>(
        [&](B& b, auto&& pol) { detail::assign_exact<detail::rational>(b, *w, pol, detail::no_action{}); });
}

//---------------------------------------------------------------------------
// just
//---------------------------------------------------------------------------
// The point grid holds the value, so no value constructor is needed: grid
// numbers past 64 bits (`_g`) work too. just<N, D> is the point N/D
// for an integer D ≥ 1, a builtin or a grid number: just<1, 3>, just<-6, 5>
// for -1.2, just<1, 0x1p70_g>.
namespace detail {
template <auto den>
consteval bool integer_den() {
    if constexpr (std::integral<decltype(den)>)
        return den >= 1;
    else if constexpr (std::same_as<decltype(den), grid_rational>)
        return den >= 1 && grid_denominator(den) == grid_int{1};
    else
        return false;
}
template <auto value, auto den>
consteval auto just_point() {
    if constexpr (std::integral<decltype(den)> && den == 1)
        return inside<grid{value}>::from_raw({});
    else {
        static_assert(integer_den<den>(), "just<N, D>: D is an integer of at least 1");
        return inside<grid{grid_rational{value} / grid_rational{den}}>::from_raw({});
    }
}
} // namespace detail
template <auto value, auto den = 1>
inline constexpr auto just = detail::just_point<value, den>();

//---------------------------------------------------------------------------
// index_t<T>, step_count_of<T> — a grid's counts as insides, for run time
// (a grid number past 64 bits exists only at compile time):
//   index_t<T>        inside<{0, step_count}>: what t.index() returns and
//                     T::from_index(i) takes back; same raw type as T's index
//   step_count_of<T>  (Upper − Lower)/Notch as a point: 0 bytes, the largest
//                     index, one less than the number of values
//---------------------------------------------------------------------------
template <insidable T>
    requires(notch_of<T> != 0 || lower_of<T> == upper_of<T>)
using index_t = inside<detail::index_grid(grid_of<T>)>;

template <insidable T>
    requires(notch_of<T> != 0 || lower_of<T> == upper_of<T>)
inline constexpr auto step_count_of = just<detail::grid_rational{grid_of<T>.step_count()}>;

//---------------------------------------------------------------------------
// _ins literal — compile-time `inside<{V, V}>` from a numeric literal.
//   5_ins           // inside<{5, 5}>            integer
//   1.25_ins        // inside<{rational{5,4}}>   decimal
//   1.5e2_ins       // inside<{150}>             decimal scientific
//   0xff_ins        // inside<{255}>             hex integer
//   0b1010_ins      // inside<{10}>              binary integer
//   0x1p15_ins      // inside<{32768}>           hex with 2^N exponent (Q-format)
//   0x1p-15_ins     // inside<{rational{1,32768}}>   1/2^15 grid notch
//   0x1.8p3_ins     // inside<{12}>              hex float
//   1'000_ins       // inside<{1000}>            digit separator
//
// Parse is exact (no double round-trip) and of any size, as `_g` reads it
// (inside/detail/grid_rational.hpp): 18446744073709551616_ins is the point
// 2^64. `-1.5_ins` parses as `-(1.5_ins)`.
//---------------------------------------------------------------------------
inline namespace literals {
template <char... Chars>
constexpr auto operator""_ins() {
    return just<detail::parse_grid_literal<Chars...>()>;
}
} // namespace literals

} // namespace beman::inside

// A cursor is a bidirectional iterator: it steps by ++/--, *t reads its value,
// and end(t) is its sentinel. Not random access: t - u is arithmetic, an
// inside, not a distance. Plain insides get no traits, so they stay numbers.
template <beman::inside::grid G, beman::inside::policy_flag P>
    requires(beman::inside::has_flag(P, beman::inside::detail::cursor_marker))
struct std::incrementable_traits<beman::inside::inside<G, P>> {
    using difference_type = std::ptrdiff_t;
};

template <beman::inside::grid G, beman::inside::policy_flag P>
    requires(beman::inside::has_flag(P, beman::inside::detail::cursor_marker))
struct std::indirectly_readable_traits<beman::inside::inside<G, P>> {
    using value_type = beman::inside::detail::cursor_value_t<G, P>;
};

#endif // BEMAN_INSIDE_INSIDE_DETAIL_CORE_HPP
