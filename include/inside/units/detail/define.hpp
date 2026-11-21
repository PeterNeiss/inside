// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal: include <inside/units.hpp> (or units/core.hpp), not this directly.
#ifndef INSIDE_UNITS_DETAIL_DEFINE_HPP
#define INSIDE_UNITS_DETAIL_DEFINE_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units/detail/algebra.hpp>
#include <inside/units/detail/point.hpp>
#include <inside/units/detail/quantity.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

// Units, kinds, origins and dimensions of your own: one declaration each.
//
//   inline constexpr auto fathom  = unit<"ftm", 1.8288_ins * si::metre>;
//   inline constexpr auto depth   = kind<"depth", isq::length>;
//   inline constexpr auto rankine = unit<"°R", just<5, 9> * si::kelvin,
//                                        {.ascii = "R", .origin = si::absolute_zero}>;
//   inline constexpr auto EUR     = base_unit<"EUR", "currency">;
//   inline constexpr auto ct      = unit<"ct", 0.01_ins * EUR>;
//   inline constexpr auto ice     = origin<"ice point", si::absolute_zero + 273.15_ins * si::kelvin>;
//   inline constexpr auto e1      = basis_vector<"e₁", geometric<+1>>;
//
// The name is a template argument and the definition a static member of a class template,
// so a declaration is its own identity: the same declaration in two places is the same unit.
// Diagnostics and symbols spell such a unit by its definition.
//
// For short type names, name the definition object and take its unit with defined_by,
// as the catalog does (units/systems.hpp). The definition types take the same arguments:
//
//   inline constexpr unit_def fathom_def{"ftm", 1.8288_ins * si::metre};
//   inline constexpr auto     fathom = defined_by<fathom_def>;    // spelled (& fathom_def)
//   inline constexpr kind_def count_def{"count", base_dimension<"count">};
//   inline constexpr unit_def cts_def  {"cts", defined_by<count_def>};
//   inline constexpr auto     cts = defined_by<cts_def>;          // base_unit<"cts", "count">, named
//
// A named definition is its own identity: one with equal content elsewhere is equal (==) and converts,
// but is another type. In a header it is inline constexpr, so that every translation unit has the same one.
namespace ins
{
  // A definition object of type T, named by a template argument:
  // T{name, args…} as a static member, which gives it static storage and one identity.
  template <class T, symbol_text Name, auto... Args>
  struct definition
  {
    static constexpr T value{Name.view(), Args...};
  };

  namespace detail
  {
    // The unit a constant quantity is: 1.8288_ins * si::metre is mag<1.8288> * si::metre.
    template <class Q>
    consteval auto unit_of_constant()
    {
      static_assert(lower_of<typename Q::rep> == upper_of<typename Q::rep>,
                    "units: a unit is defined by a constant - 1.8288_ins * si::metre");
      return mag<lower_of<typename Q::rep>> * unit_t<Q::unit>{};
    }

    // What a unit is defined by, as its key: a unit (si::joule / si::second),
    // a kind (the base unit of that kind), or the quantity it equals (1.8288_ins * si::metre):
    // its number times its unit.
    template <auto Def>
    struct unit_definition
    {
      static constexpr auto value = Def;
    };
    template <auto Def>
      requires quantity_like<decltype(Def)>
    struct unit_definition<Def>
    {
      static constexpr auto value = unit_of_constant<decltype(Def)>();
    };
  } // namespace detail

  template <class Q>
    requires detail::is_quantity_v<Q>
  constexpr unit_def::unit_def(std::string_view sym, const Q&, unit_options o)
    : unit_def{sym, detail::unit_of_constant<Q>(), o} {}

  template <class P>
    requires requires(const P& p) { p.quantity_from_origin(); }
  constexpr origin_def::origin_def(std::string_view name, const P& p)
    : origin_def{name, dimension_of(std::remove_cvref_t<decltype(p.quantity_from_origin())>::unit)}
  {
    using Q = std::remove_cvref_t<decltype(p.quantity_from_origin())>;
    static_assert(static_cast<bool>(P::origin), "units: an origin at o + q - the point counts from an origin");
    static_assert(lower_of<typename Q::rep> == upper_of<typename Q::rep>,
                  "units: an origin lies at a constant point - si::absolute_zero + 273.15_ins * si::kelvin");
    static_assert(magnitude_of(Q::unit).Pi == 0, "units: an origin's offset has no factor of pi");
    Absolute = detail::absolute_of(P::origin);
    Offset   = detail::offset_of(P::origin) + lower_of<typename Q::rep> * magnitude_of(Q::unit).Ratio;
  }

