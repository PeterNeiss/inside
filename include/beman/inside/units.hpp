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
// target representation and a rounding flag (units_math.hpp).
//
// Also here:
//   quantity_point, point<U>(v), the origins     ℃ and ℉ points, exact shifts
//   quantity_cast, value_cast, try_value_cast    kinds and representations
//   named_unit, quantity_spec, point_origin,     units, kinds, origins and
//   base_dimension, unit_c / spec_c / origin_c   dimensions of your own
//   q.on_wrap / on_clamp / on_error / on_overflow, ++q, Q::try_make
//   mul_into, div_into, add_all, mul_all, sum<Q>, divmod, parse<Q>,
//   conversion_overflows / rounds / loses<Q>, clamp_cast / wrap_cast<Q>,
//   dot, cross, lerp                             inside's algorithms
//   percent, per_mille                           dimensionless units
//   to_string, operator<<, std::formatter        number, then the symbol
// The catalog: isq, si, si2019, international. Grid numbers are 64-bit under
// C++23; the prefixes past 10^±18 (quecto … quetta), the eV, the Da and the
// SI constants h, e, k and N_A exist only under C++26 big grids
// (BEMAN_INSIDE_BIG_GRIDS). The derived physical constants are in
// constants.hpp.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_UNITS_HPP
#define BEMAN_INSIDE_UNITS_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/predicates.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// The diagnostic of a π conversion asked to be exact (rescale, in()).
#define BEMAN_INSIDE_UNITS_WHY_PI                                                                  \
    "units: this conversion multiplies by a power of pi, which has no exact grid - round it with " \
    "value_cast<unit, Rep>(q) from <beman/inside/units_math.hpp>, Rep declared with a rounding mode"

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
        for (std::size_t i = 0; i < detail::base_dimensions; ++i)
            if (a.Exp[i] != b.Exp[i])
                return false;
        if (a.UserSize != b.UserSize)
            return false;
        for (std::int8_t i = 0; i < a.UserSize; ++i)
            if (a.User[i].D->Name != b.User[i].D->Name || a.User[i].Exp != b.User[i].Exp)
                return false;
        return true;
    }
    friend consteval dimension operator*(dimension a, const dimension& b) {
        for (std::size_t i = 0; i < detail::base_dimensions; ++i)
            a.Exp[i] = exponent(a.Exp[i] + b.Exp[i]);
        for (std::int8_t i = 0; i < b.UserSize; ++i)
            a.add_user(b.User[i].D, b.User[i].Exp);
        return a;
    }
    friend consteval dimension operator/(dimension a, const dimension& b) {
        for (std::size_t i = 0; i < detail::base_dimensions; ++i)
            a.Exp[i] = exponent(a.Exp[i] - b.Exp[i]);
        for (std::int8_t i = 0; i < b.UserSize; ++i)
            a.add_user(b.User[i].D, -b.User[i].Exp);
        return a;
    }
    consteval dimension pow(int n) const {
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
    constexpr void add_user(const base_dimension* b, int e) {
        for (std::int8_t i = 0; i < UserSize; ++i)
            if (User[i].D->Name == b->Name) {
                User[i].Exp = exponent(User[i].Exp + e);
                if (User[i].Exp == 0) {
                    for (std::int8_t j = i; j + 1 < UserSize; ++j)
                        User[j] = User[j + 1];
                    User[--UserSize] = {};
                }
                return;
            }
        if (e == 0)
            return;
        if (UserSize == static_cast<std::int8_t>(detail::max_user_dimensions))
            beman::inside::detail::constexpr_error<"units: a dimension holds at most 4 base dimensions of your own">();
        std::int8_t at = UserSize;
        while (at > 0 && b->Name < User[at - 1].D->Name) {
            User[at] = User[at - 1];
            --at;
        }
        User[at] = {b, exponent(e)};
        ++UserSize;
    }
};

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
        if (a.Size != b.Size)
            return false;
        for (std::int8_t i = 0; i < a.Size; ++i)
            if (!(a.F[i] == b.F[i]))
                return false;
        return true;
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

consteval bool name_before(const quantity_spec* a, const quantity_spec* b) { return a->Name < b->Name; }

