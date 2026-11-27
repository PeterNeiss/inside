// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A quantity's type spelled as text: the unit of id<"[0..100] [km/h]">, read among the units visible where the
// type is named (as parse<Q> reads one), and written back as its canonical text. The text is ASCII
// (kg*m^2/s^2, us, [1/381*ft]): GCC prints the other bytes of a key as octal escapes.
// Internal: include <inside/units.hpp>.
#ifndef INSIDE_UNITS_DETAIL_UNIT_KEY_HPP
#define INSIDE_UNITS_DETAIL_UNIT_KEY_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/key.hpp>
#include <inside/units/detail/algebra.hpp>
#include <inside/units/detail/unit_grammar.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ins::detail
{
  // The units a key's text may name: the unit of every unit tag visible where it is read (collect_tag_units).
  consteval std::vector<const unit_def*> key_text_units()
  {
    std::vector<const unit_def*> units;
    collect_tag_units(^^::, units);
    return units;
  }

  // ---- Reading ----

  // A unit's text read into its value: symbols, each with an optional prefix and an exponent (^n, ^-n or
  // superscripts), joined by '*', '·' or a space, then '/' and a symbol or a parenthesized product (or 1/…);
  // a scaled unit in brackets ([1/381*ft], [pi*rad], [2*pi^-1]); 1 is the unit one.
  // False where it does not read, with what was expected.
  struct unit_key_reader
  {
    const std::vector<const unit_def*>& Units;
    std::string_view                    Expected{};

    consteval bool fail(std::string_view expected)
    {
      Expected = expected;
      return false;
    }

    // The one unit a symbol names exactly (several units equal by content are one), else null.
    // (By index: GCC under -fsanitize=pointer-compare rejects a pointer test in a constant evaluation.)
    consteval bool match_exact(std::string_view w, std::size_t& found, bool& ambiguous) const
    {
      std::size_t n{0};
      for (std::size_t i = 0; i < Units.size(); ++i)
        if (Units[i]->Utf8 == w || Units[i]->ascii() == w)
        {
          if (n++ > 0)
            ambiguous = true;
          found = i;
        }
      return n == 1;
    }
    // A symbol exactly, else a prefix and a symbol (min is the minute, mm the millimetre).
    consteval bool match_symbol(std::string_view w, unit_atom& a, bool& ambiguous) const
    {
      std::size_t i{0};
      if (match_exact(w, i, ambiguous))
      {
        a = unit_atom{Units[i], 0, 1};
        return true;
      }
      if (ambiguous)
        return false;
      for (const text_prefix& p : text_prefixes)
        if (w.size() > p.Sym.size() && w.starts_with(p.Sym) && match_exact(w.substr(p.Sym.size()), i, ambiguous))
        {
          a = unit_atom{Units[i], p.Pow, 1};
          return true;
        }
      return false;
    }

    // factors ['/' (symbol | '(' factors ')')], or 1/…
    consteval bool read_plain(std::string_view t, unit_value& u)
    {
      std::size_t i{0};
      int         total{0};
      const auto  term = [&](int sign)
      {
        const std::size_t end{symbol_end(t, i)};
        if (end == i)
          return fail("a unit's symbol");
        unit_atom a{};
        bool      ambiguous{false};
        if (!match_symbol(t.substr(i, end - i), a, ambiguous))
          return fail(ambiguous ? "a symbol one visible unit has (several have this one)"
                                : "a unit visible here (its symbol, with a prefix: km, us, kg*m/s^2)");
        i = end;
        const int e{read_text_exponent(t, i)};
        total += e < 0 ? -e : e;
        if (e == 0 || e > max_text_exponent || e < -max_text_exponent || total > max_text_exponents)
          return fail("an exponent of at most 9, 12 over the unit");
        a.Exp = static_cast<std::int8_t>(sign * e);
        if (u.size() == static_cast<std::int8_t>(max_unit_atoms))
          return fail("at most 6 units in a product");
        unit_add(u, a);
        return true;
      };
      const auto factors = [&](int sign)
      {
        if (!term(sign))
          return false;
        while (i < t.size())
        {
          const std::size_t s{text_separator(t, i)};
          if (s == 0)
            break;
          i += s;
          if (!term(sign))
            return false;
        }
        return true;
      };
      const bool numerator{!t.starts_with("1/")};
      if (numerator && !factors(1))
        return false;
      if (!numerator || (i < t.size() && t[i] == '/'))
      {
        i += numerator ? 1 : 2;
        if (i < t.size() && t[i] == '(')
        {
          ++i;
          if (!factors(-1))
            return false;
          if (i >= t.size() || t[i] != ')')
            return fail("')' to close the denominator");
          ++i;
        }
        else if (!term(-1))
          return false;
      }
      return i == t.size() || fail("the end of the unit, '*' or '/'");
    }

    // [N/D π^k·unit]: a scale (a number, π with an exponent, or both: 2*pi, pi^-1), then the unit.
    consteval bool read_scaled(std::string_view s, unit_value& u)
    {
      const std::size_t i{number_text_end(s)};
      grid_number       ratio{1};
      if (i > 0)
      {
        id_reader        r{s.substr(0, i)};
        std::string_view expected;
        if (!r.number(ratio, expected) || r.At != i || !(ratio > 0))
          return fail("a scale above 0");
      }
      std::size_t            at{i};
      const std::string_view rest{s.substr(i)};
      const std::size_t      pi{rest.starts_with("π") || rest.starts_with("pi") ? std::size_t{2}
                                : i > 0 && rest.starts_with("*pi")             ? std::size_t{3}
                                                                                : std::size_t{0}};
      std::int8_t pi_exp{0};
      if (pi != 0)
      {
        at += pi;
        const int e{read_text_exponent(s, at)};
        if (e == 0)
          return fail("an exponent of pi");
        pi_exp = static_cast<std::int8_t>(e);
      }
      else if (i == 0)
        return fail("a scale: a number, pi, or both");
      if (at < s.size())
      {
        const std::size_t sep{text_separator(s, at)};
        if (sep == 0 || !read_plain(s.substr(at + sep), u))
          return Expected.empty() ? fail("'*' and the unit") : false;
      }
      u.Scale = magnitude{ratio, pi_exp} * u.Scale;
      return true;
    }

    consteval bool read(std::string_view t, unit_value& u)
    {
      if (t == "1")
        return true; // the unit one
      if (t.starts_with('['))
      {
        if (!t.ends_with(']'))
          return fail("']' to close the scaled unit");
        return read_scaled(t.substr(1, t.size() - 2), u);
      }
      return read_plain(t, u);
    }
  };

  // ---- Writing ----

  // A unit's canonical text: its symbols (ASCII, with an ASCII prefix: us, not µs) in the unit's order, those with
  // a positive exponent first, then '/' and the others; a scale in brackets; 1 for the unit one.
  // False for a unit no text names: a basis vector or a polynomial variable, a prefix no symbol has, a symbol that
  // is not plain ASCII.
  consteval bool write_unit_text(text_out& t, const unit_value& u)
  {
    const auto plain = [](std::string_view s)
    {
      for (char c : s)
        if (static_cast<unsigned char>(c) >= 0x80 || c == ' ' || c == '*' || c == '/' || c == '^' || c == '['
            || c == ']' || c == '(' || c == ')')
          return false;
      return !s.empty();
    };
    const auto prefix = [&](int p, std::string_view& sym)
    {
      if (p == 0)
      {
        sym = {};
        return true;
      }
      for (const text_prefix& x : text_prefixes)
        if (x.Pow == p && plain(x.Sym))
        {
          sym = x.Sym;
          return true;
        }
      return false;
    };
    const std::int8_t n{u.size()};
    int               num{0};
    for (std::int8_t i = 0; i < n; ++i)
    {
      std::string_view p;
      if (is_basis_atom(u.Atoms[i]) || !plain(u.Atoms[i].U->ascii()) || !prefix(u.Atoms[i].Prefix, p))
        return false;
      num += u.Atoms[i].Exp > 0;
    }
    // The atoms of one sign with |exponent| (sign ±1), or all of them with their own exponent (sign 0).
    const auto atoms = [&](int sign)
    {
      bool first{true};
      for (std::int8_t i = 0; i < n; ++i)
      {
        const unit_atom& a{u.Atoms[i]};
        if (sign != 0 && (a.Exp > 0) != (sign > 0))
          continue;
        if (!first)
          t.put('*');
        first = false;
        std::string_view p;
        (void)prefix(a.Prefix, p);
        t.put(p);
        t.put(a.U->ascii());
        const int e{sign == 0 ? a.Exp : a.Exp * sign};
        if (e != 1)
        {
          t.put('^');
          if (e < 0)
            t.put('-');
          t.put(static_cast<std::uint64_t>(e < 0 ? -e : e));
        }
      }
    };
    const bool scaled{!u.Scale.is_one()};
    if (scaled)
    {
      t.put('[');
      if (u.Scale.Pi == 0 || !(u.Scale.Ratio == grid_number{1}))
        t.put(u.Scale.Ratio);
      if (u.Scale.Pi != 0)
      {
        t.put(u.Scale.Ratio == grid_number{1} ? "pi" : "*pi");
        if (u.Scale.Pi != 1)
        {
          t.put('^');
          if (u.Scale.Pi < 0)
            t.put('-');
          t.put(static_cast<std::uint64_t>(u.Scale.Pi < 0 ? -u.Scale.Pi : u.Scale.Pi));
        }
      }
      if (n > 0)
        t.put('*');
    }
    else if (n == 0)
      t.put('1');
    if (n > 0)
    {
      if (num == 0) // only a denominator: s^-1
        atoms(0);
      else
      {
        atoms(1);
        if (num < n)
        {
          t.put('/');
          if (n - num > 1)
            t.put('(');
          atoms(-1);
          if (n - num > 1)
            t.put(')');
        }
      }
    }
    if (scaled)
      t.put(']');
    return true;
  }

  // ---- For a key ----

  // The canonical text of a unit a key's text names (detail::key_of_text through this overload).
  template <std::size_t N>
  consteval bool canonical_unit_text(text_hook<N>, std::string_view text, text_out& out, std::string_view& expected)
  {
    const std::vector<const unit_def*> units{key_text_units()};
    unit_key_reader                    r{units};
    unit_value                         u;
    if (!r.read(text, u))
    {
      expected = r.Expected;
      return false;
    }
    if (!write_unit_text(out, u))
    {
      expected = "a unit with a text (a basis vector, a polynomial variable or a non-ASCII symbol has none)";
      return false;
    }
    return true;
  }

  // The unit a canonical unit text names, read once where it is first used (the units visible there),
  // and whether it reads.
  struct unit_read
  {
    unit_value U{};
    bool       Ok{false};
  };
  consteval unit_read read_unit_name(std::string_view text)
  {
    const std::vector<const unit_def*> units{key_text_units()};
    unit_key_reader                    r{units};
    unit_read                          u;
    u.Ok = r.read(text, u.U);
    return u;
  }
  template <unit_name T>
  inline constexpr unit_read unit_of_name = read_unit_name(T.view());

  // A key's unit text, as a unit_name.
  consteval unit_name unit_name_of(std::string_view text)
  {
    unit_name n;
    for (std::size_t i = 0; i < text.size() && i + 1 < sizeof n.Text; ++i)
      n.Text[i] = text[i];
    return n;
  }
  consteval unit_value checked_unit(const unit_read& u)
  {
    if (!u.Ok)
      constexpr_error<"units: a quantity's unit is no unit visible here (its symbol names none, or several)">();
    return u.U;
  }
  // The unit and the reference of a key with a unit: the unit and its own kind, as a unit tag gives.
  template <key K>
  inline constexpr unit_value key_unit = checked_unit(unit_of_name<unit_name_of(key_spec<K>.Unit)>);
  template <key K>
  inline constexpr reference key_reference = reference_of(key_unit<K>);

  // The canonical text of a unit, if it has one that reads back to it where this is first asked.
  struct unit_name_or_none
  {
    unit_name Name;
    bool      Ok{false};
  };
  consteval unit_name_or_none unit_name_for(unit_value u)
  {
    unit_name_or_none r;
    text_out          t{r.Name.Text, sizeof r.Name.Text};
    r.Ok = write_unit_text(t, u);
    return r;
  }

  // Whether a quantity on R is spelled by a key: R a unit and its own kind (as a unit tag gives, not a kind such as
  // isq::height[m]), with a text that reads back to that unit where the type is first formed.
  template <reference R>
  consteval bool reference_has_text()
  {
    if (!(R == reference_of(R.unit())))
      return false;
    constexpr unit_name_or_none n{unit_name_for(R.unit())};
    if constexpr (!n.Ok)
      return false;
    else
      return unit_of_name<n.Name>.Ok && unit_of_name<n.Name>.U == R.unit();
  }
} // namespace ins::detail

#endif // INSIDE_UNITS_DETAIL_UNIT_KEY_HPP
