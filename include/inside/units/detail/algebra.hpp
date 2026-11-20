// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The compile-time algebra of units.hpp: dimensions, magnitudes, quantity kinds, point origins, units, named units,
// references and the exact unit conversion of a number.
// Internal: include <inside/units.hpp> or <inside/units/core.hpp>.
// Defines INSIDE_UNITS_WHY_PI, which units/core.hpp undefines.
#ifndef INSIDE_UNITS_DETAIL_ALGEBRA_HPP
#define INSIDE_UNITS_DETAIL_ALGEBRA_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside.hpp>
#include <inside/inside/predicates.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <meta>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// The diagnostic of a π conversion asked to be exact (rescale, in()).
#define INSIDE_UNITS_WHY_PI                                                                      \
  "units: this conversion multiplies by a power of pi, which has no exact grid - round it with " \
  "value_cast<unit, Rep>(q) from <inside/units/math.hpp>, Rep declared with a rounding mode"

namespace ins
{
  using ins::grid_number;

  // dimension: a product of base dimensions to integer powers.
  // Every base dimension is a base_dimension<"name">: the SI ones (length, mass, time…) and yours (currency,
  // information) alike. Angle is dimensionless, as in SI.
  struct unit_def;

  // A base dimension: base_dimension<"currency"> (units/detail/define.hpp).
  // A basis vector of a geometric algebra is one too (see basis_vector): its exponent is 0 or 1,
  // it squares to Square (+1, -1 or 0), two of them anticommute, and Vector is its unit.
  // Vector is read only when Basis is set.
  struct base_dimension_def
  {
    std::string_view Name;
    std::int8_t      Square{0};
    bool             Basis{false};
    const unit_def*  Vector{};
  };

  struct dimension;

  namespace detail
  {
    // The base dimensions of a dimension being computed, in transient vectors: sorted by name, no zero exponents.
    struct exponents
    {
      std::vector<const base_dimension_def*> D;
      std::vector<std::int8_t>               E;

      constexpr exponents() = default;
      constexpr explicit exponents(const dimension& d);
      // b^e merged in: an exponent reaching 0 drops out.
      constexpr void add(const base_dimension_def* b, int e);
      // The dimension of these, interned.
      constexpr dimension intern() const;
    };
  } // namespace detail

  struct dimension
  {
    // The base dimensions, sorted by name, and their exponents (none zero).
    // Interned in static storage with std::define_static_array, as big grid numbers' limbs are,
    // so a dimension holds any number of them.
    // Two arrays of scalars, not one of pairs: GCC 16 cannot intern an array of a class type that equals one
    // already interned.
    const base_dimension_def* const* Base{};
    const std::int8_t*               Exp{};
    std::size_t                      Size{0}; // tested instead of the pointers (see big_int::Size)

    // An exponent past ±127 is an error, not a wrap.
    static constexpr std::int8_t exponent(int e)
    {
      if (e < -127 || e > 127)
        ins::detail::constexpr_error<"units: a dimension exponent past 127">();
      return static_cast<std::int8_t>(e);
    }

    // By content: a base dimension is its name (and, for a basis vector, its square).
    friend constexpr bool operator==(const dimension& a, const dimension& b)
    {
      if (a.Size != b.Size)
        return false;
      for (std::size_t i = 0; i < a.Size; ++i)
        if (a.Base[i]->Name != b.Base[i]->Name || a.Base[i]->Square != b.Base[i]->Square || a.Exp[i] != b.Exp[i])
          return false;
      return true;
    }
    friend consteval dimension operator*(const dimension& a, const dimension& b)
    {
      if (b.Size == 0)
        return a;
      if (a.Size == 0)
        return b;
      detail::exponents u{a};
      for (std::size_t i = 0; i < b.Size; ++i)
        u.add(b.Base[i], b.Exp[i]);
      return u.intern();
    }
    friend consteval dimension operator/(const dimension& a, const dimension& b) { return a * b.pow(-1); }
    // Merged afresh: a basis vector's exponent is taken mod 2 and drops out at 0.
    consteval dimension pow(int n) const
    {
      detail::exponents u;
      for (std::size_t i = 0; i < Size; ++i)
        u.add(Base[i], Exp[i] * n);
      return u.intern();
    }
    // The n-th root; every exponent must divide.
    consteval dimension root(int n) const
    {
      detail::exponents u{*this};
      for (auto& e : u.E)
      {
        if (e % n != 0)
          ins::detail::constexpr_error<"units: the root of a dimension needs exponents it divides">();
        e = static_cast<std::int8_t>(e / n);
      }
      return u.intern();
    }
  };

  namespace detail
  {
    constexpr exponents::exponents(const dimension& d)
    {
      for (std::size_t i = 0; i < d.Size; ++i)
      {
        D.push_back(d.Base[i]);
        E.push_back(d.Exp[i]);
      }
    }

    // A basis vector's exponent is 0 or 1: v·v is a number (its square).
    constexpr int basis_exponent(int e) { return ((e % 2) + 2) % 2; }

    constexpr void exponents::add(const base_dimension_def* b, int e)
    {
      if (b->Basis)
        e = basis_exponent(e);
      if (e == 0)
        return;
      std::size_t i{0};
      while (i < D.size() && D[i]->Name < b->Name)
        ++i;
      if (i < D.size() && D[i]->Name == b->Name)
      {
        if (D[i]->Basis != b->Basis || D[i]->Square != b->Square)
          ins::detail::constexpr_error<"units: two base dimensions of one name differ">();
        E[i] = b->Basis ? static_cast<std::int8_t>(basis_exponent(E[i] + e)) : dimension::exponent(E[i] + e);
        if (E[i] == 0)
        {
          D.erase(D.begin() + i);
          E.erase(E.begin() + i);
        }
        return;
      }
      D.insert(D.begin() + i, b);
      E.insert(E.begin() + i, dimension::exponent(e));
    }

    constexpr dimension exponents::intern() const
    {
      if (D.empty())
        return {};
      if consteval
      {
        const auto names{std::define_static_array(D)};
        return {names.data(), std::define_static_array(E).data(), names.size()};
      }
      else
      {
        ins::detail::constexpr_error<"units: a dimension is formed at compile time">();
      }
    }

    // x^e merged into the sorted items[0, size): an item `same` as x takes the summed exponent and drops out at 0;
    // otherwise x goes in at its place by `before`. False when items is full.
    template <class T, std::size_t N, class Same, class Before>
    constexpr bool sorted_add(T (&items)[N], std::int8_t& size, T x, int e, Same same, Before before)
    {
      T* const end{items + size};
      if (T* const it{std::find_if(items, end, [&](const T& y) { return same(y, x); })}; it != end)
      {
        it->Exp = dimension::exponent(it->Exp + e);
        if (it->Exp == 0)
        {
          std::shift_left(it, end, 1);
          items[--size] = {};
        }
        return true;
      }
      if (e == 0)
        return true;
      if (size == static_cast<std::int8_t>(N))
        return false;
      x.Exp = dimension::exponent(e);
      std::int8_t at{size};
      while (at > 0 && before(x, items[at - 1]))
      {
        items[at] = items[at - 1];
        --at;
      }
      items[at] = x;
      ++size;
      return true;
    }
  } // namespace detail

