// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The compile-time algebra of units.hpp: dimensions, magnitudes, quantity
// specs, point origins, units, named units, references and the exact unit
// conversion of a number. Internal — include <beman/inside/units.hpp> or
// <beman/inside/units/core.hpp>. Defines BEMAN_INSIDE_UNITS_WHY_PI, which
// units/core.hpp undefines.
#ifndef BEMAN_INSIDE_UNITS_DETAIL_ALGEBRA_HPP
#define BEMAN_INSIDE_UNITS_DETAIL_ALGEBRA_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/predicates.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

// The diagnostic of a π conversion asked to be exact (rescale, in()).
#define BEMAN_INSIDE_UNITS_WHY_PI                                                                  \
    "units: this conversion multiplies by a power of pi, which has no exact grid - round it with " \
    "value_cast<unit, Rep>(q) from <beman/inside/units/math.hpp>, Rep declared with a rounding mode"

namespace beman::inside::units {
using beman::inside::detail::grid_rational;

//---------------------------------------------------------------------------
// dimension — the exponents of the seven SI base dimensions
// (L M T I Θ N J), and of up to four base dimensions you define
// (base_dimension: currency, information). Angle is dimensionless, as in SI.
//---------------------------------------------------------------------------
struct base_dimension;

namespace detail {
inline constexpr std::size_t base_dimensions     = 7;
inline constexpr std::size_t max_user_dimensions = 4;

// A user base dimension to a power. By content (its name), as specs and units.
struct user_exponent {
    const base_dimension* D{};
    std::int8_t           Exp{};
};
} // namespace detail

// A base dimension of your own: inline constexpr base_dimension currency{"currency"}.
struct base_dimension {
    std::string_view Name;
};

struct dimension {
    std::int8_t           Exp[detail::base_dimensions]{};
    detail::user_exponent User[detail::max_user_dimensions]{}; // sorted by name, no zero exponents
    std::int8_t           UserSize{0};

    // An exponent past ±127 is an error, not a wrap.
    static constexpr std::int8_t exponent(int e) {
        if (e < -127 || e > 127)
            beman::inside::detail::constexpr_error<"units: a dimension exponent past 127">();
        return static_cast<std::int8_t>(e);
    }

    friend constexpr bool operator==(const dimension& a, const dimension& b) {
        return std::ranges::equal(a.Exp, b.Exp) && a.UserSize == b.UserSize &&
               std::equal(a.User, a.User + a.UserSize, b.User, [](const auto& x, const auto& y) {
                   return x.D->Name == y.D->Name && x.Exp == y.Exp;
               });
    }
    friend consteval dimension operator*(dimension a, const dimension& b) {
        for (std::size_t i = 0; i < detail::base_dimensions; ++i)
            a.Exp[i] = exponent(a.Exp[i] + b.Exp[i]);
        for (std::int8_t i = 0; i < b.UserSize; ++i)
            a.add_user(b.User[i].D, b.User[i].Exp);
        return a;
    }
    friend consteval dimension operator/(const dimension& a, const dimension& b) { return a * b.pow(-1); }
    consteval dimension        pow(int n) const {
        if (n == 0)
            return dimension{};
        dimension r = *this;
        for (auto& e : r.Exp)
            e = exponent(e * n);
        for (std::int8_t i = 0; i < r.UserSize; ++i)
            r.User[i].Exp = exponent(r.User[i].Exp * n);
        return r;
    }
    // The n-th root; every exponent must divide.
    consteval dimension root(int n) const {
        dimension  r    = *this;
        const auto take = [n](std::int8_t& e) {
            if (e % n != 0)
                beman::inside::detail::constexpr_error<"units: the root of a dimension needs exponents it divides">();
            e = static_cast<std::int8_t>(e / n);
        };
        for (auto& e : r.Exp)
            take(e);
        for (std::int8_t i = 0; i < r.UserSize; ++i)
            take(r.User[i].Exp);
        return r;
    }

    // b^e merged in: kept sorted by name, an exponent reaching 0 drops out.
    constexpr void add_user(const base_dimension* b, int e);
};

namespace detail {
// x^e merged into the sorted items[0, size): an item `same` as x takes the
// summed exponent and drops out at 0; otherwise x goes in at its place by
// `before`. False when items is full.
template <class T, std::size_t N, class Same, class Before>
constexpr bool sorted_add(T (&items)[N], std::int8_t& size, T x, int e, Same same, Before before) {
    T* const end = items + size;
    if (T* const it = std::find_if(items, end, [&](const T& y) { return same(y, x); }); it != end) {
        it->Exp = dimension::exponent(it->Exp + e);
        if (it->Exp == 0) {
            std::shift_left(it, end, 1);
            items[--size] = {};
        }
        return true;
    }
    if (e == 0)
        return true;
    if (size == static_cast<std::int8_t>(N))
        return false;
    x.Exp          = dimension::exponent(e);
    std::int8_t at = size;
    while (at > 0 && before(x, items[at - 1])) {
        items[at] = items[at - 1];
        --at;
    }
    items[at] = x;
    ++size;
    return true;
}
} // namespace detail

constexpr void dimension::add_user(const base_dimension* b, int e) {
    auto name = [](const detail::user_exponent& u) { return u.D->Name; };
    if (!detail::sorted_add(
            User,
            UserSize,
            detail::user_exponent{b, 0},
            e,
            [&](const auto& a, const auto& c) { return name(a) == name(c); },
            [&](const auto& a, const auto& c) { return name(a) < name(c); }))
        beman::inside::detail::constexpr_error<"units: a dimension holds at most 4 base dimensions of your own">();
}

// The dimension of a base dimension of your own (its exponent 1).
constexpr dimension dimension_of(const base_dimension& b) {
    dimension d{};
    d.add_user(&b, 1);
    return d;
}

inline constexpr dimension dim_one{};
inline constexpr dimension dim_length{{1, 0, 0, 0, 0, 0, 0}};
inline constexpr dimension dim_mass{{0, 1, 0, 0, 0, 0, 0}};
inline constexpr dimension dim_time{{0, 0, 1, 0, 0, 0, 0}};
inline constexpr dimension dim_electric_current{{0, 0, 0, 1, 0, 0, 0}};
inline constexpr dimension dim_thermodynamic_temperature{{0, 0, 0, 0, 1, 0, 0}};
inline constexpr dimension dim_amount_of_substance{{0, 0, 0, 0, 0, 1, 0}};
inline constexpr dimension dim_luminous_intensity{{0, 0, 0, 0, 0, 0, 1}};

//---------------------------------------------------------------------------
// magnitude — Ratio·π^Pi, exact.
//---------------------------------------------------------------------------
struct magnitude {
    grid_rational Ratio{1};
    std::int8_t   Pi{0};

