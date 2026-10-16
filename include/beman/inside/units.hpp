// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Opt-in: physical units and quantity kinds on inside — the main features of
// mp-units with an inside as the only representation.
//
//   quantity<si::metre, inside<{0, 10}>> d{7};      // 7 m
//   auto k = just<5> * si::kilo(si::metre);         // 5 km, a point grid
//   auto s = k + d;                                 // 5007 m, exact
//   auto v = d.in(si::milli(si::metre));            // 7000 mm, the same raw
//   quantity<isq::height[si::metre], H> h{…};       // a quantity kind
//
// A unit conversion is a grid rescale: km → m multiplies the interval and
// notch by 1000 and keeps the raw, so it is exact, total and free. A sum of
// two units runs in their common unit (the gcd of the magnitudes), so it is
// exact too. Products and quotients take inside's result grids.
//
// Compile time: dimensions, magnitudes, units, quantity specs and references
// are structural values combined by consteval functions — no type lists, no
// type-level sorting. The names a user writes (si::metre, isq::height) are
// empty tag objects whose type carries the value.
//
// Magnitudes are exact rationals times a power of π. A π magnitude (degree ↔
// radian) has no exact rescale: it rounds once, through value_cast with a
// target representation and a rounding flag. Grid numbers are 64-bit under
// C++23; the prefixes and constants that need more (yocto, quetta, the eV)
// exist only under C++26 big grids (BEMAN_INSIDE_BIG_GRIDS).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_UNITS_HPP
#define BEMAN_INSIDE_UNITS_HPP

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace beman::inside::units {
using beman::inside::detail::grid_rational;

//---------------------------------------------------------------------------
// dimension — the exponents of the seven SI base dimensions
// (L M T I Θ N J). Angle is dimensionless, as in SI.
//---------------------------------------------------------------------------
inline constexpr std::size_t base_dimensions = 7;

struct dimension {
    std::int8_t Exp[base_dimensions]{};

    friend constexpr bool      operator==(const dimension&, const dimension&) = default;
    friend consteval dimension operator*(dimension a, const dimension& b) {
        for (std::size_t i = 0; i < base_dimensions; ++i)
            a.Exp[i] = static_cast<std::int8_t>(a.Exp[i] + b.Exp[i]);
        return a;
    }
    friend consteval dimension operator/(dimension a, const dimension& b) {
        for (std::size_t i = 0; i < base_dimensions; ++i)
            a.Exp[i] = static_cast<std::int8_t>(a.Exp[i] - b.Exp[i]);
        return a;
    }
    consteval dimension pow(int n) const {
        dimension r = *this;
        for (auto& e : r.Exp)
            e = static_cast<std::int8_t>(e * n);
        return r;
    }
    // The n-th root; every exponent must divide.
    consteval dimension root(int n) const {
        dimension r = *this;
        for (auto& e : r.Exp) {
            if (e % n != 0)
                beman::inside::detail::constexpr_error<"units: the root of a dimension needs exponents it divides">();
            e = static_cast<std::int8_t>(e / n);
        }
        return r;
    }
};

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
        return {beman::inside::detail::grid_mul(a.Ratio, b.Ratio), static_cast<std::int8_t>(a.Pi + b.Pi)};
    }
    friend consteval magnitude operator/(const magnitude& a, const magnitude& b) {
        return {beman::inside::detail::grid_div_of(a.Ratio, b.Ratio), static_cast<std::int8_t>(a.Pi - b.Pi)};
    }
    consteval magnitude pow(int n) const {
        magnitude r{};
        for (int i = 0; i < (n < 0 ? -n : n); ++i)
            r = r * *this;
        return n < 0 ? magnitude{} / r : r;
    }
    constexpr bool is_one() const { return Ratio == grid_rational{1} && Pi == 0; }
};

// mag<r> — a magnitude tag: mag<1000> * si::metre, mag<just<1>.per<3>> * …
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

struct spec_factor {
    const quantity_spec*  Spec{};
    std::int8_t           Exp{};
    friend constexpr bool operator==(const spec_factor&, const spec_factor&) = default;
};

inline constexpr std::size_t max_spec_factors = 6;

// A product of named specs to integer powers, sorted by name.
struct recipe {
    spec_factor           F[max_spec_factors]{};
    std::int8_t           Size{0};
    friend constexpr bool operator==(const recipe&, const recipe&) = default;
};

// An anonymous spec expression: what length / time is, before it is named.
struct spec_expr {
    recipe    Recipe{};
    dimension Dim{};
};

struct is_kind_t {
    explicit is_kind_t() = default;
};
inline constexpr is_kind_t is_kind{};

template <const quantity_spec* S>
struct spec_t;

struct quantity_spec {
    std::string_view     Name;
    dimension            Dim{};
    const quantity_spec* Parent{};
    recipe               Recipe{};
    bool                 IsKind{false};

    // A base quantity: a dimension of its own.
    constexpr quantity_spec(std::string_view name, dimension d) : Name{name}, Dim{d} {}
    // A child of a named spec.
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>) : Name{name}, Dim{P->Dim}, Parent{P} {}
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, is_kind_t)
        : Name{name}, Dim{P->Dim}, Parent{P}, IsKind{true} {}
    // A derived root: named by its recipe.
    constexpr quantity_spec(std::string_view name, const spec_expr& e) : Name{name}, Dim{e.Dim}, Recipe{e.Recipe} {}
    constexpr quantity_spec(std::string_view name, const spec_expr& e, is_kind_t)
        : Name{name}, Dim{e.Dim}, Recipe{e.Recipe}, IsKind{true} {}
    // A child with a recipe of its own (velocity: speed, displacement / duration).
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, const spec_expr& e)
        : Name{name}, Dim{P->Dim}, Parent{P}, Recipe{e.Recipe} {
        if (!(e.Dim == P->Dim))
            beman::inside::detail::constexpr_error<"units: a spec's recipe must have its parent's dimension">();
    }
    template <const quantity_spec* P>
    constexpr quantity_spec(std::string_view name, spec_t<P>, const spec_expr& e, is_kind_t)
        : quantity_spec{name, spec_t<P>{}, e} {
        IsKind = true;
    }
};