  // The dimension of one base dimension (its exponent 1).
  constexpr dimension dimension_of(const base_dimension_def& b)
  {
    detail::exponents u;
    u.add(&b, 1);
    return u.intern();
  }

  // The dimension of a number.
  inline constexpr dimension dim_one{};

  // Blades: the basis vectors of a dimension.
  // A blade's canonical value is the product of its basis vectors in name order;
  // two different basis vectors anticommute and each squares to its Square.
  namespace detail
  {
    // A blade being computed: its basis vectors, sorted by name.
    using blade = std::vector<const base_dimension_def*>;

    constexpr blade blade_of(const dimension& d)
    {
      blade b;
      for (std::size_t i = 0; i < d.Size; ++i)
        if (d.Base[i]->Basis)
          b.push_back(d.Base[i]);
      return b;
    }
    constexpr bool has_blade(const dimension& d)
    {
      for (std::size_t i = 0; i < d.Size; ++i)
        if (d.Base[i]->Basis)
          return true;
      return false;
    }
    // Whether a and b are equal but for their basis vectors.
    constexpr bool same_outside_blade(const dimension& a, const dimension& b)
    {
      std::size_t i{0}, j{0};
      for (;;)
      {
        while (i < a.Size && a.Base[i]->Basis)
          ++i;
        while (j < b.Size && b.Base[j]->Basis)
          ++j;
        if (i == a.Size || j == b.Size)
          return i == a.Size && j == b.Size;
        if (a.Base[i]->Name != b.Base[j]->Name || a.Exp[i] != b.Exp[j])
          return false;
        ++i;
        ++j;
      }
    }

    // The sign s of A·B = s·(A ⊕ B), A and B canonical: -1 for each pair out of name order,
    // times the square of each basis vector in both. 0 when a null vector meets itself.
    constexpr int blade_sign(const blade& a, const blade& b)
    {
      int s{1};
      for (const base_dimension_def* x : a)
        for (const base_dimension_def* y : b)
        {
          if (x->Name > y->Name)
            s = -s;
          else if (x->Name == y->Name)
            s *= x->Square;
        }
      return s;
    }
    // a·b into a (the symmetric difference); the sign of the product.
    constexpr int blade_mul(blade& a, const blade& b)
    {
      const int   s{blade_sign(a, b)};
      blade       r;
      std::size_t i{0}, j{0};
      while (i < a.size() || j < b.size())
      {
        if (j == b.size() || (i < a.size() && a[i]->Name < b[j]->Name))
          r.push_back(a[i++]);
        else if (i == a.size() || b[j]->Name < a[i]->Name)
          r.push_back(b[j++]);
        else // in both: v·v is a number
        {
          ++i;
          ++j;
        }
      }
      a = std::move(r);
      return s;
    }
    constexpr int blade_sign(const dimension& a, const dimension& b) { return blade_sign(blade_of(a), blade_of(b)); }
    // B·B: +1 or -1, 0 for a blade with a null vector.
    constexpr int blade_square(const dimension& d)
    {
      const blade b = blade_of(d);
      return blade_sign(b, b);
    }
    // The sign of B's reverse, involute and Clifford conjugate, by its grade k:
    // (-1)^(k(k-1)/2), (-1)^k, (-1)^(k(k+1)/2).
    constexpr int grade_of(const dimension& d) { return static_cast<int>(blade_of(d).size()); }
    constexpr int reverse_sign(int k) { return (k * (k - 1) / 2) % 2 == 0 ? 1 : -1; }
    constexpr int involute_sign(int k) { return k % 2 == 0 ? 1 : -1; }
    constexpr int conjugate_sign(int k) { return (k * (k + 1) / 2) % 2 == 0 ? 1 : -1; }
  } // namespace detail

  // magnitude: Ratio·π^Pi, exact.
  struct magnitude
  {
    grid_number Ratio{1};
    std::int8_t Pi{0};

    // A power of π past ±127 is an error, not a wrap (as a dimension exponent is).
    static consteval std::int8_t pi_exponent(int e)
    {
      if (e < -127 || e > 127)
        ins::detail::constexpr_error<"units: a pi exponent past 127">();
      return static_cast<std::int8_t>(e);
    }

    friend constexpr bool operator==(const magnitude&, const magnitude&) = default;
    friend consteval magnitude operator*(const magnitude& a, const magnitude& b)
    {
      return {a.Ratio * b.Ratio, pi_exponent(a.Pi + b.Pi)};
    }
    friend consteval magnitude operator/(const magnitude& a, const magnitude& b)
    {
      return {a.Ratio / b.Ratio, pi_exponent(a.Pi - b.Pi)};
    }
    consteval magnitude pow(int n) const
    {
      magnitude r{};
      for (int i = 0; i < (n < 0 ? -n : n); ++i)
        r = r * *this;
      return n < 0 ? magnitude{} / r : r;
    }
    constexpr bool is_one() const { return Ratio == grid_number{1} && Pi == 0; }
  };

  // mag<r>: a magnitude tag, mag<1000> * si::metre, mag<1_g / 3_g> * …
  template <magnitude M>
  struct mag_t
  {
    static constexpr magnitude value = M;
  };
  namespace detail
  {
    template <auto R>
    inline constexpr mag_t<magnitude{grid_number{R}}>    mag{};
    inline constexpr mag_t<magnitude{grid_number{1}, 1}> mag_pi{};
  } // namespace detail

  template <magnitude A, magnitude B>
  consteval auto operator*(mag_t<A>, mag_t<B>)
  {
    return mag_t<A * B>{};
  }
  template <magnitude A, magnitude B>
  consteval auto operator/(mag_t<A>, mag_t<B>)
  {
    return mag_t<A / B>{};
  }

  // kind_def: a quantity kind (length, height, speed, energy, torque…).
  // A named kind's definition is a static object (units/detail/define.hpp); the tags users write (isq::height) carry
  // a pointer to it. A kind has a parent (height → altitude → length) or a recipe (speed = length / time), or both
  // (velocity: a speed whose recipe is displacement / duration).
  // Distinct starts a separate kind under its parent: it never converts implicitly across that boundary
  // (angular_measure vs a dimensionless ratio).
  struct kind_def;

  // An optional reference to a static object (a parent kind, a unit's kind, an origin).
  // It is tested by its flag, never against nullptr, and compared by content, never by address:
  // under GCC's -fsanitize=undefined `&obj == nullptr`, and under -fsanitize=pointer-compare `&a == &b`,
  // is no constant expression.
  template <class T>
  struct ref_or_none
  {
    const T* P{};
    bool     Set{false};

    constexpr ref_or_none() = default;
    constexpr ref_or_none(std::nullptr_t) {}
    constexpr ref_or_none(const T* p) : P{p}, Set{true} {} // p is not null
    constexpr explicit operator bool() const { return Set; }
    constexpr const T* operator->() const { return P; }
  };

  using kind_ref = ref_or_none<kind_def>;

  struct kind_factor
  {
    const kind_def* Kind{};
    std::int8_t     Exp{};
    friend constexpr bool operator==(const kind_factor& a, const kind_factor& b);
  };

  namespace detail
  {
    inline constexpr std::size_t max_kind_factors = 6;
  } // namespace detail

