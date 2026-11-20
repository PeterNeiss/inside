// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: text for quantities, as io.hpp is for insides: a unit's symbol (unit_symbol), a quantity or point as
// "5 km" (to_string, <<, std::format), and read back with its unit (parse<Q>, >> into an expected).
// Brings <string>, <ostream> and <format> through io.hpp; units.hpp alone does not.
#ifndef INSIDE_UNITS_IO_HPP
#define INSIDE_UNITS_IO_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/detail/unit_text.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>

namespace ins
{
  // Reading a quantity or a point from text, with its unit (the inverse of to_string):
  //   parse<Q, F>(text)           "12.5 km", "1.5 h", "21.5 ℃": the number, then its unit
  //   parse<Q, F>(number, unit)   the two apart: a JSON key price[USD/item], a CSV header
  //   parse<Q, F>(number, u)      a bare number in a unit named in code (u a unit tag)
  //   parse_exact<Q>(…)           the same, accepting only a value Q holds exactly
  // The unit may be any unit visible at the call that converts exactly to Q's (a rational factor): the catalog's
  // and your own, with SI prefixes, products, quotients and powers ("km/h", "kg m²/s²"). The number is multiplied
  // by that factor exactly (a point also shifted between origins), then stored by Q's number type and the flags F.
  // A single unit's kind must add to Q's (Hz is no Bq); a compound unit's kind is Q's.
  // Errors: missing_unit, unknown_unit, incompatible_unit, then the number's and the store's own.
  template <class Q, policy_flag F = detail::none, detail::unit_set Units = detail::visible_units<Q>()>
    requires quantity_like<Q> || detail::is_quantity_point_v<Q>
  [[nodiscard]] constexpr std::expected<Q, error> parse(std::string_view text)
  {
    const auto t{detail::split_quantity_text(text)};
    return detail::parse_quantity_text<Q, F, false, Units>(t.Number, t.Unit);
  }
  template <class Q, policy_flag F = detail::none, detail::unit_set Units = detail::visible_units<Q>()>
    requires quantity_like<Q> || detail::is_quantity_point_v<Q>
  [[nodiscard]] constexpr std::expected<Q, error> parse(std::string_view number, std::string_view unit)
  {
    return detail::parse_quantity_text<Q, F, false, Units>(number, unit);
  }
  template <class Q, policy_flag F = detail::none, class Tag>
    requires(quantity_like<Q> || detail::is_quantity_point_v<Q>) && detail::is_reference_tag_v<Tag>
         && (detail::text_unit_converts<Q, reference_of(Tag{}).U>())
  [[nodiscard]] constexpr std::expected<Q, error> parse(std::string_view number, Tag)
  {
    return detail::parse_quantity_in<Q, F, false, reference_of(Tag{}).U>(number);
  }
  template <class Q, detail::unit_set Units = detail::visible_units<Q>()>
    requires quantity_like<Q> || detail::is_quantity_point_v<Q>
  [[nodiscard]] constexpr std::expected<Q, error> parse_exact(std::string_view text)
  {
    const auto t{detail::split_quantity_text(text)};
    return detail::parse_quantity_text<Q, detail::none, true, Units>(t.Number, t.Unit);
  }
  template <class Q, detail::unit_set Units = detail::visible_units<Q>()>
    requires quantity_like<Q> || detail::is_quantity_point_v<Q>
  [[nodiscard]] constexpr std::expected<Q, error> parse_exact(std::string_view number, std::string_view unit)
  {
    return detail::parse_quantity_text<Q, detail::none, true, Units>(number, unit);
  }
  template <class Q, class Tag>
    requires(quantity_like<Q> || detail::is_quantity_point_v<Q>) && detail::is_reference_tag_v<Tag>
         && (detail::text_unit_converts<Q, reference_of(Tag{}).U>())
  [[nodiscard]] constexpr std::expected<Q, error> parse_exact(std::string_view number, Tag)
  {
    return detail::parse_quantity_in<Q, detail::none, true, reference_of(Tag{}).U>(number);
  }