namespace detail {
// The spec's tree root, and its kind root: the first IsKind spec on the way
// up, else the tree root.
consteval const quantity_spec* tree_root(const quantity_spec* s) {
    while (s->Parent)
        s = s->Parent;
    return s;
}
consteval const quantity_spec* kind_root(const quantity_spec* s) {
    while (!s->IsKind && s->Parent)
        s = s->Parent;
    return s;
}
// a is s or an ancestor of s within s's kind (not above an IsKind boundary).
consteval bool is_within_kind_ancestor(const quantity_spec* s, const quantity_spec* a) {
    for (;;) {
        if (s == a)
            return true;
        if (s->IsKind || !s->Parent)
            return false;
        s = s->Parent;
    }
}

consteval bool name_before(const quantity_spec* a, const quantity_spec* b) { return a->Name < b->Name; }

// r · f^e, merged and kept sorted; an exponent that reaches 0 drops out.
// Returns false when the recipe is full.
consteval bool recipe_add(recipe& r, const quantity_spec* f, int e) {
    for (std::int8_t i = 0; i < r.Size; ++i)
        if (r.F[i].Spec == f) {
            r.F[i].Exp = static_cast<std::int8_t>(r.F[i].Exp + e);
            if (r.F[i].Exp == 0) {
                for (std::int8_t j = i; j + 1 < r.Size; ++j)
                    r.F[j] = r.F[j + 1];
                r.F[--r.Size] = {};
            }
            return true;
        }
    if (e == 0)
        return true;
    if (r.Size == static_cast<std::int8_t>(max_spec_factors))
        return false;
    std::int8_t at = r.Size;
    while (at > 0 && name_before(f, r.F[at - 1].Spec)) {
        r.F[at] = r.F[at - 1];
        --at;
    }
    r.F[at] = {f, static_cast<std::int8_t>(e)};
    ++r.Size;
    return true;
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
    // isq::height[si::metre] — defined after unit_t and reference_t.
    template <class U>
    consteval auto operator[](U) const;
};

// Spec algebra for recipes: length / time, pow<2>(length), inverse(radius).
template <const quantity_spec* S>
consteval spec_expr expr_of(spec_t<S>) {
    spec_expr e{};
    detail::recipe_add(e.Recipe, S, 1);
    e.Dim = S->Dim;
    return e;
}
consteval spec_expr expr_of(const spec_expr& e) { return e; }

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
consteval spec_expr operator*(A a, B b) {
    return detail::spec_combine(expr_of(a), expr_of(b), 1);
}
template <spec_operand A, spec_operand B>
consteval spec_expr operator/(A a, B b) {
    return detail::spec_combine(expr_of(a), expr_of(b), -1);
}
template <int N, spec_operand A>
consteval spec_expr pow(A a) {
    spec_expr r{};
    for (int i = 0; i < (N < 0 ? -N : N); ++i)
        r = detail::spec_combine(r, expr_of(a), N < 0 ? -1 : 1);
    return r;
}
template <spec_operand A>
consteval spec_expr inverse(A a) {
    return detail::spec_combine(spec_expr{}, expr_of(a), -1);
}

//---------------------------------------------------------------------------
// named_unit — a unit with a symbol (m, s, N, Ω, °C): a static object. Its
// magnitude is relative to the coherent SI unit of its dimension; Kind (if
// any) is the quantity kind it measures (Hz: frequency, Bq: activity).
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// unit — a product of named units to integer powers, each with an optional
// decimal prefix (k, m, µ…), times a scale (mag<1000> * m; the common unit of
// a sum). A structural value: it is the template argument.
//---------------------------------------------------------------------------
struct named_unit;

//---------------------------------------------------------------------------
// point_origin — where a quantity_point's number counts from. An absolute
// origin (absolute zero) or one at an exact offset from one, in the coherent
// SI unit of the dimension (the ice point: absolute zero + 273.15 K).
//---------------------------------------------------------------------------
struct point_origin {
    std::string_view    Name;
    dimension           Dim{};
    const point_origin* Absolute{}; // null for an absolute origin
    grid_rational       Offset{0};  // from Absolute, in the coherent unit
};

template <const point_origin* O>
struct origin_t {
    static constexpr const point_origin* value = O;
};

namespace detail {
consteval const point_origin* absolute_of(const point_origin* o) { return o && o->Absolute ? o->Absolute : o; }
consteval grid_rational offset_of(const point_origin* o) { return o && o->Absolute ? o->Offset : grid_rational{0}; }
} // namespace detail

struct no_space_before_t {
    explicit no_space_before_t() = default;
};
inline constexpr no_space_before_t no_space_before{};

struct unit_atom {
    const named_unit*     U{};
    std::int8_t           Prefix{0}; // power of ten
    std::int8_t           Exp{0};
    friend constexpr bool operator==(const unit_atom&, const unit_atom&) = default;
};

inline constexpr std::size_t max_unit_atoms = 6;

struct unit {
    unit_atom   Atoms[max_unit_atoms]{};
    std::int8_t Size{0};
    magnitude   Scale{};

    constexpr unit() = default;
    constexpr unit(const named_unit& u) : Atoms{{&u, 0, 1}}, Size{1} {}

    friend constexpr bool operator==(const unit&, const unit&) = default;
};

template <unit U>
struct unit_t {
    static constexpr unit value = U;
};

struct named_unit {
    std::string_view     Utf8;
    std::string_view     Ascii;
    dimension            Dim{};
    magnitude            Mag{};
    const quantity_spec* Kind{};
    bool                 SpaceBefore{true};
    const point_origin*  Origin{}; // the default origin of its points (K: absolute zero)