  // A product of named kinds to integer powers, sorted by name.
  struct recipe
  {
    kind_factor F[detail::max_kind_factors]{};
    std::int8_t Size{0};
    friend constexpr bool operator==(const recipe& a, const recipe& b)
    {
      return a.Size == b.Size && std::equal(a.F, a.F + a.Size, b.F);
    }
  };

  // An anonymous kind expression: what length / time is, before it is named.
  struct kind_expr
  {
    recipe    Recipe{};
    dimension Dim{};
  };

  // A kind's options, all optional:
  //   .distinct  true for a kind that never mixes with its siblings, although
  //              they share a dimension (activity and frequency are both 1/s)
  //   .recipe    the quantities it is made of, for a child that has a recipe of
  //              its own (velocity: displacement / duration, under speed)
  struct kind_options
  {
    bool      distinct{false};
    kind_expr recipe{};
  };

  template <const kind_def* S>
  struct kind_t;

  struct kind_def
  {
    std::string_view Name;
    dimension        Dim{};
    kind_ref         Parent{};
    recipe           Recipe{};
    bool             Distinct{false};

    // A base quantity: a dimension of its own (length, currency).
    constexpr kind_def(std::string_view name, dimension d) : Name{name}, Dim{d} {}
    // A child of a named kind.
    template <const kind_def* P>
    constexpr kind_def(std::string_view name, kind_t<P>) : Name{name}, Dim{P->Dim}, Parent{P} {}
    // A derived root: named by its recipe.
    constexpr kind_def(std::string_view name, const kind_expr& e) : Name{name}, Dim{e.Dim}, Recipe{e.Recipe} {}
    // Any of these with options, as kind<"name", parent, options> takes them:
    // kind_def{"velocity", isq::speed, {.recipe = isq::displacement / isq::duration}}.
    template <class P>
    constexpr kind_def(std::string_view name, const P& parent, const kind_options& o) : kind_def{name, parent}
    {
      Distinct = o.distinct;
      if (o.recipe.Recipe.Size == 0)
        return;
      if (!Parent)
        ins::detail::constexpr_error<"units: a recipe of its own is for a child of a named kind - "
                                     "kind<\"velocity\", isq::speed, {.recipe = …}>">();
      if (!(o.recipe.Dim == Dim))
        ins::detail::constexpr_error<"units: a kind's recipe must have its parent's dimension">();
      Recipe = o.recipe.Recipe;
    }
  };

  namespace detail
  {
    // Identity by content (see ref_or_none).
    // A kind is its name, dimension, recipe, kind boundary and parent chain:
    // a user kind that reuses a name ("length") under another parent is another kind.
    constexpr bool same(const kind_def* a, const kind_def* b) // both set
    {
      if (a->Name != b->Name || !(a->Dim == b->Dim) || a->Distinct != b->Distinct || !(a->Recipe == b->Recipe))
        return false;
      if (!a->Parent || !b->Parent)
        return !a->Parent && !b->Parent;
      return same(a->Parent.P, b->Parent.P);
    }
    constexpr bool same(const kind_ref& a, const kind_ref& b)
    {
      if (!a || !b)
        return !a && !b;
      return same(a.P, b.P);
    }

  } // namespace detail

  constexpr bool operator==(const kind_factor& a, const kind_factor& b)
  {
    return detail::same(a.Kind, b.Kind) && a.Exp == b.Exp;
  }

  namespace detail
  {

    // The kind's kind root: the first Distinct kind on the way up, else the root.
    consteval const kind_def* kind_root(const kind_def* s)
    {
      while (!s->Distinct && s->Parent)
        s = s->Parent.P;
      return s;
    }
    // a is s or an ancestor of s within s's kind (not above an Distinct boundary).
    consteval bool is_within_kind_ancestor(const kind_def* s, const kind_def* a)
    {
      for (;;)
      {
        if (same(s, a))
          return true;
        if (s->Distinct || !s->Parent)
          return false;
        s = s->Parent.P;
      }
    }

    // r · f^e, merged and kept sorted by name; an exponent that reaches 0 drops out.
    // Returns false when the recipe is full.
    consteval bool recipe_add(recipe& r, const kind_def* f, int e)
    {
      return sorted_add(
        r.F, r.Size, kind_factor{f, 0}, e,
        [](const kind_factor& a, const kind_factor& b) { return same(a.Kind, b.Kind); },
        [](const kind_factor& a, const kind_factor& b) { return a.Kind->Name < b.Kind->Name; });
    }

    // a · b^sign; false when the product does not fit a recipe.
    consteval bool recipe_merge(recipe& a, const recipe& b, int sign)
    {
      for (std::int8_t i = 0; i < b.Size; ++i)
        if (!recipe_add(a, b.F[i].Kind, b.F[i].Exp * sign))
          return false;
      return true;
    }

    consteval recipe recipe_of(const kind_def* s)
    {
      if (s->Recipe.Size > 0)
        return s->Recipe;
      recipe r{};
      recipe_add(r, s, 1);
      return r;
    }

    // The recipe with every factor replaced by its kind root, summed.
    consteval recipe recipe_by_kind(const recipe& r)
    {
      recipe out{};
      for (std::int8_t i = 0; i < r.Size; ++i)
        recipe_add(out, kind_root(r.F[i].Kind), r.F[i].Exp);
      return out;
    }

    // Each factor of `want` matched by a factor of `have` with the same exponent that is it or descends from it,
    // one to one.
    consteval bool recipe_matches(const recipe& have, const recipe& want)
    {
      if (have.Size != want.Size)
        return false;
      bool used[max_kind_factors]{};
      for (std::int8_t i = 0; i < want.Size; ++i)
      {
        bool found{false};
        for (std::int8_t j = 0; j < have.Size && !found; ++j)
          if (!used[j] && have.F[j].Exp == want.F[i].Exp
              && is_within_kind_ancestor(have.F[j].Kind, want.F[i].Kind))
          {
            used[j] = true;
            found   = true;
          }
        if (!found)
          return false;
      }
      return true;
    }
  } // namespace detail

  template <const kind_def* S>
  struct kind_t
  {
    static constexpr const kind_def* value = S;
    constexpr operator kind_ref() const { return S; } // unit_options{.kind = isq::frequency}
    // isq::height[si::metre], defined after unit_t and reference_t.
    template <class U>
    consteval auto operator[](U) const;
  };

  namespace detail
  {
    // Kind algebra for recipes: length / time, pow<2>(length), inverse(radius).
    template <const kind_def* S>
    consteval kind_expr expr_of(kind_t<S>)
    {
      kind_expr e{};
      recipe_add(e.Recipe, S, 1);
      e.Dim = S->Dim;
      return e;
    }
    consteval kind_expr expr_of(const kind_expr& e) { return e; }

    template <class T>
    concept kind_operand = requires(T t) { expr_of(t); };

    consteval kind_expr kind_combine(const kind_expr& a, const kind_expr& b, int sign)
    {
      kind_expr r{a};
      if (!recipe_merge(r.Recipe, b.Recipe, sign))
        ins::detail::constexpr_error<"units: a recipe holds at most 6 factors">();
      r.Dim = sign > 0 ? a.Dim * b.Dim : a.Dim / b.Dim;
      return r;
    }
  } // namespace detail