  // is >> e for e a std::expected of a quantity or a point: a number token, then a unit token unless the number's
  // token carries it (90°). e holds the value or the error; the stream stays good on a parse error, and only
  // running out of input sets failbit (e: invalid_format without a number, missing_unit without a unit).
  // (No >> into a plain quantity: every way to read one reports through an expected.)
  template <class Q, detail::unit_set Units = detail::visible_units<Q>()>
    requires quantity_like<Q> || detail::is_quantity_point_v<Q>
  std::istream& operator>>(std::istream& stream, std::expected<Q, error>& e)
  {
    std::string number;
    if (!(stream >> number))
    {
      e = std::unexpected{error::invalid_format};
      return stream;
    }
    const auto t{detail::split_quantity_text(number)};
    if (!t.Unit.empty())
    {
      e = detail::parse_quantity_text<Q, detail::none, false, Units>(t.Number, t.Unit);
      return stream;
    }
    std::string unit;
    if constexpr (!(detail::text_target<Q>::type::reference.U == unit_value{}))
      if (!(stream >> unit))
      {
        e = std::unexpected{error::missing_unit};
        return stream;
      }
    e = detail::parse_quantity_text<Q, detail::none, false, Units>(number, unit);
    return stream;
  }

  // Text: a unit's symbol (kg m²/s², km/h, 1/s), a quantity as "5 km".
  enum class text_encoding : unsigned char { utf8, ascii };