    // A base unit of a kind: the coherent unit of its dimension.
    template <const quantity_spec* K>
    constexpr named_unit(std::string_view sym, spec_t<K>) : Utf8{sym}, Ascii{sym}, Dim{K->Dim}, Kind{K} {}
    template <magnitude M, const quantity_spec* K>
    constexpr named_unit(std::string_view sym, mag_t<M>, spec_t<K>)
        : Utf8{sym}, Ascii{sym}, Dim{K->Dim}, Mag{M}, Kind{K} {}
    template <const quantity_spec* K>
    constexpr named_unit(std::string_view utf8, std::string_view ascii, spec_t<K>)
        : Utf8{utf8}, Ascii{ascii}, Dim{K->Dim}, Kind{K} {}
    // A unit defined by another (named_unit{"N", kg * m / s², isq::force}).
    template <unit U>
    constexpr named_unit(std::string_view sym, unit_t<U>);
    template <unit U>
    constexpr named_unit(std::string_view utf8, std::string_view ascii, unit_t<U>);
    template <unit U, const quantity_spec* K>
    constexpr named_unit(std::string_view sym, unit_t<U>, spec_t<K>);
    template <unit U, const quantity_spec* K>
    constexpr named_unit(std::string_view utf8, std::string_view ascii, unit_t<U>, spec_t<K>);
    // A unit with a default point origin (K: absolute zero, ℃: the ice point).
    template <const quantity_spec* K, const point_origin* O>
    constexpr named_unit(std::string_view sym, spec_t<K>, origin_t<O>) : named_unit{sym, spec_t<K>{}} {
        Origin = O;
    }
    template <unit U, const point_origin* O>
    constexpr named_unit(std::string_view utf8, std::string_view ascii, unit_t<U>, origin_t<O>)
        : named_unit{utf8, ascii, unit_t<U>{}} {
        Origin = O;
    }
    // A symbol written without a space after the number (90°).
    template <unit U>
    constexpr named_unit(std::string_view utf8, std::string_view ascii, unit_t<U>, no_space_before_t)
        : named_unit{utf8, ascii, unit_t<U>{}} {
        SpaceBefore = false;
    }
};

namespace detail {
consteval bool atom_before(const unit_atom& a, const unit_atom& b) {
    if (a.U->Utf8 != b.U->Utf8)
        return a.U->Utf8 < b.U->Utf8;
    return a.Prefix < b.Prefix;
}

consteval void unit_add(unit& u, unit_atom a) {
    for (std::int8_t i = 0; i < u.Size; ++i)
        if (u.Atoms[i].U == a.U && u.Atoms[i].Prefix == a.Prefix) {
            u.Atoms[i].Exp = static_cast<std::int8_t>(u.Atoms[i].Exp + a.Exp);
            if (u.Atoms[i].Exp == 0) {
                for (std::int8_t j = i; j + 1 < u.Size; ++j)
                    u.Atoms[j] = u.Atoms[j + 1];
                u.Atoms[--u.Size] = {};
            }
            return;
        }
    if (a.Exp == 0)
        return;
    if (u.Size == static_cast<std::int8_t>(max_unit_atoms))
        beman::inside::detail::constexpr_error<"units: a unit holds at most 6 factors - name it with named_unit">();
    std::int8_t at = u.Size;
    while (at > 0 && atom_before(a, u.Atoms[at - 1])) {
        u.Atoms[at] = u.Atoms[at - 1];
        --at;
    }
    u.Atoms[at] = a;
    ++u.Size;
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
        unit_add(r, {u.Atoms[i].U, u.Atoms[i].Prefix, static_cast<std::int8_t>(u.Atoms[i].Exp * n)});
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
// The kind a unit measures: a named unit's (with a prefix too), else none.
consteval const quantity_spec* kind_of_unit(const unit& u) {
    return u.Size == 1 && u.Atoms[0].Exp == 1 && u.Scale.is_one() ? u.Atoms[0].U->Kind : nullptr;
}

namespace detail {
// The kind a unit defined by U inherits: its one factor's (min: 60 s).
consteval const quantity_spec* inherited_kind(const unit& u) {
    return u.Size == 1 && u.Atoms[0].Exp == 1 ? u.Atoms[0].U->Kind : nullptr;
}
} // namespace detail

template <unit U>
constexpr named_unit::named_unit(std::string_view sym, unit_t<U>)
    : Utf8{sym}, Ascii{sym}, Dim{dimension_of(U)}, Mag{magnitude_of(U)}, Kind{detail::inherited_kind(U)} {}
template <unit U>
constexpr named_unit::named_unit(std::string_view utf8, std::string_view ascii, unit_t<U>)
    : Utf8{utf8}, Ascii{ascii}, Dim{dimension_of(U)}, Mag{magnitude_of(U)}, Kind{detail::inherited_kind(U)} {}
template <unit U, const quantity_spec* K>
constexpr named_unit::named_unit(std::string_view sym, unit_t<U>, spec_t<K>)
    : Utf8{sym}, Ascii{sym}, Dim{dimension_of(U)}, Mag{magnitude_of(U)}, Kind{K} {
    if (!(K->Dim == Dim))
        beman::inside::detail::constexpr_error<"units: a unit's kind must have the unit's dimension">();
}
template <unit U, const quantity_spec* K>
constexpr named_unit::named_unit(std::string_view utf8, std::string_view ascii, unit_t<U>, spec_t<K>)
    : Utf8{utf8}, Ascii{ascii}, Dim{dimension_of(U)}, Mag{magnitude_of(U)}, Kind{K} {
    if (!(K->Dim == Dim))
        beman::inside::detail::constexpr_error<"units: a unit's kind must have the unit's dimension">();
}

// unit_c<named> — the tag of a named unit: inline constexpr auto metre = unit_c<metre_def>.
template <const named_unit& N>
inline constexpr unit_t<unit{N}> unit_c{};

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
    const quantity_spec* Spec{};
    bool                 KindOf{false};
    recipe               Derived{};
    unit                 U{};

    constexpr reference() = default;
    constexpr reference(const quantity_spec* s, bool kind_of, recipe derived, unit u)
        : Spec{s}, KindOf{kind_of}, Derived{derived}, U{u} {}
    // A unit tag as a reference: quantity<si::metre, …>.
    template <unit V>
    constexpr reference(unit_t<V>) : Spec{kind_of_unit(V)}, KindOf{kind_of_unit(V) != nullptr}, U{V} {}

    friend constexpr bool operator==(const reference&, const reference&) = default;
};

consteval reference reference_of(const unit& u) {
    const quantity_spec* k = kind_of_unit(u);
    return reference{k, k != nullptr, recipe{}, u};
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
    constexpr const quantity_spec* k = kind_of_unit(u);
    static_assert(k == nullptr || detail::kind_root(k) == detail::kind_root(S),
                  "units: spec[unit] - the unit measures another kind of quantity (Hz for activity?)");
    return reference_t<reference{S, false, recipe{}, u}>{};
}

namespace detail {
// Which spec facts a reference carries.
consteval bool is_generic(const reference& r) { return r.Spec == nullptr && r.Derived.Size == 0; }

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
        return from.Spec == nullptr || !kind_root(from.Spec)->IsKind;
    if (is_generic(from))
        return to.Spec == nullptr || !kind_root(to.Spec)->IsKind;
    if (to.Spec == nullptr) { // a derived target: the source's recipe, by kind
        const recipe have = from.Spec ? recipe_of(from.Spec) : from.Derived;
        return recipe_by_kind(have) == recipe_by_kind(to.Derived);
    }
    if (from.Spec == nullptr)
        return derived_converts_to(from, to.KindOf ? kind_root(to.Spec) : to.Spec);
    if (to.KindOf || from.KindOf) // a unit's kind takes and gives any spec of that kind
        return kind_root(from.Spec) == kind_root(to.Spec);
    return is_within_kind_ancestor(from.Spec, to.Spec);
}

// Explicit conversion (quantity_cast): any spec of the same dimension, but
// not across IsKind kinds.
consteval bool spec_casts(const reference& from, const quantity_spec* to) {
    const quantity_spec* fk = from.Spec ? kind_root(from.Spec) : nullptr;
    const quantity_spec* tk = kind_root(to);
    if (fk == tk)
        return true;
    if (fk && fk->IsKind)
        return false;
    return !tk->IsKind || from.Spec == nullptr;
}

// The spec part of a sum's reference; ok = false when the two cannot add.
struct spec_result {
    bool      Ok{false};
    reference R{};
};

consteval spec_result common_spec(const reference& a, const reference& b) {
    if (a.Spec == b.Spec && a.KindOf == b.KindOf && a.Derived == b.Derived)
        return {true, a};
    if (spec_converts(a, b) && !spec_converts(b, a))
        return {true, b};
    if (spec_converts(b, a) && !spec_converts(a, b))
        return {true, a};
    if (a.Spec && b.Spec) {
        if (a.KindOf && b.KindOf && kind_root(a.Spec) == kind_root(b.Spec))
            return {true, reference{kind_root(a.Spec), true, {}, a.U}};
        if (a.KindOf != b.KindOf && kind_root(a.Spec) == kind_root(b.Spec)) // kind_of + a named spec of it
            return {true, a.KindOf ? b : a};
        // The nearest common ancestor within one kind.
        for (const quantity_spec* s = a.Spec;; s = s->Parent) {
            if (is_within_kind_ancestor(b.Spec, s))
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
    const auto empty = [](const reference& r) { return r.Spec == nullptr && r.KindOf && r.Derived.Size == 0; };
    if ((is_generic(a) && !empty(a)) || (is_generic(b) && !empty(b)))
        return reference{nullptr, false, {}, u};
    const auto factors = [](const reference& r) {
        if (r.Spec == nullptr)
            return r.Derived;
        recipe f{};
        recipe_add(f, r.KindOf ? kind_root(r.Spec) : r.Spec, 1);
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
    static_assert(
        exact_factor_v<M>,
        "units: this conversion multiplies by a power of pi, which has no exact grid - round it "
        "with value_cast<unit, Rep>(q) from <beman/inside/units_math.hpp>, Rep declared with a rounding mode");
    if constexpr (M.is_one())
        return v;
    else {
        // The product's grid with v's policy: a conversion keeps clamp, wrap
        // and the rounding mode.
        const auto x = v * just<M.Ratio>;
        return inside<grid_of<decltype(x)>, flags_of<V>>::from_raw(x.raw());
    }
}

template <unit From, unit To>
inline constexpr magnitude conversion_factor = magnitude_of(From) / magnitude_of(To);

// The common unit of a sum: the smaller unit when it divides the other
// (km + m → m), else the first unit scaled to the gcd of the magnitudes
// (ft + m → [1/1250 m]).
consteval unit common_unit(const unit& a, const unit& b) {
    if (a == b)
        return a;
    const magnitude ma = magnitude_of(a), mb = magnitude_of(b);
    if (ma == mb)
        return a;
    if (ma.Pi != mb.Pi)
        beman::inside::detail::constexpr_error<
            "units: no common unit of a pi multiple and a rational one - convert one side with value_cast first">();
    if (!beman::inside::detail::grid_gcd_fits(ma.Ratio, mb.Ratio))
        beman::inside::detail::constexpr_error<"units: the common unit needs C++26 big grids">();
    const grid_rational g = beman::inside::detail::grid_gcd_of(ma.Ratio, mb.Ratio);
    if (g == ma.Ratio)
        return a;
    if (g == mb.Ratio)
        return b;
    unit u  = a;
    u.Scale = u.Scale * magnitude{beman::inside::detail::grid_div_of(g, ma.Ratio)};
    return u;
}
} // namespace detail

//---------------------------------------------------------------------------
// quantity<R, Rep> — a number Rep (an inside) of the reference R.
//---------------------------------------------------------------------------
template <reference R, insidable Rep>
class quantity;

template <class T>
inline constexpr bool is_quantity_v = false;
template <reference R, class Rep>
inline constexpr bool is_quantity_v<quantity<R, Rep>> = true;

template <class T>
concept quantity_like = is_quantity_v<std::remove_cvref_t<T>>;

namespace detail {
template <reference From, reference To>
inline constexpr bool implicit_v = dimension_of(From.U) == dimension_of(To.U) && spec_converts(From, To) &&
                                   exact_factor_v<conversion_factor<From.U, To.U>>;

template <reference From, reference To, insidable V>
using rescaled_t = decltype(rescale<conversion_factor<From.U, To.U>>(std::declval<const V&>()));

template <class T>
inline constexpr bool is_expected_v = false;
template <class T, class E>
inline constexpr bool is_expected_v<std::expected<T, E>> = true;

// A quantity of reference R on a number that inside's arithmetic returned: an
// inside, or an expected of one (division by a grid holding 0).
template <reference R, class V>
constexpr auto make_quantity(V&& v) {
    using T = std::remove_cvref_t<V>;
    if constexpr (is_expected_v<T>) {
        using Q = quantity<R, typename T::value_type>;
        if (!v)
            return std::expected<Q, errc>{std::unexpect, v.error()};
        return std::expected<Q, errc>{Q{*std::forward<V>(v)}};
    } else
        return quantity<R, T>{std::forward<V>(v)};
}
} // namespace detail

template <reference R, insidable Rep>
class quantity {
    Rep Num;

  public:
    using rep                                   = Rep;
    static constexpr units::reference reference = R;
    static constexpr units::unit      unit      = R.U;
    static constexpr units::dimension dimension = dimension_of(R.U);

    constexpr quantity() = default;

    // From a number in the unit R: quantity<si::metre, inside<{0, 100}>>{42}.
    template <class V>
        requires std::constructible_from<Rep, V> && (!quantity_like<V>)
    constexpr explicit quantity(V&& v) : Num(std::forward<V>(v)) {}

    // From another quantity: implicit when the spec converts implicitly and
    // the number, in R's unit, converts implicitly into Rep.
    template <units::reference R2, class V2>
        requires detail::implicit_v<R2, R> && std::constructible_from<Rep, detail::rescaled_t<R2, R, V2>>
    constexpr explicit(!std::convertible_to<detail::rescaled_t<R2, R, V2>, Rep>) quantity(const quantity<R2, V2>& q)
        : Num(detail::rescale<detail::conversion_factor<R2.U, R.U>>(q.numerical_value())) {}

    [[nodiscard]] constexpr const Rep& numerical_value() const noexcept { return Num; }

    // The same quantity in unit U: exact, the number's grid rescaled.
    template <units::unit U>
    [[nodiscard]] constexpr auto in() const {
        static_assert(dimension_of(U) == dimension, "units: in() - the unit has another dimension");
        constexpr units::reference to{R.Spec, R.KindOf, R.Derived, U};
        return detail::make_quantity<to>(detail::rescale<detail::conversion_factor<R.U, U>>(Num));
    }
    template <units::unit U>
    [[nodiscard]] constexpr auto in(units::unit_t<U>) const {
        return in<U>();
    }
    // The number in unit U.
    template <units::unit U>
    [[nodiscard]] constexpr auto numerical_value_in(units::unit_t<U>) const {
        return in<U>().numerical_value();
    }

    // A dimensionless quantity of unit one is its number.
    constexpr operator Rep() const
        requires(R.U == units::unit{} && R.Spec == nullptr)
    {
        return Num;
    }

    [[nodiscard]] constexpr auto operator+() const { return detail::make_quantity<R>(+Num); }
    [[nodiscard]] constexpr auto operator-() const { return detail::make_quantity<R>(-Num); }

    // Compound assignment keeps the type: the number is stored back under Rep's
    // policy (checked by default).
    template <units::reference R2, class V2>
        requires detail::implicit_v<R2, R>
    constexpr quantity& operator+=(const quantity<R2, V2>& q) {
        Num += detail::rescale<detail::conversion_factor<R2.U, R.U>>(q.numerical_value());
        return *this;
    }
    template <units::reference R2, class V2>
        requires detail::implicit_v<R2, R>
    constexpr quantity& operator-=(const quantity<R2, V2>& q) {
        Num -= detail::rescale<detail::conversion_factor<R2.U, R.U>>(q.numerical_value());
        return *this;
    }
    template <insidable V>
    constexpr quantity& operator*=(const V& v) {
        Num *= v;
        return *this;
    }
    template <insidable V>
    constexpr quantity& operator/=(const V& v) {
        Num /= v;
        return *this;
    }
};

//---------------------------------------------------------------------------
// Construction: number * unit, number * spec[unit]. A raw scalar has no grid.
//---------------------------------------------------------------------------
template <insidable V, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const V& v, Tag t) {
    return quantity<reference_of(t), V>{v};
}
template <class N, class Tag>
    requires is_reference_tag_v<Tag> && std::is_arithmetic_v<N>
constexpr auto operator*(N, Tag) {
    static_assert(!std::is_arithmetic_v<N>,
                  "units: a raw number has no grid - write just<42> * unit, or quantity<unit, inside<…>>{42}");
}
// number / unit: 5 / s.
template <insidable V, unit U>
[[nodiscard]] constexpr auto operator/(const V& v, unit_t<U>) {
    return quantity<reference_of(detail::unit_pow(U, -1)), V>{v};
}

//---------------------------------------------------------------------------
// Arithmetic
//---------------------------------------------------------------------------
namespace detail {
template <reference A, reference B>
consteval reference sum_reference() {
    static_assert(dimension_of(A.U) == dimension_of(B.U),
                  "units: adding or comparing quantities of different dimensions");
    constexpr spec_result s = common_spec(A, B);
    static_assert(s.Ok, "units: adding or comparing quantities of different kinds");
    reference r = s.R;
    r.U         = common_unit(A.U, B.U);
    return r;
}

template <reference A, reference B, int Sign>
consteval reference product_reference() {
    const unit u = unit_mul(A.U, B.U, Sign);
    return spec_product(A, B, Sign, u);
}
} // namespace detail

template <reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr auto operator+(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) +
                                    detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value()));
}
template <reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr auto operator-(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) -
                                    detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value()));
}
template <reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr auto operator%(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) %
                                    detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value()));
}
template <reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr bool operator==(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) ==
           detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value());
}
template <reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr auto operator<=>(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) <=>
           detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value());
}

