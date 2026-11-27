// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A quantity's type spelled as text: the unit, kind and origin of id<"[0..100] [m]{height} @sea_level">, read
// among the tags visible where the type is named, and written back as their canonical text:
//   unit    its symbols (kg*m^2/s^2, us, [1/381*ft], e1*e2), ASCII where the unit has an ASCII symbol (GCC prints
//           the other bytes of a key as octal escapes)
//   kind    in braces after the unit, when it is not the unit's own: {height}, {any length*duration}, {none}
//   origin  after @, a point's: @ice_point, @0 for the zero of the dimension
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

// The catalog's tags, read once after the catalog (units/systems.hpp defines catalog_index::table).
namespace ins::detail::catalog_index
{
}

namespace ins::detail
{
  // The catalog's units, kinds and origins, interned.
  struct kind_set
  {
    const kind_def* const* Data{};
    std::size_t            Size{0};
  };
  struct origin_set
  {
    const origin_def* const* Data{};
    std::size_t              Size{0};
  };
  struct catalog_sets
  {
    unit_set   Units;
    kind_set   Kinds;
    origin_set Origins;
  };
  consteval catalog_sets intern_tags(const tag_vectors& v)
  {
    const auto u{std::define_static_array(v.Units)};
    const auto k{std::define_static_array(v.Kinds)};
    const auto o{std::define_static_array(v.Origins)};
    return {{u.data(), u.size()}, {k.data(), k.size()}, {o.data(), o.size()}};
  }

  // The catalog's tags where they have been read, else none (inside the catalog itself, while it is declared).
  // (A template on what asks, a text or a literal: a call without arguments is evaluated once, where it is first
  // asked, which is inside the catalog.)
  template <class Context>
  consteval catalog_sets catalog_table()
  {
    for (const std::meta::info m : std::meta::members_of(^^catalog_index, std::meta::access_context::unchecked()))
      if (std::meta::is_variable(m) && std::meta::identifier_of(m) == "table")
        return std::meta::extract<const catalog_sets&>(m);
    return {};
  }

  // A name of a kind or an origin: letters, digits and _ (a key's text spells it).
  constexpr bool identifier_name(std::string_view s)
  {
    for (char c : s)
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
        return false;
    return !s.empty();
  }

  // ---- Reading ----

  // A quantity's text read into its reference and origin.
  //   unit: symbols, each with an optional prefix and an exponent (^n, ^-n or superscripts), joined by '*', '·' or a
  //         space, then '/' and a symbol or a parenthesized product (or 1/…); a scaled unit in brackets
  //         ([1/381*ft], [pi*rad], [2*pi^-1]); 1 is the unit one. Read factor by factor (a basis vector's order
  //         holds: e1*e2 is not e2*e1).
  //   kind: none, any, or [any] names with exponents joined by '*' and '/' (length*duration, area/duration).
  //   origin: a name, or 0.
  // False where it does not read, with what was expected.
  // A symbol or name is looked up in the catalog first (catalog_table, read once), a symbol with a prefix too: one
  // the catalog reads names the catalog's. Only one it does not read reads every other tag visible here, once
  // (the walk, which costs most of a lookup); inside the catalog, before its table, every one does.
  struct quantity_key_reader
  {
    catalog_sets     Catalog;
    tag_vectors      Others{};
    bool             Walked{false};
    std::string_view Expected{};

    consteval bool fail(std::string_view expected)
    {
      Expected = expected;
      return false;
    }
    consteval const tag_vectors& others()
    {
      if (!Walked) // every other tag (all of them before the catalog's table: ins is skipped only after it)
      {
        collect_tags(^^::, Others, Catalog.Units.Size != 0 ? ^^ins : std::meta::info{});
        Walked = true;
      }
      return Others;
    }

