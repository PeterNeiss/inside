// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// io — ALL of the library's string / stream / std::format support, gathered
// into one opt-in header. The core (inside.hpp and everything it pulls) never
// includes this, so a freestanding / bare-metal build that never includes
// "beman/inside/io.hpp" pays zero <string>/<ostream>/<format> cost. In the single-
// header amalgamation this whole region is wrapped in `#ifndef BEMAN_INSIDE_NO_STRING`,
// so defining BEMAN_INSIDE_NO_STRING drops it (and its heavy includes) wholesale.
//
// Provides:
//   * to_string(rational | interval | grid | insidable | arithmetic | errc)
//   * to_string_debug(insidable)        — value + raw + raw type + grid
//   * operator<< and operator>> (one token, failbit on error)
//   * parse<B, F>(std::string_view), parse_exact<B>(std::string_view)
//   * std::formatter<inside<G, P>>, std::formatter<rational> (with <format>)
//   * type_name_of<B>                   — a readable name of an inside type
// units.hpp builds on it (to_string and std::formatter of quantities).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_IO_HPP
#define BEMAN_INSIDE_IO_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>

#include <algorithm>
#include <format>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

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

// Decimal digits of num/den (den > 0) on demand: the integer part, then the
// fraction one chunk at a time. Tail() says whether anything nonzero is left,
// which rounding needs to tell a tie from a value above it.
template <std::size_t K>
struct decimal_stream {
    using W = wide_uint<K + 1>;
    W Rem, Den;

    decimal_stream(const wide_uint<K>& num, const wide_uint<K>& den) : Rem{num}, Den{den} {}

    std::string whole() {
        const auto [q, r] = W::divmod(Rem, Den);
        Rem               = r;
        return wide_to_decimal(q);
    }
    std::string next(int n) {
        std::string out;
        for (; n > 0; n -= 19) {
            const int chunk = n < 19 ? n : 19;
            umax      p     = 1;
            for (int i = 0; i < chunk; ++i)
                p *= 10;
            const auto [q, r]      = W::divmod(Rem * W{p}, Den);
            const std::string part = std::to_string(static_cast<umax>(q));
            out.append(static_cast<std::size_t>(chunk) - part.size(), '0') += part;
            Rem = r;
        }
        return out;
    }
    bool tail() const { return !Rem.is_zero(); }
};

// num/den (den > 0) in the forms parse reads: its exact decimal when the
// reduced denominator is 2^a·5^b (however many digits, and at least
// min_digits decimals), else "num/den".
template <std::size_t K>
std::string fraction_to_string(bool neg, wide_uint<K> num, wide_uint<K> den, int min_digits = 0) {
    using U   = wide_uint<K>;
    const U g = wide_gcd(den, num);
    num /= g;
    den /= g;
    std::string str   = neg && !num.is_zero() ? "-" : "";
    U           rest  = den;
    const int   twos  = static_cast<int>(strip_factor(rest, 2)); // ≤ the bit width
    const int   fives = static_cast<int>(strip_factor(rest, 5));
    if (!(rest == U{1}))
        return str += wide_to_decimal(num) + "/" + wide_to_decimal(den);
    // num·10^digits/den is an integer: its decimals end there.
    decimal_stream<K> ds{num, den};
    str += ds.whole();
    int digits = twos > fives ? twos : fives;
    if (digits < min_digits)
        digits = min_digits;
    if (digits != 0)
        str += '.' + ds.next(digits);
    return str;
}
} // namespace detail

//-------------------------------------------------------------------------
// to_string — pretty-prints `rational`, `interval`, `grid`, plus a fallback
// for plain arithmetic types and the exact form for insidables. A value
// prints as its exact decimal when it has one, else as num/den.
//-------------------------------------------------------------------------
[[nodiscard]] inline std::string to_string(beman::inside::detail::rational r) {
    return detail::fraction_to_string<1>(r.Den < 0, r.Num, detail::abs_den(r.Den));
}

namespace detail {
// The decimals every value of B prints with: n for a decimal lattice — one
// whose value unit (the notch, or gcd(Notch, Lower) off 0) has a
// denominator 2^a·5^b with b ≥ 1 (per<100>, 0.05, 1e-18) — with
// n = max(a, b); else 0 (the value decides).
template <insidable B>
inline constexpr int fixed_decimals = [] {
    if constexpr (!notched<B>)
        return 0;
    else {
        grid_int  q    = wide_denominator(grid_of<B>.value_unit());
        const int two  = static_cast<int>(strip_factor(q, 2)); // ≤ the bit width
        const int five = static_cast<int>(strip_factor(q, 5));
        return q == grid_int{1} && five > 0 ? (two > five ? two : five) : 0;
    }
}();
} // namespace detail

