// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A quantity read from text with its unit: the units its text names (units/detail/unit_grammar.hpp), and the exact
// factor (for a point also the shift) from that unit into a quantity's.
// Internal: include <inside/units/io.hpp>.
#ifndef INSIDE_UNITS_DETAIL_UNIT_TEXT_HPP
#define INSIDE_UNITS_DETAIL_UNIT_TEXT_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units.hpp>
#include <inside/units/detail/unit_grammar.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <meta>
#include <string_view>
#include <vector>

namespace ins::detail
{
  // A quantity's text split: the number up to the first space and the unit after it, or without a space the number
  // as far as its grammar reads and the unit against it (90°, 90deg). Spaces around the unit are dropped.
  struct quantity_text
  {
    std::string_view Number;
    std::string_view Unit;
  };
  constexpr quantity_text split_quantity_text(std::string_view t)
  {
    while (t.ends_with(' '))
      t.remove_suffix(1);
    const std::size_t space{t.find(' ')};
    const std::size_t i{space != std::string_view::npos ? space : number_text_end(t)};
    std::string_view  unit{t.substr(i)};
    while (unit.starts_with(' '))
      unit.remove_prefix(1);
    while (unit.ends_with(' '))
      unit.remove_suffix(1);
    return {t.substr(0, i), unit};
  }

  struct text_term
  {
    std::size_t Atom{0};
    std::int8_t Prefix{0};
    std::int8_t Exp{1};
  };

  // A unit's text, read: its symbols with prefixes and exponents, and a scale ([1/381 ft], [π rad]).
  struct read_unit_text
  {
    text_term                       Terms[max_unit_atoms]{};
    std::size_t                     Size{0};
    wide_rational<text_scale_limbs> Scale{1, 1};
    std::int8_t                     ScalePi{0};
    bool                            Scaled{false};
  };

  // ---- What each unit says, for a quantity (unit R.unit()) or a point (and its origin O) ----

  struct text_atom
  {
    std::string_view Utf8;
    std::string_view Ascii;
    grid_number      Ratio{1};        // its magnitude, against the coherent unit of its dimension
    std::int8_t      Pi{0};
    bool             KindOk{false};   // as a whole text: a quantity in it adds to one of the target's kind
    bool             OriginOk{false}; // a point: its origin (or the dimension's zero) shares the target's absolute one
    grid_number      Shift{0};        // a point: that origin into the target's, in the target's unit
  };

  template <unit_set S, reference R, origin_ref O, bool Point>
  struct unit_text_facts
  {
    // The base dimensions of the target and of every unit: a dimension is a vector of exponents over them.
    static consteval std::vector<const base_dimension_def*> basis_of()
    {
      std::vector<const base_dimension_def*> b;
      const auto add = [&](const dimension& d)
      {
        for (std::size_t i = 0; i < d.Size; ++i)
          if (std::ranges::none_of(b, [&](const base_dimension_def* x) { return x->Name == d.Base[i]->Name; }))
            b.push_back(d.Base[i]);
      };
      add(dimension_of(R.unit()));
      for (std::size_t a = 0; a < S.Size; ++a)
        add(S.Data[a]->Dim);
      return b;
    }
    static constexpr std::size_t B     = basis_of().size();
    static constexpr auto        basis = std::define_static_array(basis_of());

    static consteval std::array<std::int8_t, B> exponents_of(const dimension& d)
    {
      std::array<std::int8_t, B> e{};
      for (std::size_t i = 0; i < d.Size; ++i)
        for (std::size_t j = 0; j < B; ++j)
          if (basis[j]->Name == d.Base[i]->Name)
            e[j] = d.Exp[i];
      return e;
    }
    static constexpr std::array<std::int8_t, B> target_dim = exponents_of(dimension_of(R.unit()));
    static constexpr magnitude                  target_mag = magnitude_of(R.unit());

    static consteval std::array<std::int8_t, S.Size * B> atom_dims_of()
    {
      std::array<std::int8_t, S.Size * B> e{};
      for (std::size_t a = 0; a < S.Size; ++a)
      {
        const auto x{exponents_of(S.Data[a]->Dim)};
        std::ranges::copy(x, e.begin() + static_cast<std::ptrdiff_t>(a * B));
      }
      return e;
    }
    static constexpr std::array<std::int8_t, S.Size * B> atom_dims = atom_dims_of();

    // A text in no single unit counts from the dimension's zero.
    static constexpr bool        implicit_origin_ok = !Point || (!has(absolute_of(O)) && target_mag.Pi == 0);
    static constexpr grid_number implicit_shift     = implicit_origin_ok && Point
                                                        ? (grid_number{0} - offset_of(O)) / target_mag.Ratio
                                                        : grid_number{0};