    // The one unit of units[0, n) a symbol names exactly (several units equal by content are one).
    // (By index: GCC under -fsanitize=pointer-compare rejects a pointer test in a constant evaluation.)
    static consteval bool match_exact(const unit_def* const* units, std::size_t n, std::string_view w,
                                      const unit_def*& found, bool& ambiguous)
    {
      std::size_t hits{0};
      for (std::size_t i = 0; i < n; ++i)
        if (units[i]->Utf8 == w || units[i]->ascii() == w)
        {
          if (hits++ > 0)
            ambiguous = true;
          found = units[i];
        }
      return hits == 1;
    }
    // A symbol exactly, else a prefix and a symbol (min is the minute, mm the millimetre).
    static consteval bool match_in(const unit_def* const* units, std::size_t n, std::string_view w, unit_atom& a,
                                   bool& ambiguous)
    {
      const unit_def* u{};
      if (match_exact(units, n, w, u, ambiguous))
      {
        a = unit_atom{u, 0, 1};
        return true;
      }
      if (ambiguous)
        return false;
      for (const text_prefix& p : text_prefixes)
        if (w.size() > p.Sym.size() && w.starts_with(p.Sym)
            && match_exact(units, n, w.substr(p.Sym.size()), u, ambiguous))
        {
          a = unit_atom{u, p.Pow, 1};
          return true;
        }
      return false;
    }
    consteval bool match_symbol(std::string_view w, unit_atom& a, bool& ambiguous)
    {
      if (Catalog.Units.Size != 0 && match_in(Catalog.Units.Data, Catalog.Units.Size, w, a, ambiguous))
        return true;
      if (ambiguous)
        return false;
      return match_in(others().Units.data(), others().Units.size(), w, a, ambiguous);
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
        a.Exp = static_cast<std::int8_t>(e);
        if (u.size() == static_cast<std::int8_t>(max_unit_atoms))
          return fail("at most 6 units in a product");
        u = unit_mul(u, unit_value{a}, sign);
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

    consteval bool read_unit(std::string_view t, unit_value& u)
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

    // The one kind of kinds[0, n) a name names.
    static consteval bool match_kind_in(const kind_def* const* kinds, std::size_t n, std::string_view w,
                                        const kind_def*& found, bool& ambiguous)
    {
      std::size_t hits{0};
      for (std::size_t i = 0; i < n; ++i)
        if (kinds[i]->Name == w)
        {
          if (hits++ > 0)
            ambiguous = true;
          found = kinds[i];
        }
      return hits == 1;
    }
    consteval bool match_kind(std::string_view w, const kind_def*& k)
    {
      bool ambiguous{false};
      if (Catalog.Kinds.Size != 0 && match_kind_in(Catalog.Kinds.Data, Catalog.Kinds.Size, w, k, ambiguous))
        return true;
      if (!ambiguous && match_kind_in(others().Kinds.data(), others().Kinds.size(), w, k, ambiguous))
        return true;
      return fail(ambiguous ? "a name one visible kind has (several have this one)" : "a kind visible here");
    }

    // The kind of a quantity on unit u: r, its reference.
    consteval bool read_kind(std::string_view t, const unit_value& u, reference& r)
    {
      if (t == "none" || t == "any")
      {
        r = reference{&kind_def::none, t == "any", {}, u};
        return true;
      }
      const bool any{t.starts_with("any ")};
      if (any)
      {
        t.remove_prefix(4);
        while (t.starts_with(' '))
          t.remove_prefix(1);
      }
      recipe      f{};
      dimension   d{dim_one};
      std::size_t i{0};
      int         sign{1};
      while (true)
      {
        std::size_t end{i};
        while (end < t.size() && identifier_name(t.substr(end, 1)))
          ++end;
        if (end == i)
          return fail("a kind's name");
        const kind_def* k{};
        if (!match_kind(t.substr(i, end - i), k))
          return false;
        i = end;
        const int e{read_text_exponent(t, i)};
        if (e == 0)
          return fail("an exponent");
        if (!recipe_add(f, k, sign * e))
          return fail("at most 6 kinds in a product");
        d = d * k->Dim.pow(sign * e);
        if (i == t.size())
          break;
        if (t[i] != '*' && t[i] != '/')
          return fail("'*', '/' or the end of the kind");
        sign = t[i] == '*' ? 1 : -1;
        ++i;
      }
      if (!(d == dimension_of(u)))
        return fail("a kind of the unit's dimension");
      if (f.Size == 1 && f.F[0].Exp == 1)
      {
        const kind_ref own{kind_of(u)};
        if (has(own) && !same(kind_root(own), kind_root(f.F[0].Kind)))
          return fail("a kind the unit measures (not Hz for activity)");
        r = reference{f.F[0].Kind, any, {}, u};
      }
      else
        r = reference{&kind_def::none, any, f, u};
      return true;
    }

    consteval bool read_origin(std::string_view t, origin_ref& o)
    {
      if (t == "0")
      {
        o = &origin_def::none;
        return true;
      }
      const auto match_in = [&](const origin_def* const* origins, std::size_t n, bool& ambiguous)
      {
        std::size_t hits{0};
        for (std::size_t i = 0; i < n; ++i)
          if (origins[i]->Name == t)
          {
            if (hits++ > 0)
              ambiguous = true;
            o = origins[i];
          }
        return hits == 1;
      };
      bool ambiguous{false};
      if (Catalog.Origins.Size != 0 && match_in(Catalog.Origins.Data, Catalog.Origins.Size, ambiguous))
        return true;
      if (!ambiguous && match_in(others().Origins.data(), others().Origins.size(), ambiguous))
        return true;
      return fail(ambiguous ? "a name one visible origin has (several have this one)"
                            : "an origin visible here (its name, or 0)");
    }
  };