// A grid number of any size, in the forms of to_string(rational).
[[nodiscard]] inline std::string to_string(const detail::big_rational& r);

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
    if (g.MaxDenominator != 0) {
        str += ", den<=";
        str += beman::inside::to_string(g.MaxDenominator);
    }
    str += "}";
    return str;
}

// delegate to std::to_string
template <typename V>
    requires(std::integral<V> || std::floating_point<V>)
[[nodiscard]] auto to_string(V value) {
    return std::to_string(value);
}

//-------------------------------------------------------------------------
// type_name_of<T> — short raw-type label for to_string_debug. Lives here (not
// in numbers.hpp) so the core never pulls <string_view>.
//-------------------------------------------------------------------------
namespace detail {
template <typename T>
inline constexpr std::string_view type_name_of = wide_integer<T>         ? "wide_int"
                                                 : wide_rational_like<T> ? "wide_rational"
                                                                         : "unknown";
template <>
inline constexpr std::string_view type_name_of<std::uint8_t> = "uint8_t";
template <>
inline constexpr std::string_view type_name_of<std::uint16_t> = "uint16_t";
template <>
inline constexpr std::string_view type_name_of<std::uint32_t> = "uint32_t";
template <>
inline constexpr std::string_view type_name_of<std::uint64_t> = "uint64_t";
template <>
inline constexpr std::string_view type_name_of<std::int8_t> = "int8_t";
template <>
inline constexpr std::string_view type_name_of<std::int16_t> = "int16_t";
template <>
inline constexpr std::string_view type_name_of<std::int32_t> = "int32_t";
template <>
inline constexpr std::string_view type_name_of<std::int64_t> = "int64_t";
template <>
inline constexpr std::string_view type_name_of<rational> = "rational";
template <>
inline constexpr std::string_view type_name_of<point_slot> = "point";
template <>
inline constexpr std::string_view type_name_of<wide_uint<2>> = "wide_uint<2>";
template <>
inline constexpr std::string_view type_name_of<wide_uint<3>> = "wide_uint<3>";
} // namespace detail

//-------------------------------------------------------------------------
// to_string / to_string_debug / operator<< for insidables and rational.
// The debug form also prints the raw value, raw type, and grid — useful when
// inspecting failing tests or storage choices.
//-------------------------------------------------------------------------
namespace detail {
// An exact value, with at least min_digits decimals.
template <std::size_t K>
std::string exact_to_string(wide_rational<K> f, int min_digits = 0) {
    const bool neg = f.Num.negative();
    return fraction_to_string(neg, wide_uint<K>{neg ? -f.Num : f.Num}, wide_uint<K>{f.Den}, min_digits);
}
} // namespace detail

template <std::size_t N, bool S>
[[nodiscard]] inline std::string to_string(detail::wide_int<N, S> v) {
    return detail::wide_to_decimal(v);
}

// A grid number of any size, in decimal (reads the interned limbs; no new
// big values are formed at runtime).
[[nodiscard]] inline std::string to_string(const detail::big_int& v) {
    detail::big::mag m = v.magnitude();
    std::string      out;
    do {
        // m /= 10^19, rem = m % 10^19
        const umax rem = detail::limb::div_1(m.data(), m.data(), m.size(), umax{10'000'000'000'000'000'000ull});
        detail::big::trim(m);
        std::string part = std::to_string(rem);
        if (!m.empty())
            part.insert(0, 19 - part.size(), '0');
        out.insert(0, part);
    } while (!m.empty());
    return v.negative() ? "-" + out : out;
}

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

// An inside's exact value: with a decimal notch, every value prints that
// notch's decimals (19.90, 2.00); otherwise its shortest exact form — a
// decimal when it has one, else N/D.
template <insidable B>
[[nodiscard]] inline std::string to_string(B b) {
    constexpr int n = detail::fixed_decimals<B>;
    if constexpr (detail::wide_valued<B>)
        return detail::exact_to_string(detail::exact_of(b), n);
    else {
        const detail::rational r = detail::as_rational(b);
        return detail::fraction_to_string<1>(r.Den < 0, r.Num, detail::abs_den(r.Den), n);
    }
}