    friend constexpr bool      operator==(const magnitude&, const magnitude&) = default;
    friend consteval magnitude operator*(const magnitude& a, const magnitude& b) {
        return {a.Ratio * b.Ratio, static_cast<std::int8_t>(a.Pi + b.Pi)};
    }
    friend consteval magnitude operator/(const magnitude& a, const magnitude& b) {
        return {a.Ratio / b.Ratio, static_cast<std::int8_t>(a.Pi - b.Pi)};
    }
    consteval magnitude pow(int n) const {
        magnitude r{};
        for (int i = 0; i < (n < 0 ? -n : n); ++i)
            r = r * *this;
        return n < 0 ? magnitude{} / r : r;
    }
    constexpr bool is_one() const { return Ratio == grid_rational{1} && Pi == 0; }
};

// mag<r> — a magnitude tag: mag<1000> * si::metre, mag<just<1, 3>> * …
template <magnitude M>
struct mag_t {
    static constexpr magnitude value = M;
};
template <auto R>
inline constexpr mag_t<magnitude{grid_rational{R}}>    mag{};
inline constexpr mag_t<magnitude{grid_rational{1}, 1}> mag_pi{};

template <magnitude A, magnitude B>
consteval auto operator*(mag_t<A>, mag_t<B>) {
    return mag_t<A * B>{};
}
template <magnitude A, magnitude B>
consteval auto operator/(mag_t<A>, mag_t<B>) {
    return mag_t<A / B>{};
}

//---------------------------------------------------------------------------
// quantity_spec — a quantity kind (length, height, speed, energy, torque…).
// Named specs are static objects; the tags users write (isq::height) carry a
// pointer to one. A spec has a parent (height → altitude → length) or a
// recipe (speed = length / time), or both (velocity: a speed whose recipe is
// displacement / duration). IsKind starts a separate kind under its parent:
// it never converts implicitly across that boundary (angular_measure vs a
// dimensionless ratio).
//---------------------------------------------------------------------------
struct quantity_spec;

// An optional reference to a static object (a parent spec, a unit's kind, an
// origin). It is tested by its flag, never against nullptr, and compared by
// content, never by address: under GCC's -fsanitize=undefined `&obj ==
// nullptr`, and under -fsanitize=pointer-compare `&a == &b`, is no constant
// expression.
namespace detail {
template <class T>
struct ref_or_none {
    const T* P{};
    bool     Set{false};

    constexpr ref_or_none() = default;
    constexpr ref_or_none(std::nullptr_t) {}
    constexpr ref_or_none(const T* p) : P{p}, Set{true} {} // p is not null
    constexpr explicit operator bool() const { return Set; }
    constexpr const T* operator->() const { return P; }
};

using spec_ref = ref_or_none<quantity_spec>;

struct spec_factor {
    const quantity_spec*  Spec{};
    std::int8_t           Exp{};
    friend constexpr bool operator==(const spec_factor& a, const spec_factor& b);
};

inline constexpr std::size_t max_spec_factors = 6;

// A product of named specs to integer powers, sorted by name.
struct recipe {
    spec_factor           F[max_spec_factors]{};
    std::int8_t           Size{0};
    friend constexpr bool operator==(const recipe& a, const recipe& b) {
        return a.Size == b.Size && std::equal(a.F, a.F + a.Size, b.F);
    }
};

// An anonymous spec expression: what length / time is, before it is named.
struct spec_expr {
    recipe    Recipe{};
    dimension Dim{};
};
} // namespace detail

struct is_kind_t {
    explicit is_kind_t() = default;
};
inline constexpr is_kind_t is_kind{};

template <const quantity_spec* S>
struct spec_t;

struct quantity_spec {
    std::string_view Name;
    dimension        Dim{};
    detail::spec_ref Parent{};
    detail::recipe   Recipe{};
    bool             IsKind{false};

