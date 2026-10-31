// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: text for quantities, as io.hpp is for insides — a unit's symbol
// (unit_symbol), a quantity or point as "5 km" (to_string, <<, std::format)
// and parse<Q>. Brings <string>, <ostream> and <format> through io.hpp;
// units.hpp alone does not.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_UNITS_IO_HPP
#define BEMAN_INSIDE_UNITS_IO_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/io.hpp>
#include <beman/inside/units.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <ostream>
#include <string>
#include <string_view>

namespace beman::inside::units {
// parse<Q, F>(text) / parse_exact<Q>(text): the number, in Q's unit.
template <class Q, policy_flag F = none>
    requires quantity_like<Q>
[[nodiscard]] constexpr std::expected<Q, errc> parse(std::string_view text) {
    return detail::wrap_out<Q>(beman::inside::parse<typename Q::rep, F>(text));
}
template <class Q>
    requires quantity_like<Q>
[[nodiscard]] constexpr std::expected<Q, errc> parse_exact(std::string_view text) {
    return detail::wrap_out<Q>(beman::inside::parse_exact<typename Q::rep>(text));
}

//---------------------------------------------------------------------------
// Text: a unit's symbol (kg m²/s², km/h, 1/s), a quantity as "5 km".
//---------------------------------------------------------------------------
enum class text_encoding : unsigned char { utf8, ascii };

namespace detail {
constexpr std::string_view prefix_symbol(int p, text_encoding e) {
    // 10^-2 … 10^2, then 10^-30 … 10^30 in steps of 3.
    constexpr std::string_view near_one[]  = {"c", "d", "", "da", "h"};
    constexpr std::string_view thousands[] = {"q", "r", "y", "z", "a", "f", "p", "n", "µ", "m", "",
                                              "k", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"};
    if (p == -6 && e == text_encoding::ascii)
        return "u";
    if (p >= -2 && p <= 2)
        return near_one[p + 2];
    if (p >= -30 && p <= 30 && p % 3 == 0)
        return thousands[(p + 30) / 3];
    return "?";
}

inline void append_exponent(std::string& s, int e, text_encoding enc) {
    if (e == 1)
        return;
    if (enc == text_encoding::ascii) {
        s += '^';
        s += std::to_string(e);
        return;
    }
    static constexpr std::string_view sup[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    if (e < 0) {
        s += "⁻";
        e = -e;
    }
    const std::string digits = std::to_string(e);
    for (char c : digits)
        s += sup[c - '0'];
}

// The atoms of one sign with |exponent| (sign ±1), or all of them with their
// own exponent (sign 0).
inline void append_atoms(std::string& s, const unit& u, int sign, text_encoding enc) {
    bool first = true;
    for (std::int8_t i = 0; i < u.Size; ++i) {
        const unit_atom& a = u.Atoms[i];
        if (sign != 0 && (a.Exp > 0) != (sign > 0))
            continue;
        if (!first)
            s += ' ';
        first = false;
        s += prefix_symbol(a.Prefix, enc);
        s += enc == text_encoding::utf8 ? a.U->Utf8 : a.U->Ascii;
        append_exponent(s, sign == 0 ? a.Exp : a.Exp * sign, enc);
    }
}
} // namespace detail

// The symbol of a unit: "km", "kg m²/s²", "s⁻¹", "[1/381 ft]" (a scaled unit).
inline std::string unit_symbol(const unit& u, text_encoding enc = text_encoding::utf8) {
    std::string s;
    const int   num =
        static_cast<int>(std::count_if(u.Atoms, u.Atoms + u.Size, [](const auto& a) { return a.Exp > 0; }));
    const int den = u.Size - num;
    if (num == 0) // only a denominator: s⁻¹, m⁻¹ s⁻¹
        detail::append_atoms(s, u, 0, enc);
    else
        detail::append_atoms(s, u, 1, enc);
    if (num > 0 && den > 0) {
        s += '/';
        if (den > 1)
            s += '(';
        detail::append_atoms(s, u, -1, enc);
        if (den > 1)
            s += ')';
    }
    if (!u.Scale.is_one()) {
        // "1/1250", "π", "2π", "π⁻¹" (ASCII "pi^-1").
        std::string scale =
            u.Scale.Pi != 0 && u.Scale.Ratio == grid_rational{1} ? "" : beman::inside::to_string(u.Scale.Ratio);
        if (u.Scale.Pi != 0) {
            scale += enc == text_encoding::utf8 ? "π" : (scale.empty() ? "pi" : "*pi");
            detail::append_exponent(scale, u.Scale.Pi, enc);
        }
        s = "[" + scale + (s.empty() ? "" : " " + s) + "]";
    }
    return s;
}
template <unit U>
inline std::string unit_symbol(unit_t<U>, text_encoding enc = text_encoding::utf8) {
    return unit_symbol(U, enc);
}

// "5 km", "90°", "3" (unit one).
template <reference R, class V>
std::string to_string(const quantity<R, V>& q, text_encoding enc = text_encoding::utf8) {
    std::string       s   = beman::inside::to_string(q.numerical_value());
    const std::string sym = unit_symbol(R.U, enc);
    if (sym.empty())
        return s;
    return s + (detail::space_before(R.U) ? " " : "") + sym;
}

template <reference R, class V>
std::ostream& operator<<(std::ostream& os, const quantity<R, V>& q) {
    return os << units::to_string(q);
}

// A point as its quantity from its origin: "20 ℃".
template <reference R, detail::origin_ref O, class V>
std::string to_string(const quantity_point<R, O, V>& p, text_encoding enc = text_encoding::utf8) {
    return units::to_string(p.quantity_from_origin(), enc);
}
template <reference R, detail::origin_ref O, class V>
std::ostream& operator<<(std::ostream& os, const quantity_point<R, O, V>& p) {
    return os << units::to_string(p);
}
} // namespace beman::inside::units

// std::format: the spec formats the number as for its inside ({:.2f}, {:>8}),
// then the unit symbol follows ("5 km", "90°").
template <beman::inside::units::reference R, class V>
struct std::formatter<beman::inside::units::quantity<R, V>> : std::formatter<V> {
    template <typename Ctx>
    auto format(const beman::inside::units::quantity<R, V>& q, Ctx& ctx) const {
        namespace u           = beman::inside::units;
        auto              out = std::formatter<V>::format(q.numerical_value(), ctx);
        const std::string sym = u::unit_symbol(R.U);
        if (sym.empty())
            return out;
        if constexpr (u::detail::space_before(R.U))
            *out++ = ' ';
        return std::ranges::copy(sym, out).out;
    }
};

template <beman::inside::units::reference R, beman::inside::units::detail::origin_ref O, class V>
struct std::formatter<beman::inside::units::quantity_point<R, O, V>>
    : std::formatter<beman::inside::units::quantity<R, V>> {
    template <typename Ctx>
    auto format(const beman::inside::units::quantity_point<R, O, V>& p, Ctx& ctx) const {
        return std::formatter<beman::inside::units::quantity<R, V>>::format(p.quantity_from_origin(), ctx);
    }
};

#endif // BEMAN_INSIDE_UNITS_IO_HPP