template <reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, 1>()>(a.numerical_value() * b.numerical_value());
}
template <reference A, class VA, reference B, class VB>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, -1>()>(a.numerical_value() / b.numerical_value());
}

// A quantity scaled by a dimensionless number (an inside).
template <reference A, class VA, insidable V>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(a.numerical_value() * v);
}
template <insidable V, reference A, class VA>
[[nodiscard]] constexpr auto operator*(const V& v, const quantity<A, VA>& a) {
    return detail::make_quantity<A>(v * a.numerical_value());
}
template <reference A, class VA, insidable V>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(a.numerical_value() / v);
}
template <insidable V, reference A, class VA>
[[nodiscard]] constexpr auto operator/(const V& v, const quantity<A, VA>& a) {
    constexpr reference R = detail::product_reference<reference{nullptr, true, {}, unit{}}, A, -1>();
    return detail::make_quantity<R>(v / a.numerical_value());
}
// A quantity times a unit: (5 * m) / s.
template <reference A, class VA, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, Tag t) {
    return detail::make_quantity<detail::product_reference<A, reference_of(t), 1>()>(a.numerical_value());
}
template <reference A, class VA, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, Tag t) {
    return detail::make_quantity<detail::product_reference<A, reference_of(t), -1>()>(a.numerical_value());
}
// No raw scalars: q * 2 needs a grid for the 2.
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
constexpr auto operator*(const quantity<A, VA>&, N) {
    static_assert(!std::is_arithmetic_v<N>, "units: a raw number has no grid - scale by just<2>, not 2");
}
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
constexpr auto operator*(N, const quantity<A, VA>&) {
    static_assert(!std::is_arithmetic_v<N>, "units: a raw number has no grid - scale by just<2>, not 2");
}
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
constexpr auto operator/(const quantity<A, VA>&, N) {
    static_assert(!std::is_arithmetic_v<N>, "units: a raw number has no grid - divide by just<2>, not 2");
}

// pow<N>(q) — q·q·…·q (N ≥ 1): exact, the grid of each product.
template <int N, reference R, class V>
    requires(N >= 1)
[[nodiscard]] constexpr auto pow(const quantity<R, V>& q) {
    if constexpr (N == 1)
        return q;
    else if constexpr (N % 2 == 1)
        return q * pow<N - 1>(q);
    else {
        const auto h = pow<N / 2>(q);
        return h * h;
    }
}

