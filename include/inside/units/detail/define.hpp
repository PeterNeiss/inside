// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal — include <inside/units.hpp> (or units/core.hpp), not this
// directly.
#ifndef INSIDE_UNITS_DETAIL_DEFINE_HPP
#define INSIDE_UNITS_DETAIL_DEFINE_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units/detail/algebra.hpp>
#include <inside/units/detail/point.hpp>
#include <inside/units/detail/quantity.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

//---------------------------------------------------------------------------
// Units, kinds, origins and dimensions of your own: one declaration each.
//
//   inline constexpr auto fathom  = unit<"ftm", 1.8288_ins * si::metre>;
//   inline constexpr auto depth   = kind<"depth", isq::length>;
//   inline constexpr auto rankine = unit<"°R", just<5, 9> * si::kelvin,
//                                        {.ascii = "R", .origin = si::absolute_zero}>;
//   inline constexpr auto EUR     = base_unit<"EUR", "currency">;
//   inline constexpr auto ct      = unit<"ct", 0.01_ins * EUR>;
//   inline constexpr auto ice     = origin<"ice point", si::absolute_zero + 273.15_ins * si::kelvin>;
//   inline constexpr auto e1      = basis_vector<"e₁", +1>;
//
// The name is a template argument and the definition a static member of a
// class template, so a declaration is its own identity: the same declaration
// in two places is the same unit.
//---------------------------------------------------------------------------
namespace ins {
// A kind's options, all optional:
//   .distinct  true for a kind that never mixes with its siblings, although
//              they share a dimension (activity and frequency are both 1/s)
//   .recipe    the quantities it is made of, for a child that has a recipe of
//              its own (velocity: displacement / duration, under speed)
struct kind_options {
    bool      distinct{false};
    kind_expr recipe{};
};

// A definition object of type T, named by a template argument: T{name, args…}
// as a static member, which gives it static storage and one identity.
template <class T, symbol_text Name, auto... Args>
struct definition {
    static constexpr T value{Name.view(), Args...};
};

namespace detail {
// What a unit is defined by, as unit_def takes it: a unit (si::joule /
// si::second), a kind (the base unit of that kind), or the quantity it
// equals (1.8288_ins * si::metre): its number times its unit.
template <auto Def>
struct unit_definition {
    static constexpr auto value = Def;
};
template <auto Def>
    requires quantity_like<decltype(Def)>
struct unit_definition<Def> {
    using Q = decltype(Def);
    static_assert(lower_of<typename Q::rep> == upper_of<typename Q::rep>,
                  "units: a unit is defined by a constant - unit<\"ftm\", 1.8288_ins * si::metre>");
    static constexpr auto value = mag<lower_of<typename Q::rep>> * unit_t<Q::unit>{};
};
} // namespace detail

// A kind's definition by its options (specializations, not a lambda: the
// catalog declares some eighty kinds).
template <symbol_text Name, kind_options O, auto Parent>
struct kind_holder {
    static constexpr kind_def value{Name.view(), Parent};
};
template <symbol_text Name, kind_options O, auto Parent>
    requires(O.distinct && O.recipe.Recipe.Size == 0)
struct kind_holder<Name, O, Parent> {
    static constexpr kind_def value{Name.view(), Parent, detail::distinct};
};
template <symbol_text Name, kind_options O, auto Parent>
    requires(!O.distinct && O.recipe.Recipe.Size != 0)
struct kind_holder<Name, O, Parent> {
    static constexpr kind_def value{Name.view(), Parent, O.recipe};
};
template <symbol_text Name, kind_options O, auto Parent>
    requires(O.distinct && O.recipe.Recipe.Size != 0)
struct kind_holder<Name, O, Parent> {
    static constexpr kind_def value{Name.view(), Parent, O.recipe, detail::distinct};
};

// An origin is absolute (named by a kind or a dimension: the zero of that
// dimension) or lies at a point of another origin.
template <symbol_text Name, auto Def>
struct origin_holder {
    static constexpr origin_def value = [] {
        if constexpr (requires { Def.quantity_from_origin(); }) {
            using P = decltype(Def);
            static_assert(static_cast<bool>(P::origin),
                          "units: origin<name, o + q> - the point counts from an origin");
            constexpr auto q = Def.quantity_from_origin();
            using Q          = decltype(q);
            static_assert(lower_of<typename Q::rep> == upper_of<typename Q::rep>,
                          "units: an origin lies at a constant point - si::absolute_zero + 273.15_ins * si::kelvin");
            static_assert(magnitude_of(Q::unit).Pi == 0, "units: an origin's offset has no factor of pi");
            return origin_def{Name.view(),
                              dimension_of(Q::unit),
                              detail::absolute_of(P::origin),
                              detail::offset_of(P::origin) + lower_of<typename Q::rep> * magnitude_of(Q::unit).Ratio};
        } else if constexpr (requires { dimension{Def}; })
            return origin_def{Name.view(), dimension{Def}};
        else
            return origin_def{Name.view(), Def.value->Dim};
    }();
};

// unit<"sym", definition, options>: a unit of your own. The definition is
// the quantity it equals (1.8288_ins * si::metre), another unit (si::joule /
// si::second), or a kind, whose base unit it then is.
template <symbol_text Sym, auto Def, unit_options O = {}>
inline constexpr auto unit =
    detail::unit_tag<definition<unit_def, Sym, detail::unit_definition<Def>::value, O>::value>;

// kind<"name", parent, options>: a kind of quantity. The parent is a kind
// (depth under length), a product of kinds (speed: length / duration) or a
// dimension (a base quantity).
template <symbol_text Name, auto Parent, kind_options O = {}>
inline constexpr auto kind = detail::kind_tag<kind_holder<Name, O, Parent>::value>;

// base_dimension<"name">: a dimension of your own, which mixes with no other.
template <symbol_text Name>
inline constexpr dimension base_dimension = dimension_of(definition<base_dimension_def, Name>::value);

// base_unit<"sym", "dimension">: a unit of something that mixes with nothing
// else (money, pixels, counts): a new dimension, its kind, and this unit.
template <symbol_text Sym, symbol_text Dimension, unit_options O = {}>
inline constexpr auto base_unit = unit<Sym, kind<Dimension, base_dimension<Dimension>>, O>;

// origin<"name", where>: where points count from. An absolute origin is
// named by a kind or a dimension; any other lies at a point of one:
// origin<"ice point", si::absolute_zero + 273.15_ins * si::kelvin>.
template <symbol_text Name, auto Def>
inline constexpr auto origin = detail::origin_tag<origin_holder<Name, Def>::value>;

// pi: the number π as a unit, exactly. A unit that is a multiple of π says
// so, and conversions between such units stay exact:
//   unit<"°", just<1, 180> * pi * si::radian>, unit<"tr", just<2> * pi * si::radian>.
inline constexpr auto pi = detail::mag_pi * one;
// A quantity times or over pi: the same number, in a unit scaled by π
// (pi * (2 * si::second) is 2 [π s]); value_cast rounds it once.
template <reference R, class V>
[[nodiscard]] constexpr auto operator*(decltype(pi), const quantity<R, V>& q) {
    return detail::mag_pi * q;
}
template <reference R, class V>
[[nodiscard]] constexpr auto operator*(const quantity<R, V>& q, decltype(pi)) {
    return q * detail::mag_pi;
}
template <reference R, class V>
[[nodiscard]] constexpr auto operator/(const quantity<R, V>& q, decltype(pi)) {
    return q / detail::mag_pi;
}
// (pi times a number is that number of [π].)
template <insidable V>
[[nodiscard]] constexpr auto operator*(decltype(pi), const V& v) {
    return detail::mag_pi * v;
}

// basis_vector<"e₁", +1>: a basis vector of a geometric algebra, by its
// square (+1, -1 or 0).
template <symbol_text Sym, int Square, unit_options O = {}>
inline constexpr auto basis_vector = detail::unit_tag<definition<basis_vector_def, Sym, Square, O>::value>;
} // namespace ins

#endif // INSIDE_UNITS_DETAIL_DEFINE_HPP