    // A base quantity: a dimension of its own.
    constexpr quantity_spec(std::string_view name, dimension d) : Name{name}, Dim{d} {}
    // A base quantity of a base dimension of your own (money: currency).
    constexpr quantity_spec(std::string_view name, const base_dimension& b) : Name{name}, Dim{dimension_of(b)} {}
    // A child of a named spec.
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>) : Name{name}, Dim{P->Dim}, Parent{P} {}
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, is_kind_t)
        : Name{name}, Dim{P->Dim}, Parent{P}, IsKind{true} {}
    // A derived root: named by its recipe.
    constexpr quantity_spec(std::string_view name, const detail::spec_expr& e)
        : Name{name}, Dim{e.Dim}, Recipe{e.Recipe} {}
    constexpr quantity_spec(std::string_view name, const detail::spec_expr& e, is_kind_t)
        : Name{name}, Dim{e.Dim}, Recipe{e.Recipe}, IsKind{true} {}
    // A child with a recipe of its own (velocity: speed, displacement / duration).
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, const detail::spec_expr& e)
        : Name{name}, Dim{P->Dim}, Parent{P}, Recipe{e.Recipe} {
        if (!(e.Dim == P->Dim))
            beman::inside::detail::constexpr_error<"units: a spec's recipe must have its parent's dimension">();
    }
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, const detail::spec_expr& e, is_kind_t)
        : quantity_spec{name, spec_t<P>{}, e} {
        IsKind = true;
    }
};

namespace detail {
// Identity by content (see ref_or_none). A spec is its name, dimension, recipe, kind boundary and parent chain: a
// user spec that reuses a name ("length") under another parent is another
// spec.
constexpr bool same(const quantity_spec* a, const quantity_spec* b) { // both set
    if (a->Name != b->Name || !(a->Dim == b->Dim) || a->IsKind != b->IsKind || !(a->Recipe == b->Recipe))
        return false;
    if (!a->Parent || !b->Parent)
        return !a->Parent && !b->Parent;
    return same(a->Parent.P, b->Parent.P);
}
constexpr bool same(const spec_ref& a, const spec_ref& b) {
    if (!a || !b)
        return !a && !b;
    return same(a.P, b.P);
}

constexpr bool operator==(const spec_factor& a, const spec_factor& b) {
    return same(a.Spec, b.Spec) && a.Exp == b.Exp;
}

// The spec's kind root: the first IsKind spec on the way up, else the root.
consteval const quantity_spec* kind_root(const quantity_spec* s) {
    while (!s->IsKind && s->Parent)
        s = s->Parent.P;
    return s;
}
// a is s or an ancestor of s within s's kind (not above an IsKind boundary).
consteval bool is_within_kind_ancestor(const quantity_spec* s, const quantity_spec* a) {
    for (;;) {
        if (same(s, a))
            return true;
        if (s->IsKind || !s->Parent)
            return false;
        s = s->Parent.P;
    }
}

// r · f^e, merged and kept sorted by name; an exponent that reaches 0 drops
// out. Returns false when the recipe is full.
consteval bool recipe_add(recipe& r, const quantity_spec* f, int e) {
    return sorted_add(
        r.F,
        r.Size,
        spec_factor{f, 0},
        e,
        [](const spec_factor& a, const spec_factor& b) { return same(a.Spec, b.Spec); },
        [](const spec_factor& a, const spec_factor& b) { return a.Spec->Name < b.Spec->Name; });
}

// a · b^sign; false when the product does not fit a recipe.
consteval bool recipe_merge(recipe& a, const recipe& b, int sign) {
    for (std::int8_t i = 0; i < b.Size; ++i)
        if (!recipe_add(a, b.F[i].Spec, b.F[i].Exp * sign))
            return false;
    return true;
}

consteval recipe recipe_of(const quantity_spec* s) {
    if (s->Recipe.Size > 0)
        return s->Recipe;
    recipe r{};
    recipe_add(r, s, 1);
    return r;
}

// The recipe with every factor replaced by its kind root, summed.
consteval recipe recipe_by_kind(const recipe& r) {
    recipe out{};
    for (std::int8_t i = 0; i < r.Size; ++i)
        recipe_add(out, kind_root(r.F[i].Spec), r.F[i].Exp);
    return out;
}

// Each factor of `want` matched by a factor of `have` with the same exponent
// that is it or descends from it, one to one.
consteval bool recipe_matches(const recipe& have, const recipe& want) {
    if (have.Size != want.Size)
        return false;
    bool used[max_spec_factors]{};
    for (std::int8_t i = 0; i < want.Size; ++i) {
        bool found = false;
        for (std::int8_t j = 0; j < have.Size && !found; ++j)
            if (!used[j] && have.F[j].Exp == want.F[i].Exp &&
                is_within_kind_ancestor(have.F[j].Spec, want.F[i].Spec)) {
                used[j] = true;
                found   = true;
            }
        if (!found)
            return false;
    }
    return true;
}
} // namespace detail

template <const quantity_spec* S>
struct spec_t {
    static constexpr const quantity_spec* value = S;
    constexpr operator detail::spec_ref() const { return S; } // unit_options{.kind = isq::frequency}
    // isq::height[si::metre] — defined after unit_t and reference_t.
    template <class U>
    consteval auto operator[](U) const;
};

// Spec algebra for recipes: length / time, pow<2>(length), inverse(radius).
template <const quantity_spec* S>
consteval detail::spec_expr expr_of(spec_t<S>) {
    detail::spec_expr e{};
    detail::recipe_add(e.Recipe, S, 1);
    e.Dim = S->Dim;
    return e;
}
consteval detail::spec_expr expr_of(const detail::spec_expr& e) { return e; }

template <class T>
concept spec_operand = requires(T t) { expr_of(t); };