  template <detail::kind_operand A, detail::kind_operand B>
  consteval kind_expr operator*(A a, B b)
  {
    return detail::kind_combine(detail::expr_of(a), detail::expr_of(b), 1);
  }
  template <detail::kind_operand A, detail::kind_operand B>
  consteval kind_expr operator/(A a, B b)
  {
    return detail::kind_combine(detail::expr_of(a), detail::expr_of(b), -1);
  }
  // Two kinds are equal when their expressions are: one named kind, or one recipe of named kinds
  // (isq::length / isq::duration == isq::length / isq::time).
  template <detail::kind_operand A, detail::kind_operand B>
  consteval bool operator==(A a, B b)
  {
    const kind_expr x{detail::expr_of(a)}, y{detail::expr_of(b)};
    return x.Recipe == y.Recipe && x.Dim == y.Dim;
  }

  template <int N, detail::kind_operand A>
  consteval kind_expr pow(A a)
  {
    kind_expr r{};
    for (int i = 0; i < (N < 0 ? -N : N); ++i)
      r = detail::kind_combine(r, detail::expr_of(a), N < 0 ? -1 : 1);
    return r;
  }
  template <detail::kind_operand A>
  consteval kind_expr inverse(A a)
  {
    return detail::kind_combine(kind_expr{}, detail::expr_of(a), -1);
  }

  struct unit_def;

  // origin_def: where a quantity_point's number counts from.
  // An absolute origin (absolute zero) or one at an exact offset from one, in the coherent unit of the dimension
  // (the ice point: absolute zero + 273.15 K).
  struct origin_def
  {
    std::string_view        Name;
    dimension               Dim{};
    ref_or_none<origin_def> Absolute{}; // none for an absolute origin
    grid_number             Offset{0};  // from Absolute, in the coherent unit

    // An absolute origin: the zero of a dimension, or of a kind's (absolute zero).
    constexpr origin_def(std::string_view name, dimension d) : Name{name}, Dim{d} {}
    template <const kind_def* K>
    constexpr origin_def(std::string_view name, kind_t<K>) : Name{name}, Dim{K->Dim} {}
    // One at a constant point of another origin: origin_def{"ice point", si::absolute_zero + 273.15_ins * si::kelvin}
    // (defined in units/detail/define.hpp).
    template <class P>
      requires requires(const P& p) { p.quantity_from_origin(); }
    constexpr origin_def(std::string_view name, const P&);
  };

  using origin_ref = ref_or_none<origin_def>; // none: the implicit zero of a dimension

  template <origin_ref O>
  struct origin_t
  {
    static constexpr origin_ref value = O;
    constexpr operator origin_ref() const { return O; } // unit_options{.origin = si::ice_point}
  };

  namespace detail
  {
    consteval origin_ref absolute_of(origin_ref o) { return o && o->Absolute ? o->Absolute : o; }
    // By content, as same() for kinds and units (see there).
    constexpr bool same(const origin_ref& a, const origin_ref& b)
    {
      if (!a || !b)
        return !a && !b;
      return a->Name == b->Name && a->Dim == b->Dim && a->Offset == b->Offset && same(a->Absolute, b->Absolute);
    }
    consteval grid_number offset_of(origin_ref o) { return o && o->Absolute ? o->Offset : grid_number{0}; }
  } // namespace detail

  // A short text held by value (a symbol, a name), so that a type which holds one is structural:
  // a template argument, as in unit<"ftm", …> and {.ascii = "R"}.
  struct symbol_text
  {
    static constexpr std::size_t capacity = 64;
    char                         Text[capacity]{};
    std::uint8_t                 Size{0};

    constexpr symbol_text() = default;
    template <std::size_t N>
    consteval symbol_text(const char (&s)[N]) : Size{static_cast<std::uint8_t>(N - 1)}
    {
      if (N > capacity)
        ins::detail::constexpr_error<"units: a symbol or name is at most 63 bytes">();
      for (std::size_t i = 0; i + 1 < N; ++i)
        Text[i] = s[i];
    }
    [[nodiscard]] constexpr std::string_view view() const { return {Text, Size}; }
    [[nodiscard]] constexpr bool empty() const { return Size == 0; }
    friend constexpr bool operator==(const symbol_text&, const symbol_text&) = default;
  };

  // A named unit's options, all optional (designated, in this order):
  //   .ascii         the ASCII symbol ("ohm" for Ω), else the symbol itself
  //   .kind          the quantity kind it measures, else its definition's
  //   .origin        the default origin of its points (℃: the ice point)
  //   .space_before  false for a symbol written against the number (90°)
  struct unit_options
  {
    symbol_text ascii{};
    kind_ref    kind{};
    origin_ref  origin{};
    bool        space_before{true};
  };

  struct unit_atom
  {
    const unit_def* U{};
    std::int8_t     Prefix{0}; // power of ten
    std::int8_t     Exp{0};
    friend constexpr bool operator==(const unit_atom& a, const unit_atom& b);
  };

  namespace detail
  {
    inline constexpr std::size_t max_unit_atoms = 6;
  } // namespace detail

  // unit: a product of named units to integer powers, each with an optional decimal prefix (k, m, µ…),
  // times a scale (mag<1000> * m; the common unit of a sum). A structural value: it is the template argument.
  struct unit_value
  {
    unit_atom   Atoms[detail::max_unit_atoms]{};
    std::int8_t Size{0};
    magnitude   Scale{};

    constexpr unit_value() = default;
    constexpr unit_value(const unit_def& u) : Atoms{{&u, 0, 1}}, Size{1} {}

    friend constexpr bool operator==(const unit_value& a, const unit_value& b)
    {
      return a.Size == b.Size && a.Scale == b.Scale && std::equal(a.Atoms, a.Atoms + a.Size, b.Atoms);
    }
  };

  template <unit_value U>
  struct unit_t
  {
    static constexpr unit_value value = U;
  };

  namespace detail
  {
    // Whether T is a quantity (specialized in units/detail/quantity.hpp).
    template <class T>
    inline constexpr bool is_quantity_v = false;
  } // namespace detail

  // unit_def: the definition of a unit with a symbol (m, s, N, Ω, °C), a static object that unit<"sym", …> makes.
  // Its magnitude is relative to the coherent unit of its dimension;
  // Kind (if any) is the quantity kind it measures (Hz: frequency, Bq: activity).
  struct unit_def
  {
    std::string_view Utf8;
    symbol_text      Ascii; // empty: the symbol itself
    dimension        Dim{};
    magnitude        Mag{};
    kind_ref         Kind{};
    bool             SpaceBefore{true};
    origin_ref       Origin{}; // the default origin of its points (K: absolute zero)

    // A base unit of a kind: the coherent unit of its dimension, or a scale of it (g: 1/1000 of the coherent kg).
    template <const kind_def* K>
    constexpr unit_def(std::string_view sym, kind_t<K>, unit_options o = {})
      : unit_def{sym, mag_t<magnitude{}>{}, kind_t<K>{}, o} {}
    template <magnitude M, const kind_def* K>
    constexpr unit_def(std::string_view sym, mag_t<M>, kind_t<K>, unit_options o = {})
      : Utf8{sym}, Ascii{o.ascii}, Dim{K->Dim}, Mag{M}, Kind{K}, SpaceBefore{o.space_before}, Origin{o.origin} {}
    // A unit defined by others: unit_def{"N", kg * m / s², isq::force}.
    // Its kind is the given one, else .kind, else its one factor's (min: 60 s).
    template <unit_value U>
    constexpr unit_def(std::string_view sym, unit_t<U>, unit_options o = {});
    template <unit_value U, const kind_def* K>
    constexpr unit_def(std::string_view sym, unit_t<U>, kind_t<K>, unit_options o = {});
    // A unit defined by the constant quantity it equals: unit_def{"ftm", 1.8288_ins * si::metre}
    // (defined in units/detail/define.hpp).
    template <class Q>
      requires detail::is_quantity_v<Q>
    constexpr unit_def(std::string_view sym, const Q&, unit_options o = {});

