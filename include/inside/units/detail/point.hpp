// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Points, id<"[-40..125] [`C] @ice_point">: quantities counted from an origin, their exact shifts and comparisons.
// Internal: include <inside/units.hpp> or <inside/units/core.hpp>.
#ifndef INSIDE_UNITS_DETAIL_POINT_HPP
#define INSIDE_UNITS_DETAIL_POINT_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units/detail/quantity.hpp>

namespace ins
{

  // A point: id<K> for a key with an origin, a quantity of the key's reference counted from it
  // (@ice_point; @0, origin_def::none, the implicit zero of the dimension).
  // A change of origin shifts the grid by the exact offset, a change of unit rescales it.
  //   auto t = point<si::degree_Celsius>(inside<{-40, 60}>{20});   // 20 ℃, id<"[-40..60] [`C] @ice_point">
  //   t.quantity_from(si::absolute_zero)                         // 293.15 K… in ℃
  //   t.point_for(si::absolute_zero).in(si::kelvin)              // the point 293.15 K
  template <key K>
    requires detail::point_key<K>
  struct id<K>;

  namespace detail
  {
    template <class T>
    inline constexpr bool is_quantity_point_v = false;
    template <key K>
      requires point_key<K>
    inline constexpr bool is_quantity_point_v<id<K>> = true;
  } // namespace detail

  template <class T>
  concept point_like = detail::is_quantity_point_v<std::remove_cvref_t<T>>;

  // quantity_point<R, O, Rep>: a point of a quantity on R from origin O, its number Rep:
  // quantity_point<si::degree_Celsius, si::ice_point, id<"[-40..125]">> is id<"[-40..125] [`C] @ice_point">.
  template <reference R, origin_ref O, insidable Rep>
  using quantity_point = id<detail::quantity_key<R, Rep, O, true>()>;

  namespace detail
  {
    // The number, in unit U, that takes a quantity from origin From to origin To:
    // (offset(From) − offset(To)) / mag(U).
    template <origin_ref From, origin_ref To, unit_value U>
    consteval grid_number origin_shift()
    {
      static_assert(same(absolute_of(From), absolute_of(To)), "units: points of different absolute origins");
      constexpr magnitude m{magnitude_of(U)};
      static_assert(m.Pi == 0, "units: an origin shift in a unit with a pi magnitude has no exact grid");
      return (offset_of(From) - offset_of(To)) / m.Ratio;
    }

    // Whether shift<C> forms on V: inside's v + C exists.
    template <grid_number C, class V>
    concept shift_forms = C == grid_number{0} || requires(const V& v) { v + just<C>; };

    template <grid_number C, insidable V>
    constexpr auto shift(const V& v)
    {
      if constexpr (C == grid_number{0})
        return v;
      else
        return v + just<C>;
    }

    // The default origin of a unit's points: its named unit's (K, ℃, ℉).
    consteval origin_ref origin_of_unit(const unit_value& u)
    {
      return is_single_named(u) ? u.Atoms[0].U->Origin : &origin_def::none;
    }

    // Whether Rep stores the plain number V (under the one-shot flags Pol) only while compiling:
    // the condition of inside's consteval constructors.
    template <class Rep, class Pol>
    inline constexpr policy_flag point_flags_with =
      ins::detail::override_flags(policy_of<Rep>, policy_of<std::remove_cvref_t<Pol>>);
    template <class Rep, class V, class... Pol>
    inline constexpr bool point_store_immediate = false;
    template <class Rep, class V>
      requires ins::detail::numeric<std::remove_cvref_t<V>> && (!insidable<std::remove_cvref_t<V>>)
    inline constexpr bool point_store_immediate<Rep, V> =
      !ins::detail::store_proven<Rep, std::remove_cvref_t<V>, policy_of<Rep>>;
    template <class Rep, class V, class Pol>
      requires ins::detail::numeric<std::remove_cvref_t<V>> && (!insidable<std::remove_cvref_t<V>>)
    inline constexpr bool point_store_immediate<Rep, V, Pol> =
      !ins::detail::store_proven<inside<grid_of<Rep>, point_flags_with<Rep, Pol>>, std::remove_cvref_t<V>,
                                 point_flags_with<Rep, Pol>>
      && !ins::detail::reports_into_errc<Pol>;

    template <origin_ref O, class Q>
    constexpr auto make_point(Q&& q)
    {
      using T = ins::detail::unwrap_t<Q>;
      return lift_to<quantity_point<T::reference, O, typename T::rep>>(std::forward<Q>(q));
    }
  } // namespace detail