namespace detail {
consteval spec_expr spec_combine(const spec_expr& a, const spec_expr& b, int sign) {
    spec_expr r = a;
    if (!recipe_merge(r.Recipe, b.Recipe, sign))
        beman::inside::detail::constexpr_error<"units: a recipe holds at most 6 factors">();
    r.Dim = sign > 0 ? a.Dim * b.Dim : a.Dim / b.Dim;
    return r;
}
} // namespace detail

template <spec_operand A, spec_operand B>
consteval detail::spec_expr operator*(A a, B b) {
    return detail::spec_combine(expr_of(a), expr_of(b), 1);
}
template <spec_operand A, spec_operand B>
consteval detail::spec_expr operator/(A a, B b) {
    return detail::spec_combine(expr_of(a), expr_of(b), -1);
}
// Two specs are equal when their expressions are: one named spec, or one
// recipe of named specs (isq::length / isq::duration == isq::length / isq::time).
template <spec_operand A, spec_operand B>
consteval bool operator==(A a, B b) {
    const detail::spec_expr x = expr_of(a), y = expr_of(b);
    return x.Recipe == y.Recipe && x.Dim == y.Dim;
}

template <int N, spec_operand A>
consteval detail::spec_expr pow(A a) {
    detail::spec_expr r{};
    for (int i = 0; i < (N < 0 ? -N : N); ++i)
        r = detail::spec_combine(r, expr_of(a), N < 0 ? -1 : 1);
    return r;
}
template <spec_operand A>
consteval detail::spec_expr inverse(A a) {
    return detail::spec_combine(detail::spec_expr{}, expr_of(a), -1);
}

struct named_unit;

//---------------------------------------------------------------------------
// point_origin — where a quantity_point's number counts from. An absolute
// origin (absolute zero) or one at an exact offset from one, in the coherent
// SI unit of the dimension (the ice point: absolute zero + 273.15 K).
//---------------------------------------------------------------------------
struct point_origin {
    std::string_view                  Name;
    dimension                         Dim{};
    detail::ref_or_none<point_origin> Absolute{}; // none for an absolute origin
    grid_rational                     Offset{0};  // from Absolute, in the coherent unit
};

namespace detail {
using origin_ref = ref_or_none<point_origin>; // none: the implicit zero of a dimension
} // namespace detail

template <detail::origin_ref O>
struct origin_t {
    static constexpr detail::origin_ref value = O;
    constexpr operator detail::origin_ref() const { return O; } // unit_options{.origin = si::ice_point}
};

namespace detail {
consteval origin_ref absolute_of(origin_ref o) { return o && o->Absolute ? o->Absolute : o; }
// By content, as same() for specs and units (see there).
constexpr bool same(const origin_ref& a, const origin_ref& b) {
    if (!a || !b)
        return !a && !b;
    return a->Name == b->Name && a->Dim == b->Dim && a->Offset == b->Offset && same(a->Absolute, b->Absolute);
}
consteval grid_rational offset_of(origin_ref o) { return o && o->Absolute ? o->Offset : grid_rational{0}; }
} // namespace detail

// A named unit's options, all optional (designated, in this order):
//   .ascii         the ASCII symbol ("ohm" for Ω), else the symbol itself
//   .kind          the quantity kind it measures, else its definition's
//   .origin        the default origin of its points (℃: the ice point)
//   .space_before  false for a symbol written against the number (90°)
struct unit_options {
    std::string_view   ascii{};
    detail::spec_ref   kind{};
    detail::origin_ref origin{};
    bool               space_before{true};
};

namespace detail {
struct unit_atom {
    const named_unit*     U{};
    std::int8_t           Prefix{0}; // power of ten
    std::int8_t           Exp{0};
    friend constexpr bool operator==(const unit_atom& a, const unit_atom& b);
};

inline constexpr std::size_t max_unit_atoms = 6;
} // namespace detail

//---------------------------------------------------------------------------
// unit — a product of named units to integer powers, each with an optional
// decimal prefix (k, m, µ…), times a scale (mag<1000> * m; the common unit of
// a sum). A structural value: it is the template argument.
//---------------------------------------------------------------------------
struct unit {
    detail::unit_atom Atoms[detail::max_unit_atoms]{};
    std::int8_t       Size{0};
    magnitude         Scale{};

    constexpr unit() = default;
    constexpr unit(const named_unit& u) : Atoms{{&u, 0, 1}}, Size{1} {}

    friend constexpr bool operator==(const unit& a, const unit& b) {
        return a.Size == b.Size && a.Scale == b.Scale && std::equal(a.Atoms, a.Atoms + a.Size, b.Atoms);
    }
};

template <unit U>
struct unit_t {
    static constexpr unit value = U;
};

//---------------------------------------------------------------------------
// named_unit — a unit with a symbol (m, s, N, Ω, °C): a static object. Its
// magnitude is relative to the coherent SI unit of its dimension; Kind (if
// any) is the quantity kind it measures (Hz: frequency, Bq: activity).
//---------------------------------------------------------------------------
struct named_unit {
    std::string_view   Utf8;
    std::string_view   Ascii;
    dimension          Dim{};
    magnitude          Mag{};
    detail::spec_ref   Kind{};
    bool               SpaceBefore{true};
    detail::origin_ref Origin{}; // the default origin of its points (K: absolute zero)

