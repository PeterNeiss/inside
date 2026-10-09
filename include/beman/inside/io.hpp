// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// io — ALL of the library's string / stream / std::format support, gathered
// into one opt-in header. The core (inside.hpp and everything it pulls) never
// includes this, so a freestanding / bare-metal build that never includes
// "beman/inside/io.hpp" pays zero <string>/<ostream>/<format> cost. In the single-
// header amalgamation this whole region is wrapped in `#ifndef BEMAN_INSIDE_NO_STRING`,
// so defining BEMAN_INSIDE_NO_STRING drops it (and its heavy includes) wholesale.
//
// Provides:
//   * to_string(rational | interval | grid | insidable | arithmetic)
//   * to_string_debug(insidable)        — value + raw + raw-type + grid
//   * operator<<(std::ostream&, ...)
//   * std::formatter<inside<G,P>> / std::formatter<rational>   (when <format>)
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_IO_HPP
#define BEMAN_INSIDE_IO_HPP

#include <beman/inside/inside.hpp>

#include <string>
#include <string_view>
#include <istream>
#include <ostream>
#include <version> // __cpp_lib_format feature-test macro

namespace beman::inside {
namespace detail {
// Decimal digits of a wide integer: 19 digits per division by 10^19.
template <std::size_t N, bool S>
std::string wide_to_decimal(wide_int<N, S> v) {
    const bool         neg = v.negative();
    wide_int<N, false> u(neg ? -v : v);
    std::string        out;
    do {
        const auto [q, r] = divmod_small(u, 10'000'000'000'000'000'000ull);
        std::string part  = std::to_string(r);
        if (!q.is_zero())
            part.insert(0, 19 - part.size(), '0');
        out.insert(0, part);
        u = q;
    } while (!u.is_zero());
    return neg ? "-" + out : out;
}

// num/den (den > 0) in the forms from_chars reads: its exact decimal when the
// reduced denominator is 2^a·5^b (however many digits), else "num/den".
template <std::size_t K>
std::string fraction_to_string(bool neg, wide_uint<K> num, wide_uint<K> den) {
    using U = wide_uint<K>;
    U g     = den;
    for (U x = num; !x.is_zero();) {
        const U t = g % x;
        g         = x;
        x         = t;
    }
    num /= g;
    den /= g;
    std::string str  = neg && !num.is_zero() ? "-" : "";
    int         twos = 0, fives = 0;
    U           rest = den;
    for (; (rest.Word[0] & 1) == 0; rest >>= 1)
        ++twos;
    for (auto d = divmod_small(rest, 5); d.Remainder == 0; d = divmod_small(rest, 5)) {
        rest = d.Quotient;
        ++fives;
    }
    if (!(rest == U{1}))
        return str += wide_to_decimal(num) + "/" + wide_to_decimal(den);
    // num·10^digits/den is an integer; 19 digits at a time keep the scratch
    // below den·10^19.
    const auto [whole, frac] = U::divmod(num, den);
    str += wide_to_decimal(whole);
    if (frac.is_zero())
        return str;
    str += '.';
    using W = wide_uint<K + 1>;
    W r{frac};
    for (int left = twos > fives ? twos : fives; left > 0; left -= 19) {
        const int chunk = left < 19 ? left : 19;
        umax      p     = 1;
        for (int i = 0; i < chunk; ++i)
            p *= 10;
        const auto [q, m]      = W::divmod(r * W{p}, W{den});
        const std::string part = std::to_string(static_cast<umax>(q));
        str.append(static_cast<std::size_t>(chunk) - part.size(), '0') += part;
        r = m;
    }
    return str;
}
} // namespace detail

//-------------------------------------------------------------------------
// to_string — pretty-prints `rational`, `interval`, `grid`, plus a fallback
// for plain arithmetic types and the exact form for insidables. A value
// prints as its exact decimal when it has one, else as num/den.
//-------------------------------------------------------------------------
[[nodiscard]] inline std::string to_string(beman::inside::detail::rational r) {
    return detail::fraction_to_string<1>(r.Denominator < 0, r.Numerator, detail::abs_den(r.Denominator));
}

#if BEMAN_INSIDE_BIG_GRIDS
// A grid number of any size, in the forms of to_string(rational).
[[nodiscard]] inline std::string to_string(const detail::big_rational& r);
#endif

[[nodiscard]] inline std::string to_string(interval ival) {
    std::string str{"["};

    str += beman::inside::to_string(ival.Lower);
    str += "..";
    str += beman::inside::to_string(ival.Upper);
    str += "]";
    return str;
}

[[nodiscard]] inline std::string to_string(grid g) {
    std::string str{"{"};

    str += beman::inside::to_string(g.Interval);
    str += ", ";
    str += beman::inside::to_string(g.Notch);
    str += "}";
    return str;
}

// delegate to std::to_string
template <typename V>
[[nodiscard]] auto to_string(V value) {
    return std::to_string(value);
}

namespace detail {
// A double's exact value: a binary fraction, so always a finite decimal.
inline std::string double_to_string(double d) {
    int        e    = 0;
    const umax mant = static_cast<umax>(ldexp(frexp(d < 0 ? -d : d, &e), 53)); // |d| = mant·2^(e−53)
    e -= 53;
    using U = wide_uint<18>; // 2^1024 and 2^1074 both fit
    return fraction_to_string(d < 0, e >= 0 ? U{mant} << e : U{mant}, e >= 0 ? U{1} : U{1} << -e);
}
} // namespace detail

// `f64` (double-backed) and `exact` (rational-backed) insides: the exact
// value. A continuous (Notch == 0) f64 inside prints the double's exact
// decimal.
template <insidable B>
    requires(detail::fp_raw<B> || detail::rational_raw<B>)
[[nodiscard]] inline std::string to_string(B b) {
    if constexpr (detail::fp_raw<B> && detail::notch64<B> == beman::inside::detail::rational{0})
        return detail::double_to_string(detail::as_double(b));
    else
        return to_string(beman::inside::detail::as_rational(b));
}

//-------------------------------------------------------------------------
// type_name<T>() — short raw-type label for to_string_debug. Lives here (not
// in the core math header) so the core never pulls <string_view>.
//-------------------------------------------------------------------------
namespace detail {
template <typename T>
constexpr std::string_view type_name() {
    if constexpr (std::is_same_v<T, std::uint8_t>)
        return "uint8_t";
    if constexpr (std::is_same_v<T, std::uint16_t>)
        return "uint16_t";
    if constexpr (std::is_same_v<T, std::uint32_t>)
        return "uint32_t";
    if constexpr (std::is_same_v<T, std::uint64_t>)
        return "uint64_t";
    if constexpr (std::is_same_v<T, std::int8_t>)
        return "int8_t";
    if constexpr (std::is_same_v<T, std::int16_t>)
        return "int16_t";
    if constexpr (std::is_same_v<T, std::int32_t>)
        return "int32_t";
    if constexpr (std::is_same_v<T, std::int64_t>)
        return "int64_t";
    if constexpr (std::is_same_v<T, rational>)
        return "rational";
    if constexpr (std::is_same_v<T, point_slot>)
        return "point";
    if constexpr (std::is_same_v<T, wide_uint<2>>)
        return "wide_uint<2>";
    if constexpr (std::is_same_v<T, wide_uint<3>>)
        return "wide_uint<3>";
    if constexpr (is_wide_int_v<T>)
        return "wide_int";
    if constexpr (is_exact_frac_v<T>)
        return "exact_frac";
    return "unknown";
}
} // namespace detail

//-------------------------------------------------------------------------
// to_string / to_string_debug / operator<< for insidables and rational.
// The debug form also prints the raw value, raw type, and grid — useful when
// inspecting failing tests or storage choices.
//-------------------------------------------------------------------------
namespace detail {
// A wide-index value, exactly.
template <std::size_t K>
std::string exact_to_string(exact_frac<K> f) {
    const bool neg = f.Num.negative();
    return fraction_to_string(neg, wide_uint<K>{neg ? -f.Num : f.Num}, wide_uint<K>{f.Den});
}
} // namespace detail

template <std::size_t N, bool S>
[[nodiscard]] inline std::string to_string(detail::wide_int<N, S> v) {
    return detail::wide_to_decimal(v);
}

#if BEMAN_INSIDE_BIG_GRIDS
// A grid number of any size, in decimal (reads the interned limbs; no new
// big values are formed at runtime).
[[nodiscard]] inline std::string to_string(const detail::big_int& v) {
    detail::big::mag m = v.magnitude();
    std::string      out;
    do {
        umax rem = 0; // m /= 10^19, rem = m % 10^19
        for (std::size_t i = m.size(); i-- > 0;) {
            const auto d = detail::limb::div(rem, m[i], umax{10'000'000'000'000'000'000ull});
            m[i]         = d.Hi;
            rem          = d.Lo;
        }
        detail::big::trim(m);
        std::string part = std::to_string(rem);
        if (!m.empty())
            part.insert(0, 19 - part.size(), '0');
        out.insert(0, part);
    } while (!m.empty());
    return v.negative() ? "-" + out : out;
}
#endif

#if BEMAN_INSIDE_BIG_GRIDS
namespace detail {
template <std::size_t K>
std::string big_fraction_to_string(const big_rational& r) {
    const auto n = static_cast<wide_uint<K>>(r.Num); // two's complement
    return fraction_to_string(r.Num.negative(), r.Num.negative() ? -n : n, static_cast<wide_uint<K>>(r.Den));
}
} // namespace detail

[[nodiscard]] inline std::string to_string(const detail::big_rational& r) {
    const int bits = r.Num.bit_width() > r.Den.bit_width() ? r.Num.bit_width() : r.Den.bit_width();
    if (bits < 512)
        return detail::big_fraction_to_string<8>(r);
    if (bits < 4096)
        return detail::big_fraction_to_string<64>(r);
    const std::string num = beman::inside::to_string(r.Num);
    return r.is_integer() ? num : num + "/" + beman::inside::to_string(r.Den);
}
#endif

template <insidable B>
[[nodiscard]] inline std::string to_string(B b) {
    if constexpr (detail::exact_valued<B>)
        return detail::exact_to_string(detail::exact_of(b));
    else
        return beman::inside::to_string(detail::as_rational(b));
}

template <insidable B>
[[nodiscard]] inline std::string to_string_debug(B b) {
    std::string str;
    str += beman::inside::to_string(b);
    str += " {";
    if constexpr (detail::frac_raw<B>)
        str += detail::exact_to_string(b.raw());
    else
        str += beman::inside::to_string(+b.raw());
    str += "[" + std::string(detail::type_name<detail::raw_t<B>>());
    constexpr auto slots = grid_of<B>.slot_count();
    str += " Max:" + beman::inside::to_string(slots) + "] ";
    str += beman::inside::to_string(grid_of<B>);
    str += "}";
    return str;
}

inline std::ostream& operator<<(std::ostream& stream, beman::inside::detail::rational r) {
    stream << beman::inside::to_string(r);
    return stream;
}

template <insidable B>
inline std::ostream& operator<<(std::ostream& stream, B b) {
    stream << beman::inside::to_string(b);
    return stream;
}

// from_chars<B>(text) — the std::string_view form of from_chars<B>(first, last).
template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> from_chars(std::string_view text) {
    return from_chars<B>(text.data(), text.data() + text.size());
}

// Reads one whitespace-delimited token and parses it with from_chars<B>. On an
// error the stream's failbit is set and `b` is left unchanged.
template <insidable B>
inline std::istream& operator>>(std::istream& stream, B& b) {
    std::string token;
    if (!(stream >> token))
        return stream;
    if (const auto r = from_chars<B>(token); r)
        b = *r;
    else
        stream.setstate(std::ios_base::failbit);
    return stream;
}

} // namespace beman::inside