  template <symbol_text Name, kind_options O, auto Parent>
  struct kind_holder
  {
    static constexpr kind_def value{Name.view(), Parent, O};
  };

  // An origin is absolute (named by a kind or a dimension: the zero of that dimension)
  // or lies at a point of another origin.
  template <symbol_text Name, auto Def>
  struct origin_holder
  {
    static constexpr origin_def value{Name.view(), Def};
  };

  // unit<"sym", definition, options>: a unit of your own.
  // The definition is the quantity it equals (1.8288_ins * si::metre), another unit (si::joule / si::second),
  // or a kind, whose base unit it then is.
  template <symbol_text Sym, auto Def, unit_options O = {}>
  inline constexpr auto unit =
    detail::unit_tag<definition<unit_def, Sym, detail::unit_definition<Def>::value, O>::value>;

  // kind<"name", parent, options>: a kind of quantity.
  // The parent is a kind (depth under length), a product of kinds (speed: length / duration)
  // or a dimension (a base quantity).
  template <symbol_text Name, auto Parent, kind_options O = {}>
  inline constexpr auto kind = detail::kind_tag<kind_holder<Name, O, Parent>::value>;

  // base_dimension<"name">: a dimension of your own, which mixes with no other.
  template <symbol_text Name>
  inline constexpr dimension base_dimension = dimension_of(definition<base_dimension_def, Name>::value);

  // base_unit<"sym", "dimension">: a unit of something that mixes with nothing else (money, pixels, counts):
  // a new dimension, its kind, and this unit.
  template <symbol_text Sym, symbol_text Dimension, unit_options O = {}>
  inline constexpr auto base_unit = unit<Sym, kind<Dimension, base_dimension<Dimension>>, O>;

  // origin<"name", where>: where points count from.
  // An absolute origin is named by a kind or a dimension; any other lies at a point of one:
  // origin<"ice point", si::absolute_zero + 273.15_ins * si::kelvin>.
  template <symbol_text Name, auto Def>
  inline constexpr auto origin = detail::origin_tag<origin_holder<Name, Def>::value>;

  namespace detail
  {
    template <const auto& D>
    consteval auto defined_by()
    {
      using T = std::remove_cvref_t<decltype(D)>;
      if constexpr (std::same_as<T, unit_def> || std::same_as<T, basis_vector_def>)
        return unit_tag<D>;
      else if constexpr (std::same_as<T, kind_def>)
        return kind_tag<D>;
      else if constexpr (std::same_as<T, origin_def>)
        return origin_tag<D>;
      else
        static_assert(false, "units: defined_by<d> - d is a unit_def, kind_def, origin_def or basis_vector_def");
    }
  } // namespace detail

  // defined_by<d>: the unit, kind or origin that the named definition object d defines
  // (a unit_def, kind_def, origin_def or basis_vector_def). Diagnostics and symbols spell it by d's name.
  template <const auto& D>
  inline constexpr auto defined_by = detail::defined_by<D>();

  // pi: the number π as a unit, exactly.
  // A unit that is a multiple of π says so, and conversions between such units stay exact:
  //   unit<"°", just<1, 180> * pi * si::radian>, unit<"tr", just<2> * pi * si::radian>.
  inline constexpr auto pi = detail::mag_pi * one;
  // A quantity times or over pi: the same number, in a unit scaled by π (pi * (2 * si::second) is 2 [π s]);
  // value_cast rounds it once.
  template <reference R, class V>
  [[nodiscard]] constexpr auto operator*(decltype(pi), const quantity<R, V>& q)
  {
    return detail::mag_pi * q;
  }
  template <reference R, class V>
  [[nodiscard]] constexpr auto operator*(const quantity<R, V>& q, decltype(pi))
  {
    return q * detail::mag_pi;
  }
  template <reference R, class V>
  [[nodiscard]] constexpr auto operator/(const quantity<R, V>& q, decltype(pi))
  {
    return q / detail::mag_pi;
  }
  // (pi times a number is that number of [π].)
  template <insidable V>
  [[nodiscard]] constexpr auto operator*(decltype(pi), const V& v)
  {
    return detail::mag_pi * v;
  }

  // basis_vector<"e₁", geometric<+1>>: a basis vector, by how it multiplies: geometric<S> for a geometric algebra,
  // by its square S (+1, -1 or 0).
  template <symbol_text Sym, basis_rule Rule, unit_options O = {}>
  inline constexpr auto basis_vector = detail::unit_tag<definition<basis_vector_def, Sym, Rule, O>::value>;
} // namespace ins

#endif // INSIDE_UNITS_DETAIL_DEFINE_HPP