    [[nodiscard]] constexpr std::string_view ascii() const { return Ascii.empty() ? Utf8 : Ascii.view(); }

  private:
    friend struct basis_vector_def;
    // A basis vector's unit, its dimension set by basis_vector.
    constexpr unit_def(std::string_view sym, unit_options o)
      : Utf8{sym}, Ascii{o.ascii}, SpaceBefore{o.space_before} {}
  };

  // basis_vector_def: the definition of a basis vector of a geometric algebra (basis_vector<"e₁", +1>):
  // a base dimension of your own and its unit in one static object, with its square, v·v: +1, -1 or 0 (a null
  // vector). Two different basis vectors anticommute, so a product of units is again a unit, up to a sign:
  // e₂e₁ = -e₁e₂.
  struct basis_vector_def
  {
    unit_def           Unit;
    base_dimension_def Self;

    consteval basis_vector_def(std::string_view sym, int square, unit_options o = {})
      : Unit{sym, o}, Self{sym, static_cast<std::int8_t>(square), true, &Unit}
    {
      if (square != 1 && square != -1 && square != 0)
        ins::detail::constexpr_error<"units: a basis vector squares to +1, -1 or 0">();
      if (o.kind || o.origin)
        ins::detail::constexpr_error<"units: a basis vector has no kind or origin">();
      Unit.Dim = dimension_of(Self);
    }
  };

  namespace detail
  {
    // A unit is its symbols, dimension, magnitude, kind and origin.
    constexpr bool same(const unit_def* a, const unit_def* b) // both set
    {
      return a->Utf8 == b->Utf8 && a->ascii() == b->ascii() && a->Dim == b->Dim && a->Mag == b->Mag
             && same(a->Kind, b->Kind) && same(a->Origin, b->Origin);
    }

  } // namespace detail

  constexpr bool operator==(const unit_atom& a, const unit_atom& b)
  {
    return detail::same(a.U, b.U) && a.Prefix == b.Prefix && a.Exp == b.Exp;
  }

  namespace detail
  {

    constexpr bool atom_before(const unit_atom& a, const unit_atom& b)
    {
      if (a.U->Utf8 != b.U->Utf8)
        return a.U->Utf8 < b.U->Utf8;
      return a.Prefix < b.Prefix;
    }

    consteval void unit_add(unit_value& u, unit_atom a)
    {
      if (!sorted_add(
            u.Atoms, u.Size, a, a.Exp,
            [](const unit_atom& x, const unit_atom& y) { return same(x.U, y.U) && x.Prefix == y.Prefix; },
            atom_before))
        ins::detail::constexpr_error<"units: a unit holds at most 6 factors - name it with unit<\"sym\", …>">();
    }

    // An atom whose named unit holds basis vectors (e₁, i = e₁e₂).
    constexpr bool is_blade_atom(const unit_atom& a) { return has_blade(a.U->Dim); }
    constexpr bool has_blade_atoms(const unit_value& u)
    {
      return std::any_of(u.Atoms, u.Atoms + u.Size, [](const unit_atom& a) { return is_blade_atom(a); });
    }

    consteval unit_value blade_unit_mul(const unit_value& a, const unit_value& b, int sign);

    consteval unit_value unit_mul(unit_value a, const unit_value& b, int sign)
    {
      if (has_blade_atoms(a) || has_blade_atoms(b))
        return blade_unit_mul(a, b, sign);
      for (std::int8_t i = 0; i < b.Size; ++i)
      {
        unit_atom t{b.Atoms[i]};
        t.Exp = static_cast<std::int8_t>(t.Exp * sign);
        unit_add(a, t);
      }
      a.Scale = sign > 0 ? a.Scale * b.Scale : a.Scale / b.Scale;
      return a;
    }

    consteval unit_value unit_pow(const unit_value& u, int n)
    {
      if (has_blade_atoms(u)) // repeated products: the blades do not commute
      {
        unit_value r{};
        for (int i = 0; i < (n < 0 ? -n : n); ++i)
          r = unit_mul(r, u, n < 0 ? -1 : 1);
        return r;
      }
      unit_value r{};
      for (std::int8_t i = 0; i < u.Size; ++i)
        unit_add(r, {u.Atoms[i].U, u.Atoms[i].Prefix, dimension::exponent(u.Atoms[i].Exp * n)});
      r.Scale = u.Scale.pow(n);
      return r;
    }

    consteval unit_value unit_scaled(unit_value u, const magnitude& m)
    {
      u.Scale = m * u.Scale;
      return u;
    }
    consteval unit_value unit_prefixed(unit_value u, int p)
    {
      u.Atoms[0].Prefix = static_cast<std::int8_t>(p);
      return u;
    }

    consteval magnitude ten_pow(int p)
    {
      magnitude ten{grid_number{10}};
      return ten.pow(p);
    }
  } // namespace detail

  // A unit's dimension, magnitude (relative to the coherent unit of its dimension) and kind.
  consteval dimension dimension_of(const unit_value& u)
  {
    if (u.Size == 1 && u.Atoms[0].Exp == 1) // a named unit: its dimension, interned already
      return u.Atoms[0].U->Dim;
    detail::exponents d;
    for (std::int8_t i = 0; i < u.Size; ++i)
    {
      const dimension& a{u.Atoms[i].U->Dim};
      for (std::size_t j = 0; j < a.Size; ++j)
        d.add(a.Base[j], a.Exp[j] * u.Atoms[i].Exp);
    }
    return d.intern();
  }
  namespace detail
  {
    // The sign of the product of u's blade atoms in stored order against the canonical blade of their product:
    // ±1, or 0 for a product that vanishes.
    consteval int atoms_blade_sign(const unit_value& u)
    {
      blade acc;
      int   s{1};
      for (std::int8_t i = 0; i < u.Size; ++i)
      {
        if (!is_blade_atom(u.Atoms[i]))
          continue;
        const blade b = blade_of(u.Atoms[i].U->Dim);
        const int   e = u.Atoms[i].Exp;
        if (e < 0 && blade_sign(b, b) == 0)
          ins::detail::constexpr_error<"units: a null blade (a basis vector that squares to 0) has no "
                                       "inverse">();
        for (int k = 0; k < (e < 0 ? -e : e); ++k)
          s *= (e < 0 ? blade_sign(b, b) : 1) * blade_mul(acc, b); // B⁻¹ = B·B²
      }
      return s;
    }
  } // namespace detail