    static consteval text_atom atom_of(const unit_def* u)
    {
      text_atom t{u->Utf8, u->ascii(), u->Mag.Ratio, u->Mag.Pi, false, false, grid_number{0}};
      if (u->Dim == dimension_of(R.unit()))
        t.KindOk = common_kind(reference_of(unit_value{*u}), R).Ok;
      if constexpr (Point)
        if (target_mag.Pi == 0 && same(absolute_of(u->Origin), absolute_of(O)))
        {
          t.OriginOk = true;
          t.Shift    = (offset_of(u->Origin) - offset_of(O)) / target_mag.Ratio;
        }
      return t;
    }
    static consteval std::array<text_atom, S.Size> atoms_of()
    {
      std::array<text_atom, S.Size> t{};
      for (std::size_t a = 0; a < S.Size; ++a)
        t[a] = atom_of(S.Data[a]);
      return t;
    }
    static constexpr std::array<text_atom, S.Size> atoms = atoms_of();

    // The width of the exact factor: the powers of the units (each with a prefix up to 10^30), the scale, the
    // target's magnitude and a point's shift.
    static consteval int ratio_bits(const grid_number& r)
    {
      return std::max(grid_bits(grid_numerator(r)), grid_bits(grid_denominator(r)));
    }
    static consteval int factor_bits()
    {
      int a{0};
      for (const text_atom& t : atoms)
        a = std::max(a, ratio_bits(t.Ratio));
      int s{ratio_bits(implicit_shift)};
      for (const text_atom& t : atoms)
        s = std::max(s, ratio_bits(t.Shift));
      return max_text_exponents * (a + text_prefix_bits) + 2 * 64 * static_cast<int>(text_scale_limbs)
             + 2 * ratio_bits(target_mag.Ratio) + 2 * s + 8;
    }
    static constexpr std::size_t KF = limbs_for_bits(factor_bits());

    // ---- Reading ----

    struct symbol_match
    {
      std::size_t Atom{std::string_view::npos};
      std::int8_t Prefix{0};
    };
    // The unit a symbol names exactly; none when no unit, or more than one, has it.
    static constexpr symbol_match match_exact(std::string_view w)
    {
      symbol_match m;
      for (std::size_t a = 0; a < S.Size; ++a)
        if (atoms[a].Utf8 == w || atoms[a].Ascii == w)
        {
          if (m.Atom != std::string_view::npos)
            return {};
          m.Atom = a;
        }
      return m;
    }
    // A symbol exactly, else a prefix and a symbol (min is the minute, mm the millimetre).
    static constexpr symbol_match match_symbol(std::string_view w)
    {
      if (const symbol_match m{match_exact(w)}; m.Atom != std::string_view::npos)
        return m;
      for (const text_prefix& p : text_prefixes)
        if (w.size() > p.Sym.size() && w.starts_with(p.Sym))
          if (symbol_match m{match_exact(w.substr(p.Sym.size()))}; m.Atom != std::string_view::npos)
          {
            m.Prefix = p.Pow;
            return m;
          }
      return {};
    }

    // [N/D π^k·unit]: a scale (a number, π with an exponent, or both: 2π, ASCII 2*pi, pi^-1), then a separator and
    // the unit (the separator a space in older text). Where the unit starts (the end for none); npos when the scale
    // does not read.
    static constexpr std::size_t read_scale(std::string_view s, read_unit_text& u)
    {
      std::size_t i{number_text_end(s)};
      if (i > 0)
      {
        const auto w{parse_exact<text_scale_limbs>(s.data(), s.data() + i)};
        if (!w || !(wide_rational<text_scale_limbs>{0, 1} < *w))
          return std::string_view::npos;
        u.Scale = *w;
      }
      const std::string_view rest{s.substr(i)};
      const std::size_t      pi{rest.starts_with("π") || rest.starts_with("pi") ? std::size_t{2}
                                : i > 0 && rest.starts_with("*pi")             ? std::size_t{3}
                                                                                : std::size_t{0}};
      if (pi != 0)
      {
        i += pi;
        const int e{read_text_exponent(s, i)};
        if (e == 0)
          return std::string_view::npos;
        u.ScalePi = static_cast<std::int8_t>(e);
      }
      else if (i == 0)
        return std::string_view::npos;
      u.Scaled = true;
      if (i == s.size())
        return i;
      const std::size_t sep{text_separator(s, i)};
      return sep == 0 ? std::string_view::npos : i + sep;
    }