// r · f^e, merged and kept sorted; an exponent that reaches 0 drops out.
// Returns false when the recipe is full.
consteval bool recipe_add(recipe& r, const quantity_spec* f, int e) {
    for (std::int8_t i = 0; i < r.Size; ++i)
        if (same(r.F[i].Spec, f)) {
            r.F[i].Exp = dimension::exponent(r.F[i].Exp + e);
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
    r.F[at] = {f, dimension::exponent(e)};
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
        if (a.Size != b.Size || !(a.Scale == b.Scale))
            return false;
        for (std::int8_t i = 0; i < a.Size; ++i)
            if (!(a.Atoms[i] == b.Atoms[i]))
                return false;
        return true;
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

consteval bool atom_before(const unit_atom& a, const unit_atom& b) {
    if (a.U->Utf8 != b.U->Utf8)
        return a.U->Utf8 < b.U->Utf8;
    return a.Prefix < b.Prefix;
}

consteval void unit_add(unit& u, unit_atom a) {
    for (std::int8_t i = 0; i < u.Size; ++i)
        if (same(u.Atoms[i].U, a.U) && u.Atoms[i].Prefix == a.Prefix) {
            u.Atoms[i].Exp = dimension::exponent(u.Atoms[i].Exp + a.Exp);
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
    for (std::int8_t i = 0; i < a.Size; ++i) {
        if (atom_before(a.Atoms[i], b.Atoms[i]))
            return true;
        if (atom_before(b.Atoms[i], a.Atoms[i]))
            return false;
        if (a.Atoms[i].Exp != b.Atoms[i].Exp)
            return a.Atoms[i].Exp < b.Atoms[i].Exp;
    }
    return false;
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
    if (!beman::inside::detail::grid_gcd_fits(ma.Ratio, mb.Ratio))
        beman::inside::detail::constexpr_error<"units: the common unit needs C++26 big grids">();
    const grid_rational g = beman::inside::detail::grid_gcd_of(ma.Ratio, mb.Ratio);
    if (g == ma.Ratio)
        return a;
    if (g == mb.Ratio)
        return b;
    const bool a_smaller = ma.Ratio < mb.Ratio;
    unit       u         = a_smaller ? a : b;
    u.Scale              = u.Scale * magnitude{beman::inside::detail::grid_div_of(g, a_smaller ? ma.Ratio : mb.Ratio)};
    return u;
}

// Whether the factor between two units is a grid number: always with big
// grid numbers; with 64-bit ones au → nm (1.5·10^20) is none.
template <unit From, unit To>
inline constexpr bool factor_fits_v = [] {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    return beman::inside::detail::try_div(magnitude_of(From).Ratio, magnitude_of(To).Ratio).has_value();
#endif
}();

// Whether rescale<M>(v) forms: inside's v · M exists. A pi factor passes, for
// rescale's static_assert to explain.
template <magnitude M, class V>
concept rescale_forms = !exact_factor_v<M> || M.is_one() || requires(const V& v) { v * just<M.Ratio>; };

// Whether a number of V in unit From rescales to unit To.
template <unit From, unit To, class V>
concept rescalable = factor_fits_v<From, To> && rescale_forms<conversion_factor<From, To>, V>;

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
// An exact conversion factor from unit From to unit To: it fits the grid
// numbers and holds no π.
template <unit From, unit To>
inline constexpr bool exact_rescale_v = [] {
    if constexpr (!factor_fits_v<From, To>)
        return false;
    else
        return exact_factor_v<conversion_factor<From, To>>;
}();

template <reference From, reference To>
inline constexpr bool implicit_v =
    exact_rescale_v<From.U, To.U> && dimension_of(From.U) == dimension_of(To.U) && spec_converts(From, To);

// A dimensionless quantity that is a plain number: no is_kind spec (an
// angle), and an exact factor to unit one.
template <reference R>
inline constexpr bool number_like_v = dimension_of(R.U) == dim_one && exact_factor_v<conversion_factor<R.U, unit{}>> &&
                                      (!R.Spec || !kind_root(R.Spec.P)->IsKind);

// Its number's type; void for any other quantity (never forming the
// rescale, whose pi check would fire for an angle in degrees).
template <reference R, class V, bool = number_like_v<R>>
struct number_type {
    using type = void;
};
template <reference R, class V>
struct number_type<R, V, true> {
    using type = decltype(rescale<conversion_factor<R.U, unit{}>>(std::declval<const V&>()));
};
template <reference R, class V>
using number_t = typename number_type<R, V>::type;

template <reference From, reference To, insidable V>
using rescaled_t = decltype(rescale<conversion_factor<From.U, To.U>>(std::declval<const V&>()));

using beman::inside::detail::is_expected_v;

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

// q.on_wrap(f) and the other hooks: the inside's policy_ref on q's number,
// with stores taking quantities (rescaled exactly, as +=) and f called on the
// quantity. The other arguments (carry, overshoot, errc) are the inside's.
template <class F, class Q>
constexpr auto on_quantity(Q& q, F&& f) {
    return [&q, f = std::forward<F>(f)](auto&, auto&&... a) mutable -> decltype(auto) {
        return f(q, std::forward<decltype(a)>(a)...);
    };
}

template <reference R, class Q, class Ref>
struct quantity_action_ref {
    Q&  Self;
    Ref NumRef;

    template <reference R2, class V2>
        requires implicit_v<R2, R> && rescalable<R2.U, R.U, V2> &&
                 requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r = v; }
    constexpr Q& operator=(const quantity<R2, V2>& q) {
        NumRef = rescale<conversion_factor<R2.U, R.U>>(q.numerical_value());
        return Self;
    }
    template <reference R2, class V2>
        requires implicit_v<R2, R> && rescalable<R2.U, R.U, V2> &&
                 requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r += v; }
    constexpr Q& operator+=(const quantity<R2, V2>& q) {
        NumRef += rescale<conversion_factor<R2.U, R.U>>(q.numerical_value());
        return Self;
    }
    template <reference R2, class V2>
        requires implicit_v<R2, R> && rescalable<R2.U, R.U, V2> &&
                 requires(Ref& r, const rescaled_t<R2, R, V2>& v) { r -= v; }
    constexpr Q& operator-=(const quantity<R2, V2>& q) {
        NumRef -= rescale<conversion_factor<R2.U, R.U>>(q.numerical_value());
        return Self;
    }
};
template <reference R, class Q, class Ref>
constexpr auto make_action_ref(Q& q, Ref&& r) {
    return quantity_action_ref<R, Q, std::remove_cvref_t<Ref>>{q, std::forward<Ref>(r)};
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
    // The spec as a value: its tag for a named spec (isq::height), else the
    // spec expression of its recipe. Compare with ==: q.quantity_spec == isq::length.
    static constexpr auto quantity_spec = [] {
        if constexpr (static_cast<bool>(R.Spec))
            return spec_t<R.Spec.P>{};
        else
            return detail::spec_expr{R.Derived, dimension_of(R.U)};
    }();

    constexpr quantity() = default;

    // From a number in the unit R: quantity<si::metre, inside<{0, 100}>>{42}.
    template <class V>
        requires std::constructible_from<Rep, V> && (!quantity_like<V>)
    constexpr explicit quantity(V&& v) : Num(std::forward<V>(v)) {}

    // From another quantity: implicit when the spec converts implicitly and
    // the number, in R's unit, converts implicitly into Rep.
    template <units::reference R2, class V2>
        requires detail::implicit_v<R2, R> && detail::rescalable<R2.U, R.U, V2> &&
                 std::constructible_from<Rep, detail::rescaled_t<R2, R, V2>>
    constexpr explicit(!std::convertible_to<detail::rescaled_t<R2, R, V2>, Rep>) quantity(const quantity<R2, V2>& q)
        : Num(detail::rescale<detail::conversion_factor<R2.U, R.U>>(q.numerical_value())) {}

    [[nodiscard]] constexpr const Rep& numerical_value() const noexcept { return Num; }

    // The same quantity in unit U: exact, the number's grid rescaled.
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::none && detail::rescalable<R.U, U, Rep>)
    [[nodiscard]] constexpr auto in() const {
        constexpr units::reference to{R.Spec, R.KindOf, R.Derived, U};
        return detail::make_quantity<to>(detail::rescale<detail::conversion_factor<R.U, U>>(Num));
    }
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::none && detail::rescalable<R.U, U, Rep>)
    [[nodiscard]] constexpr auto in(units::unit_t<U>) const {
        return in<U>();
    }
    // The number in unit U.
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::none && detail::rescalable<R.U, U, Rep>)
    [[nodiscard]] constexpr auto numerical_value_in(units::unit_t<U>) const {
        return in<U>().numerical_value();
    }
    BEMAN_INSIDE_DELETE_BEGIN
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::dimension)
    void in() const = BEMAN_INSIDE_DELETE("units: in() - the unit has another dimension");
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::dimension)
    void in(units::unit_t<U>) const = BEMAN_INSIDE_DELETE("units: in() - the unit has another dimension");
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::pi)
    void in() const = BEMAN_INSIDE_DELETE(BEMAN_INSIDE_UNITS_WHY_PI);
    template <units::unit U>
        requires(detail::in_error_of<R.U, U>() == detail::in_error::pi)
    void in(units::unit_t<U>) const = BEMAN_INSIDE_DELETE(BEMAN_INSIDE_UNITS_WHY_PI);
    BEMAN_INSIDE_DELETE_END

    // A dimensionless quantity is a number: its value in unit one (km/m is
    // ×1000, % is ×1/100), into any inside that takes it — implicitly when
    // that number converts implicitly. An angle (an is_kind spec) stays a
    // quantity; numerical_value() reads its number.
    template <insidable T>
        requires detail::number_like_v<R> && std::constructible_from<T, detail::number_t<R, Rep>>
    constexpr explicit(!std::convertible_to<detail::number_t<R, Rep>, T>) operator T() const {
        return T(detail::rescale<detail::conversion_factor<R.U, units::unit{}>>(Num));
    }
    BEMAN_INSIDE_DELETE_BEGIN
    template <insidable T>
        requires(dimension_of(R.U) == dim_one && !detail::number_like_v<R>)
    operator T() const = BEMAN_INSIDE_DELETE("units: an angle (an is_kind quantity) or a pi-scaled ratio is no plain "
                                             "number - read numerical_value(), or value_cast it first");
    BEMAN_INSIDE_DELETE_END

    [[nodiscard]] constexpr auto operator+() const { return detail::make_quantity<R>(+Num); }
    [[nodiscard]] constexpr auto operator-() const { return detail::make_quantity<R>(-Num); }

    // Compound assignment keeps the type: the number is stored back under Rep's
    // policy (checked by default).
    template <units::reference R2, class V2>
        requires detail::implicit_v<R2, R> && detail::rescalable<R2.U, R.U, V2> &&
                 requires(Rep& n, const detail::rescaled_t<R2, R, V2>& r) { n += r; }
    constexpr quantity& operator+=(const quantity<R2, V2>& q) {
        Num += detail::rescale<detail::conversion_factor<R2.U, R.U>>(q.numerical_value());
        return *this;
    }
    template <units::reference R2, class V2>
        requires detail::implicit_v<R2, R> && detail::rescalable<R2.U, R.U, V2> &&
                 requires(Rep& n, const detail::rescaled_t<R2, R, V2>& r) { n -= r; }
    constexpr quantity& operator-=(const quantity<R2, V2>& q) {
        Num -= detail::rescale<detail::conversion_factor<R2.U, R.U>>(q.numerical_value());
        return *this;
    }
    template <insidable V>
        requires requires(Rep& n, const V& v) { n *= v; }
    constexpr quantity& operator*=(const V& v) {
        Num *= v;
        return *this;
    }
    template <insidable V>
        requires requires(Rep& n, const V& v) { n /= v; }
    constexpr quantity& operator/=(const V& v) {
        Num /= v;
        return *this;
    }

    // One unit up or down: ++q is q += 1·unit, under Rep's policy.
    constexpr quantity& operator++()
        requires requires(Rep& n) { ++n; }
    {
        ++Num;
        return *this;
    }
    constexpr quantity& operator--()
        requires requires(Rep& n) { --n; }
    {
        --Num;
        return *this;
    }
    constexpr quantity operator++(int)
        requires requires(Rep& n) { ++n; }
    {
        auto old = *this;
        ++Num;
        return old;
    }
    constexpr quantity operator--(int)
        requires requires(Rep& n) { --n; }
    {
        auto old = *this;
        --Num;
        return old;
    }

    // A store with a hook, as on inside: q.on_wrap(f) += d calls f(q, carry)
    // when the sum wraps. Stores take quantities; f gets the quantity.
    template <class F>
    [[nodiscard]] constexpr auto on_wrap(F&& f) {
        return detail::make_action_ref<R>(*this, Num.on_wrap(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_clamp(F&& f) {
        return detail::make_action_ref<R>(*this, Num.on_clamp(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_error(F&& f) {
        return detail::make_action_ref<R>(*this, Num.on_error(detail::on_quantity(*this, std::forward<F>(f))));
    }
    template <class F>
    [[nodiscard]] constexpr auto on_overflow(F&& f) {
        return detail::make_action_ref<R>(*this, Num.on_overflow(detail::on_quantity(*this, std::forward<F>(f))));
    }

    // Rep's try_make in the unit R: quantity<si::metre, M>::try_make(12.5).
    template <policy_flag F = none, class V>
        requires requires(const V& v) { Rep::template try_make<F>(v); }
    [[nodiscard]] static constexpr auto try_make(const V& v) {
        return detail::make_quantity<R>(Rep::template try_make<F>(v));
    }
};

// A quantity chains through inside's expected-lift operators: a division by
// a grid holding 0 and the rest of the expression stay one std::expected.
} // namespace beman::inside::units
namespace beman::inside::detail {
template <units::reference R, class Rep>
inline constexpr bool lift_operand_v<units::quantity<R, Rep>> = true;
} // namespace beman::inside::detail
namespace beman::inside::units {

//---------------------------------------------------------------------------
// Construction: number * unit, number * spec[unit]. A raw scalar has no grid.
//---------------------------------------------------------------------------
template <insidable V, class Tag>
    requires is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const V& v, Tag t) {
    return quantity<reference_of(t), V>{v};
}
BEMAN_INSIDE_DELETE_BEGIN
template <class N, class Tag>
    requires is_reference_tag_v<Tag> && std::is_arithmetic_v<N>
void operator*(N, Tag) =
    BEMAN_INSIDE_DELETE("units: a raw number has no grid - write just<42> * unit, or quantity<unit, inside<…>>{42}");
BEMAN_INSIDE_DELETE_END
// number / unit: 5 / s.
template <insidable V, unit U>
[[nodiscard]] constexpr auto operator/(const V& v, unit_t<U>) {
    return quantity<reference_of(detail::unit_pow(U, -1)), V>{v};
}

// A magnitude scales a quantity's unit, not its number: mag_pi * q is q
// in [π·unit], exact and free (π is never rounded; value_cast to a plain
// unit rounds it once, units_math.hpp). mag<2> * q, q / mag_pi likewise.
namespace detail {
consteval reference scaled_reference(reference r, const magnitude& m) {
    r.U = unit_scaled(r.U, m);
    return r;
}
} // namespace detail
template <magnitude M, reference R, class V>
[[nodiscard]] constexpr auto operator*(mag_t<M>, const quantity<R, V>& q) {
    return quantity<detail::scaled_reference(R, M), V>{q.numerical_value()};
}
template <reference R, class V, magnitude M>
[[nodiscard]] constexpr auto operator*(const quantity<R, V>& q, mag_t<M>) {
    return quantity<detail::scaled_reference(R, M), V>{q.numerical_value()};
}
// A magnitude times a number: the number in the scaled unit one ([π]), so
// mag_pi * just<2> * si::second is 2 in [π s].
template <magnitude M, insidable V>
[[nodiscard]] constexpr auto operator*(mag_t<M>, const V& v) {
    return quantity<reference_of(detail::unit_scaled(unit{}, M)), V>{v};
}
template <insidable V, magnitude M>
[[nodiscard]] constexpr auto operator*(const V& v, mag_t<M>) {
    return quantity<reference_of(detail::unit_scaled(unit{}, M)), V>{v};
}
template <reference R, class V, magnitude M>
[[nodiscard]] constexpr auto operator/(const quantity<R, V>& q, mag_t<M>) {
    return quantity<detail::scaled_reference(R, magnitude{} / M), V>{q.numerical_value()};
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

// Why a sum or comparison of A and B is a unit error, if it is: other
// dimensions or kinds, or no common unit. Each reason is a deleted overload
// with its message.
enum class sum_error : unsigned char { none, dimensions, kinds, pi, big_grids };

template <reference A, reference B>
consteval sum_error sum_error_of() {
    if (dimension_of(A.U) != dimension_of(B.U))
        return sum_error::dimensions;
    if (!common_spec(A, B).Ok)
        return sum_error::kinds;
    const magnitude ma = magnitude_of(A.U), mb = magnitude_of(B.U);
    if (ma.Pi != mb.Pi)
        return sum_error::pi;
    if (!beman::inside::detail::grid_gcd_fits(ma.Ratio, mb.Ratio))
        return sum_error::big_grids;
    return sum_error::none;
}
template <reference A, reference B>
consteval bool sum_diagnosed() {
    return sum_error_of<A, B>() != sum_error::none;
}

// Whether a Op b exists in the common unit: both numbers rescale to it and
// inside's Op takes them.
template <reference A, class VA, reference B, class VB>
concept common_rescalable =
    rescalable<A.U, sum_reference<A, B>().U, VA> && rescalable<B.U, sum_reference<A, B>().U, VB>;
// The number of a quantity of A on V, in A and B's common unit.
template <reference A, class V, reference B>
using common_number_t = rescaled_t<A, sum_reference<A, B>(), V>;

template <reference A, class VA, reference B, class VB, char Op>
concept summable = (!sum_diagnosed<A, B>()) && common_rescalable<A, VA, B, VB> &&
                   number_op<common_number_t<A, VA, B>, common_number_t<B, VB, A>, Op>;

template <reference A, reference B, int Sign>
consteval reference product_reference() {
    const unit u = unit_mul(A.U, B.U, Sign);
    return spec_product(A, B, Sign, u);
}
} // namespace detail

template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '+'>
[[nodiscard]] constexpr auto operator+(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) +
                                    detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value()));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '-'>
[[nodiscard]] constexpr auto operator-(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) -
                                    detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value()));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '%'>