    // A base unit of a kind: the coherent unit of its dimension, or a scale
    // of it (g: 1/1000 of the coherent kg).
    template <const quantity_spec* K>
    constexpr named_unit(std::string_view sym, spec_t<K>, unit_options o = {})
        : named_unit{sym, mag_t<magnitude{}>{}, spec_t<K>{}, o} {}
    template <magnitude M, const quantity_spec* K>
    constexpr named_unit(std::string_view sym, mag_t<M>, spec_t<K>, unit_options o = {})
        : Utf8{sym},
          Ascii{o.ascii.empty() ? sym : o.ascii},
          Dim{K->Dim},
          Mag{M},
          Kind{K},
          SpaceBefore{o.space_before},
          Origin{o.origin} {}
    // A unit defined by others: named_unit{"N", kg * m / s², isq::force}. Its
    // kind is the given one, else .kind, else its one factor's (min: 60 s).
    template <unit U>
    constexpr named_unit(std::string_view sym, unit_t<U>, unit_options o = {});
    template <unit U, const quantity_spec* K>
    constexpr named_unit(std::string_view sym, unit_t<U>, spec_t<K>, unit_options o = {});
};

namespace detail {
// A unit is its symbols, dimension, magnitude, kind and origin.
constexpr bool same(const named_unit* a, const named_unit* b) { // both set
    return a->Utf8 == b->Utf8 && a->Ascii == b->Ascii && a->Dim == b->Dim && a->Mag == b->Mag &&
           same(a->Kind, b->Kind) && same(a->Origin, b->Origin);
}

constexpr bool operator==(const unit_atom& a, const unit_atom& b) {
    return same(a.U, b.U) && a.Prefix == b.Prefix && a.Exp == b.Exp;
}

constexpr bool atom_before(const unit_atom& a, const unit_atom& b) {
    if (a.U->Utf8 != b.U->Utf8)
        return a.U->Utf8 < b.U->Utf8;
    return a.Prefix < b.Prefix;
}

consteval void unit_add(unit& u, unit_atom a) {
    if (!sorted_add(
            u.Atoms,
            u.Size,
            a,
            a.Exp,
            [](const unit_atom& x, const unit_atom& y) { return same(x.U, y.U) && x.Prefix == y.Prefix; },
            atom_before))
        beman::inside::detail::constexpr_error<"units: a unit holds at most 6 factors - name it with named_unit">();
}

consteval unit unit_mul(unit a, const unit& b, int sign) {
    for (std::int8_t i = 0; i < b.Size; ++i) {
        unit_atom t = b.Atoms[i];
        t.Exp       = static_cast<std::int8_t>(t.Exp * sign);
        unit_add(a, t);
    }
    a.Scale = sign > 0 ? a.Scale * b.Scale : a.Scale / b.Scale;
    return a;
}

consteval unit unit_pow(const unit& u, int n) {
    unit r{};
    for (std::int8_t i = 0; i < u.Size; ++i)
        unit_add(r, {u.Atoms[i].U, u.Atoms[i].Prefix, dimension::exponent(u.Atoms[i].Exp * n)});
    r.Scale = u.Scale.pow(n);
    return r;
}

consteval unit unit_scaled(unit u, const magnitude& m) {
    u.Scale = m * u.Scale;
    return u;
}
consteval unit unit_prefixed(unit u, int p) {
    u.Atoms[0].Prefix = static_cast<std::int8_t>(p);
    return u;
}

consteval magnitude ten_pow(int p) {
    magnitude ten{grid_rational{10}};
    return ten.pow(p);
}
} // namespace detail

// A unit's dimension, magnitude (relative to the coherent SI unit) and kind.
consteval dimension dimension_of(const unit& u) {
    dimension d{};
    for (std::int8_t i = 0; i < u.Size; ++i)
        d = d * u.Atoms[i].U->Dim.pow(u.Atoms[i].Exp);
    return d;
}
consteval magnitude magnitude_of(const unit& u) {
    magnitude m = u.Scale;
    for (std::int8_t i = 0; i < u.Size; ++i)
        m = m * (u.Atoms[i].U->Mag * detail::ten_pow(u.Atoms[i].Prefix)).pow(u.Atoms[i].Exp);
    return m;
}
namespace detail {
// One named unit (with a prefix too) to the power 1, unscaled: its kind,
// origin and spacing are the unit's.
consteval bool is_single_named(const unit& u) { return u.Size == 1 && u.Atoms[0].Exp == 1 && u.Scale.is_one(); }
// Whether a quantity's text puts a space before the unit (not before °).
consteval bool space_before(const unit& u) { return !(is_single_named(u) && !u.Atoms[0].U->SpaceBefore); }
} // namespace detail

// The kind a unit measures: a named unit's (with a prefix too), else none.
consteval detail::spec_ref kind_of(const unit& u) {
    return detail::is_single_named(u) ? u.Atoms[0].U->Kind : detail::spec_ref{};
}

namespace detail {
// The kind a unit defined by U inherits: its one factor's (min: 60 s).
consteval spec_ref inherited_kind(const unit& u) {
    return u.Size == 1 && u.Atoms[0].Exp == 1 ? u.Atoms[0].U->Kind : spec_ref{};
}
} // namespace detail