//---------------------------------------------------------------------------
// Casts
//---------------------------------------------------------------------------
// quantity_cast<spec>(q) — the same number as another spec of the dimension:
// down the tree (length → height) or across one kind; never across IsKind.
template <class SpecTag, reference R, class V>
[[nodiscard]] constexpr auto quantity_cast(const quantity<R, V>& q) {
    constexpr const quantity_spec* to = SpecTag::value;
    static_assert(to->Dim == dimension_of(R.U), "units: quantity_cast - another dimension");
    static_assert(detail::spec_casts(R, to), "units: quantity_cast - across an is_kind boundary");
    return quantity<reference{to, false, {}, R.U}, V>{q.numerical_value()};
}
template <auto Spec, reference R, class V>
[[nodiscard]] constexpr auto quantity_cast(const quantity<R, V>& q) {
    return quantity_cast<decltype(Spec)>(q);
}

// value_cast<Rep2>(q) — another representation, the same unit: Rep2's
// constructor (checked unless Rep2's policy says otherwise).
template <insidable Rep2, reference R, class V>
[[nodiscard]] constexpr quantity<R, Rep2> value_cast(const quantity<R, V>& q) {
    return quantity<R, Rep2>{Rep2(q.numerical_value())};
}
// value_cast<U, Rep2>(q) — in unit U with representation Rep2: the exact
// value in U, stored by Rep2's constructor. (A pi factor, degree ↔ radian,
// rounds once: units_math.hpp.)
template <auto U, insidable Rep2, reference R, class V>
    requires is_unit_tag_v<decltype(U)> && detail::exact_factor_v<detail::conversion_factor<R.U, decltype(U)::value>>
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    constexpr units::unit u = decltype(U)::value;
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    return quantity<to, Rep2>{Rep2(q.in(U).numerical_value())};
}

// try_value_cast<Rep2, F>(q) — as Rep2::try_make<F>: an expected.
template <insidable Rep2, policy_flag F = none, reference R, class V>
[[nodiscard]] constexpr std::expected<quantity<R, Rep2>, errc> try_value_cast(const quantity<R, V>& q) {
    auto v = Rep2::template try_make<F>(q.numerical_value());
    if (!v)
        return std::unexpected{v.error()};
    return quantity<R, Rep2>{*v};
}

//---------------------------------------------------------------------------
// quantity_point<R, O, Rep> — a point: a quantity of R counted from origin O
// (null: the implicit zero of the dimension). A change of origin shifts the
// grid by the exact offset, a change of unit rescales it.
//   auto t = point<si::degree_Celsius>(inside<{-40, 60}>{20});   // 20 ℃
//   t.quantity_from(si::absolute_zero)                         // 293.15 K… in ℃
//   t.point_for(si::absolute_zero).in(si::kelvin)              // the point 293.15 K
//---------------------------------------------------------------------------
template <reference R, const point_origin* O, insidable Rep>
class quantity_point;

template <class T>
inline constexpr bool is_quantity_point_v = false;
template <reference R, const point_origin* O, class Rep>
inline constexpr bool is_quantity_point_v<quantity_point<R, O, Rep>> = true;

namespace detail {
// The number, in unit U, that takes a quantity from origin From to origin To:
// (offset(From) − offset(To)) / mag(U).
template <const point_origin* From, const point_origin* To, unit U>
consteval grid_rational origin_shift() {
    static_assert(absolute_of(From) == absolute_of(To), "units: points of different absolute origins");
    constexpr magnitude m = magnitude_of(U);
    static_assert(m.Pi == 0, "units: an origin shift in a unit with a pi magnitude has no exact grid");
    return beman::inside::detail::grid_div_of(beman::inside::detail::grid_sub(offset_of(From), offset_of(To)),
                                              m.Ratio);
}

template <grid_rational C, insidable V>
constexpr auto shift(const V& v) {
    if constexpr (C == grid_rational{0})
        return v;
    else
        return v + just<C>;
}

// The default origin of a unit's points: its named unit's (K, ℃, ℉).
consteval const point_origin* origin_of_unit(const unit& u) {
    return u.Size == 1 && u.Atoms[0].Exp == 1 && u.Scale.is_one() ? u.Atoms[0].U->Origin : nullptr;
}

template <const point_origin* O, class Q>
constexpr auto make_point(Q&& q) {
    using T = std::remove_cvref_t<Q>;
    if constexpr (is_expected_v<T>) {
        using P = quantity_point<T::value_type::reference, O, typename T::value_type::rep>;
        if (!q)
            return std::expected<P, errc>{std::unexpect, q.error()};
        return std::expected<P, errc>{P{*std::forward<Q>(q)}};
    } else
        return quantity_point<T::reference, O, typename T::rep>{std::forward<Q>(q)};
}
} // namespace detail

template <reference R, const point_origin* O, insidable Rep>
class quantity_point {
    quantity<R, Rep> Q;

  public:
    using quantity_type                            = quantity<R, Rep>;
    using rep                                      = Rep;
    static constexpr units::reference    reference = R;
    static constexpr const point_origin* origin    = O;

    constexpr quantity_point() = default;
    constexpr explicit quantity_point(const quantity<R, Rep>& q) : Q{q} {}

    // From a point of another origin (of the same absolute one) or unit:
    // shifted, then rescaled, exactly; implicit as the quantities are.
    template <units::reference R2, const point_origin* O2, class V2>
        requires(detail::absolute_of(O2) == detail::absolute_of(O)) && detail::implicit_v<R2, R> &&
                std::constructible_from<
                    quantity<R, Rep>,
                    decltype(std::declval<const quantity_point<R2, O2, V2>&>().quantity_from(origin_t<O>{}))>
    constexpr explicit(
        !std::convertible_to<decltype(std::declval<const quantity_point<R2, O2, V2>&>().quantity_from(origin_t<O>{})),
                             quantity<R, Rep>>) quantity_point(const quantity_point<R2, O2, V2>& p)
        : Q(p.quantity_from(origin_t<O>{})) {}

    // The quantity from this point's origin, or from another one.
    [[nodiscard]] constexpr const quantity<R, Rep>& quantity_from_origin() const noexcept { return Q; }
    template <const point_origin* O2>
    [[nodiscard]] constexpr auto quantity_from(origin_t<O2>) const {
        constexpr grid_rational c = detail::origin_shift<O, O2, R.U>();
        return quantity<R, std::remove_cvref_t<decltype(detail::shift<c>(Q.numerical_value()))>>{
            detail::shift<c>(Q.numerical_value())};
    }
    // The same point counted from another origin.
    template <const point_origin* O2>
    [[nodiscard]] constexpr auto point_for(origin_t<O2> o) const {
        return detail::make_point<O2>(quantity_from(o));
    }
    // The same point in another unit.
    template <units::unit U>
    [[nodiscard]] constexpr auto in(unit_t<U> u) const {
        return detail::make_point<O>(Q.in(u));
    }

    template <units::reference R2, class V2>
        requires detail::implicit_v<R2, R>
    constexpr quantity_point& operator+=(const quantity<R2, V2>& q) {
        Q += q;
        return *this;
    }
    template <units::reference R2, class V2>
        requires detail::implicit_v<R2, R>
    constexpr quantity_point& operator-=(const quantity<R2, V2>& q) {
        Q -= q;
        return *this;
    }
};

// point<si::degree_Celsius>(v) — a point at the unit's default origin (the
// implicit zero for a unit without one).
template <auto U, insidable V>
[[nodiscard]] constexpr auto point(const V& v) {
    return quantity_point<reference_of(U), detail::origin_of_unit(decltype(U)::value), V>{
        quantity<reference_of(U), V>{v}};
}
// delta<si::degree_Celsius>(v) — a difference: a plain quantity.
template <auto U, insidable V>
[[nodiscard]] constexpr auto delta(const V& v) {
    return v * U;
}

// origin + quantity: si::absolute_zero + just<300> * si::kelvin.
template <const point_origin* O, reference R, class V>
[[nodiscard]] constexpr auto operator+(origin_t<O>, const quantity<R, V>& q) {
    return quantity_point<R, O, V>{q};
}
template <reference R, class V, const point_origin* O>
[[nodiscard]] constexpr auto operator+(const quantity<R, V>& q, origin_t<O>) {
    return quantity_point<R, O, V>{q};
}