[[nodiscard]] constexpr auto operator%(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::make_quantity<C>(detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) %
                                    detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value()));
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '='>
[[nodiscard]] constexpr bool operator==(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) ==
           detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value());
}
template <reference A, class VA, reference B, class VB>
    requires detail::summable<A, VA, B, VB, '<'>
[[nodiscard]] constexpr auto operator<=>(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()) <=>
           detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value());
}

// A sum, difference, remainder or comparison of two units that cannot meet:
// a deleted overload per reason, so a requires-test is false and a call says
// why.
#define BEMAN_INSIDE_UNITS_SUM_DELETED(ret, op)                                                                \
    template <reference A, class VA, reference B, class VB>                                                    \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::dimensions)                                \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                                                   \
        BEMAN_INSIDE_DELETE("units: adding or comparing quantities of different dimensions");                  \
    template <reference A, class VA, reference B, class VB>                                                    \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::kinds)                                     \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                                                   \
        BEMAN_INSIDE_DELETE("units: adding or comparing quantities of different kinds");                       \
    template <reference A, class VA, reference B, class VB>                                                    \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::pi)                                        \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) = BEMAN_INSIDE_DELETE(                              \
        "units: no common unit of a pi multiple and a rational one - convert one side with value_cast first"); \
    template <reference A, class VA, reference B, class VB>                                                    \
        requires(detail::sum_error_of<A, B>() == detail::sum_error::big_grids)                                 \
    ret op(const quantity<A, VA>&, const quantity<B, VB>&) =                                                   \
        BEMAN_INSIDE_DELETE("units: the common unit needs C++26 big grids");