template <unit U>
constexpr named_unit::named_unit(std::string_view sym, unit_t<U>, unit_options o)
    : Utf8{sym},
      Ascii{o.ascii.empty() ? sym : o.ascii},
      Dim{dimension_of(U)},
      Mag{magnitude_of(U)},
      Kind{o.kind ? o.kind : detail::inherited_kind(U)},
      SpaceBefore{o.space_before},
      Origin{o.origin} {
    if (Kind && !(Kind->Dim == Dim))
        beman::inside::detail::constexpr_error<"units: a unit's kind must have the unit's dimension">();
    if (Origin && !(Origin->Dim == Dim))
        beman::inside::detail::constexpr_error<"units: a unit's origin must have the unit's dimension">();
}
template <unit U, const quantity_spec* K>
constexpr named_unit::named_unit(std::string_view sym, unit_t<U>, spec_t<K>, unit_options o)
    : named_unit{sym, unit_t<U>{}, unit_options{o.ascii, K, o.origin, o.space_before}} {}

// The tags of your own entries, each made from its static object:
//   inline constexpr auto metre  = unit_c<metre_def>;     // a named_unit
//   inline constexpr auto height = spec_c<height_def>;    // a quantity_spec
//   inline constexpr auto origin = origin_c<origin_def>;  // a point_origin
template <const named_unit& N>
inline constexpr unit_t<unit{N}> unit_c{};
template <const quantity_spec& S>
inline constexpr spec_t<&S> spec_c{};
template <const point_origin& O>
inline constexpr origin_t<&O> origin_c{};

// The dimensionless unit 1.
inline constexpr unit_t<unit{}> one{};

template <unit A, unit B>
consteval bool operator==(unit_t<A>, unit_t<B>) {
    return A == B;
}
template <unit A, unit B>
consteval auto operator*(unit_t<A>, unit_t<B>) {
    return unit_t<detail::unit_mul(A, B, 1)>{};
}
template <unit A, unit B>
consteval auto operator/(unit_t<A>, unit_t<B>) {
    return unit_t<detail::unit_mul(A, B, -1)>{};
}
template <magnitude M, unit B>
consteval auto operator*(mag_t<M>, unit_t<B>) {
    return unit_t<detail::unit_scaled(B, M)>{};
}
template <magnitude M, unit B>
consteval auto operator/(mag_t<M>, unit_t<B>) {
    return unit_t<detail::unit_scaled(detail::unit_pow(B, -1), M)>{};
}
template <unit B, magnitude M>
consteval auto operator*(unit_t<B>, mag_t<M>) {
    return unit_t<detail::unit_scaled(B, M)>{};
}
template <unit B, magnitude M>
consteval auto operator/(unit_t<B>, mag_t<M>) {
    return unit_t<detail::unit_scaled(B, magnitude{} / M)>{};
}
template <int N, unit A>
consteval auto pow(unit_t<A>) {
    return unit_t<detail::unit_pow(A, N)>{};
}
template <unit A>
consteval auto square(unit_t<A>) {
    return unit_t<detail::unit_pow(A, 2)>{};
}
template <unit A>
consteval auto cubic(unit_t<A>) {
    return unit_t<detail::unit_pow(A, 3)>{};
}
template <unit A>
consteval auto inverse(unit_t<A>) {
    return unit_t<detail::unit_pow(A, -1)>{};
}

// A decimal prefix: si::kilo(si::metre). Only on a named unit, once.
template <int P>
struct prefix_t {
    template <unit U>
    consteval auto operator()(unit_t<U>) const {
        static_assert(U.Size == 1 && U.Atoms[0].Exp == 1 && U.Atoms[0].Prefix == 0 && U.Scale.is_one(),
                      "units: a prefix applies to a named unit without a prefix (kilo(metre), not kilo(m/s))");
        return unit_t<detail::unit_prefixed(U, P)>{};
    }
};

//---------------------------------------------------------------------------
// reference — what a quantity's number means: a unit plus a quantity spec.
//   Spec named, KindOf false   isq::height[m]: exactly that spec
//   Spec named, KindOf true    a unit's kind (si::hertz): any spec of its kind
//   Spec null, Derived set     a product of specs (width * height)
//   Spec null, Derived empty   any quantity of the unit's dimension (m/s)
//---------------------------------------------------------------------------
struct reference {
    detail::spec_ref Spec{};
    bool             KindOf{false};
    detail::recipe   Derived{};
    unit             U{};

    constexpr reference() = default;
    constexpr reference(detail::spec_ref s, bool kind_of, detail::recipe derived, unit u)
        : Spec{s}, KindOf{kind_of}, Derived{derived}, U{u} {}
    // A unit tag as a reference: quantity<si::metre, …>.
    template <unit V>
    constexpr reference(unit_t<V>) : Spec{kind_of(V)}, KindOf{static_cast<bool>(kind_of(V))}, U{V} {}

    friend constexpr bool operator==(const reference& a, const reference& b) {
        return detail::same(a.Spec, b.Spec) && a.KindOf == b.KindOf && a.Derived == b.Derived && a.U == b.U;
    }
};

consteval reference reference_of(const unit& u) {
    const detail::spec_ref k = kind_of(u);
    return reference{k, static_cast<bool>(k), detail::recipe{}, u};
}

template <reference R>
struct reference_t {
    static constexpr reference value = R;
    constexpr                  operator reference() const { return R; }
};

template <class T>
inline constexpr bool is_unit_tag_v = false;
template <unit U>
inline constexpr bool is_unit_tag_v<unit_t<U>> = true;

template <class T>
inline constexpr bool is_reference_tag_v = false;
template <unit U>
inline constexpr bool is_reference_tag_v<unit_t<U>> = true;
template <reference R>
inline constexpr bool is_reference_tag_v<reference_t<R>> = true;