  // ---- Writing ----

  // A unit's canonical text: its symbols (the ASCII one where it has one, with an ASCII prefix: us, not µs) in the
  // unit's order, those with a positive exponent first, then '/' and the others; a unit with a basis vector all in
  // its order (e1*e2); a scale in brackets; 1 for the unit one.
  // False for a symbol no text reads (a space, '*', '/', '^', a bracket or a parenthesis in it), or a prefix that
  // has no symbol.
  consteval bool write_unit_text(text_out& t, const unit_value& u)
  {
    const auto readable = [](std::string_view s)
    {
      for (char c : s)
        if (c == ' ' || c == '*' || c == '/' || c == '^' || c == '[' || c == ']' || c == '(' || c == ')')
          return false;
      return !s.empty();
    };
    const auto ascii = [](std::string_view s)
    {
      for (char c : s)
        if (static_cast<unsigned char>(c) >= 0x80)
          return false;
      return true;
    };
    const auto prefix = [&](int p, std::string_view& sym)
    {
      if (p == 0)
      {
        sym = {};
        return true;
      }
      for (const text_prefix& x : text_prefixes)
        if (x.Pow == p && ascii(x.Sym))
        {
          sym = x.Sym;
          return true;
        }
      return false;
    };
    const std::int8_t n{u.size()};
    int               num{0};
    bool              blades{false};
    for (std::int8_t i = 0; i < n; ++i)
    {
      std::string_view p;
      if (!readable(u.Atoms[i].U->ascii()) || !prefix(u.Atoms[i].Prefix, p))
        return false;
      num += u.Atoms[i].Exp > 0;
      blades = blades || is_basis_atom(u.Atoms[i]);
    }
    // (A basis vector or a polynomial variable gets its text with the flat quantity type: until then a sum of
    // blades takes the base class's type of its parts.)
    if (blades)
      return false;
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
      if (num == 0 || blades) // only a denominator (s^-1), or a basis vector's order to keep
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

  // A reference's kind when it is not its unit's own: none, any, [any] a name, or [any] a recipe
  // (those with a positive exponent first, then '/' and the others, in the recipe's order: by name).
  // Nothing for the unit's own kind. False for a kind whose name is no identifier.
  consteval bool write_kind_text(text_out& t, const reference& r)
  {
    if (r == reference_of(r.unit()))
      return true;
    const recipe f{r.derived()};
    if (!has(r.kind()) && f.Size == 0)
    {
      t.put(r.kind_of() ? "any" : "none");
      return true;
    }
    if (r.kind_of())
      t.put("any ");
    if (has(r.kind()))
    {
      if (!identifier_name(r.kind()->Name))
        return false;
      t.put(r.kind()->Name);
      return true;
    }
    int pos{0};
    for (std::int8_t i = 0; i < f.Size; ++i)
    {
      if (!identifier_name(f.F[i].Kind->Name))
        return false;
      pos += f.F[i].Exp > 0;
    }
    bool first{true};
    for (const int sign : {1, -1})
      for (std::int8_t i = 0; i < f.Size; ++i)
      {
        const kind_factor& k{f.F[i]};
        if ((k.Exp > 0) != (sign > 0))
          continue;
        if (!first || (sign < 0 && pos > 0))
          t.put(sign < 0 && pos > 0 ? '/' : '*');
        first = false;
        t.put(k.Kind->Name);
        const int e{pos > 0 ? k.Exp * sign : k.Exp};
        if (e != 1)
        {
          t.put('^');
          if (e < 0)
            t.put('-');
          t.put(static_cast<std::uint64_t>(e < 0 ? -e : e));
        }
      }
    return true;
  }

  // A point's origin: its name, or 0 for the zero of the dimension. False for a name that is no identifier.
  consteval bool write_origin_text(text_out& t, origin_ref o)
  {
    if (!has(o))
    {
      t.put('0');
      return true;
    }
    if (!identifier_name(o->Name))
      return false;
    t.put(o->Name);
    return true;
  }

  // ---- For a key ----

  // A quantity's reference (and a point's origin) a text names.
  struct quantity_read
  {
    reference        Ref{};
    origin_ref       Origin{&origin_def::none};
    bool             Ok{false};
    std::string_view Expected{};
    std::size_t      At{0}; // which part went wrong: 0 the unit, 1 the kind, 2 the origin
  };
  template <class Context>
  consteval quantity_read read_quantity(std::string_view unit, std::string_view kind, std::string_view origin)
  {
    quantity_key_reader r{catalog_table<Context>()};
    quantity_read       q;
    unit_value          u;
    if (!r.read_unit(unit, u))
    {
      q.Expected = r.Expected;
      return q;
    }
    q.Ref = reference_of(u);
    if (!kind.empty() && !r.read_kind(kind, u, q.Ref))
    {
      q.Expected = r.Expected;
      q.At       = 1;
      return q;
    }
    if (!origin.empty())
    {
      if (!r.read_origin(origin, q.Origin))
      {
        q.Expected = r.Expected;
        q.At       = 2;
        return q;
      }
      if (has(q.Origin) && !(q.Origin->Dim == dimension_of(u)))
      {
        q.Expected = "an origin of the unit's dimension";
        q.At       = 2;
        return q;
      }
    }
    q.Ok = true;
    return q;
  }

  // The canonical texts of a quantity's reference (and a point's origin).
  struct quantity_name_or_none
  {
    quantity_name Name;
    bool          Ok{false};
  };
  consteval quantity_name_or_none quantity_name_for(const reference& r, origin_ref o, bool point)
  {
    quantity_name_or_none n;
    text_out              tu{n.Name.Unit.Text, sizeof n.Name.Unit.Text};
    text_out              tk{n.Name.Kind.Text, sizeof n.Name.Kind.Text};
    text_out              to{n.Name.Origin.Text, sizeof n.Name.Origin.Text};
    n.Ok = write_unit_text(tu, r.unit()) && write_kind_text(tk, r) && (!point || write_origin_text(to, o));
    return n;
  }

  // The canonical text of the quantity a key's text names (detail::key_of_text through this overload).
  template <std::size_t N>
  consteval bool canonical_quantity_text(text_hook<N>, const id_spec& s, quantity_name& q, std::string_view& expected,
                                         std::size_t& at)
  {
    const quantity_read r{read_quantity<text_hook<N>>(s.Unit, s.Kind, s.Origin)};
    if (!r.Ok)
    {
      expected = r.Expected;
      at       = r.At == 0 ? s.UnitAt : r.At == 1 ? s.KindAt : s.OriginAt;
      return false;
    }
    const quantity_name_or_none n{quantity_name_for(r.Ref, r.Origin, !s.Origin.empty())};
    if (!n.Ok)
    {
      expected = "a unit, kind and origin a text names (a symbol without a space, * / ^ brackets or parentheses; "
                 "names of letters, digits and _)";
      at       = s.UnitAt;
      return false;
    }
    q = n.Name;
    return true;
  }

  // A canonical quantity text, read once where it is first used (the tags visible there).
  template <quantity_name T>
  struct quantity_name_context
  {
  };
  template <quantity_name T>
  inline constexpr quantity_read quantity_of_name =
    read_quantity<quantity_name_context<T>>(T.Unit.view(), T.Kind.view(), T.Origin.view());

  // A key's quantity texts, as a quantity_name.
  consteval quantity_name quantity_name_of(const id_spec& s)
  {
    quantity_name n;
    const auto    copy = [](unit_name& out, std::string_view text)
    {
      for (std::size_t i = 0; i < text.size() && i + 1 < sizeof out.Text; ++i)
        out.Text[i] = text[i];
    };
    copy(n.Unit, s.Unit);
    copy(n.Kind, s.Kind);
    copy(n.Origin, s.Origin);
    return n;
  }
  consteval quantity_read checked_quantity(const quantity_read& q)
  {
    if (!q.Ok)
      constexpr_error<"units: a quantity's unit, kind or origin is none visible here (it names none, or several)">();
    return q;
  }
  // The reference (and origin) of a key with a unit.
  template <key K>
  inline constexpr quantity_read key_quantity = checked_quantity(quantity_of_name<quantity_name_of(key_spec<K>)>);
  template <key K>
  inline constexpr reference key_reference = key_quantity<K>.Ref;
  template <key K>
  inline constexpr unit_value key_unit = key_reference<K>.unit();
  template <key K>
  inline constexpr origin_ref key_origin = key_quantity<K>.Origin;

  // Whether a quantity on R (a point from origin O) is spelled by a key: a text that reads back to R (and O) where
  // the type is first formed.
  template <reference R, origin_ref O = &origin_def::none, bool Point = false>
  consteval bool reference_has_text()
  {
    constexpr quantity_name_or_none n{quantity_name_for(R, O, Point)};
    if constexpr (!n.Ok)
      return false;
    else
    {
      constexpr quantity_read q{quantity_of_name<n.Name>};
      return q.Ok && q.Ref == R && (!Point || same(q.Origin, O));
    }
  }
} // namespace ins::detail

#endif // INSIDE_UNITS_DETAIL_UNIT_KEY_HPP