template <insidable B>
[[nodiscard]] inline std::string to_string_debug(B b) {
    std::string str;
    str += beman::inside::to_string(b);
    str += " {";
    if constexpr (detail::fraction_storage<B>)
        str += detail::exact_to_string(b.raw());
    else
        str += beman::inside::to_string(+b.raw());
    str += "[" + std::string(detail::type_name_of<detail::raw_t<B>>);
    constexpr auto slots = grid_of<B>.step_count();
    str += " Max:" + beman::inside::to_string(slots) + "] ";
    str += beman::inside::to_string(grid_of<B>);
    str += "}";
    return str;
}

inline std::ostream& operator<<(std::ostream& stream, beman::inside::detail::rational r) {
    stream << beman::inside::to_string(r);
    return stream;
}

// An error code as its message: "division by zero".
[[nodiscard]] inline std::string to_string(errc e) { return errc_message(e); }

inline std::ostream& operator<<(std::ostream& stream, errc e) { return stream << errc_message(e); }

template <insidable B>
inline std::ostream& operator<<(std::ostream& stream, B b) {
    stream << beman::inside::to_string(b);
    return stream;
}

// parse<B, F>(text) / parse_exact<B>(text) — the std::string_view
// forms of the pointer-pair overloads.
template <insidable B, policy_flag F = none>
[[nodiscard]] constexpr std::expected<B, errc> parse(std::string_view text) {
    return parse<B, F>(text.data(), text.data() + text.size());
}
template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> parse_exact(std::string_view text) {
    return parse_exact<B>(text.data(), text.data() + text.size());
}

// Reads one whitespace-delimited token and parses it with parse<B>. On an
// error the stream's failbit is set and `b` is left unchanged.
template <insidable B>
inline std::istream& operator>>(std::istream& stream, B& b) {
    std::string token;
    if (!(stream >> token))
        return stream;
    if (const auto r = parse<B>(token); r)
        b = *r;
    else
        stream.setstate(std::ios_base::failbit);
    return stream;
}

} // namespace beman::inside

//---------------------------------------------------------------------------
// std::format integration
//---------------------------------------------------------------------------
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

// Keep the first `keep` digits of the magnitude `d` (digits[keep..] and a
// nonzero tail are what is cut), rounded by `mode` for a value of the given
// sign. A carry out of the first digit prepends a '1'; returns whether it did.
inline bool round_digits(std::string& d, std::size_t keep, bool tail, round_mode mode, bool negative) {
    const bool more = tail || d.find_first_not_of('0', keep + 1) != std::string::npos;
    const char cut  = keep < d.size() ? d[keep] : '0';
    d.resize(keep);
    const bool inexact = cut != '0' || more;
    bool       up      = false;
    switch (mode) {
    case round_mode::trunc:
        break;
    case round_mode::floor:
        up = negative && inexact;
        break;
    case round_mode::ceil:
        up = !negative && inexact;
        break;
    case round_mode::nearest: // ties away from zero
        up = cut >= '5';
        break;
    case round_mode::half_even:
        up = cut > '5' || (cut == '5' && (more || (keep > 0 && (d[keep - 1] - '0') % 2 == 1)));
        break;
    }
    if (!up)
        return false;
    for (std::size_t i = keep; i-- > 0;) {
        if (d[i] != '9') {
            ++d[i];
            return false;
        }
        d[i] = '0';
    }
    d.insert(d.begin(), '1');
    return true;
}

// A std::format spec for exact values: [[fill]align][sign][#][0][width]
// [.precision][type], type one of f F e E g G (or none). The digits come from
// the exact value, rounded once by the type's rounding mode (display_rounding),
// so wide and big values print correctly.
struct exact_format_spec {
    char Fill = ' ', Align = 0, Sign = '-', Type = 0;
    bool Alt = false, Zero = false;
    int  Width = 0, Precision = -1;