    // factors ['/' (symbol | '(' factors ')')], or 1/…, with a whole symbol tried first.
    static constexpr bool read_plain(std::string_view t, read_unit_text& u)
    {
      if (const symbol_match m{match_exact(t)}; m.Atom != std::string_view::npos)
      {
        u.Terms[u.Size++] = {m.Atom, 0, 1};
        return true;
      }
      std::size_t i{0};
      int         total{0};
      const auto  term = [&](int sign)
      {
        const std::size_t end{symbol_end(t, i)};
        if (end == i || u.Size == max_unit_atoms)
          return false;
        const symbol_match m{match_symbol(t.substr(i, end - i))};
        if (m.Atom == std::string_view::npos)
          return false;
        i = end;
        const int e{read_text_exponent(t, i)};
        total += e < 0 ? -e : e;
        if (e == 0 || e > max_text_exponent || e < -max_text_exponent || total > max_text_exponents)
          return false;
        u.Terms[u.Size++] = {m.Atom, m.Prefix, static_cast<std::int8_t>(sign * e)};
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
          if (!factors(-1) || i >= t.size() || t[i] != ')')
            return false;
          ++i;
        }
        else if (!term(-1))
          return false;
      }
      return i == t.size();
    }

    static constexpr std::expected<read_unit_text, error> read_unit(std::string_view t)
    {
      read_unit_text u;
      if (t.starts_with('['))
      {
        if (!t.ends_with(']'))
          return std::unexpected{error::unknown_unit};
        const std::string_view inner{t.substr(1, t.size() - 2)};
        const std::size_t      unit{read_scale(inner, u)};
        if (unit == std::string_view::npos || (unit < inner.size() && !read_plain(inner.substr(unit), u)))
          return std::unexpected{error::unknown_unit};
        return u;
      }
      if (!read_plain(t, u))
        return std::unexpected{error::unknown_unit};
      return u;
    }

    // ---- Converting ----

