// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The units a text may name (every unit visible where it is read, found by reflection) and the grammar of a
// unit's text: shared by a quantity read from text (units/detail/unit_text.hpp) and a quantity's type spelled as
// text (units/detail/unit_key.hpp).
// Internal: include <inside/units.hpp>.
#ifndef INSIDE_UNITS_DETAIL_UNIT_GRAMMAR_HPP
#define INSIDE_UNITS_DETAIL_UNIT_GRAMMAR_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units/detail/algebra.hpp>

#include <cstddef>
#include <cstdint>
#include <meta>
#include <string_view>
#include <vector>

namespace ins::detail
{
  // ---- The units a text may name: every unit visible at the call ----

  // The units a quantity's text may name, interned: named units with their prefixes (a tag of km names km), in
  // groups searched one after another (units/detail/unit_key.hpp: the catalog's tables, then every other unit tag
  // visible where the text is read). A template argument, so each set is a specialization of its own.
  struct unit_set
  {
    const unit_def* const* Units{};
    const std::int8_t*     Prefixes{};
    const std::size_t*     Ends{}; // where each group ends
    std::size_t            Groups{0};
    std::size_t            Size{0};
  };

  // The namespaces searched: all but std, detail and reserved names. Unnamed namespaces are searched.
  consteval bool searched_namespace(std::meta::info ns)
  {
    if (std::meta::is_namespace_alias(ns))
      return false;
    if (!std::meta::has_identifier(ns))
      return true;
    const std::string_view name{std::meta::identifier_of(ns)};
    return name != "std" && name != "detail" && !name.starts_with('_');
  }

  // The tags' definitions a key's text may name: the units (each with its prefix: a tag of km names km), kinds and
  // origins of every tag variable read.
  struct tag_vectors
  {
    std::vector<unit_atom>         Units; // (exponent 1)
    std::vector<const kind_def*>   Kinds;
    std::vector<const origin_def*> Origins;

    consteval void add(unit_atom a)
    {
      a.Exp = 1;
      for (const unit_atom& v : Units)
        if (v.Prefix == a.Prefix && same(v.U, a.U))
          return;
      Units.push_back(a);
    }
  };

  // The units, kinds and origins of the tags only (si::metre, isq::height, si::ice_point, defined_by<cent_def>, a
  // one-line unit<"ftm", …>), not of the definition objects: a type named while a definition object is initialized
  // (unit_def cent_def{"ct", 0.01_q * euro}) would read that object before its value exists. A tag's value is in
  // its type, and a tag exists only once its definition does. Every catalog definition has its tag.
  // Basis vectors and polynomial variables are units too here.
  // (skip: a namespace already read, the catalog's; nested: whether to read the namespaces within ns too.)
  consteval void collect_tags(std::meta::info ns, tag_vectors& out, std::meta::info skip = {}, bool nested = true)
  {
    for (const std::meta::info m : std::meta::members_of(ns, std::meta::access_context::unchecked()))
      if (std::meta::is_namespace(m))
      {
        if (nested && m != skip && searched_namespace(m))
          collect_tags(m, out, skip);
      }
      else if (std::meta::is_variable(m))
      {
        const std::meta::info t{std::meta::remove_cvref(std::meta::type_of(m))};
        if (!std::meta::has_template_arguments(t))
          continue;
        const std::meta::info of{std::meta::template_of(t)};
        if (of == ^^unit_t)
        {
          const unit_value u{std::meta::extract<unit_value>(std::meta::template_arguments_of(t)[0])};
          for (std::int8_t i = 0; i < u.size(); ++i)
            out.add(u.Atoms[i]);
        }
        else if (of == ^^kind_t)
        {
          const kind_def* k{std::meta::extract<const kind_def*>(std::meta::template_arguments_of(t)[0])};
          bool            known{false};
          for (const kind_def* v : out.Kinds)
            known = known || same(v, k);
          if (!known)
            out.Kinds.push_back(k);
        }
        else if (of == ^^origin_t)
        {
          const origin_def* o{std::meta::extract<origin_ref>(std::meta::template_arguments_of(t)[0])};
          bool              known{false};
          for (const origin_def* v : out.Origins)
            known = known || same(v, o);
          if (!known)
            out.Origins.push_back(o);
        }
      }
  }

  // ---- The grammar of a unit's text ----

  inline constexpr int max_text_exponent  = 9;  // |exponent| of one symbol
  inline constexpr int max_text_exponents = 12; // the sum of |exponent| over a unit's symbols
  inline constexpr int text_prefix_bits   = 100; // 10^30, the largest prefix
  inline constexpr std::size_t text_scale_limbs = 4; // a scaled unit's [N/D …]

  struct text_prefix
  {
    std::string_view Sym;
    std::int8_t      Pow;
  };
  // The longest first: da before d.
  inline constexpr text_prefix text_prefixes[] = {
    {"da", 1},  {"q", -30}, {"r", -27}, {"y", -24}, {"z", -21}, {"a", -18}, {"f", -15}, {"p", -12}, {"n", -9},
    {"µ", -6},  {"μ", -6},  {"u", -6},  {"m", -3},  {"c", -2},  {"d", -1},  {"h", 2},   {"k", 3},   {"M", 6},
    {"G", 9},   {"T", 12},  {"P", 15},  {"E", 18},  {"Z", 21},  {"Y", 24},  {"R", 27},  {"Q", 30}};