BEMAN_INSIDE_DELETE_BEGIN
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator+)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator-)
BEMAN_INSIDE_UNITS_SUM_DELETED(void, operator%)
BEMAN_INSIDE_UNITS_SUM_DELETED(bool, operator==)
BEMAN_INSIDE_UNITS_SUM_DELETED(std::strong_ordering, operator<=>)
#undef BEMAN_INSIDE_UNITS_SUM_DELETED
BEMAN_INSIDE_DELETE_END

template <reference A, class VA, reference B, class VB>
    requires detail::number_op<VA, VB, '*'>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, 1>()>(a.numerical_value() * b.numerical_value());
}
template <reference A, class VA, reference B, class VB>
    requires detail::number_op<VA, VB, '/'>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::make_quantity<detail::product_reference<A, B, -1>()>(a.numerical_value() / b.numerical_value());
}

// A quantity scaled by a dimensionless number (an inside).
template <reference A, class VA, insidable V>
    requires detail::number_op<VA, V, '*'>
[[nodiscard]] constexpr auto operator*(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(a.numerical_value() * v);
}
template <insidable V, reference A, class VA>
    requires detail::number_op<V, VA, '*'>
[[nodiscard]] constexpr auto operator*(const V& v, const quantity<A, VA>& a) {
    return detail::make_quantity<A>(v * a.numerical_value());
}
template <reference A, class VA, insidable V>
    requires detail::number_op<VA, V, '/'>
[[nodiscard]] constexpr auto operator/(const quantity<A, VA>& a, const V& v) {
    return detail::make_quantity<A>(a.numerical_value() / v);
}
template <insidable V, reference A, class VA>
    requires detail::number_op<V, VA, '/'>
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
BEMAN_INSIDE_DELETE_BEGIN
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator*(const quantity<A, VA>&,
               N) = BEMAN_INSIDE_DELETE("units: a raw number has no grid - scale by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator*(N, const quantity<A, VA>&) =
    BEMAN_INSIDE_DELETE("units: a raw number has no grid - scale by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator/(const quantity<A, VA>&,
               N) = BEMAN_INSIDE_DELETE("units: a raw number has no grid - divide by just<2>, not 2");
template <reference A, class VA, class N>
    requires std::is_arithmetic_v<N>
void operator/(N, const quantity<A, VA>&) =
    BEMAN_INSIDE_DELETE("units: a raw number has no grid - divide by just<2>, not 2");
BEMAN_INSIDE_DELETE_END

// pow<N>(q) — q·q·…·q (N ≥ 1): exact, the grid of each product.
template <int N, reference R, class V>
    requires(N == 1 || (N % 2 == 1 && requires(const quantity<R, V>& q) { q * pow<N - 1>(q); }) ||
             (N >= 2 && N % 2 == 0 && requires(const quantity<R, V>& q) { pow<N / 2>(q) * pow<N / 2>(q); }))
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
namespace detail {
// Why quantity_cast<To> cannot take R, if it cannot.
enum class cast_error : unsigned char { none, dimension, is_kind };
template <reference R, const quantity_spec* To>
consteval cast_error cast_error_of() {
    if (!(To->Dim == dimension_of(R.U)))
        return cast_error::dimension;
    return spec_casts(R, To) ? cast_error::none : cast_error::is_kind;
}
} // namespace detail

template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::none)
[[nodiscard]] constexpr auto quantity_cast(const quantity<R, V>& q) {
    return quantity<reference{SpecTag::value, false, {}, R.U}, V>{q.numerical_value()};
}
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::none)
[[nodiscard]] constexpr auto quantity_cast(const quantity<R, V>& q) {
    return quantity_cast<decltype(Spec)>(q);
}
BEMAN_INSIDE_DELETE_BEGIN
template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::dimension)
void quantity_cast(const quantity<R, V>&) = BEMAN_INSIDE_DELETE("units: quantity_cast - another dimension");
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::dimension)
void quantity_cast(const quantity<R, V>&) = BEMAN_INSIDE_DELETE("units: quantity_cast - another dimension");
template <class SpecTag, reference R, class V>
    requires(detail::cast_error_of<R, SpecTag::value>() == detail::cast_error::is_kind)
void quantity_cast(const quantity<R, V>&) = BEMAN_INSIDE_DELETE("units: quantity_cast - across an is_kind boundary");
template <auto Spec, reference R, class V>
    requires(detail::cast_error_of<R, decltype(Spec)::value>() == detail::cast_error::is_kind)
void quantity_cast(const quantity<R, V>&) = BEMAN_INSIDE_DELETE("units: quantity_cast - across an is_kind boundary");
BEMAN_INSIDE_DELETE_END

// value_cast<Rep2>(q) — another representation, the same unit: Rep2's
// constructor (checked unless Rep2's policy says otherwise).
template <insidable Rep2, reference R, class V>
    requires std::constructible_from<Rep2, const V&>
[[nodiscard]] constexpr quantity<R, Rep2> value_cast(const quantity<R, V>& q) {
    return quantity<R, Rep2>{Rep2(q.numerical_value())};
}
// value_cast<U, Rep2>(q) — in unit U with representation Rep2: the exact
// value in U, stored by Rep2's constructor. (A pi factor, degree ↔ radian,
// rounds once: units_math.hpp.)
template <auto U, insidable Rep2, reference R, class V>
    requires is_unit_tag_v<decltype(U)> && detail::exact_rescale_v<R.U, decltype(U)::value> &&
             requires(const quantity<R, V>& q) { Rep2(q.in(U).numerical_value()); }
