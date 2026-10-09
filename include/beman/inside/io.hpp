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
// reduced denominator is 2^a·5^b (however many digits, and at least
// min_digits decimals), else "num/den".
template <std::size_t K>
std::string fraction_to_string(bool neg, wide_uint<K> num, wide_uint<K> den, int min_digits = 0) {
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
    int digits = twos > fives ? twos : fives;
    if (digits < min_digits)
        digits = min_digits;
    if (digits == 0)
        return str;
    str += '.';
    using W = wide_uint<K + 1>;
    W r{frac};
    for (int left = digits; left > 0; left -= 19) {
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

namespace detail {
// The decimals every value of B prints with: n for a decimal notch — one
// whose denominator is 2^a·5^b with b ≥ 1 (per<100>, 0.05, 1e-18) — with
// n = max(a, b); else 0 (the value decides).
template <insidable B>
inline constexpr int fixed_decimals = [] {
    if constexpr (notch_of<B> == 0)
        return 0;
    else {
        grid_wide q   = wide_denominator(notch_of<B>);
        int       two = 0, five = 0;
        for (; q % grid_wide{2} == grid_wide{0}; q = q / grid_wide{2})
            ++two;
        for (; q % grid_wide{5} == grid_wide{0}; q = q / grid_wide{5})
            ++five;
        return q == grid_wide{1} && five > 0 ? (two > five ? two : five) : 0;
    }
}();
} // namespace detail

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
// An exact value, with at least min_digits decimals.
template <std::size_t K>
std::string exact_to_string(exact_frac<K> f, int min_digits = 0) {
    const bool neg = f.Num.negative();
    return fraction_to_string(neg, wide_uint<K>{neg ? -f.Num : f.Num}, wide_uint<K>{f.Den}, min_digits);
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

// An inside's exact value: with a decimal notch, every value prints that
// notch's decimals (19.90, 2.00); otherwise its shortest exact form — a
// decimal when it has one, else N/D. A continuous f64 inside prints the
// double's exact decimal.
template <insidable B>
[[nodiscard]] inline std::string to_string(B b) {
    constexpr int n = detail::fixed_decimals<B>;
    if constexpr (detail::fp_raw<B> && notch_of<B> == 0)
        return detail::double_to_string(detail::as_double(b));
    else if constexpr (detail::exact_valued<B>)
        return detail::exact_to_string(detail::exact_of(b), n);
    else {
        const detail::rational r = detail::as_rational(b);
        return detail::fraction_to_string<1>(r.Denominator < 0, r.Numerator, detail::abs_den(r.Denominator), n);
    }
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

// from_chars<B, F>(text) / from_chars_exact<B>(text) — the std::string_view
// forms of the pointer-pair overloads.
template <insidable B, policy_flag F = none>
[[nodiscard]] constexpr std::expected<B, errc> from_chars(std::string_view text) {
    return from_chars<B, F>(text.data(), text.data() + text.size());
}
template <insidable B>
[[nodiscard]] constexpr std::expected<B, errc> from_chars_exact(std::string_view text) {
    return from_chars_exact<B>(text.data(), text.data() + text.size());
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

namespace beman::inside::detail {
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
        for (char& c : s)
            if (c == 'e')
                c = 'E';
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
} // namespace beman::inside::detail

namespace beman::inside::detail {
// The rounding a format spec applies to B: B's own mode when its policy
// names one (as storing at that precision would round), else ties to even.
template <insidable B>
inline constexpr round_mode display_rounding =
    has_any_flag(policy_of<B>, round_floor | round_ceil | round_nearest | round_half_even | snap)
        ? rounding_of(policy_of<B>)
        : round_mode::half_even;

// x's exact value as a sign and a magnitude fraction, for the exact specs.
template <insidable B>
auto spec_value(const B& b) {
    if constexpr (fp_raw<B>)
        return exact_of_double(as_double(b));
    else
        return exact_of(b);
}
template <std::size_t K>
bool spec_negative(const exact_frac<K>& f) {
    return f.Num.negative();
}
template <std::size_t K>
wide_uint<K> spec_magnitude(const exact_frac<K>& f) {
    return wide_uint<K>{f.Num.negative() ? -f.Num : f.Num};
}
template <std::size_t K>
wide_uint<K> spec_denominator(const exact_frac<K>& f) {
    return wide_uint<K>{f.Den};
}
} // namespace beman::inside::detail

// Empty `{}` prints to_string. A notched integer grid within imax takes the
// integer specs (std::formatter<imax>: {:>4}, {:#x}); every other inside takes
// the exact specs ({:.2f}, {:e}, {:g}, fill / align / sign / width), rounded
// from the exact value by the type's rounding mode (ties to even without one).
template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::formatter<beman::inside::inside<G, P>>
    : beman::inside::detail::numeric_spec_formatter<
          std::conditional_t<beman::inside::detail::is_integer_aligned<beman::inside::inside<G, P>> && G.Notch != 0 &&
                                 beman::inside::detail::values_fit_imax<beman::inside::inside<G, P>>,
                             std::formatter<beman::inside::imax>,
                             beman::inside::detail::exact_format_spec>> {
    using B = beman::inside::inside<G, P>;
    // Integer formatting only for a notched integer grid: a continuous grid
    // (notch 0) holds fractions even between integer bounds.
    static constexpr bool integer_path =
        beman::inside::detail::is_integer_aligned<B> && G.Notch != 0 && beman::inside::detail::values_fit_imax<B>;

    template <typename Ctx>
    auto format(const B& b, Ctx& ctx) const {
        namespace d = beman::inside::detail;
        if constexpr (integer_path)
            return this->Numeric.format(d::to_value(b), ctx);
        else if (!this->HasSpec)
            return std::format_to(ctx.out(), "{}", beman::inside::to_string(b));
        else {
            const auto  v     = d::spec_value(b);
            std::string plain = beman::inside::to_string(b);
            if (!plain.empty() && plain[0] == '-')
                plain.erase(0, 1);
            const bool neg = d::spec_negative(v);
            return this->Numeric.write(
                neg,
                this->Numeric.body(d::spec_magnitude(v), d::spec_denominator(v), plain, neg, d::display_rounding<B>),
                ctx);
        }
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
        std::string plain = beman::inside::to_string(r);
        if (!plain.empty() && plain[0] == '-')
            plain.erase(0, 1);
        const bool neg = r.Denominator < 0 && r.Numerator != 0;
        return Numeric.write(neg,
                             Numeric.body(d::wide_uint<1>{r.Numerator},
                                          d::wide_uint<1>{d::abs_den(r.Denominator)},
                                          plain,
                                          neg,
                                          d::round_mode::half_even),
                             ctx);
    }
};

#endif // __cpp_lib_format

#endif // BEMAN_INSIDE_IO_HPP