  // The symbol of the prefix 10^p (the first of the table: µ in UTF-8, u in ASCII); empty for none, and for p = 0.
  constexpr std::string_view prefix_text(int p, bool ascii)
  {
    if (p != 0)
      for (const text_prefix& x : text_prefixes)
        if (x.Pow == p && (!ascii || (x.Sym != "µ" && x.Sym != "μ")))
          return x.Sym;
    return {};
  }

  // The superscript digits and the superscript minus (⁻).
  inline constexpr std::string_view text_superscripts[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹", "⁻"};

  // Whether w is the symbol of the unit u with the prefix p: the prefix's symbol (µ, μ or u for micro) and the
  // unit's ASCII symbol, or its symbol where that is not ASCII (µs, Ω, ℃). A plain symbol with an ASCII one of its
  // own is only printed: Boltzmann's k reads as k_B, so that k stays free (a quaternion's).
  constexpr bool names_unit(const unit_def* u, std::int8_t p, std::string_view w)
  {
    const auto rest = [&](std::string_view s)
    {
      if (s == u->ascii())
        return true;
      bool plain{true};
      for (char c : u->Utf8)
        plain = plain && static_cast<unsigned char>(c) < 0x80;
      return !plain && s == u->Utf8;
    };
    if (p == 0)
      return rest(w);
    for (const text_prefix& x : text_prefixes)
      if (x.Pow == p && w.starts_with(x.Sym) && rest(w.substr(x.Sym.size())))
        return true;
    return false;
  }

  // A separator between a unit's factors at i: a space, '*' or the middle dot; its length in bytes, 0 for none.
  constexpr std::size_t text_separator(std::string_view t, std::size_t i)
  {
    if (t[i] == ' ' || t[i] == '*')
      return 1;
    return t.substr(i, 2) == "·" ? 2 : 0;
  }

  // A superscript at i: its digit, 10 for ⁻, -1 for none; len its bytes.
  constexpr int superscript_at(std::string_view t, std::size_t i, std::size_t& len)
  {
    for (int d = 0; d < 11; ++d)
      if (t.substr(i, text_superscripts[d].size()) == text_superscripts[d])
      {
        len = text_superscripts[d].size();
        return d;
      }
    return -1;
  }

  // Where a symbol starting at i ends: at a separator, '/', a parenthesis or bracket, '^' or a superscript.
  constexpr std::size_t symbol_end(std::string_view t, std::size_t i)
  {
    std::size_t len{0};
    while (i < t.size() && text_separator(t, i) == 0 && t[i] != '/' && t[i] != '(' && t[i] != ')' && t[i] != '['
           && t[i] != ']' && t[i] != '^' && superscript_at(t, i, len) < 0)
      ++i;
    return i;
  }

  // The exponent after a symbol: ^n, ^-n, or superscript digits after an optional ⁻; 1 when none, 0 when invalid.
  constexpr int read_text_exponent(std::string_view t, std::size_t& i)
  {
    int         sign{1}, e{0}, digits{0};
    std::size_t len{0};
    if (i < t.size() && t[i] == '^')
    {
      ++i;
      if (i < t.size() && (t[i] == '-' || t[i] == '+'))
        sign = t[i++] == '-' ? -1 : 1;
      for (; i < t.size() && t[i] >= '0' && t[i] <= '9' && digits < 3; ++i, ++digits)
        e = e * 10 + (t[i] - '0');
      return digits == 0 || digits == 3 ? 0 : sign * e;
    }
    int d{superscript_at(t, i, len)};
    if (d < 0)
      return 1;
    if (d == 10)
    {
      sign = -1;
      i += len;
      d    = superscript_at(t, i, len);
    }
    for (; d >= 0 && d < 10 && digits < 3; ++digits)
    {
      e = e * 10 + d;
      i += len;
      d = superscript_at(t, i, len);
    }
    return digits == 0 || digits == 3 ? 0 : sign * e;
  }

  // Where a number's text ends, in the grammar parse reads: a sign, then a decimal (digits, '.', ' separators, an
  // e exponent), a 0x hexadecimal (with a p exponent) or a 0b binary number, then /denominator.
  constexpr std::size_t number_text_end(std::string_view t)
  {
    const auto digit = [&](std::size_t i, int base)
    {
      if (i >= t.size())
        return false;
      const char c{t[i]};
      return (c >= '0' && c <= '9' && c - '0' < base)
             || (base == 16 && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));
    };
    const auto exponent = [&](std::size_t& i, char lower)
    {
      if (i < t.size() && (t[i] == lower || t[i] == lower - 32))
      {
        std::size_t j{i + 1};
        if (j < t.size() && (t[j] == '+' || t[j] == '-'))
          ++j;
        if (digit(j, 10))
        {
          for (i = j; digit(i, 10); ++i)
          {
          }
        }
      }
    };
    const auto number = [&](std::size_t i)
    {
      if (i < t.size() && (t[i] == '+' || t[i] == '-'))
        ++i;
      const int base{t.substr(i, 2) == "0x" || t.substr(i, 2) == "0X"   ? 16
                     : t.substr(i, 2) == "0b" || t.substr(i, 2) == "0B" ? 2
                                                                          : 10};
      if (base != 10)
        i += 2;
      while (digit(i, base) || (i < t.size() && (t[i] == '.' || t[i] == '\'')))
        ++i;
      if (base == 10)
        exponent(i, 'e');
      else if (base == 16)
        exponent(i, 'p');
      return i;
    };
    std::size_t i{number(0)};
    if (i + 1 < t.size() && t[i] == '/' && digit(i + 1, 10))
      i = number(i + 1);
    return i;
  }
} // namespace ins::detail

#endif // INSIDE_UNITS_DETAIL_UNIT_GRAMMAR_HPP