    constexpr auto parse(std::format_parse_context& ctx) {
        auto       it       = ctx.begin();
        const auto end      = ctx.end();
        auto       is_align = [](char c) { return c == '<' || c == '>' || c == '^'; };
        auto       number   = [&](int& v) {
            for (v = 0; it != end && *it >= '0' && *it <= '9'; ++it)
                v = v * 10 + (*it - '0');
        };
        if (it != end && it + 1 != end && is_align(it[1]) && *it != '{' && *it != '}') {
            Fill  = *it;
            Align = it[1];
            it += 2;
        } else if (it != end && is_align(*it))
            Align = *it++;
        if (it != end && (*it == '+' || *it == '-' || *it == ' '))
            Sign = *it++;
        if (it != end && *it == '#') {
            Alt = true;
            ++it;
        }
        if (it != end && *it == '0') {
            Zero = true;
            ++it;
        }
        number(Width);
        if (it != end && *it == '.') {
            ++it;
            if (it == end || *it < '0' || *it > '9')
                throw std::format_error("inside: precision needs digits");
            number(Precision);
        }
        if (it != end && std::string_view{"fFeEgG"}.find(*it) != std::string_view::npos)
            Type = *it++;
        if (it != end && *it != '}')
            throw std::format_error("inside: unsupported format spec (use f, e or g with fill, align, sign, #, 0, "
                                    "width and precision)");
        return it;
    }

    // v's shortest form (to_string) without its sign, which body() reads only
    // without a type or precision (else empty).
    template <class V>
    std::string plain_text(const V& v) const {
        if (Type != 0 || Precision >= 0)
            return {};
        std::string s = beman::inside::to_string(v);
        if (!s.empty() && s[0] == '-')
            s.erase(0, 1);
        return s;
    }

    // An exact value's text by the spec, then written with its sign.
    template <std::size_t K, typename Ctx>
    auto write_exact(const wide_rational<K>& f, std::string_view plain, round_mode mode, Ctx& ctx) const {
        const bool neg = f.Num.negative();
        return write(neg, body(wide_uint<K>{abs(f.Num)}, wide_uint<K>{f.Den}, plain, neg, mode), ctx);
    }

    // The value's text: `plain` (to_string) for no type and no precision,
    // else the digits the spec asks for.
    template <std::size_t K>
    std::string body(const wide_uint<K>& num,
                     const wide_uint<K>& den,
                     std::string_view    plain,
                     bool                negative,
                     round_mode          mode) const {
        if (Type == 0 && Precision < 0)
            return std::string{plain};
        const char t     = Type == 0 ? 'g' : static_cast<char>(Type | 0x20); // lower case
        const bool upper = Type == 'F' || Type == 'E' || Type == 'G';
        int        p     = Precision < 0 ? 6 : Precision;
        if (t == 'f')
            return fixed(num, den, p, negative, mode);
        if (t == 'g') {
            p = p == 0 ? 1 : p;
            // The exponent after rounding to p significant digits picks the form.
            int         x = 0;
            const auto  e = scientific(num, den, p - 1, x, negative, mode);
            std::string out;
            if (x < p && x >= -4)
                out = fixed(num, den, p - 1 - x, negative, mode);
            else
                out = e;
            if (!Alt && out.find('.') != std::string::npos) {
                const std::size_t ep = out.find('e');
                std::string       m = out.substr(0, ep), tail = ep == std::string::npos ? "" : out.substr(ep);
                m.erase(m.find_last_not_of('0') + 1);
                if (m.back() == '.')
                    m.pop_back();
                out = m + tail;
            }
            return upper ? to_upper(out) : out;
        }
        int               x   = 0;
        const std::string out = scientific(num, den, p, x, negative, mode);
        return upper ? to_upper(out) : out;
    }

    // Sign, then fill and alignment (numbers align right; `0` pads after the sign).
    template <typename Ctx>
    auto write(bool negative, std::string text, Ctx& ctx) const {
        std::string sign = negative ? "-" : Sign == '+' ? "+" : Sign == ' ' ? " " : "";
        std::size_t len  = sign.size() + text.size();
        std::size_t pad  = static_cast<std::size_t>(Width) > len ? static_cast<std::size_t>(Width) - len : 0;
        std::string out;
        if (Zero && Align == 0)
            out = sign + std::string(pad, '0') + text;
        else if (Align == '<')
            out = sign + text + std::string(pad, Fill);
        else if (Align == '^')
            out = std::string(pad / 2, Fill) + sign + text + std::string(pad - pad / 2, Fill);
        else
            out = std::string(pad, Fill) + sign + text;
        return std::format_to(ctx.out(), "{}", out);
    }

  private:
    static std::string to_upper(std::string s) {
        std::ranges::replace(s, 'e', 'E');
        return s;
    }