  consteval magnitude magnitude_of(const unit_value& u)
  {
    magnitude m{u.Scale};
    for (std::int8_t i = 0; i < u.Size; ++i)
      m = m * (u.Atoms[i].U->Mag * detail::ten_pow(u.Atoms[i].Prefix)).pow(u.Atoms[i].Exp);
    if (detail::has_blade_atoms(u))
      m = m * magnitude{grid_number{detail::atoms_blade_sign(u)}};
    return m;
  }

  namespace detail
  {
    // a·b (sign 1) or a/b (sign -1) of units with blades. The non-blade atoms merge as usual.
    // The blade atoms are kept when they share no basis vector (1·i stays i, i·j stays "i j"),
    // else the blade of the product is spelled by its basis vectors (i·i is 1, e₁·e₁₂₃ is e₂e₃).
    // The scale makes the magnitude the true one, sign included:
    // a negative scale is the number's sign, a zero scale a product that vanishes (ε·ε).
    consteval unit_value blade_unit_mul(const unit_value& a, const unit_value& b, int sign)
    {
      const dimension da{dimension_of(a)}, db{dimension_of(b)};
      blade           bl = blade_of(da);
      const blade     br = blade_of(db);
      int             s{blade_mul(bl, br)};
      if (sign < 0) // B⁻¹ = B·B²
      {
        const int sq{blade_sign(br, br)};
        if (sq == 0)
          ins::detail::constexpr_error<"units: a null blade (a basis vector that squares to 0) has no "
                                       "inverse">();
        s *= sq;
      }
      const magnitude target{sign > 0 ? magnitude_of(a) * magnitude_of(b) : magnitude_of(a) / magnitude_of(b)};

      unit_value             r{};
      std::vector<unit_atom> blades;
      const auto take = [&](const unit_value& u, int sgn)
      {
        for (std::int8_t i = 0; i < u.Size; ++i)
        {
          unit_atom t{u.Atoms[i]};
          if (is_blade_atom(t))
          {
            t.Exp = 1;
            blades.push_back(t);
          }
          else
          {
            t.Exp = static_cast<std::int8_t>(t.Exp * sgn);
            unit_add(r, t);
          }
        }
      };
      take(a, 1);
      take(b, sign);
      bool disjoint{true};
      for (std::size_t i = 0; i < blades.size() && disjoint; ++i)
        for (std::size_t j = i + 1; j < blades.size() && disjoint; ++j)
        {
          blade x = blade_of(blades[i].U->Dim);
          blade_mul(x, blade_of(blades[j].U->Dim));
          disjoint = x.size() == blade_of(blades[i].U->Dim).size() + blade_of(blades[j].U->Dim).size();
        }
      if (disjoint)
        for (const unit_atom& t : blades)
          unit_add(r, t);
      else
        for (const base_dimension_def* v : bl)
          unit_add(r, unit_atom{v->Vector, 0, 1});
      r.Scale = s == 0 ? magnitude{grid_number{0}} : target * magnitude{grid_number{s}} / magnitude_of(r);
      return r;
    }
  } // namespace detail
  namespace detail
  {
    // One named unit (with a prefix too) to the power 1, unscaled: its kind, origin and spacing are the unit's.
    consteval bool is_single_named(const unit_value& u)
    {
      return u.Size == 1 && u.Atoms[0].Exp == 1 && u.Scale.is_one();
    }
    // Whether a quantity's text puts a space before the unit (not before °).
    consteval bool space_before(const unit_value& u)
    {
      return !(is_single_named(u) && !u.Atoms[0].U->SpaceBefore);
    }
  } // namespace detail

  // The kind a unit measures: a named unit's (with a prefix too), else none.
  consteval kind_ref kind_of(const unit_value& u)
  {
    return detail::is_single_named(u) ? u.Atoms[0].U->Kind : kind_ref{};
  }

  namespace detail
  {
    // The kind a unit defined by U inherits: its one factor's (min: 60 s).
    consteval kind_ref inherited_kind(const unit_value& u)
    {
      return u.Size == 1 && u.Atoms[0].Exp == 1 ? u.Atoms[0].U->Kind : kind_ref{};
    }
  } // namespace detail

  template <unit_value U>
  constexpr unit_def::unit_def(std::string_view sym, unit_t<U>, unit_options o)
    : Utf8{sym},
      Ascii{o.ascii},
      Dim{dimension_of(U)},
      Mag{magnitude_of(U)},
      Kind{o.kind ? o.kind : detail::inherited_kind(U)},
      SpaceBefore{o.space_before},
      Origin{o.origin}
  {
    if (Kind && !(Kind->Dim == Dim))
      ins::detail::constexpr_error<"units: a unit's kind must have the unit's dimension">();
    if (Origin && !(Origin->Dim == Dim))
      ins::detail::constexpr_error<"units: a unit's origin must have the unit's dimension">();
    if (detail::has_blade(Dim) && !detail::same_outside_blade(Dim, dim_one))
      ins::detail::constexpr_error<"units: a named unit with basis vectors names only a blade - "
                                   "write si::metre * e1, not a unit of both">();
    if (Mag.Ratio == grid_number{0})
      ins::detail::constexpr_error<"units: a named unit of a product that vanishes (a null vector "
                                   "squared)">();
  }
  template <unit_value U, const kind_def* K>
  constexpr unit_def::unit_def(std::string_view sym, unit_t<U>, kind_t<K>, unit_options o)
    : unit_def{sym, unit_t<U>{}, unit_options{o.ascii, K, o.origin, o.space_before}} {}

  // The tag of a definition object: what unit<…>, kind<…> and origin<…> give (units/detail/define.hpp).
  namespace detail
  {
    template <class N>
    consteval const unit_def& def_of(const N& n)
    {
      if constexpr (std::same_as<N, basis_vector_def>)
        return n.Unit;
      else
        return n;
    }
  } // namespace detail
  namespace detail
  {
    template <const auto& N>
      requires std::same_as<std::remove_cvref_t<decltype(N)>, unit_def>
            || std::same_as<std::remove_cvref_t<decltype(N)>, basis_vector_def>
    inline constexpr unit_t<unit_value{detail::def_of(N)}> unit_tag{};
    template <const kind_def& S>
    inline constexpr kind_t<&S> kind_tag{};
    template <const origin_def& O>
    inline constexpr origin_t<&O> origin_tag{};
  } // namespace detail

  // The dimensionless unit 1.
  inline constexpr unit_t<unit_value{}> one{};

  template <unit_value A, unit_value B>
  consteval bool operator==(unit_t<A>, unit_t<B>)
  {
    return A == B;
  }
  template <unit_value A, unit_value B>
  consteval auto operator*(unit_t<A>, unit_t<B>)
  {
    return unit_t<detail::unit_mul(A, B, 1)>{};
  }
  template <unit_value A, unit_value B>
  consteval auto operator/(unit_t<A>, unit_t<B>)
  {
    return unit_t<detail::unit_mul(A, B, -1)>{};
  }
  template <magnitude M, unit_value B>
  consteval auto operator*(mag_t<M>, unit_t<B>)
  {
    return unit_t<detail::unit_scaled(B, M)>{};
  }
  template <magnitude M, unit_value B>
  consteval auto operator/(mag_t<M>, unit_t<B>)
  {
    return unit_t<detail::unit_scaled(detail::unit_pow(B, -1), M)>{};
  }
  template <unit_value B, magnitude M>
  consteval auto operator*(unit_t<B>, mag_t<M>)
  {
    return unit_t<detail::unit_scaled(B, M)>{};
  }
  template <unit_value B, magnitude M>
  consteval auto operator/(unit_t<B>, mag_t<M>)
  {
    return unit_t<detail::unit_scaled(B, magnitude{} / M)>{};
  }
  template <int N, unit_value A>
  consteval auto pow(unit_t<A>)
  {
    return unit_t<detail::unit_pow(A, N)>{};
  }
  template <unit_value A>
  consteval auto square(unit_t<A>)
  {
    return unit_t<detail::unit_pow(A, 2)>{};
  }
  template <unit_value A>
  consteval auto cubic(unit_t<A>)
  {
    return unit_t<detail::unit_pow(A, 3)>{};
  }
  template <unit_value A>
  consteval auto inverse(unit_t<A>)
  {
    return unit_t<detail::unit_pow(A, -1)>{};
  }