[[nodiscard]] constexpr auto value_cast(const quantity<R, V>& q) {
    constexpr units::unit u = decltype(U)::value;
    static_assert(dimension_of(u) == dimension_of(R.U), "units: value_cast - the unit has another dimension");
    constexpr reference to{R.Spec, R.KindOf, R.Derived, u};
    return quantity<to, Rep2>{Rep2(q.in(U).numerical_value())};
}

// value_cast<Q>(q) — into the quantity type Q = quantity<R2, Rep2>: q in
// R2's unit, stored by Rep2's constructor. Q's spec must take q's, as for an
// implicit conversion; only the representation may round.
namespace detail {
template <class Q, reference R>
concept value_castable_to = quantity_like<Q> && dimension_of(Q::reference.U) == dimension_of(R.U) &&
                            spec_converts(R, Q::reference) && exact_rescale_v<R.U, Q::reference.U>;
}
template <class Q, reference R, class V>
    requires detail::value_castable_to<Q, R> && requires(const quantity<R, V>& q) {
        static_cast<typename Q::rep>(q.template in<Q::reference.U>().numerical_value());
    }
[[nodiscard]] constexpr Q value_cast(const quantity<R, V>& q) {
    return Q{static_cast<typename Q::rep>(q.template in<Q::reference.U>().numerical_value())};
}
// try_value_cast<Q, F>(q) — the same through Q's Rep2::try_make<F>.
template <class Q, policy_flag F = none, reference R, class V>
    requires detail::value_castable_to<Q, R> && requires(const quantity<R, V>& q) {
        Q::rep::template try_make<F>(q.template in<Q::reference.U>().numerical_value());
    }
[[nodiscard]] constexpr std::expected<Q, errc> try_value_cast(const quantity<R, V>& q) {
    auto v = Q::rep::template try_make<F>(q.template in<Q::reference.U>().numerical_value());
    if (!v)
        return std::unexpected{v.error()};
    return Q{*v};
}

// try_value_cast<Rep2, F>(q) — as Rep2::try_make<F>: an expected.
template <insidable Rep2, policy_flag F = none, reference R, class V>
    requires requires(const V& v) { Rep2::template try_make<F>(v); }
[[nodiscard]] constexpr std::expected<quantity<R, Rep2>, errc> try_value_cast(const quantity<R, V>& q) {
    auto v = Rep2::template try_make<F>(q.numerical_value());
    if (!v)
        return std::unexpected{v.error()};
    return quantity<R, Rep2>{*v};
}

// try_value_cast<U, Rep2, F>(q) — in unit U, as Rep2::try_make<F>.
template <auto U, insidable Rep2, policy_flag F = none, reference R, class V>
    requires is_unit_tag_v<decltype(U)> && detail::exact_rescale_v<R.U, decltype(U)::value> &&
             requires(const quantity<R, V>& q) { Rep2::template try_make<F>(q.in(U).numerical_value()); }
[[nodiscard]] constexpr auto try_value_cast(const quantity<R, V>& q) {
    constexpr reference to{R.Spec, R.KindOf, R.Derived, decltype(U)::value};
    using Q = quantity<to, Rep2>;
    auto v  = Rep2::template try_make<F>(q.in(U).numerical_value());
    if (!v)
        return std::expected<Q, errc>{std::unexpect, v.error()};
    return std::expected<Q, errc>{Q{*v}};
}

//---------------------------------------------------------------------------
// inside's algorithms on quantities: the number goes through the inside
// function in the target's unit, so each still rounds once and checks once.
//---------------------------------------------------------------------------
namespace detail {
// A plain inside is a quantity of unit one.
template <class T>
constexpr auto as_quantity(const T& x) {
    if constexpr (quantity_like<T>)
        return x;
    else
        return quantity<reference_of(one), T>{x};
}
template <class T>
using as_quantity_t = decltype(as_quantity(std::declval<const T&>()));

// The reference an Out takes: a quantity's, or unit one for a plain inside.
template <class Out>
constexpr reference out_reference() {
    if constexpr (quantity_like<Out>)
        return Out::reference;
    else
        return reference_of(one);
}
template <class Out>
struct out_rep {
    using type = Out;
};
template <class Out>
    requires quantity_like<Out>
struct out_rep<Out> {
    using type = typename Out::rep;
};

// Out from its number (or an expected of one).
template <class Out, class V>
constexpr auto wrap_out(V&& v) {
    if constexpr (!quantity_like<Out>)
        return std::forward<V>(v);
    else if constexpr (is_expected_v<std::remove_cvref_t<V>>) {
        if (!v)
            return std::expected<Out, errc>{std::unexpect, v.error()};
        return std::expected<Out, errc>{Out{*std::forward<V>(v)}};
    } else
        return Out{std::forward<V>(v)};
}

// a·b (E = 1) or a/b (E = −1) stored into Out: the product's reference must
// convert to Out's, implicitly; a plain Out takes a dimensionless result.
template <class Out, int E, class A, class B>
concept into_operands =
    (quantity_like<A> || quantity_like<B> || quantity_like<Out>) &&
    requires {
        requires implicit_v<product_reference<as_quantity_t<A>::reference, as_quantity_t<B>::reference, E>(),
                            out_reference<Out>()>;
    } &&
    rescalable<product_reference<as_quantity_t<A>::reference, as_quantity_t<B>::reference, E>().U,
               out_reference<Out>().U,
               typename as_quantity_t<A>::rep>;
template <class Out, int E, class A>
constexpr auto into_scaled(const A& a, const auto& b) {
    constexpr reference P =
        product_reference<as_quantity_t<A>::reference, std::remove_cvref_t<decltype(as_quantity(b))>::reference, E>();
    return rescale<conversion_factor<P.U, out_reference<Out>().U>>(as_quantity(a).numerical_value());
}
} // namespace detail

// mul_into<Out, F>(a, b) / div_into<Out, F>(a, b): a·b or a/b rounded once
// into Out, a quantity type (or a plain inside for a dimensionless result).
// The unit factor is folded into the exact product: mul_into<usd>(eth, usd/eth).
template <class Out, policy_flag F = none, class A, class B>
    requires detail::into_operands<Out, 1, A, B> && requires(const A& a, const B& b) {
        beman::inside::mul_into<typename detail::out_rep<Out>::type, F>(detail::into_scaled<Out, 1>(a, b),
                                                                        detail::as_quantity(b).numerical_value());
    }
[[nodiscard]] constexpr auto mul_into(const A& a, const B& b) {
    return detail::wrap_out<Out>(beman::inside::mul_into<typename detail::out_rep<Out>::type, F>(
        detail::into_scaled<Out, 1>(a, b), detail::as_quantity(b).numerical_value()));
}
template <class Out, policy_flag F = none, class A, class B>
    requires detail::into_operands<Out, -1, A, B> && requires(const A& a, const B& b) {
        beman::inside::div_into<typename detail::out_rep<Out>::type, F>(detail::into_scaled<Out, -1>(a, b),
                                                                        detail::as_quantity(b).numerical_value());
    }
[[nodiscard]] constexpr auto div_into(const A& a, const B& b) {
    return detail::wrap_out<Out>(beman::inside::div_into<typename detail::out_rep<Out>::type, F>(
        detail::into_scaled<Out, -1>(a, b), detail::as_quantity(b).numerical_value()));
}