// point ± quantity: a point at the same origin; point − point: a quantity
// (the second counted from the first's origin).
template <reference R, const point_origin* O, class V, reference R2, class V2>
[[nodiscard]] constexpr auto operator+(const quantity_point<R, O, V>& p, const quantity<R2, V2>& q) {
    return detail::make_point<O>(p.quantity_from_origin() + q);
}
template <reference R2, class V2, reference R, const point_origin* O, class V>
[[nodiscard]] constexpr auto operator+(const quantity<R2, V2>& q, const quantity_point<R, O, V>& p) {
    return detail::make_point<O>(q + p.quantity_from_origin());
}
template <reference R, const point_origin* O, class V, reference R2, class V2>
[[nodiscard]] constexpr auto operator-(const quantity_point<R, O, V>& p, const quantity<R2, V2>& q) {
    return detail::make_point<O>(p.quantity_from_origin() - q);
}
template <reference R, const point_origin* O, class V, reference R2, const point_origin* O2, class V2>
[[nodiscard]] constexpr auto operator-(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() - b.quantity_from(origin_t<O>{});
}
template <reference R, const point_origin* O, class V, reference R2, const point_origin* O2, class V2>
[[nodiscard]] constexpr bool operator==(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() == b.quantity_from(origin_t<O>{});
}
template <reference R, const point_origin* O, class V, reference R2, const point_origin* O2, class V2>
[[nodiscard]] constexpr auto operator<=>(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() <=> b.quantity_from(origin_t<O>{});
}

//---------------------------------------------------------------------------
// Text: a unit's symbol (kg m²/s², km/h, 1/s), a quantity as "5 km".
//---------------------------------------------------------------------------
enum class text_encoding : unsigned char { utf8, ascii };