//---------------------------------------------------------------------------
// std::format integration is gated on a working <format>; without it this
// compiles as a no-op and to_string()/operator<< remain.
//---------------------------------------------------------------------------
#ifdef __cpp_lib_format

    #include <format>
    #include <type_traits>

namespace beman::inside::detail {
// Shared spec handling: an empty `{}` is left to the derived format() (exact
// to_string); a non-empty spec is parsed and applied by `Numeric`.
template <class Inner>
struct numeric_spec_formatter {
    Inner Numeric{};
    bool  HasSpec = false;

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            HasSpec = true;
            return Numeric.parse(ctx);
        }
        return it;
    }
};
} // namespace beman::inside::detail

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::formatter<beman::inside::inside<G, P>>
    : beman::inside::detail::numeric_spec_formatter<
          std::conditional_t<beman::inside::detail::is_integer_aligned<beman::inside::inside<G, P>> && G.Notch != 0 &&
                                 beman::inside::detail::values_fit_imax<beman::inside::inside<G, P>>,
                             std::formatter<beman::inside::imax>,
                             std::formatter<double>>> {
    using B = beman::inside::inside<G, P>;
    // Integer formatting only for a notched integer grid: a continuous grid
    // (notch 0) holds fractions even between integer bounds.
    static constexpr bool integer_path =
        beman::inside::detail::is_integer_aligned<B> && G.Notch != 0 && beman::inside::detail::values_fit_imax<B>;

    template <typename Ctx>
    auto format(const B& b, Ctx& ctx) const {
        if constexpr (integer_path)
            return this->Numeric.format(beman::inside::detail::to_value(b), ctx);
        else if (this->HasSpec)
            return this->Numeric.format(beman::inside::detail::as_double(b), ctx);
        else
            return std::format_to(ctx.out(), "{}", beman::inside::to_string(b));
    }
};

template <>
struct std::formatter<beman::inside::detail::rational>
    : beman::inside::detail::numeric_spec_formatter<std::formatter<double>> {
    template <typename Ctx>
    auto format(const beman::inside::detail::rational& r, Ctx& ctx) const {
        if (HasSpec)
            return Numeric.format(static_cast<double>(r), ctx);
        return std::format_to(ctx.out(), "{}", beman::inside::to_string(r));
    }
};

#endif // __cpp_lib_format

#endif // BEMAN_INSIDE_IO_HPP