  // A decimal prefix: si::kilo(si::metre). Only on a named unit, once.
  template <int P>
  struct prefix_t
  {
    template <unit_value U>
    consteval auto operator()(unit_t<U>) const
    {
      static_assert(U.Size == 1 && U.Atoms[0].Exp == 1 && U.Atoms[0].Prefix == 0 && U.Scale.is_one(),
                    "units: a prefix applies to a named unit without a prefix (kilo(metre), not kilo(m/s))");
      return unit_t<detail::unit_prefixed(U, P)>{};
    }
  };

  // reference: what a quantity's number means, a unit plus a kind.
  //   Kind named, KindOf false   isq::height[m]: exactly that kind
  //   Kind named, KindOf true    a unit's kind (si::hertz): any kind of its kind
  //   Kind null, Derived set     a product of kinds (width * height)
  //   Kind null, Derived empty   any quantity of the unit's dimension (m/s)
  struct reference
  {
    kind_ref   Kind{};
    bool       KindOf{false};
    recipe     Derived{};
    unit_value U{};

    constexpr reference() = default;
    constexpr reference(kind_ref s, bool kind_of, recipe derived, unit_value u)
      : Kind{s}, KindOf{kind_of}, Derived{derived}, U{u} {}
    // A unit tag as a reference: quantity<si::metre, …>.
    template <unit_value V>
    constexpr reference(unit_t<V>) : Kind{kind_of(V)}, KindOf{static_cast<bool>(kind_of(V))}, U{V} {}

    friend constexpr bool operator==(const reference& a, const reference& b)
    {
      return detail::same(a.Kind, b.Kind) && a.KindOf == b.KindOf && a.Derived == b.Derived && a.U == b.U;
    }
  };

  consteval reference reference_of(const unit_value& u)
  {
    const kind_ref k{kind_of(u)};
    return reference{k, static_cast<bool>(k), recipe{}, u};
  }

  template <reference R>
  struct reference_t
  {
    static constexpr reference value = R;
    constexpr operator reference() const { return R; }
  };

  namespace detail
  {
    template <class T>
    inline constexpr bool is_unit_tag_v = false;
    template <unit_value U>
    inline constexpr bool is_unit_tag_v<unit_t<U>> = true;

    template <class T>
    inline constexpr bool is_reference_tag_v = false;
    template <unit_value U>
    inline constexpr bool is_reference_tag_v<unit_t<U>> = true;
    template <reference R>
    inline constexpr bool is_reference_tag_v<reference_t<R>> = true;
  } // namespace detail

  // The reference of a tag: a unit_t is the unit with its kind.
  template <unit_value U>
  consteval reference reference_of(unit_t<U>)
  {
    return reference_of(U);
  }
  template <reference R>
  consteval reference reference_of(reference_t<R>)
  {
    return R;
  }

  template <const kind_def* S>
  template <class U>
  consteval auto kind_t<S>::operator[](U) const
  {
    static_assert(detail::is_reference_tag_v<U> && requires { U::value.Size; }, "units: kind[unit] takes a unit");
    constexpr unit_value u{U::value};
    static_assert(dimension_of(u) == S->Dim, "units: kind[unit] - the unit has another dimension than the kind");
    constexpr kind_ref k{kind_of(u)};
    static_assert(!k || detail::same(detail::kind_root(k.P), detail::kind_root(S)),
                  "units: kind[unit] - the unit measures another kind of quantity (Hz for activity?)");
    return reference_t<reference{S, false, recipe{}, u}>{};
  }

  namespace detail
  {
    // Which kind facts a reference carries.
    consteval bool is_generic(const reference& r) { return !r.Kind && r.Derived.Size == 0; }

    // Whether a reference's quantity may convert implicitly to a named kind `to`.
    consteval bool derived_converts_to(const reference& from, const kind_def* to)
    {
      if (to->Distinct && !from.KindOf)
        return false;
      // Any quantity of the dimension names a kind root: m/s → speed, N·m → energy.
      if (!to->Parent && !to->Distinct)
        return true;
      if (from.KindOf)
        return derived_converts_to(reference{nullptr, false, from.Derived, from.U}, kind_root(to))
               || recipe_matches(from.Derived, recipe_of(kind_root(to)));
      return to->Recipe.Size > 0 && recipe_matches(from.Derived, to->Recipe);
    }

    // Implicit conversion between the kinds of two references of one dimension.
    consteval bool kind_converts(const reference& from, const reference& to)
    {
      if (is_generic(to)) // a dimension's quantity takes any of its kinds but an Distinct one
        return !from.Kind || !kind_root(from.Kind.P)->Distinct;
      if (is_generic(from))
        return !to.Kind || !kind_root(to.Kind.P)->Distinct;
      if (!to.Kind) // a derived target: the source's recipe, by kind
      {
        const recipe have{from.Kind ? recipe_of(from.Kind.P) : from.Derived};
        return recipe_by_kind(have) == recipe_by_kind(to.Derived);
      }
      if (!from.Kind)
        return derived_converts_to(from, to.KindOf ? kind_root(to.Kind.P) : to.Kind.P);
      if (to.KindOf || from.KindOf) // a unit's kind takes and gives any kind of that kind
        return same(kind_root(from.Kind.P), kind_root(to.Kind.P));
      return is_within_kind_ancestor(from.Kind.P, to.Kind.P);
    }

    // Explicit conversion (kind_cast): any kind of the same dimension, but not across Distinct kinds.
    consteval bool kind_casts(const reference& from, const kind_def* to)
    {
      const kind_ref  fk{from.Kind ? kind_ref{kind_root(from.Kind.P)} : kind_ref{}};
      const kind_def* tk{kind_root(to)};
      if (same(fk, kind_ref{tk}))
        return true;
      if (fk && fk->Distinct)
        return false;
      return !tk->Distinct || !from.Kind;
    }

    // The kind part of a sum's reference; ok = false when the two cannot add.
    struct kind_result
    {
      bool      Ok{false};
      reference R{};
    };