  template <key K>
    requires detail::point_key<K>
  struct id<K>
    : public detail::member_holder<detail::key_rep<K>, quantity<detail::key_reference<K>, detail::key_rep<K>>, id<K>>
  {
  private:
    static constexpr ins::reference R{detail::key_reference<K>};
    static constexpr origin_ref     O{detail::key_origin<K>};
    using Rep    = detail::key_rep<K>;
    using holder = detail::member_holder<Rep, quantity<R, Rep>, id>;
    using holder::Value; // the quantity from the origin

    static_assert(!R.unit().Scale.Ratio.negative() && !R.unit().Scale.Ratio.is_zero(),
                  "units: this unit carries a sign (e2 * e1 is -(e1 * e2)) - write the unit in another order and "
                  "negate the number");

  public:
    using quantity_type = quantity<R, Rep>;
    using rep           = Rep;
    static constexpr ins::reference reference = R;
    static constexpr origin_ref     origin    = O;

    constexpr id() = default;
    constexpr explicit id(const quantity<R, Rep>& q) : holder(q) {}

    // From a number in the point's own unit, counted from its origin, stored as quantity<R, Rep>{v, u} stores it:
    // celsius{21.5, si::degree_Celsius}, celsius{v, si::degree_Celsius, clamp}.
    // A plain number the grids cannot prove is stored only while compiling, as by inside's consteval constructor;
    // this constructor is consteval then itself, so no escalation is left to infer.
    template <class V, class Tag, class... Pol>
      requires(sizeof...(Pol) <= 1) && detail::is_reference_tag_v<std::remove_cvref_t<Tag>>
           && detail::own_unit_v<quantity<R, Rep>, Tag> && std::constructible_from<quantity<R, Rep>, V, Tag, Pol...>
           && (!detail::point_store_immediate<Rep, V, Pol...>)
    constexpr id(V&& v, Tag u, Pol&&... pol)
      : holder(quantity<R, Rep>(std::forward<V>(v), u, std::forward<Pol>(pol)...)) {}
    template <class V, class Tag, class... Pol>
      requires(sizeof...(Pol) <= 1) && detail::is_reference_tag_v<std::remove_cvref_t<Tag>>
           && detail::own_unit_v<quantity<R, Rep>, Tag> && std::constructible_from<quantity<R, Rep>, V, Tag, Pol...>
           && detail::point_store_immediate<Rep, V, Pol...>
    consteval id(V&& v, Tag u, Pol&&... pol)
      : holder(quantity<R, Rep>(std::forward<V>(v), u, std::forward<Pol>(pol)...)) {}
    // A number in another unit names no origin (20 in K from the ice point, or 20 K?).
    template <class V, class Tag, class... Pol>
      requires(sizeof...(Pol) <= 1) && detail::is_reference_tag_v<std::remove_cvref_t<Tag>>
           && (!detail::own_unit_v<quantity<R, Rep>, Tag>)
    id(V&&, Tag, Pol&&...) = delete ("units: a point from a number takes the point's own unit - write point<u>(v) "
                                     "and convert it with point_in or point_for");

    // From a point of another origin (of the same absolute one) or unit:
    // shifted, then rescaled, exactly; implicit as the quantities are.
    template <ins::point_like P2, ins::reference R2 = P2::reference, origin_ref O2 = P2::origin, class V2 = typename P2::rep>
      requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O))) && detail::implicit_v<R2, R>
            && std::constructible_from<quantity<R, Rep>,
                                       decltype(std::declval<const P2&>().quantity_from(origin_t<O>{}))>
    constexpr explicit(!std::convertible_to<decltype(std::declval<const P2&>().quantity_from(origin_t<O>{})),
                                            quantity<R, Rep>>) id(const P2& p)
      : holder(quantity<R, Rep>(p.quantity_from(origin_t<O>{}))) {}

    // The quantity from this point's origin, or from another one.
    [[nodiscard]] constexpr const quantity<R, Rep>& quantity_from_origin() const noexcept { return Value; }
    template <origin_ref O2>
      requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O)) && magnitude_of(R.unit()).Pi == 0
               && detail::shift_forms<detail::origin_shift<O, O2, R.unit()>(), Rep>)
    [[nodiscard]] constexpr auto quantity_from(origin_t<O2>) const
    {
      constexpr grid_number c{detail::origin_shift<O, O2, R.unit()>()};
      return detail::quantity_access::make<
        quantity<R, std::remove_cvref_t<decltype(detail::shift<c>(ins::detail::value_of(Value)))>>>(
        detail::shift<c>(ins::detail::value_of(Value)));
    }
    template <origin_ref O2>
      requires(!detail::same(detail::absolute_of(O2), detail::absolute_of(O)))
    void quantity_from(origin_t<O2>) const = delete ("units: points of different absolute origins");
    template <origin_ref O2>
      requires(detail::same(detail::absolute_of(O2), detail::absolute_of(O)) && magnitude_of(R.unit()).Pi != 0)
    void quantity_from(origin_t<O2>) const
      = delete ("units: an origin shift in a unit with a pi magnitude has no exact grid");
    // The quantity from the absolute origin of the dimension (absolute zero, or the implicit zero):
    // 20 ℃ → 293.15 K counted in ℃.
    [[nodiscard]] constexpr auto quantity_from_zero() const
      requires requires(const id& p) { p.quantity_from(origin_t<detail::absolute_of(O)>{}); }
    {
      return quantity_from(origin_t<detail::absolute_of(O)>{});
    }
    // The same point counted from another origin.
    template <origin_ref O2>
      requires requires(const id& p, origin_t<O2> o) { p.quantity_from(o); }
    [[nodiscard]] constexpr auto point_for(origin_t<O2> o) const
    {
      return detail::make_point<O2>(quantity_from(o));
    }
    // The same point in another unit.
    template <ins::unit_value U>
      requires requires(const quantity<R, Rep>& q, unit_t<U> u) { q.in(u); }
    [[nodiscard]] constexpr auto in(unit_t<U> u) const
    {
      return detail::make_point<O>(Value.in(u));
    }
    // The same point in unit U, counted from U's own origin: 350 ℉ → 530/3 ℃.
    template <ins::unit_value U>
      requires requires(const id& p, unit_t<U> u)
               {
                 p.point_for(origin_t<detail::origin_of_unit(U)>{}).in(u);
               }
    [[nodiscard]] constexpr auto point_in(unit_t<U> u) const
    {
      return point_for(origin_t<detail::origin_of_unit(U)>{}).in(u);
    }

    template <ins::quantity_like QR2, ins::reference R2 = QR2::reference, class V2 = typename QR2::rep>
      requires requires(quantity<R, Rep>& a, const QR2& b) { a += b; }
    constexpr id& operator+=(const QR2& q)
    {
      Value += q;
      return *this;
    }
    template <ins::quantity_like QR2, ins::reference R2 = QR2::reference, class V2 = typename QR2::rep>
      requires requires(quantity<R, Rep>& a, const QR2& b) { a -= b; }
    constexpr id& operator-=(const QR2& q)
    {
      Value -= q;
      return *this;
    }
  };

  // A point with its type: "21.5 [-40..125] 1/10 [`C] @ice_point"_ins is 21.5 ℃, the number in the type's unit
  // counted from its origin, stored while compiling as P{just<v>, unit} stores it.
  inline namespace literals
  {
    template <detail::value_text T>
      requires detail::point_key<T.Key>
    consteval auto operator""_ins()
    {
      return id<T.Key>{just<T.Value>, reference_t<detail::key_reference<T.Key>>{}};
    }
  } // namespace literals

  // value_cast on a point: its quantity from the origin cast, the origin kept.
  // (The point of a cast quantity, or an expected one: the cast's error passed on.)
  namespace detail
  {
    template <origin_ref O, class Q>
    constexpr auto point_at(const Q& q)
    {
      if constexpr (ins::detail::is_expected_v<Q>)
        return q.transform([](const auto& x) { return point_at<O>(x); });
      else
        return quantity_point<Q::reference, O, typename Q::rep>{q};
    }
  } // namespace detail
  template <insidable Rep2, point_like P, reference R = P::reference, origin_ref O = P::origin, class V = typename P::rep>
    requires requires(const quantity<R, V>& q) { value_cast<Rep2>(q); }
  [[nodiscard]] constexpr auto value_cast(const P& p)
  {
    return detail::point_at<O>(value_cast<Rep2>(p.quantity_from_origin()));
  }
  template <auto U, insidable Rep2, point_like P, reference R = P::reference, origin_ref O = P::origin, class V = typename P::rep>
    requires requires(const quantity<R, V>& q) { value_cast<U, Rep2>(q); }
  [[nodiscard]] constexpr auto value_cast(const P& p)
  {
    return detail::point_at<O>(value_cast<U, Rep2>(p.quantity_from_origin()));
  }

  // point<si::degree_Celsius>(v): a point at the unit's default origin (the implicit zero for a unit without one).
  template <auto U, insidable V>
  [[nodiscard]] constexpr auto point(const V& v)
  {
    return quantity_point<reference_of(U), detail::origin_of_unit(decltype(U)::value), V>{
      detail::quantity_access::make<quantity<reference_of(U), V>>(v)};
  }
  // delta<si::degree_Celsius>(v): a difference, a plain quantity.
  template <auto U, insidable V>
  [[nodiscard]] constexpr auto delta(const V& v)
  {
    return v * U;
  }

  // origin + quantity: si::absolute_zero + just<300> * si::kelvin.
  template <origin_ref O, quantity_like QR, reference R = QR::reference, class V = typename QR::rep>
  [[nodiscard]] constexpr auto operator+(origin_t<O>, const QR& q)
  {
    return quantity_point<R, O, V>{q};
  }
  template <quantity_like QR, origin_ref O, reference R = QR::reference, class V = typename QR::rep>
  [[nodiscard]] constexpr auto operator+(const QR& q, origin_t<O>)
  {
    return quantity_point<R, O, V>{q};
  }

  // point ± quantity: a point at the same origin,
  // in the point's unit when the quantity converts to it exactly (5 K onto a ℃ point stays ℃),
  // else in the common unit; point − point: a quantity (the second counted from the first's origin).
  namespace detail
  {
    template <unit_value U, quantity_like QR2, reference R2 = QR2::reference, class V2 = typename QR2::rep>
    constexpr auto in_point_unit(const QR2& q)
    {
      if constexpr (requires { q.template in<U>(); })
        return q.template in<U>();
      else
        return q;
    }
  } // namespace detail
  template <quantity_like QR2, point_like P, reference R = P::reference, origin_ref O = P::origin, class V = typename P::rep>
    requires requires(const quantity<R, V>& a, const QR2& b) {
      a + detail::in_point_unit<R.unit()>(b);
    }
  [[nodiscard]] constexpr auto operator+(const P& p, const QR2& q)
  {
    return detail::make_point<O>(p.quantity_from_origin() + detail::in_point_unit<R.unit()>(q));
  }
  template <quantity_like QR2, point_like P, reference R = P::reference, origin_ref O = P::origin, class V = typename P::rep>
    requires requires(const QR2& a, const quantity<R, V>& b) {
      detail::in_point_unit<R.unit()>(a) + b;
    }
  [[nodiscard]] constexpr auto operator+(const QR2& q, const P& p)
  {
    return detail::make_point<O>(detail::in_point_unit<R.unit()>(q) + p.quantity_from_origin());
  }
  template <quantity_like QR2, point_like P, reference R = P::reference, origin_ref O = P::origin, class V = typename P::rep>
    requires requires(const quantity<R, V>& a, const QR2& b) {
      a - detail::in_point_unit<R.unit()>(b);
    }
  [[nodiscard]] constexpr auto operator-(const P& p, const QR2& q)
  {
    return detail::make_point<O>(p.quantity_from_origin() - detail::in_point_unit<R.unit()>(q));
  }
  template <point_like P, point_like P2, origin_ref O = P::origin>
    requires requires(const P& a, const P2& b) { a.quantity_from_origin() - b.quantity_from(origin_t<O>{}); }
  [[nodiscard]] constexpr auto operator-(const P& a, const P2& b)
  {
    return a.quantity_from_origin() - b.quantity_from(origin_t<O>{});
  }
  template <point_like P, point_like P2, origin_ref O = P::origin>
    requires requires(const P& a, const P2& b) { a.quantity_from_origin() == b.quantity_from(origin_t<O>{}); }
  [[nodiscard]] constexpr bool operator==(const P& a, const P2& b)
  {
    return a.quantity_from_origin() == b.quantity_from(origin_t<O>{});
  }
  template <point_like P, point_like P2, origin_ref O = P::origin>
    requires requires(const P& a, const P2& b) { a.quantity_from_origin() <=> b.quantity_from(origin_t<O>{}); }
  [[nodiscard]] constexpr auto operator<=>(const P& a, const P2& b)
  {
    return a.quantity_from_origin() <=> b.quantity_from(origin_t<O>{});
  }
} // namespace ins

#endif // INSIDE_UNITS_DETAIL_POINT_HPP