  namespace detail
  {
    constexpr std::string_view prefix_symbol(int p, text_encoding e)
    {
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

    inline void append_exponent(std::string& s, int e, text_encoding enc)
    {
      if (e == 1)
        return;
      if (enc == text_encoding::ascii)
      {
        s += '^';
        s += std::to_string(e);
        return;
      }
      static constexpr std::string_view sup[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
      if (e < 0)
      {
        s += "⁻";
        e = -e;
      }
      const std::string digits{std::to_string(e)};
      for (char c : digits)
        s += sup[c - '0'];
    }

    // The atoms of one sign with |exponent| (sign ±1), or all of them with their own exponent (sign 0).
    // The blade atoms come last, in their order: 2 m e₁.
    inline void append_atoms(std::string& s, const unit_value& u, int sign, text_encoding enc)
    {
      bool first{true};
      for (const bool blades : {false, true})
        for (std::int8_t i = 0; i < u.Size; ++i)
        {
          const unit_atom& a{u.Atoms[i]};
          if ((sign != 0 && (a.Exp > 0) != (sign > 0)) || is_blade_atom(a) != blades)
            continue;
          if (!first)
            s += ' ';
          first = false;
          s += prefix_symbol(a.Prefix, enc);
          s += enc == text_encoding::utf8 ? a.U->Utf8 : a.U->ascii();
          append_exponent(s, sign == 0 ? a.Exp : a.Exp * sign, enc);
        }
    }
  } // namespace detail

  // The symbol of a unit: "km", "kg m²/s²", "s⁻¹", "[1/381 ft]" (a scaled unit).
  inline std::string unit_symbol(const unit_value& u, text_encoding enc = text_encoding::utf8)
  {
    std::string s;
    const int   num{
      static_cast<int>(std::count_if(u.Atoms, u.Atoms + u.Size, [](const auto& a) { return a.Exp > 0; }))};
    const int   den{u.Size - num};
    if (num == 0) // only a denominator: s⁻¹, m⁻¹ s⁻¹
      detail::append_atoms(s, u, 0, enc);
    else
      detail::append_atoms(s, u, 1, enc);
    if (num > 0 && den > 0)
    {
      s += '/';
      if (den > 1)
        s += '(';
      detail::append_atoms(s, u, -1, enc);
      if (den > 1)
        s += ')';
    }
    if (!u.Scale.is_one())
    {
      // "1/1250", "π", "2π", "π⁻¹" (ASCII "pi^-1").
      std::string scale{u.Scale.Pi != 0 && u.Scale.Ratio == grid_number{1} ? "" : ins::to_string(u.Scale.Ratio)};
      if (u.Scale.Pi != 0)
      {
        scale += enc == text_encoding::utf8 ? "π" : (scale.empty() ? "pi" : "*pi");
        detail::append_exponent(scale, u.Scale.Pi, enc);
      }
      s = "[" + scale + (s.empty() ? "" : " " + s) + "]";
    }
    return s;
  }
  template <unit_value U>
  inline std::string unit_symbol(unit_t<U>, text_encoding enc = text_encoding::utf8)
  {
    return unit_symbol(U, enc);
  }

  // "5 km", "90°", "3" (unit one).
  template <reference R, class V>
  std::string to_string(const quantity<R, V>& q, text_encoding enc = text_encoding::utf8)
  {
    std::string       s  {ins::to_string(ins::detail::value_of(q))};
    const std::string sym{unit_symbol(R.U, enc)};
    if (sym.empty())
      return s;
    return s + (detail::space_before(R.U) ? " " : "") + sym;
  }

  template <reference R, class V>
  std::ostream& operator<<(std::ostream& os, const quantity<R, V>& q)
  {
    return os << ins::to_string(q);
  }

  // A multivector (units/geoalg.hpp): its parts joined by " + ", a negative one by " - ":
  // "3 - 4 i", "2 m e₁ + 1 m e₂"; the zero is "0".
  template <class... P>
  std::string to_string(const multivector<P...>& x, text_encoding enc = text_encoding::utf8)
  {
    if constexpr (sizeof...(P) == 0)
      return "0";
    else
    {
      std::string s;
      const auto append = [&](const auto& p)
      {
        std::string t;
        if constexpr (quantity_like<std::remove_cvref_t<decltype(p)>>)
          t = ins::to_string(p, enc);
        else
          t = ins::to_string(p);
        if (s.empty())
          s = std::move(t);
        else if (t.starts_with('-'))
          s += " - " + t.substr(1);
        else
          s += " + " + t;
      };
      std::apply([&](const auto&... p) { (append(p), ...); }, x.parts());
      return s;
    }
  }
  template <class... P>
  std::ostream& operator<<(std::ostream& os, const multivector<P...>& x)
  {
    return os << ins::to_string(x);
  }

  // A point as its quantity from its origin: "20 ℃".
  template <reference R, origin_ref O, class V>
  std::string to_string(const quantity_point<R, O, V>& p, text_encoding enc = text_encoding::utf8)
  {
    return ins::to_string(p.quantity_from_origin(), enc);
  }
  template <reference R, origin_ref O, class V>
  std::ostream& operator<<(std::ostream& os, const quantity_point<R, O, V>& p)
  {
    return os << ins::to_string(p);
  }
} // namespace ins

// std::format: the spec formats the number as for its inside ({:.2f}, {:>8}), then the unit symbol follows
// ("5 km", "90°").
template <ins::reference R, class V>
struct std::formatter<ins::quantity<R, V>> : std::formatter<V>
{
  template <typename Ctx>
  auto format(const ins::quantity<R, V>& q, Ctx& ctx) const
  {
    namespace u = ins;
    auto              out{std::formatter<V>::format(ins::detail::value_of(q), ctx)};
    const std::string sym{u::unit_symbol(R.U)};
    if (sym.empty())
      return out;
    if constexpr (u::detail::space_before(R.U))
      *out++ = ' ';
    return std::ranges::copy(sym, out).out;
  }
};

// A multivector as to_string prints it; the spec pads the whole text ({:>20}).
template <class... P>
struct std::formatter<ins::multivector<P...>> : std::formatter<std::string_view>
{
  template <typename Ctx>
  auto format(const ins::multivector<P...>& x, Ctx& ctx) const
  {
    return std::formatter<std::string_view>::format(ins::to_string(x), ctx);
  }
};

template <ins::reference R, ins::origin_ref O, class V>
struct std::formatter<ins::quantity_point<R, O, V>> : std::formatter<ins::quantity<R, V>>
{
  template <typename Ctx>
  auto format(const ins::quantity_point<R, O, V>& p, Ctx& ctx) const
  {
    return std::formatter<ins::quantity<R, V>>::format(p.quantity_from_origin(), ctx);
  }
};

#endif // INSIDE_UNITS_IO_HPP