// The reference of a tag: a unit_t is the unit with its kind.
template <unit U>
consteval reference reference_of(unit_t<U>) {
    return reference_of(U);
}
template <reference R>
consteval reference reference_of(reference_t<R>) {
    return R;
}

template <const quantity_spec* S>
template <class U>
consteval auto spec_t<S>::operator[](U) const {
    static_assert(is_reference_tag_v<U> && requires { U::value.Size; }, "units: spec[unit] takes a unit");
    constexpr unit u = U::value;
    static_assert(dimension_of(u) == S->Dim, "units: spec[unit] - the unit has another dimension than the spec");
    constexpr detail::spec_ref k = kind_of(u);
    static_assert(!k || detail::same(detail::kind_root(k.P), detail::kind_root(S)),
                  "units: spec[unit] - the unit measures another kind of quantity (Hz for activity?)");
    return reference_t<reference{S, false, detail::recipe{}, u}>{};
}

namespace detail {
// Which spec facts a reference carries.
consteval bool is_generic(const reference& r) { return !r.Spec && r.Derived.Size == 0; }

// Whether a reference's quantity may convert implicitly to a named spec `to`.
consteval bool derived_converts_to(const reference& from, const quantity_spec* to) {
    if (to->IsKind && !from.KindOf)
        return false;
    // Any quantity of the dimension names a kind root: m/s → speed, N·m → energy.
    if (!to->Parent && !to->IsKind)
        return true;
    if (from.KindOf)
        return derived_converts_to(reference{nullptr, false, from.Derived, from.U}, kind_root(to)) ||
               recipe_matches(from.Derived, recipe_of(kind_root(to)));
    return to->Recipe.Size > 0 && recipe_matches(from.Derived, to->Recipe);
}

// Implicit conversion between the specs of two references of one dimension.
consteval bool spec_converts(const reference& from, const reference& to) {
    if (is_generic(to)) // a dimension's quantity takes any of its kinds but an IsKind one
        return !from.Spec || !kind_root(from.Spec.P)->IsKind;
    if (is_generic(from))
        return !to.Spec || !kind_root(to.Spec.P)->IsKind;
    if (!to.Spec) { // a derived target: the source's recipe, by kind
        const recipe have = from.Spec ? recipe_of(from.Spec.P) : from.Derived;
        return recipe_by_kind(have) == recipe_by_kind(to.Derived);
    }
    if (!from.Spec)
        return derived_converts_to(from, to.KindOf ? kind_root(to.Spec.P) : to.Spec.P);
    if (to.KindOf || from.KindOf) // a unit's kind takes and gives any spec of that kind
        return same(kind_root(from.Spec.P), kind_root(to.Spec.P));
    return is_within_kind_ancestor(from.Spec.P, to.Spec.P);
}

// Explicit conversion (quantity_cast): any spec of the same dimension, but
// not across IsKind kinds.
consteval bool spec_casts(const reference& from, const quantity_spec* to) {
    const spec_ref       fk = from.Spec ? spec_ref{kind_root(from.Spec.P)} : spec_ref{};
    const quantity_spec* tk = kind_root(to);
    if (same(fk, spec_ref{tk}))
        return true;
    if (fk && fk->IsKind)
        return false;
    return !tk->IsKind || !from.Spec;
}

// The spec part of a sum's reference; ok = false when the two cannot add.
struct spec_result {
    bool      Ok{false};
    reference R{};
};

consteval spec_result common_spec(const reference& a, const reference& b) {
    if (same(a.Spec, b.Spec) && a.KindOf == b.KindOf && a.Derived == b.Derived)
        return {true, a};
    if (spec_converts(a, b) && !spec_converts(b, a))
        return {true, b};
    if (spec_converts(b, a) && !spec_converts(a, b))
        return {true, a};
    if (a.Spec && b.Spec) {
        if (a.KindOf && b.KindOf && same(kind_root(a.Spec.P), kind_root(b.Spec.P)))
            return {true, reference{kind_root(a.Spec.P), true, {}, a.U}};
        if (a.KindOf != b.KindOf && same(kind_root(a.Spec.P), kind_root(b.Spec.P))) // kind_of + a named spec of it
            return {true, a.KindOf ? b : a};
        // The nearest common ancestor within one kind.
        for (const quantity_spec* s = a.Spec.P;; s = s->Parent.P) {
            if (is_within_kind_ancestor(b.Spec.P, s))
                return {true, reference{s, false, {}, a.U}};
            if (s->IsKind || !s->Parent)
                break;
        }
        return {};
    }
    if (spec_converts(a, b) && spec_converts(b, a)) // e.g. two recipes of one kind
        return {true, reference{nullptr, false, {}, a.U}};
    return {};
}

// The spec part of a product: named factors multiply into a recipe.
// (Spec null, KindOf, no recipe) is the empty product: 1 / x starts there.
consteval reference spec_product(const reference& a, const reference& b, int sign, const unit& u) {
    const auto empty = [](const reference& r) consteval { return !r.Spec && r.KindOf && r.Derived.Size == 0; };
    if ((is_generic(a) && !empty(a)) || (is_generic(b) && !empty(b)))
        return reference{nullptr, false, {}, u};
    const auto factors = [](const reference& r) consteval {
        if (!r.Spec)
            return r.Derived;
        recipe f{};
        recipe_add(f, r.KindOf ? kind_root(r.Spec.P) : r.Spec.P, 1);
        return f;
    };
    recipe f = factors(a);
    if (!recipe_merge(f, factors(b), sign) || f.Size == 0) // too many factors, or a pure ratio
        return reference{nullptr, false, {}, u};
    const bool kind_of = (a.KindOf || empty(a)) && (b.KindOf || empty(b));
    if (f.Size == 1 && f.F[0].Exp == 1)
        return reference{f.F[0].Spec, kind_of, {}, u};
    return reference{nullptr, kind_of, f, u};
}

consteval reference spec_pow(const reference& a, int n, const unit& u) {
    if (is_generic(a) || n == 0)
        return reference{nullptr, false, {}, u};
    reference r = a;
    r.U         = u;
    for (int i = 1; i < (n < 0 ? -n : n); ++i)
        r = spec_product(r, a, 1, u);
    return n < 0 ? spec_product(reference{nullptr, true, {}, u}, r, -1, u) : r;
}
} // namespace detail