    struct conversion
    {
      wide_rational<KF> Factor{1, 1};
      wide_rational<KF> Shift{0, 1};
    };
    static constexpr wide_rational<KF> power_of_ten(int p)
    {
      wide_sint<KF> t{1};
      for (int k = 0; k < (p < 0 ? -p : p); ++k)
        t = t * wide_sint<KF>{10};
      return p < 0 ? wide_rational<KF>{wide_sint<KF>{1}, t} : wide_rational<KF>{t, wide_sint<KF>{1}};
    }
    // The factor from the text's unit to the target's, and a point's shift; incompatible_unit when the dimensions
    // or the powers of π differ, a single unit's kind does not add to the target's, or a point's origin has
    // another absolute origin.
    static constexpr std::expected<conversion, error> convert(const read_unit_text& u)
    {
      std::array<int, B> d{};
      int                pi{u.ScalePi};
      for (std::size_t k = 0; k < u.Size; ++k)
      {
        const text_term& t{u.Terms[k]};
        for (std::size_t j = 0; j < B; ++j)
          d[j] += atom_dims[t.Atom * B + j] * t.Exp;
        pi += atoms[t.Atom].Pi * t.Exp;
      }
      for (std::size_t j = 0; j < B; ++j)
        if (d[j] != target_dim[j])
          return std::unexpected{error::incompatible_unit};
      if (pi != target_mag.Pi)
        return std::unexpected{error::incompatible_unit};
      const bool single{u.Size == 1 && u.Terms[0].Exp == 1 && !u.Scaled};
      if (single && !atoms[u.Terms[0].Atom].KindOk)
        return std::unexpected{error::incompatible_unit};
      conversion c;
      c.Factor = wide_rational<KF>{u.Scale};
      for (std::size_t k = 0; k < u.Size; ++k)
      {
        const text_term&        t{u.Terms[k]};
        const wide_rational<KF> a{exact_of_grid<KF>(atoms[t.Atom].Ratio) * power_of_ten(t.Prefix)};
        for (int n = 0; n < (t.Exp < 0 ? -t.Exp : t.Exp); ++n)
          c.Factor = t.Exp < 0 ? c.Factor / a : c.Factor * a;
      }
      c.Factor = reduced(c.Factor / exact_of_grid<KF>(target_mag.Ratio));
      if constexpr (Point)
      {
        if (single ? !atoms[u.Terms[0].Atom].OriginOk : !implicit_origin_ok)
          return std::unexpected{error::incompatible_unit};
        c.Shift = exact_of_grid<KF>(single ? atoms[u.Terms[0].Atom].Shift : implicit_shift);
      }
      return c;
    }
  };

  // The quantity type of Q (a quantity, or a point's quantity from its origin), and the origin of a point.
  template <class Q>
  struct text_target
  {
    using type                       = Q;
    static constexpr origin_ref origin = {};
    static constexpr bool       point  = false;
  };
  template <class Q>
    requires is_quantity_point_v<Q>
  struct text_target<Q>
  {
    using type                       = typename Q::quantity_type;
    static constexpr origin_ref origin = Q::origin;
    static constexpr bool       point  = true;
  };

  // The number read exactly, times the factor, plus the shift, stored by the target's number type.
  template <class Q, policy_flag F, bool Exact, std::size_t KF>
  constexpr std::expected<Q, error> store_quantity_text(std::string_view number, const wide_rational<KF>& factor,
                                                        const wide_rational<KF>& shift)
  {
    using T   = typename text_target<Q>::type;
    using Rep = typename T::rep;
    constexpr std::size_t KN{std::max(exact_limbs<Rep>, parse_limbs)};
    constexpr std::size_t K{KN + 2 * KF + 1};
    const auto            n{parse_exact<KN>(number.data(), number.data() + number.size())};
    if (!n)
      return std::unexpected{n.error()};
    const wide_rational<K> v{reduced(wide_rational<K>{*n} * wide_rational<K>{factor} + wide_rational<K>{shift})};
    const auto             r{store_text_value<Rep, F, Exact>(v)};
    if (!r)
      return std::unexpected{r.error()};
    const T q{quantity_access::make<T>(*r)};
    if constexpr (text_target<Q>::point)
      return Q{q};
    else
      return q;
  }

  // parse<Q>(number, unit text): the unit read and checked against the units S, then the number stored.
  template <class Q, policy_flag F, bool Exact, unit_set S>
  constexpr std::expected<Q, error> parse_quantity_text(std::string_view number, std::string_view unit)
  {
    using T     = typename text_target<Q>::type;
    using Facts = unit_text_facts<S, T::reference, text_target<Q>::origin, text_target<Q>::point>;
    if (unit.empty())
    {
      // Only a quantity of unit one prints no unit.
      if constexpr (T::reference.unit() == unit_value{})
        return store_quantity_text<Q, F, Exact, 1>(number, wide_rational<1>{1, 1}, wide_rational<1>{0, 1});
      else
        return std::unexpected{number.empty() ? error::invalid_format : error::missing_unit};
    }
    const auto u{Facts::read_unit(unit)};
    if (!u)
      return std::unexpected{u.error()};
    const auto c{Facts::convert(*u)};
    if (!c)
      return std::unexpected{c.error()};
    return store_quantity_text<Q, F, Exact, Facts::KF>(number, c->Factor, c->Shift);
  }

  // A unit named in code converts exactly to Q's: the same dimension, no factor of π between them, and for a
  // point the same absolute origin.
  template <class Q, unit_value U>
  consteval bool text_unit_converts()
  {
    using T = typename text_target<Q>::type;
    if (!(dimension_of(U) == dimension_of(T::reference.unit()))
        || magnitude_of(U).Pi != magnitude_of(T::reference.unit()).Pi)
      return false;
    if constexpr (text_target<Q>::point)
      return magnitude_of(T::reference.unit()).Pi == 0 && same(absolute_of(origin_of_unit(U)), absolute_of(Q::origin));
    else
      return true;
  }

  // parse<Q>(number, u): the number in the unit u, named in code.
  template <class Q, policy_flag F, bool Exact, unit_value U>
  constexpr std::expected<Q, error> parse_quantity_in(std::string_view number)
  {
    using T = typename text_target<Q>::type;
    constexpr magnitude   from{magnitude_of(U)}, to{magnitude_of(T::reference.unit())};
    constexpr grid_number factor{from.Ratio / to.Ratio};
    constexpr grid_number shift{text_target<Q>::point
                                  ? (offset_of(origin_of_unit(U)) - offset_of(text_target<Q>::origin)) / to.Ratio
                                  : grid_number{0}};
    constexpr std::size_t KF{limbs_for_bits(2 * std::max({grid_bits(grid_numerator(factor)),
                                                          grid_bits(grid_denominator(factor)),
                                                          grid_bits(grid_numerator(shift)),
                                                          grid_bits(grid_denominator(shift))})
                                            + 8)};
    return store_quantity_text<Q, F, Exact, KF>(number, exact_of_grid<KF>(factor), exact_of_grid<KF>(shift));
  }
} // namespace ins::detail

#endif // INSIDE_UNITS_DETAIL_UNIT_TEXT_HPP