// add_all / mul_all with at least one quantity: the same folds as on inside.
template <class First, class... Rest>
    requires(quantity_like<First> || ... || quantity_like<Rest>)
[[nodiscard]] constexpr auto add_all(const First& first, const Rest&... rest) {
    return (first + ... + rest);
}
template <class First, class... Rest>
    requires(quantity_like<First> || ... || quantity_like<Rest>)
[[nodiscard]] constexpr auto mul_all(const First& first, const Rest&... rest) {
    return (first * ... * rest);
}

// sum<Q>(r): the quantities of r in Q's unit, added by inside's sum with one
// range check.
template <class Q, std::ranges::input_range Rng>
    requires quantity_like<Q> && quantity_like<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>> &&
             detail::implicit_v<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>::reference, Q::reference> &&
             detail::exact_rescale_v<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>::reference.U,
                                     Q::reference.U>
[[nodiscard]] constexpr Q sum(Rng&& r) {
    return Q{beman::inside::sum<typename Q::rep>(std::forward<Rng>(r) | std::views::transform([](const auto& q) {
                                                     return q.template in<Q::reference.U>().numerical_value();
                                                 }))};
}

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

// conversion_overflows<Q>(v) / conversion_rounds / conversion_loses: v, a
// number in Q's unit, against Q's grid.
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_overflows(A value) noexcept {
    return beman::inside::conversion_overflows<typename Q::rep>(value);
}
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_rounds(A value) noexcept {
    return beman::inside::conversion_rounds<typename Q::rep>(value);
}
template <class Q, numeric A>
    requires quantity_like<Q>
[[nodiscard]] constexpr bool conversion_loses(A value) noexcept {
    return beman::inside::conversion_loses<typename Q::rep>(value);
}

// clamp_cast<Q, Round>(x) / wrap_cast<Q>(x): x a number in Q's unit, or a
// quantity converted to it exactly first.
namespace detail {
template <class Q, class X>
constexpr decltype(auto) number_in(const X& x) {
    if constexpr (quantity_like<X>) {
        // A copy: numerical_value() refers into the converted temporary.
        auto v = x.template in<Q::reference.U>().numerical_value();
        return v;
    } else
        return (x);
}
template <class Q, class X>
concept castable_quantity =
    quantity_like<X> && implicit_v<X::reference, Q::reference> && exact_rescale_v<X::reference.U, Q::reference.U>;
template <class Q, class X>
concept castable_from = quantity_like<Q> && (castable_quantity<Q, X> || (!quantity_like<X> && numeric<X>));
} // namespace detail
template <class Q, policy_flag Round = none, class X>
    requires detail::castable_from<Q, X> &&
             requires(const X& x) { beman::inside::clamp_cast<typename Q::rep, Round>(detail::number_in<Q>(x)); }
[[nodiscard]] constexpr Q clamp_cast(const X& x) {
    return Q{beman::inside::clamp_cast<typename Q::rep, Round>(detail::number_in<Q>(x))};
}
template <class Q, class X>
    requires detail::castable_from<Q, X> &&
             requires(const X& x) { beman::inside::wrap_cast<typename Q::rep>(detail::number_in<Q>(x)); }
[[nodiscard]] constexpr Q wrap_cast(const X& x) {
    return Q{beman::inside::wrap_cast<typename Q::rep>(detail::number_in<Q>(x))};
}

// dot, cross, lerp on quantity components (lerp's t is a plain number).
template <quantity_like AX, quantity_like AY, quantity_like BX, quantity_like BY>
[[nodiscard]] constexpr auto dot(const AX& ax, const AY& ay, const BX& bx, const BY& by)
    -> decltype(ax * bx + ay * by) {
    return ax * bx + ay * by;
}
template <quantity_like AX, quantity_like AY, quantity_like BX, quantity_like BY>
[[nodiscard]] constexpr auto cross(const AX& ax, const AY& ay, const BX& bx, const BY& by)
    -> decltype(ax * by - ay * bx) {
    return ax * by - ay * bx;
}
template <quantity_like A, quantity_like B, insidable T>
[[nodiscard]] constexpr auto lerp(const A& a, const B& b, const T& t) -> decltype(a + (b - a) * t) {
    return a + (b - a) * t;
}

// divmod(a, b, args...) of two quantities of one dimension, in their common
// unit: the quotient is a plain number, the remainder a quantity.
template <class Q, class R>
struct divmod_result {
    Q Quotient;
    R Remainder;
};
namespace detail {
template <reference C, class D>
constexpr auto quantity_divmod(D&& d) {
    using T = std::remove_cvref_t<D>;
    if constexpr (is_expected_v<T>) {
        using V      = typename T::value_type;
        using result = divmod_result<decltype(d->Quotient), quantity<C, decltype(d->Remainder)>>;
        if (!d)
            return std::expected<result, errc>{std::unexpect, d.error()};
        return std::expected<result, errc>{result{d->Quotient, quantity<C, decltype(V::Remainder)>{d->Remainder}}};
    } else
        return divmod_result<decltype(d.Quotient), quantity<C, decltype(d.Remainder)>>{
            d.Quotient, quantity<C, decltype(d.Remainder)>{d.Remainder}};
}
} // namespace detail
template <reference A, class VA, reference B, class VB, class... Args>
    requires(!detail::sum_diagnosed<A, B>()) && detail::common_rescalable<A, VA, B, VB> &&
            requires(const detail::common_number_t<A, VA, B>& x,
                     const detail::common_number_t<B, VB, A>& y,
                     Args&&... args) { beman::inside::divmod(x, y, std::forward<Args>(args)...); }
[[nodiscard]] constexpr auto divmod(const quantity<A, VA>& a, const quantity<B, VB>& b, Args&&... args) {
    constexpr reference C = detail::sum_reference<A, B>();
    return detail::quantity_divmod<C>(
        beman::inside::divmod(detail::rescale<detail::conversion_factor<A.U, C.U>>(a.numerical_value()),
                              detail::rescale<detail::conversion_factor<B.U, C.U>>(b.numerical_value()),
                              std::forward<Args>(args)...));
}

//---------------------------------------------------------------------------
// quantity_point<R, O, Rep> — a point: a quantity of R counted from origin O
// (null: the implicit zero of the dimension). A change of origin shifts the
// grid by the exact offset, a change of unit rescales it.
//   auto t = point<si::degree_Celsius>(inside<{-40, 60}>{20});   // 20 ℃
//   t.quantity_from(si::absolute_zero)                         // 293.15 K… in ℃
//   t.point_for(si::absolute_zero).in(si::kelvin)              // the point 293.15 K
//---------------------------------------------------------------------------
template <reference R, detail::origin_ref O, insidable Rep>
class quantity_point;

template <class T>
inline constexpr bool is_quantity_point_v = false;
template <reference R, detail::origin_ref O, class Rep>
inline constexpr bool is_quantity_point_v<quantity_point<R, O, Rep>> = true;