namespace detail {
constexpr std::string_view prefix_symbol(int p, text_encoding e) {
    switch (p) {
    case -30:
        return "q";
    case -27:
        return "r";
    case -24:
        return "y";
    case -21:
        return "z";
    case -18:
        return "a";
    case -15:
        return "f";
    case -12:
        return "p";
    case -9:
        return "n";
    case -6:
        return e == text_encoding::utf8 ? "µ" : "u";
    case -3:
        return "m";
    case -2:
        return "c";
    case -1:
        return "d";
    case 0:
        return "";
    case 1:
        return "da";
    case 2:
        return "h";
    case 3:
        return "k";
    case 6:
        return "M";
    case 9:
        return "G";
    case 12:
        return "T";
    case 15:
        return "P";
    case 18:
        return "E";
    case 21:
        return "Z";
    case 24:
        return "Y";
    case 27:
        return "R";
    case 30:
        return "Q";
    default:
        return "?";
    }
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

inline void append_atoms(std::string& s, const unit& u, int sign, text_encoding enc) {
    bool first = true;
    for (std::int8_t i = 0; i < u.Size; ++i) {
        const unit_atom& a = u.Atoms[i];
        if ((a.Exp > 0) != (sign > 0))
            continue;
        if (!first)
            s += ' ';
        first = false;
        s += prefix_symbol(a.Prefix, enc);
        s += enc == text_encoding::utf8 ? a.U->Utf8 : a.U->Ascii;
        append_exponent(s, a.Exp * sign, enc);
    }
}
} // namespace detail

// The symbol of a unit: "km", "kg m²/s²", "1/s", "[1/1250 m]" (a scaled unit).
inline std::string unit_symbol(const unit& u, text_encoding enc = text_encoding::utf8) {
    std::string s;
    int         num = 0, den = 0;
    for (std::int8_t i = 0; i < u.Size; ++i)
        (u.Atoms[i].Exp > 0 ? num : den) += 1;
    if (num > 0)
        detail::append_atoms(s, u, 1, enc);
    else if (den > 0)
        s += '1';
    if (den > 0) {
        s += '/';
        if (den > 1)
            s += '(';
        detail::append_atoms(s, u, -1, enc);
        if (den > 1)
            s += ')';
    }
    if (!u.Scale.is_one()) {
        std::string scale = beman::inside::to_string(u.Scale.Ratio);
        if (u.Scale.Pi != 0) {
            scale += enc == text_encoding::utf8 ? " π" : " pi";
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
    // A copy: clang 20 miscompiles a runtime reference to a template
    // parameter object holding pointers.
    static constexpr unit u   = R.U;
    std::string           s   = beman::inside::to_string(q.numerical_value());
    const std::string     sym = unit_symbol(u, enc);
    constexpr bool space = !(u.Size == 1 && u.Atoms[0].Exp == 1 && u.Scale.is_one() && !u.Atoms[0].U->SpaceBefore);
    if (sym.empty())
        return s;
    return s + (space ? " " : "") + sym;
}

template <reference R, class V>
std::ostream& operator<<(std::ostream& os, const quantity<R, V>& q) {
    return os << units::to_string(q);
}

// A point as its quantity from its origin: "20 ℃".
template <reference R, const point_origin* O, class V>
std::string to_string(const quantity_point<R, O, V>& p, text_encoding enc = text_encoding::utf8) {
    return units::to_string(p.quantity_from_origin(), enc);
}
template <reference R, const point_origin* O, class V>
std::ostream& operator<<(std::ostream& os, const quantity_point<R, O, V>& p) {
    return os << units::to_string(p);
}

} // namespace beman::inside::units

#ifdef __cpp_lib_format
// std::format: the spec formats the number as for its inside ({:.2f}, {:>8}),
// then the unit symbol follows ("5 km", "90°").
template <beman::inside::units::reference R, class V>
struct std::formatter<beman::inside::units::quantity<R, V>> : std::formatter<V> {
    template <typename Ctx>
    auto format(const beman::inside::units::quantity<R, V>& q, Ctx& ctx) const {
        namespace u = beman::inside::units;
        // A copy: clang 20 miscompiles a runtime reference to a template
        // parameter object holding pointers.
        static constexpr u::unit unit = R.U;
        constexpr bool           space =
            !(unit.Size == 1 && unit.Atoms[0].Exp == 1 && unit.Scale.is_one() && !unit.Atoms[0].U->SpaceBefore);
        auto              out = std::formatter<V>::format(q.numerical_value(), ctx);
        const std::string sym = u::unit_symbol(unit);
        if (sym.empty())
            return out;
        if constexpr (space)
            *out++ = ' ';
        return std::ranges::copy(sym, out).out;
    }
};

template <beman::inside::units::reference R, const beman::inside::units::point_origin* O, class V>
struct std::formatter<beman::inside::units::quantity_point<R, O, V>>
    : std::formatter<beman::inside::units::quantity<R, V>> {
    template <typename Ctx>
    auto format(const beman::inside::units::quantity_point<R, O, V>& p, Ctx& ctx) const {
        return std::formatter<beman::inside::units::quantity<R, V>>::format(p.quantity_from_origin(), ctx);
    }
};
#endif // __cpp_lib_format

namespace beman::inside::units {
//---------------------------------------------------------------------------
// The catalog: ISQ quantity specs, SI units and prefixes, international units.
// Each name is a tag object; its definition is a static object in *_defs.
//---------------------------------------------------------------------------
#define BEMAN_INSIDE_SPEC(name, ...)                         \
    namespace isq_defs {                                     \
    inline constexpr quantity_spec name{#name, __VA_ARGS__}; \
    }                                                        \
    namespace isq {                                          \
    inline constexpr spec_t<&isq_defs::name> name{};         \
    }
#define BEMAN_INSIDE_ORIGIN(ns, name, ...)                      \
    namespace ns##_defs {                                       \
        inline constexpr point_origin name{#name, __VA_ARGS__}; \
    }                                                           \
    namespace ns {                                              \
    inline constexpr origin_t<&ns##_defs::name> name{};         \
    }
#define BEMAN_INSIDE_UNIT(ns, name, ...)                  \
    namespace ns##_defs {                                 \
        inline constexpr named_unit name{__VA_ARGS__};    \
    }                                                     \
    namespace ns {                                        \
    inline constexpr auto name = unit_c<ns##_defs::name>; \
    }

// Base quantities (ISO 80000-1) and the dimensionless one.
BEMAN_INSIDE_SPEC(dimensionless, dim_one)
BEMAN_INSIDE_SPEC(length, dim_length)
BEMAN_INSIDE_SPEC(mass, dim_mass)
BEMAN_INSIDE_SPEC(duration, dim_time)
BEMAN_INSIDE_SPEC(electric_current, dim_electric_current)
BEMAN_INSIDE_SPEC(thermodynamic_temperature, dim_thermodynamic_temperature)
BEMAN_INSIDE_SPEC(amount_of_substance, dim_amount_of_substance)
BEMAN_INSIDE_SPEC(luminous_intensity, dim_luminous_intensity)
namespace isq {
inline constexpr auto time = duration;
}

// Space and time (ISO 80000-3).
BEMAN_INSIDE_SPEC(width, isq::length)
BEMAN_INSIDE_SPEC(thickness, isq::width)
BEMAN_INSIDE_SPEC(diameter, isq::width)
BEMAN_INSIDE_SPEC(radius, isq::width)
BEMAN_INSIDE_SPEC(altitude, isq::length)
BEMAN_INSIDE_SPEC(height, isq::altitude)
BEMAN_INSIDE_SPEC(path_length, isq::length)
BEMAN_INSIDE_SPEC(arc_length, isq::path_length)
BEMAN_INSIDE_SPEC(distance, isq::path_length)
BEMAN_INSIDE_SPEC(radial_distance, isq::distance)
BEMAN_INSIDE_SPEC(wavelength, isq::length)
BEMAN_INSIDE_SPEC(displacement, isq::length)
BEMAN_INSIDE_SPEC(position_vector, isq::displacement)
BEMAN_INSIDE_SPEC(area, pow<2>(isq::length))
BEMAN_INSIDE_SPEC(volume, pow<3>(isq::length))
BEMAN_INSIDE_SPEC(angular_measure, isq::dimensionless, isq::arc_length / isq::radius, is_kind)
BEMAN_INSIDE_SPEC(rotational_displacement, isq::angular_measure, isq::path_length / isq::radius)
BEMAN_INSIDE_SPEC(phase_angle, isq::angular_measure, is_kind)
BEMAN_INSIDE_SPEC(solid_angular_measure, isq::dimensionless, isq::area / pow<2>(isq::radius), is_kind)
BEMAN_INSIDE_SPEC(period_duration, isq::duration)
BEMAN_INSIDE_SPEC(time_constant, isq::duration)
BEMAN_INSIDE_SPEC(speed, isq::length / isq::duration)
BEMAN_INSIDE_SPEC(velocity, isq::speed, isq::displacement / isq::duration)
BEMAN_INSIDE_SPEC(acceleration, isq::velocity / isq::duration)
BEMAN_INSIDE_SPEC(acceleration_of_free_fall, isq::acceleration)
BEMAN_INSIDE_SPEC(angular_velocity, isq::rotational_displacement / isq::duration)
BEMAN_INSIDE_SPEC(angular_acceleration, isq::angular_velocity / isq::duration)
BEMAN_INSIDE_SPEC(frequency, inverse(isq::period_duration))
BEMAN_INSIDE_SPEC(rotation, isq::dimensionless)
BEMAN_INSIDE_SPEC(rotational_frequency, isq::rotation / isq::duration)
BEMAN_INSIDE_SPEC(angular_frequency, isq::phase_angle / isq::duration)

// Mechanics (ISO 80000-4).
BEMAN_INSIDE_SPEC(mass_density, isq::mass / isq::volume)
BEMAN_INSIDE_SPEC(momentum, isq::mass* isq::velocity)
BEMAN_INSIDE_SPEC(force, isq::mass* isq::length / pow<2>(isq::duration))
BEMAN_INSIDE_SPEC(weight, isq::force, isq::mass* isq::acceleration_of_free_fall)
BEMAN_INSIDE_SPEC(impulse, isq::momentum, isq::force* isq::duration)
BEMAN_INSIDE_SPEC(moment_of_force, isq::position_vector* isq::force)
BEMAN_INSIDE_SPEC(torque, isq::moment_of_force)
BEMAN_INSIDE_SPEC(pressure, isq::force / isq::area)
BEMAN_INSIDE_SPEC(gauge_pressure, isq::pressure)
BEMAN_INSIDE_SPEC(energy, isq::mass* pow<2>(isq::length) / pow<2>(isq::duration))
BEMAN_INSIDE_SPEC(mechanical_work, isq::energy, isq::force* isq::displacement)
BEMAN_INSIDE_SPEC(mechanical_energy, isq::mechanical_work)
BEMAN_INSIDE_SPEC(potential_energy, isq::mechanical_energy)
BEMAN_INSIDE_SPEC(kinetic_energy, isq::mechanical_energy, isq::mass* pow<2>(isq::speed))
BEMAN_INSIDE_SPEC(power, isq::mass* pow<2>(isq::length) / pow<3>(isq::duration))
BEMAN_INSIDE_SPEC(mechanical_power, isq::power, isq::force* isq::velocity)
BEMAN_INSIDE_SPEC(action, isq::energy* isq::duration)
BEMAN_INSIDE_SPEC(mass_flow_rate, isq::mass / isq::duration)
BEMAN_INSIDE_SPEC(volume_flow_rate, isq::volume / isq::duration)
namespace isq {
inline constexpr auto work = mechanical_work;
}

// Thermodynamics (ISO 80000-5).
BEMAN_INSIDE_SPEC(internal_energy, isq::energy)
BEMAN_INSIDE_SPEC(heat, isq::internal_energy)
BEMAN_INSIDE_SPEC(enthalpy, isq::internal_energy)
BEMAN_INSIDE_SPEC(heat_capacity, isq::heat / isq::thermodynamic_temperature)
BEMAN_INSIDE_SPEC(entropy, isq::heat / isq::thermodynamic_temperature)
BEMAN_INSIDE_SPEC(heat_flow_rate, isq::heat / isq::duration)

// Electromagnetism (IEC 80000-6).
BEMAN_INSIDE_SPEC(electric_charge, isq::electric_current* isq::duration)
BEMAN_INSIDE_SPEC(electric_potential, isq::energy / (isq::electric_current * isq::duration))
BEMAN_INSIDE_SPEC(voltage, isq::electric_potential)
BEMAN_INSIDE_SPEC(capacitance, isq::electric_current* isq::duration / isq::electric_potential)
BEMAN_INSIDE_SPEC(impedance, isq::electric_potential / isq::electric_current)
BEMAN_INSIDE_SPEC(resistance, isq::impedance, isq::voltage / isq::electric_current)
BEMAN_INSIDE_SPEC(admittance, inverse(isq::impedance))
BEMAN_INSIDE_SPEC(conductance, isq::admittance, inverse(isq::resistance))
BEMAN_INSIDE_SPEC(magnetic_flux_density, isq::mass / (isq::electric_current * pow<2>(isq::duration)))
BEMAN_INSIDE_SPEC(magnetic_flux, isq::magnetic_flux_density* isq::area)
BEMAN_INSIDE_SPEC(inductance, isq::magnetic_flux / isq::electric_current)
BEMAN_INSIDE_SPEC(electric_power, isq::power)

// Light (ISO 80000-7), photopic vision.
BEMAN_INSIDE_SPEC(luminous_flux, isq::luminous_intensity* isq::solid_angular_measure)
BEMAN_INSIDE_SPEC(illuminance, isq::luminous_flux / isq::area)

// Physical chemistry and ionizing radiation (ISO 80000-9, -10).
BEMAN_INSIDE_SPEC(catalytic_activity, isq::amount_of_substance / isq::duration)
BEMAN_INSIDE_SPEC(activity, inverse(isq::duration))
BEMAN_INSIDE_SPEC(absorbed_dose, isq::energy / isq::mass)
BEMAN_INSIDE_SPEC(ionizing_radiation_quality_factor, isq::dimensionless)
BEMAN_INSIDE_SPEC(dose_equivalent, isq::absorbed_dose* isq::ionizing_radiation_quality_factor)

// SI prefixes. Past 10^±18 a prefix needs a grid number past 64 bits.
namespace si {
#if BEMAN_INSIDE_BIG_GRIDS
inline constexpr prefix_t<-30> quecto{};
inline constexpr prefix_t<-27> ronto{};
inline constexpr prefix_t<-24> yocto{};
inline constexpr prefix_t<-21> zepto{};
#endif
inline constexpr prefix_t<-18> atto{};
inline constexpr prefix_t<-15> femto{};
inline constexpr prefix_t<-12> pico{};
inline constexpr prefix_t<-9>  nano{};
inline constexpr prefix_t<-6>  micro{};
inline constexpr prefix_t<-3>  milli{};
inline constexpr prefix_t<-2>  centi{};
inline constexpr prefix_t<-1>  deci{};
inline constexpr prefix_t<1>   deca{};
inline constexpr prefix_t<2>   hecto{};
inline constexpr prefix_t<3>   kilo{};
inline constexpr prefix_t<6>   mega{};
inline constexpr prefix_t<9>   giga{};
inline constexpr prefix_t<12>  tera{};
inline constexpr prefix_t<15>  peta{};
inline constexpr prefix_t<18>  exa{};
#if BEMAN_INSIDE_BIG_GRIDS
inline constexpr prefix_t<21> zetta{};
inline constexpr prefix_t<24> yotta{};
inline constexpr prefix_t<27> ronna{};
inline constexpr prefix_t<30> quetta{};
#endif
} // namespace si

// SI base units.
BEMAN_INSIDE_UNIT(si, second, "s", isq::duration)
BEMAN_INSIDE_UNIT(si, metre, "m", isq::length)
BEMAN_INSIDE_UNIT(si, gram, "g", mag<rational{1, 1000}>, isq::mass)
namespace si {
inline constexpr auto kilogram = kilo(gram);
}
BEMAN_INSIDE_UNIT(si, ampere, "A", isq::electric_current)
BEMAN_INSIDE_ORIGIN(si, absolute_zero, dim_thermodynamic_temperature)
BEMAN_INSIDE_ORIGIN(si, ice_point, dim_thermodynamic_temperature, &si_defs::absolute_zero, rational{27315, 100})
BEMAN_INSIDE_UNIT(si, kelvin, "K", isq::thermodynamic_temperature, si::absolute_zero)
BEMAN_INSIDE_UNIT(si, mole, "mol", isq::amount_of_substance)
BEMAN_INSIDE_UNIT(si, candela, "cd", isq::luminous_intensity)

// SI derived units with special names.
BEMAN_INSIDE_UNIT(si, radian, "rad", si::metre / si::metre, isq::angular_measure)
BEMAN_INSIDE_UNIT(si, steradian, "sr", square(si::metre) / square(si::metre), isq::solid_angular_measure)
BEMAN_INSIDE_UNIT(si, hertz, "Hz", one / si::second, isq::frequency)
BEMAN_INSIDE_UNIT(si, newton, "N", si::kilogram* si::metre / square(si::second), isq::force)
BEMAN_INSIDE_UNIT(si, pascal, "Pa", si::newton / square(si::metre), isq::pressure)
BEMAN_INSIDE_UNIT(si, joule, "J", si::newton* si::metre, isq::energy)
BEMAN_INSIDE_UNIT(si, watt, "W", si::joule / si::second)
BEMAN_INSIDE_UNIT(si, coulomb, "C", si::ampere* si::second)
BEMAN_INSIDE_UNIT(si, volt, "V", si::watt / si::ampere, isq::electric_potential)
BEMAN_INSIDE_UNIT(si, farad, "F", si::coulomb / si::volt, isq::capacitance)
BEMAN_INSIDE_UNIT(si, ohm, "Ω", "ohm", si::volt / si::ampere, isq::impedance)
BEMAN_INSIDE_UNIT(si, siemens, "S", one / si::ohm, isq::admittance)
BEMAN_INSIDE_UNIT(si, weber, "Wb", si::volt* si::second)
BEMAN_INSIDE_UNIT(si, tesla, "T", si::weber / square(si::metre), isq::magnetic_flux_density)
BEMAN_INSIDE_UNIT(si, henry, "H", si::weber / si::ampere)
BEMAN_INSIDE_UNIT(si, degree_Celsius, "℃", "`C", si::kelvin, si::ice_point)
BEMAN_INSIDE_UNIT(si, lumen, "lm", si::candela* si::steradian, isq::luminous_flux)
BEMAN_INSIDE_UNIT(si, lux, "lx", si::lumen / square(si::metre), isq::illuminance)
BEMAN_INSIDE_UNIT(si, becquerel, "Bq", one / si::second, isq::activity)
BEMAN_INSIDE_UNIT(si, gray, "Gy", si::joule / si::kilogram, isq::absorbed_dose)
BEMAN_INSIDE_UNIT(si, sievert, "Sv", si::joule / si::kilogram, isq::dose_equivalent)
BEMAN_INSIDE_UNIT(si, katal, "kat", si::mole / si::second, isq::catalytic_activity)

// Non-SI units accepted for use with the SI.
BEMAN_INSIDE_UNIT(si, minute, "min", mag<60>* si::second)
BEMAN_INSIDE_UNIT(si, hour, "h", mag<60>* si::minute)
BEMAN_INSIDE_UNIT(si, day, "d", mag<24>* si::hour)
BEMAN_INSIDE_UNIT(si, astronomical_unit, "au", mag<149'597'870'700>* si::metre)
BEMAN_INSIDE_UNIT(si, degree, "°", "deg", mag_pi* mag<rational{1, 180}>* si::radian, no_space_before)
BEMAN_INSIDE_UNIT(si, arcminute, "′", "'", mag<rational{1, 60}>* si::degree, no_space_before)
BEMAN_INSIDE_UNIT(si, arcsecond, "″", "''", mag<rational{1, 60}>* si::arcminute, no_space_before)
BEMAN_INSIDE_UNIT(si, are, "a", square(si::deca(si::metre)))
BEMAN_INSIDE_UNIT(si, litre, "L", cubic(si::deci(si::metre)))
BEMAN_INSIDE_UNIT(si, tonne, "t", mag<1000>* si::kilogram)
namespace si {
inline constexpr auto hectare = hecto(are);
}
#if BEMAN_INSIDE_BIG_GRIDS
BEMAN_INSIDE_UNIT(si, dalton, "Da", mag<1.66053906660e-27_g>* si::kilogram)
BEMAN_INSIDE_UNIT(si, electronvolt, "eV", mag<1.602176634e-19_g>* si::joule)
#endif

// International (yard and pound agreement, 1959) and nautical units.
BEMAN_INSIDE_UNIT(international, yard, "yd", mag<rational{9144, 10000}>* si::metre)
BEMAN_INSIDE_UNIT(international, foot, "ft", mag<rational{1, 3}>* international::yard)
BEMAN_INSIDE_UNIT(international, inch, "in", mag<rational{1, 12}>* international::foot)
BEMAN_INSIDE_UNIT(international, mile, "mi", mag<1760>* international::yard)
BEMAN_INSIDE_UNIT(international, nautical_mile, "nmi", mag<1852>* si::metre)
BEMAN_INSIDE_UNIT(international, pound, "lb", mag<rational{45359237, 100000000}>* si::kilogram)
BEMAN_INSIDE_UNIT(international, ounce, "oz", mag<rational{1, 16}>* international::pound)
BEMAN_INSIDE_UNIT(international,
                  pound_force,
                  "lbf",
                  mag<rational{980665, 100000}>* international::pound* si::metre / square(si::second))
BEMAN_INSIDE_UNIT(international, knot, "kn", international::nautical_mile / si::hour)
BEMAN_INSIDE_ORIGIN(international,
                    zeroth_degree_Fahrenheit,
                    dim_thermodynamic_temperature,
                    &si_defs::absolute_zero,
                    rational{45967, 180})
BEMAN_INSIDE_UNIT(international,
                  degree_Fahrenheit,
                  "℉",
                  "`F",
                  mag<rational{5, 9}>* si::kelvin,
                  international::zeroth_degree_Fahrenheit)

#undef BEMAN_INSIDE_UNIT
#undef BEMAN_INSIDE_ORIGIN
#undef BEMAN_INSIDE_SPEC

} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_HPP