    consteval kind_result common_kind(const reference& a, const reference& b)
    {
      if (same(a.Kind, b.Kind) && a.KindOf == b.KindOf && a.Derived == b.Derived)
        return {true, a};
      if (kind_converts(a, b) && !kind_converts(b, a))
        return {true, b};
      if (kind_converts(b, a) && !kind_converts(a, b))
        return {true, a};
      if (a.Kind && b.Kind)
      {
        if (a.KindOf && b.KindOf && same(kind_root(a.Kind.P), kind_root(b.Kind.P)))
          return {true, reference{kind_root(a.Kind.P), true, {}, a.U}};
        if (a.KindOf != b.KindOf && same(kind_root(a.Kind.P), kind_root(b.Kind.P))) // kind_of + a named kind of it
          return {true, a.KindOf ? b : a};
        // The nearest common ancestor within one kind.
        for (const kind_def* s = a.Kind.P;; s = s->Parent.P)
        {
          if (is_within_kind_ancestor(b.Kind.P, s))
            return {true, reference{s, false, {}, a.U}};
          if (s->Distinct || !s->Parent)
            break;
        }
        return {};
      }
      if (kind_converts(a, b) && kind_converts(b, a)) // e.g. two recipes of one kind
        return {true, reference{nullptr, false, {}, a.U}};
      return {};
    }

    // The kind part of a product: named factors multiply into a recipe.
    // (Kind null, KindOf, no recipe) is the empty product: 1 / x starts there.
    consteval reference kind_product(const reference& a, const reference& b, int sign, const unit_value& u)
    {
      const auto empty = [](const reference& r) consteval { return !r.Kind && r.KindOf && r.Derived.Size == 0; };
      if ((is_generic(a) && !empty(a)) || (is_generic(b) && !empty(b)))
        return reference{nullptr, false, {}, u};
      const auto factors = [](const reference& r) consteval
      {
        if (!r.Kind)
          return r.Derived;
        recipe f{};
        recipe_add(f, r.KindOf ? kind_root(r.Kind.P) : r.Kind.P, 1);
        return f;
      };
      recipe f{factors(a)};
      if (!recipe_merge(f, factors(b), sign) || f.Size == 0) // too many factors, or a pure ratio
        return reference{nullptr, false, {}, u};
      const bool kind_of{(a.KindOf || empty(a)) && (b.KindOf || empty(b))};
      if (f.Size == 1 && f.F[0].Exp == 1)
        return reference{f.F[0].Kind, kind_of, {}, u};
      return reference{nullptr, kind_of, f, u};
    }

    consteval reference kind_pow(const reference& a, int n, const unit_value& u)
    {
      if (is_generic(a) || n == 0)
        return reference{nullptr, false, {}, u};
      reference r{a};
      r.U = u;
      for (int i = 1; i < (n < 0 ? -n : n); ++i)
        r = kind_product(r, a, 1, u);
      return n < 0 ? kind_product(reference{nullptr, true, {}, u}, r, -1, u) : r;
    }
  } // namespace detail

  // Unit conversion of a number: the value in unit From as a value in unit To, value · mag(From) / mag(To).
  // A rational factor rescales the grid (the raw is kept where the storage allows); a π factor has no exact rescale.
  namespace detail
  {
    template <magnitude M>
    inline constexpr bool exact_factor_v = M.Pi == 0;

    template <magnitude M, insidable V>
    constexpr auto rescale(const V& v)
    {
      static_assert(exact_factor_v<M>, INSIDE_UNITS_WHY_PI);
      if constexpr (M.is_one())
        return v;
      else
      {
        // The product's grid with v's policy: a conversion keeps clamp, wrap and the rounding mode.
        const auto x{v * just<M.Ratio>};
        if constexpr (policy_of<decltype(x)> == policy_of<V>)
          return x;
        else
          return inside<grid_of<decltype(x)>, policy_of<V>>::from_raw(x.raw(), unsafe);
      }
    }

    template <unit_value From, unit_value To>
    inline constexpr magnitude conversion_factor = magnitude_of(From) / magnitude_of(To);

    // A fixed order of units: fewer atoms first, then by symbol and exponent.
    consteval bool unit_before(const unit_value& a, const unit_value& b)
    {
      if (a.Size != b.Size)
        return a.Size < b.Size;
      return std::lexicographical_compare(
        a.Atoms, a.Atoms + a.Size, b.Atoms, b.Atoms + b.Size,
        [](const unit_atom& x, const unit_atom& y)
        {
          if (atom_before(x, y) || atom_before(y, x))
            return atom_before(x, y);
          return x.Exp < y.Exp;
        });
    }

    // The common unit of a sum: the smaller unit when it divides the other (km + m → m),
    // else the smaller unit scaled to the gcd of the magnitudes (ft + m and m + ft → [1/381 ft]);
    // of two spellings of one size, the shorter (J + N m → J). The sum's type is the same either way.
    // A magnitude may carry a sign (e₃₁ := e₃e₁ is -1 of the canonical e₁e₃): sizes compare without it.
    consteval unit_value common_unit(const unit_value& a, const unit_value& b)
    {
      if (a == b)
        return a;
      const magnitude   ma{magnitude_of(a)}, mb{magnitude_of(b)};
      const grid_number sa{abs(ma.Ratio)}, sb{abs(mb.Ratio)};
      if (ma.Pi == mb.Pi && sa == sb) // one size, two spellings (J, N m): the shorter, then by symbol
        return unit_before(b, a) ? b : a;
      if (ma.Pi != mb.Pi)
        ins::detail::constexpr_error<
          "units: no common unit of a pi multiple and a rational one - convert one side with value_cast first">();
      const grid_number g{gcd(sa, sb)};
      if (g == sa)
        return a;
      if (g == sb)
        return b;
      const bool a_smaller{sa < sb};
      unit_value u{a_smaller ? a : b};
      u.Scale = u.Scale * magnitude{g / (a_smaller ? sa : sb)};
      return u;
    }

    // Whether rescale<M>(v) forms: inside's v · M exists. A pi factor passes, for rescale's static_assert to explain.
    template <magnitude M, class V>
    concept rescale_forms = !exact_factor_v<M> || M.is_one() || requires(const V& v) { v * just<M.Ratio>; };

    // Whether a number of V in unit From rescales to unit To.
    template <unit_value From, unit_value To, class V>
    concept rescalable = rescale_forms<conversion_factor<From, To>, V>;

    // Whether inside's x Op y exists (Op one of + - % * / = <, the last two for == and <=>).
    template <class X, class Y, char Op>
    concept number_op = (Op == '+' && requires(const X& x, const Y& y) { x + y; })
                     || (Op == '-' && requires(const X& x, const Y& y) { x - y; })
                     || (Op == '%' && requires(const X& x, const Y& y) { x % y; })
                     || (Op == '*' && requires(const X& x, const Y& y) { x * y; })
                     || (Op == '/' && requires(const X& x, const Y& y) { x / y; })
                     || (Op == '=' && requires(const X& x, const Y& y) { x == y; })
                     || (Op == '<' && requires(const X& x, const Y& y) { x <=> y; });

    // Why q.in(To) cannot convert from From, if it cannot: another dimension, or a factor of pi (which value_cast
    // rounds). A deleted overload per reason.
    enum class in_error : unsigned char { none, dimension, pi };
    template <unit_value From, unit_value To>
    consteval in_error in_error_of()
    {
      if (dimension_of(From) != dimension_of(To))
        return in_error::dimension;
      if (magnitude_of(From).Pi != magnitude_of(To).Pi)
        return in_error::pi;
      return in_error::none;
    }
  } // namespace detail
} // namespace ins

#endif // INSIDE_UNITS_DETAIL_ALGEBRA_HPP