namespace detail {
// The number, in unit U, that takes a quantity from origin From to origin To:
// (offset(From) − offset(To)) / mag(U).
template <origin_ref From, origin_ref To, unit U>
consteval grid_rational origin_shift() {
    static_assert(same(absolute_of(From), absolute_of(To)), "units: points of different absolute origins");
    constexpr magnitude m = magnitude_of(U);
    static_assert(m.Pi == 0, "units: an origin shift in a unit with a pi magnitude has no exact grid");
    return beman::inside::detail::grid_div_of(beman::inside::detail::grid_sub(offset_of(From), offset_of(To)),
                                              m.Ratio);
}

// Whether shift<C> forms on V: inside's v + C exists.
template <grid_rational C, class V>
concept shift_forms = C == grid_rational{0} || requires(const V& v) { v + just<C>; };

template <grid_rational C, insidable V>
constexpr auto shift(const V& v) {
    if constexpr (C == grid_rational{0})
        return v;
    else
        return v + just<C>;
}

// The default origin of a unit's points: its named unit's (K, ℃, ℉).
consteval origin_ref origin_of_unit(const unit& u) { return is_single_named(u) ? u.Atoms[0].U->Origin : origin_ref{}; }

template <origin_ref O, class Q>
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

template <reference R, detail::origin_ref O, insidable Rep>
class quantity_point {
    quantity<R, Rep> Q;

  public:
    using quantity_type                           = quantity<R, Rep>;
    using rep                                     = Rep;
    static constexpr units::reference   reference = R;
    static constexpr detail::origin_ref origin    = O;

    constexpr quantity_point() = default;
    constexpr explicit quantity_point(const quantity<R, Rep>& q) : Q{q} {}

    // From a point of another origin (of the same absolute one) or unit:
    // shifted, then rescaled, exactly; implicit as the quantities are.
    template <units::reference R2, detail::origin_ref O2, class V2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O))) && detail::implicit_v<R2, R> &&
                std::constructible_from<
                    quantity<R, Rep>,
                    decltype(std::declval<const quantity_point<R2, O2, V2>&>().quantity_from(origin_t<O>{}))>
    constexpr explicit(
        !std::convertible_to<decltype(std::declval<const quantity_point<R2, O2, V2>&>().quantity_from(origin_t<O>{})),
                             quantity<R, Rep>>) quantity_point(const quantity_point<R2, O2, V2>& p)
        : Q(p.quantity_from(origin_t<O>{})) {}

    // The quantity from this point's origin, or from another one.
    [[nodiscard]] constexpr const quantity<R, Rep>& quantity_from_origin() const noexcept { return Q; }
    template <detail::origin_ref O2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O)) && magnitude_of(R.U).Pi == 0 &&
                 detail::shift_forms<detail::origin_shift<O, O2, R.U>(), Rep>)
    [[nodiscard]] constexpr auto quantity_from(origin_t<O2>) const {
        constexpr grid_rational c = detail::origin_shift<O, O2, R.U>();
        return quantity<R, std::remove_cvref_t<decltype(detail::shift<c>(Q.numerical_value()))>>{
            detail::shift<c>(Q.numerical_value())};
    }
    BEMAN_INSIDE_DELETE_BEGIN
    template <detail::origin_ref O2>
        requires(!detail::same(detail::absolute_of(O2), detail::absolute_of(O)))
    void quantity_from(origin_t<O2>) const = BEMAN_INSIDE_DELETE("units: points of different absolute origins");
    template <detail::origin_ref O2>
        requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O)) && magnitude_of(R.U).Pi != 0)
    void quantity_from(origin_t<O2>) const =
        BEMAN_INSIDE_DELETE("units: an origin shift in a unit with a pi magnitude has no exact grid");
    BEMAN_INSIDE_DELETE_END
    // The quantity from the absolute origin of the dimension (absolute zero,
    // or the implicit zero): 20 ℃ → 293.15 K counted in ℃.
    [[nodiscard]] constexpr auto quantity_from_zero() const
        requires requires(const quantity_point& p) { p.quantity_from(origin_t<detail::absolute_of(O)>{}); }
    {
        return quantity_from(origin_t<detail::absolute_of(O)>{});
    }
    // The same point counted from another origin.
    template <detail::origin_ref O2>
        requires requires(const quantity_point& p, origin_t<O2> o) { p.quantity_from(o); }
    [[nodiscard]] constexpr auto point_for(origin_t<O2> o) const {
        return detail::make_point<O2>(quantity_from(o));
    }
    // The same point in another unit.
    template <units::unit U>
        requires requires(const quantity<R, Rep>& q, unit_t<U> u) { q.in(u); }
    [[nodiscard]] constexpr auto in(unit_t<U> u) const {
        return detail::make_point<O>(Q.in(u));
    }
    // The same point in unit U, counted from U's own origin: 350 ℉ → 530/3 ℃.
    template <units::unit U>
        requires requires(const quantity_point& p, unit_t<U> u) {
            p.point_for(origin_t<detail::origin_of_unit(U)>{}).in(u);
        }
    [[nodiscard]] constexpr auto point_in(unit_t<U> u) const {
        return point_for(origin_t<detail::origin_of_unit(U)>{}).in(u);
    }

    template <units::reference R2, class V2>
        requires requires(quantity<R, Rep>& a, const quantity<R2, V2>& b) { a += b; }
    constexpr quantity_point& operator+=(const quantity<R2, V2>& q) {
        Q += q;
        return *this;
    }
    template <units::reference R2, class V2>
        requires requires(quantity<R, Rep>& a, const quantity<R2, V2>& b) { a -= b; }
    constexpr quantity_point& operator-=(const quantity<R2, V2>& q) {
        Q -= q;
        return *this;
    }
};

// value_cast on a point: its quantity from the origin cast, the origin kept.
template <insidable Rep2, reference R, detail::origin_ref O, class V>
    requires std::constructible_from<Rep2, const V&>
[[nodiscard]] constexpr quantity_point<R, O, Rep2> value_cast(const quantity_point<R, O, V>& p) {
    return quantity_point<R, O, Rep2>{value_cast<Rep2>(p.quantity_from_origin())};
}
template <auto U, insidable Rep2, reference R, detail::origin_ref O, class V>
    requires requires(const quantity<R, V>& q) { value_cast<U, Rep2>(q); }
[[nodiscard]] constexpr auto value_cast(const quantity_point<R, O, V>& p) {
    const auto q = value_cast<U, Rep2>(p.quantity_from_origin());
    return quantity_point<std::remove_cvref_t<decltype(q)>::reference, O, Rep2>{q};
}

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
template <detail::origin_ref O, reference R, class V>
[[nodiscard]] constexpr auto operator+(origin_t<O>, const quantity<R, V>& q) {
    return quantity_point<R, O, V>{q};
}
template <reference R, class V, detail::origin_ref O>
[[nodiscard]] constexpr auto operator+(const quantity<R, V>& q, origin_t<O>) {
    return quantity_point<R, O, V>{q};
}

// point ± quantity: a point at the same origin, in the point's unit when the
// quantity converts to it exactly (5 K onto a ℃ point stays ℃), else in the
// common unit; point − point: a quantity (the second counted from the first's
// origin).
namespace detail {
template <unit U, reference R2, class V2>
constexpr auto in_point_unit(const quantity<R2, V2>& q) {
    if constexpr (requires { q.template in<U>(); })
        return q.template in<U>();
    else
        return q;
}
} // namespace detail
template <reference R, detail::origin_ref O, class V, reference R2, class V2>
    requires requires(const quantity<R, V>& a, const quantity<R2, V2>& b) { a + detail::in_point_unit<R.U>(b); }