//---------------------------------------------------------------------------
// Unit conversion of a number: the value in unit From as a value in unit To,
// value · mag(From) / mag(To). A rational factor rescales the grid (the raw is
// kept where the storage allows); a π factor has no exact rescale.
//---------------------------------------------------------------------------
namespace detail {
template <magnitude M>
inline constexpr bool exact_factor_v = M.Pi == 0;

template <magnitude M, insidable V>
constexpr auto rescale(const V& v) {
    static_assert(exact_factor_v<M>, BEMAN_INSIDE_UNITS_WHY_PI);
    if constexpr (M.is_one())
        return v;
    else {
        // The product's grid with v's policy: a conversion keeps clamp, wrap
        // and the rounding mode.
        const auto x = v * just<M.Ratio>;
        if constexpr (flags_of<decltype(x)> == flags_of<V>)
            return x;
        else
            return inside<grid_of<decltype(x)>, flags_of<V>>::from_raw(x.raw());
    }
}

template <unit From, unit To>
inline constexpr magnitude conversion_factor = magnitude_of(From) / magnitude_of(To);

// A fixed order of units: fewer atoms first, then by symbol and exponent.
consteval bool unit_before(const unit& a, const unit& b) {
    if (a.Size != b.Size)
        return a.Size < b.Size;
    return std::lexicographical_compare(
        a.Atoms, a.Atoms + a.Size, b.Atoms, b.Atoms + b.Size, [](const unit_atom& x, const unit_atom& y) {
            if (atom_before(x, y) || atom_before(y, x))
                return atom_before(x, y);
            return x.Exp < y.Exp;
        });
}

// The common unit of a sum: the smaller unit when it divides the other
// (km + m → m), else the smaller unit scaled to the gcd of the magnitudes
// (ft + m and m + ft → [1/381 ft]); of two spellings of one size, the
// shorter (J + N m → J). The sum's type is the same either way.
consteval unit common_unit(const unit& a, const unit& b) {
    if (a == b)
        return a;
    const magnitude ma = magnitude_of(a), mb = magnitude_of(b);
    if (ma == mb) // one size, two spellings (J, N m): the shorter, then by symbol
        return unit_before(b, a) ? b : a;
    if (ma.Pi != mb.Pi)
        beman::inside::detail::constexpr_error<
            "units: no common unit of a pi multiple and a rational one - convert one side with value_cast first">();
    const grid_rational g = beman::inside::detail::gcd(ma.Ratio, mb.Ratio);
    if (g == ma.Ratio)
        return a;
    if (g == mb.Ratio)
        return b;
    const bool a_smaller = ma.Ratio < mb.Ratio;
    unit       u         = a_smaller ? a : b;
    u.Scale              = u.Scale * magnitude{g / (a_smaller ? ma.Ratio : mb.Ratio)};
    return u;
}

// Whether rescale<M>(v) forms: inside's v · M exists. A pi factor passes, for
// rescale's static_assert to explain.
template <magnitude M, class V>
concept rescale_forms = !exact_factor_v<M> || M.is_one() || requires(const V& v) { v * just<M.Ratio>; };

// Whether a number of V in unit From rescales to unit To.
template <unit From, unit To, class V>
concept rescalable = rescale_forms<conversion_factor<From, To>, V>;

// Whether inside's x Op y exists (Op one of + - % * / = <, the last two for
// == and <=>).
template <class X, class Y, char Op>
concept number_op = (Op == '+' && requires(const X& x, const Y& y) { x + y; }) ||
                    (Op == '-' && requires(const X& x, const Y& y) { x - y; }) ||
                    (Op == '%' && requires(const X& x, const Y& y) { x % y; }) ||
                    (Op == '*' && requires(const X& x, const Y& y) { x * y; }) ||
                    (Op == '/' && requires(const X& x, const Y& y) { x / y; }) ||
                    (Op == '=' && requires(const X& x, const Y& y) { x == y; }) ||
                    (Op == '<' && requires(const X& x, const Y& y) { x <=> y; });

// Why q.in(To) cannot convert from From, if it cannot: another dimension, or
// a factor of pi (which value_cast rounds). A deleted overload per reason.
enum class in_error : unsigned char { none, dimension, pi };
template <unit From, unit To>
consteval in_error in_error_of() {
    if (dimension_of(From) != dimension_of(To))
        return in_error::dimension;
    if (magnitude_of(From).Pi != magnitude_of(To).Pi)
        return in_error::pi;
    return in_error::none;
}
} // namespace detail
} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_DETAIL_ALGEBRA_HPP