    template <std::size_t K>
    std::string fixed(const wide_uint<K>& num, const wide_uint<K>& den, int p, bool negative, round_mode mode) const {
        decimal_stream<K> ds{num, den};
        const std::string whole = ds.whole();
        std::string       d     = whole + ds.next(p + 1);
        round_digits(d, whole.size() + static_cast<std::size_t>(p), ds.tail(), mode, negative); // a carry lengthens d
        std::string out = d.substr(0, d.size() - static_cast<std::size_t>(p));
        if (p > 0 || Alt)
            out += '.';
        return out + d.substr(d.size() - static_cast<std::size_t>(p));
    }

    // d.ddd…e±XX with p decimals; x receives the exponent.
    template <std::size_t K>
    std::string
    scientific(const wide_uint<K>& num, const wide_uint<K>& den, int p, int& x, bool negative, round_mode mode) const {
        decimal_stream<K> ds{num, den};
        std::string       d = ds.whole(); // from the first significant digit on
        x                   = static_cast<int>(d.size()) - 1;
        if (d == "0" && !num.is_zero())
            for (x = 0; d == "0"; --x) // the first nonzero digit of the fraction
                d = ds.next(1);
        if (static_cast<int>(d.size()) < p + 2)
            d += ds.next(p + 2 - static_cast<int>(d.size()));
        if (round_digits(d, static_cast<std::size_t>(p) + 1, ds.tail(), mode, negative)) {
            ++x;
            d.pop_back();
        }
        std::string out = d.substr(0, 1);
        if (p > 0 || Alt)
            out += '.';
        out += d.substr(1);
        const int ax = x < 0 ? -x : x;
        return out + (x < 0 ? "e-" : "e+") + (ax < 10 ? "0" : "") + std::to_string(ax);
    }
};

// The rounding a format spec applies to B: B's own mode when its policy
// names one (as storing at that precision would round), else ties to even.
template <insidable B>
inline constexpr round_mode display_rounding =
    has_any_flag(flags_of<B>, round_floor | round_ceil | round_nearest | round_half_even | snap)
        ? rounding_of(flags_of<B>)
        : round_mode::half_even;
} // namespace beman::inside::detail

// Empty `{}` prints to_string. A notched integer grid within imax takes the
// integer specs (std::formatter<imax>: {:>4}, {:#x}); every other inside takes
// the exact specs ({:.2f}, {:e}, {:g}, fill / align / sign / width), rounded
// from the exact value by the type's rounding mode (ties to even without one).
template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::formatter<beman::inside::inside<G, P>>
    : beman::inside::detail::numeric_spec_formatter<
          std::conditional_t<beman::inside::detail::integer_lattice<beman::inside::inside<G, P>> &&
                                 beman::inside::detail::values_fit_imax<beman::inside::inside<G, P>>,
                             std::formatter<beman::inside::imax>,
                             beman::inside::detail::exact_format_spec>> {
    using B = beman::inside::inside<G, P>;
    // Integer formatting only for a notched integer grid: a continuous grid
    // (notch 0) holds fractions even between integer bounds.
    static constexpr bool integer_path =
        beman::inside::detail::integer_lattice<B> && beman::inside::detail::values_fit_imax<B>;

    template <typename Ctx>
    auto format(const B& b, Ctx& ctx) const {
        namespace d = beman::inside::detail;
        if constexpr (integer_path)
            return this->Numeric.format(d::to_value(b), ctx);
        else if (!this->HasSpec)
            return std::format_to(ctx.out(), "{}", beman::inside::to_string(b));
        else
            return this->Numeric.write_exact(d::exact_of(b), this->Numeric.plain_text(b), d::display_rounding<B>, ctx);
    }
};

template <>
struct std::formatter<beman::inside::detail::rational>
    : beman::inside::detail::numeric_spec_formatter<beman::inside::detail::exact_format_spec> {
    template <typename Ctx>
    auto format(const beman::inside::detail::rational& r, Ctx& ctx) const {
        namespace d = beman::inside::detail;
        if (!HasSpec)
            return std::format_to(ctx.out(), "{}", beman::inside::to_string(r));
        return Numeric.write_exact(d::exact_of(r), Numeric.plain_text(r), d::round_mode::half_even, ctx);
    }
};

#endif // BEMAN_INSIDE_IO_HPP