[[nodiscard]] constexpr auto operator+(const quantity_point<R, O, V>& p, const quantity<R2, V2>& q) {
    return detail::make_point<O>(p.quantity_from_origin() + detail::in_point_unit<R.U>(q));
}
template <reference R2, class V2, reference R, detail::origin_ref O, class V>
    requires requires(const quantity<R2, V2>& a, const quantity<R, V>& b) { detail::in_point_unit<R.U>(a) + b; }
[[nodiscard]] constexpr auto operator+(const quantity<R2, V2>& q, const quantity_point<R, O, V>& p) {
    return detail::make_point<O>(detail::in_point_unit<R.U>(q) + p.quantity_from_origin());
}
template <reference R, detail::origin_ref O, class V, reference R2, class V2>
    requires requires(const quantity<R, V>& a, const quantity<R2, V2>& b) { a - detail::in_point_unit<R.U>(b); }
[[nodiscard]] constexpr auto operator-(const quantity_point<R, O, V>& p, const quantity<R2, V2>& q) {
    return detail::make_point<O>(p.quantity_from_origin() - detail::in_point_unit<R.U>(q));
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() - b.quantity_from(origin_t<O>{});
    }
[[nodiscard]] constexpr auto operator-(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() - b.quantity_from(origin_t<O>{});
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() == b.quantity_from(origin_t<O>{});
    }
[[nodiscard]] constexpr bool operator==(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
    return a.quantity_from_origin() == b.quantity_from(origin_t<O>{});
}
template <reference R, detail::origin_ref O, class V, reference R2, detail::origin_ref O2, class V2>
    requires requires(const quantity_point<R, O, V>& a, const quantity_point<R2, O2, V2>& b) {
        a.quantity_from_origin() <=> b.quantity_from(origin_t<O>{});
    }
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
    int         num = 0, den = 0;
    for (std::int8_t i = 0; i < u.Size; ++i)
        (u.Atoms[i].Exp > 0 ? num : den) += 1;
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
    // A copy: clang 20 miscompiles a runtime reference to a template
    // parameter object holding pointers.
    static constexpr unit u   = R.U;
    std::string           s   = beman::inside::to_string(q.numerical_value());
    const std::string     sym = unit_symbol(u, enc);
    if (sym.empty())
        return s;
    return s + (detail::space_before(u) ? " " : "") + sym;
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

#if BEMAN_INSIDE_HAS_STD_FORMAT
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
        auto                     out  = std::formatter<V>::format(q.numerical_value(), ctx);
        const std::string        sym  = u::unit_symbol(unit);
        if (sym.empty())
            return out;
        if constexpr (u::detail::space_before(unit))
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
#endif // BEMAN_INSIDE_HAS_STD_FORMAT

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
    inline constexpr auto name = spec_c<isq_defs::name>;     \
    }
#define BEMAN_INSIDE_ORIGIN(ns, name, ...)                      \
    namespace ns##_defs {                                       \
        inline constexpr point_origin name{#name, __VA_ARGS__}; \
    }                                                           \
    namespace ns {                                              \
    inline constexpr auto name = origin_c<ns##_defs::name>;     \
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
BEMAN_INSIDE_UNIT(si, kelvin, "K", isq::thermodynamic_temperature, {.origin = si::absolute_zero})
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
BEMAN_INSIDE_UNIT(si, ohm, "Ω", si::volt / si::ampere, isq::impedance, {.ascii = "ohm"})
BEMAN_INSIDE_UNIT(si, siemens, "S", one / si::ohm, isq::admittance)
BEMAN_INSIDE_UNIT(si, weber, "Wb", si::volt* si::second)
BEMAN_INSIDE_UNIT(si, tesla, "T", si::weber / square(si::metre), isq::magnetic_flux_density)
BEMAN_INSIDE_UNIT(si, henry, "H", si::weber / si::ampere)
BEMAN_INSIDE_UNIT(si, degree_Celsius, "℃", si::kelvin, {.ascii = "`C", .origin = si::ice_point})
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
BEMAN_INSIDE_UNIT(si, degree, "°", mag_pi* mag<rational{1, 180}>* si::radian, {.ascii = "deg", .space_before = false})
BEMAN_INSIDE_UNIT(si, arcminute, "′", mag<rational{1, 60}>* si::degree, {.ascii = "'", .space_before = false})
BEMAN_INSIDE_UNIT(si, arcsecond, "″", mag<rational{1, 60}>* si::arcminute, {.ascii = "''", .space_before = false})
BEMAN_INSIDE_UNIT(si, are, "a", square(si::deca(si::metre)))
BEMAN_INSIDE_UNIT(si, litre, "L", cubic(si::deci(si::metre)))
BEMAN_INSIDE_UNIT(si, tonne, "t", mag<1000>* si::kilogram)
namespace si {
inline constexpr auto hectare = hecto(are);
}
#if BEMAN_INSIDE_BIG_GRIDS
BEMAN_INSIDE_UNIT(si, dalton, "Da", mag<1.66053906892e-27_g>* si::kilogram) // CODATA 2022
BEMAN_INSIDE_UNIT(si, electronvolt, "eV", mag<1.602176634e-19_g>* si::joule)
#endif

// The defining constants of the SI (2019) and standard gravity, as units:
// just<1> * si2019::speed_of_light_in_vacuum is c, exactly, and .in(m/s)
// gives its number. h, e, k and N_A need grid numbers past 64 bits.
BEMAN_INSIDE_UNIT(
    si2019, hyperfine_structure_transition_frequency_of_cs, "Δν_Cs", mag<9'192'631'770>* si::hertz, {.ascii = "dv_Cs"})
BEMAN_INSIDE_UNIT(si2019, speed_of_light_in_vacuum, "c", mag<299'792'458>* si::metre / si::second)
BEMAN_INSIDE_UNIT(si2019, luminous_efficacy, "K_cd", mag<683>* si::lumen / si::watt)
BEMAN_INSIDE_UNIT(
    si2019, standard_gravity, "g₀", mag<rational{980665, 100000}>* si::metre / square(si::second), {.ascii = "g_0"})
#if BEMAN_INSIDE_BIG_GRIDS
BEMAN_INSIDE_UNIT(si2019, planck_constant, "h", mag<6.62607015e-34_g>* si::joule* si::second)
BEMAN_INSIDE_UNIT(si2019, elementary_charge, "e", mag<1.602176634e-19_g>* si::coulomb)
BEMAN_INSIDE_UNIT(si2019, boltzmann_constant, "k", mag<1.380649e-23_g>* si::joule / si::kelvin)
BEMAN_INSIDE_UNIT(si2019, avogadro_constant, "N_A", mag<6.02214076e23_g> / si::mole)
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
                  mag<rational{5, 9}>* si::kelvin,
                  {.ascii = "`F", .origin = international::zeroth_degree_Fahrenheit})

// Scaled dimensionless units, as in mp-units: 8 % is the number 2/25.
namespace dimensionless_defs {
inline constexpr named_unit percent{"%", mag<rational{1, 100}> * one};
inline constexpr named_unit per_mille{"‰", mag<rational{1, 1000}> * one, {.ascii = "permille"}};
} // namespace dimensionless_defs
inline constexpr auto percent   = unit_c<dimensionless_defs::percent>;
inline constexpr auto per_mille = unit_c<dimensionless_defs::per_mille>;

#undef BEMAN_INSIDE_UNIT
#undef BEMAN_INSIDE_ORIGIN
#undef BEMAN_INSIDE_SPEC

} // namespace beman::inside::units

#undef BEMAN_INSIDE_UNITS_WHY_PI

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_UNITS_HPP
