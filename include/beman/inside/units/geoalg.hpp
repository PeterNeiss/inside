// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Geometric algebra on units. Basis vectors are units of your own
// (basis_vector, in units/core.hpp): you state each one's square, +1, -1 or
// 0, and two different ones anticommute. A product of quantities is then
// again a quantity, its sign worked out: (3 e₂)(4 e₁) = -12 e₁ e₂. This header
// adds the sums of different blades, multivectors, and the operations of the
// algebra on them:
//   - a + b, a - b, -a, a * b (the geometric product), a / b;
//   - wedge, left_contraction, right_contraction, scalar_product;
//   - reverse, involute, conjugate, grade<K>, part(x, u);
//   - dual(x, I), complement(x, I), regressive(a, b, I);
//   - squared_norm(x), inverse(x).
// Complex numbers are the even part of G(2,0), quaternions the even part of
// G(3,0). Each part of a multivector keeps its own inside, so every part's
// range is proved; the arithmetic is exact.
#ifndef BEMAN_INSIDE_UNITS_GEOALG_HPP
#define BEMAN_INSIDE_UNITS_GEOALG_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/units/core.hpp>

#include <array>
#include <cstddef>
#include <expected>
#include <tuple>
#include <type_traits>
#include <utility>

namespace beman::inside::units {

namespace detail {
template <class T>
inline constexpr bool is_multivector_v = false;
template <class... P>
inline constexpr bool is_multivector_v<multivector<P...>> = true;
} // namespace detail

// An element of the algebra: an inside (a scalar), a quantity (a number times
// a unit with or without a blade), or a multivector.
template <class T>
concept multivector_like = insidable<T> || quantity_like<T> || detail::is_multivector_v<std::remove_cvref_t<T>>;

namespace detail {
// A part as a multivector holds it: a dimensionless quantity that is a plain
// number (no blade, no kind) is its inside.
template <class P>
constexpr auto as_part(const P& p) {
    if constexpr (quantity_like<P>) {
        if constexpr (number_like_v<P::reference>)
            return static_cast<number_t<P::reference, typename P::rep>>(p);
        else
            return p;
    } else
        return p;
}

// x's parts, as a tuple: one for an inside or a quantity, none for the zero.
template <class X>
constexpr auto parts_of(const X& x) {
    if constexpr (std::same_as<X, multivector<>>)
        return std::tuple<>{};
    else if constexpr (is_multivector_v<X>)
        return x.parts();
    else
        return std::tuple{as_part(x)};
}
template <class X>
using parts_t = decltype(parts_of(std::declval<const X&>()));

// The dimension of a part: the unit's for a quantity, one for an inside.
template <class P>
consteval dimension part_dim() {
    if constexpr (quantity_like<P>)
        return dimension_of(P::reference.U);
    else
        return dim_one;
}
template <class Tuple>
consteval auto dims_of() {
    return []<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<dimension, sizeof...(I)>{part_dim<std::tuple_element_t<I, Tuple>>()...};
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// The order of parts: by grade, then by the names of the basis vectors.
consteval bool blade_less(const dimension& a, const dimension& b) {
    const blade x = blade_of(a), y = blade_of(b);
    if (x.size() != y.size())
        return x.size() < y.size();
    const auto name = [](const base_dimension* v) { return v->Name; };
    return std::ranges::lexicographical_compare(x, y, std::ranges::less{}, name, name);
}
consteval bool blade_equal(const dimension& a, const dimension& b) { return !blade_less(a, b) && !blade_less(b, a); }

// Parts in order, of distinct blades and one dimension but for the blades.
template <class... P>
consteval bool canonical_parts() {
    const std::array<dimension, sizeof...(P)> d{part_dim<P>()...};
    for (std::size_t i = 1; i < d.size(); ++i)
        if (!same_outside_blade(d[0], d[i]) || !blade_less(d[i - 1], d[i]))
            return false;
    return true;
}
template <class P>
consteval bool is_part() {
    if constexpr (quantity_like<P>)
        return !number_like_v<P::reference>;
    else
        return insidable<P>;
}

// 0, 1 or more parts (in order, distinct blades): the zero, the part itself,
// or a multivector.
template <class... P>
constexpr auto collapse(const P&... p) {
    if constexpr (sizeof...(P) == 0)
        return multivector<>{};
    else if constexpr (sizeof...(P) == 1)
        return (p, ...);
    else
        return multivector<P...>{p...};
}

// Where each blade of a sum comes from: a part of A, of B, or of both.
struct merge_item {
    int A{-1};
    int B{-1};
};
template <std::size_t N>
struct merge_plan {
    std::array<merge_item, N> Items{};
    std::size_t               Size{0};
};
template <class PA, class PB>
consteval auto make_merge_plan() {
    constexpr auto                    da = dims_of<PA>(), db = dims_of<PB>();
    merge_plan<da.size() + db.size()> p;
    std::size_t                       i = 0, j = 0;
    while (i < da.size() || j < db.size()) {
        if (j == db.size() || (i < da.size() && blade_less(da[i], db[j])))
            p.Items[p.Size++] = {static_cast<int>(i++), -1};
        else if (i == da.size() || blade_less(db[j], da[i]))
            p.Items[p.Size++] = {-1, static_cast<int>(j++)};
        else
            p.Items[p.Size++] = {static_cast<int>(i++), static_cast<int>(j++)};
    }
    return p;
}

// Whether a and b add: one dimension but for the blades, and each pair of
// parts of one blade adds.
template <class A, class B>
consteval bool mergeable() {
    using PA          = parts_t<A>;
    using PB          = parts_t<B>;
    constexpr auto da = dims_of<PA>();
    constexpr auto db = dims_of<PB>();
    if constexpr (da.size() > 0 && db.size() > 0) {
        if (!same_outside_blade(da[0], db[0]))
            return false;
        static constexpr auto plan = make_merge_plan<PA, PB>();
        return []<std::size_t... K>(std::index_sequence<K...>) {
            return ([] {
                constexpr merge_item it = plan.Items[K];
                if constexpr (it.A >= 0 && it.B >= 0)
                    return requires(const std::tuple_element_t<it.A, PA>& x, const std::tuple_element_t<it.B, PB>& y) {
                        x + y;
                    };
                else
                    return true;
            }() && ...);
        }(std::make_index_sequence<plan.Size>{});
    } else
        return true;
}

// a + b: per blade, the parts of one blade added (in their common unit).
template <class A, class B>
constexpr auto mv_add(const A& a, const B& b) {
    using PA                   = parts_t<A>;
    using PB                   = parts_t<B>;
    static constexpr auto plan = make_merge_plan<PA, PB>();
    const PA              pa   = parts_of(a);
    const PB              pb   = parts_of(b);
    return [&]<std::size_t... K>(std::index_sequence<K...>) {
        return collapse([&] {
            constexpr merge_item it = plan.Items[K];
            if constexpr (it.A >= 0 && it.B >= 0)
                return as_part(std::get<it.A>(pa) + std::get<it.B>(pb));
            else if constexpr (it.A >= 0)
                return std::get<it.A>(pa);
            else
                return std::get<it.B>(pb);
        }()...);
    }(std::make_index_sequence<plan.Size>{});
}

// The sum of any number of elements, from the zero up.
template <class T>
constexpr auto mv_sum(const T& t) {
    return t;
}
template <class T, class U, class... R>
constexpr auto mv_sum(const T& t, const U& u, const R&... r) {
    return mv_sum(mv_add(t, u), r...);
}

// f applied to each part, the results summed (f may drop a part as zero or
// move it to another blade).
template <class X, class F>
constexpr auto each_part(const X& x, F f) {
    return std::apply([&](const auto&... p) { return mv_sum(multivector<>{}, f(p)...); }, parts_of(x));
}

template <class X>
constexpr auto mv_negate(const X& x) {
    return each_part(x, [](const auto& p) { return as_part(-p); });
}

// The bilinear products: each keeps the pairs of basis blades its rule
// names, as their geometric product (for an orthogonal basis, A⌋B is AB when
// A ⊆ B, and A∧B is AB when they share no basis vector).
enum class product_rule : unsigned char { geometric, wedge, left_contraction, right_contraction, scalar };

consteval bool keeps(product_rule r, const dimension& a, const dimension& b) {
    const blade x = blade_of(a), y = blade_of(b);
    if (blade_sign(x, y) == 0) // a null vector meets itself: the product is 0
        return false;
    blade z = x;
    blade_mul(z, y);
    switch (r) {
    case product_rule::geometric:
        return true;
    case product_rule::wedge:
        return z.size() == x.size() + y.size();
    case product_rule::left_contraction:
        return z.size() + x.size() == y.size();
    case product_rule::right_contraction:
        return z.size() + y.size() == x.size();
    case product_rule::scalar:
        return z.empty();
    }
    return false;
}

template <product_rule Rule, class A, class B>
constexpr auto mv_product(const A& a, const B& b) {
    using PA                 = parts_t<A>;
    using PB                 = parts_t<B>;
    constexpr std::size_t NA = std::tuple_size_v<PA>, NB = std::tuple_size_v<PB>;
    static constexpr auto da = dims_of<PA>();
    static constexpr auto db = dims_of<PB>();
    const PA              pa = parts_of(a);
    const PB              pb = parts_of(b);
    return [&]<std::size_t... K>(std::index_sequence<K...>) {
        return mv_sum(multivector<>{}, [&] {
            constexpr std::size_t i = K / NB, j = K % NB;
            if constexpr (keeps(Rule, da[i], db[j]))
                return as_part(std::get<i>(pa) * std::get<j>(pb));
            else
                return multivector<>{};
        }()...);
    }(std::make_index_sequence<NA * NB>{});
}

// v as an expected (a quotient is one when it can fail).
template <class V>
constexpr auto as_expected(V&& v) {
    if constexpr (is_expected_v<std::remove_cvref_t<V>>)
        return std::forward<V>(v);
    else
        return std::expected<std::remove_cvref_t<V>, errc>{std::forward<V>(v)};
}

// x / s for one part s (a number, or a quantity of any blade): each part
// divided. When a quotient can fail (s's grid holds 0), the result is an
// expected, with the first part's error.
template <class X, class S>
constexpr auto divide_parts(const X& x, const S& s) {
    using P = parts_t<X>;
    return [&]<std::size_t... K>(std::index_sequence<K...>) {
        const P p = parts_of(x);
        if constexpr ((is_expected_v<decltype(std::get<K>(p) / s)> || ...)) {
            const auto q = std::tuple{as_expected(std::get<K>(p) / s)...};
            using R      = decltype(mv_sum(multivector<>{}, as_part(*std::get<K>(q))...));
            errc e{};
            if (((!std::get<K>(q) && (e = std::get<K>(q).error(), true)) || ...))
                return std::expected<R, errc>{std::unexpect, e};
            return std::expected<R, errc>{mv_sum(multivector<>{}, as_part(*std::get<K>(q))...)};
        } else
            return mv_sum(multivector<>{}, as_part(std::get<K>(p) / s)...);
    }(std::make_index_sequence<std::tuple_size_v<P>>{});
}

template <class X>
inline constexpr bool is_zero_multivector_v = std::same_as<std::remove_cvref_t<X>, multivector<>>;

// A part that is 0.
template <class P>
constexpr bool part_is_zero(const P& p) {
    if constexpr (quantity_like<P>)
        return p.numerical_value() == just<0>;
    else
        return p == just<0>;
}

template <class A, class B>
constexpr bool mv_equal(const A& a, const B& b) {
    using PA                   = parts_t<A>;
    using PB                   = parts_t<B>;
    static constexpr auto plan = make_merge_plan<PA, PB>();
    const PA              pa   = parts_of(a);
    const PB              pb   = parts_of(b);
    return [&]<std::size_t... K>(std::index_sequence<K...>) {
        return ([&] {
            constexpr merge_item it = plan.Items[K];
            if constexpr (it.A >= 0 && it.B >= 0)
                return std::get<it.A>(pa) == std::get<it.B>(pb);
            else if constexpr (it.A >= 0)
                return part_is_zero(std::get<it.A>(pa));
            else
                return part_is_zero(std::get<it.B>(pb));
        }() && ...);
    }(std::make_index_sequence<plan.Size>{});
}

// The involutions: a sign per grade.
template <int (*Sign)(int), class X>
constexpr auto graded_sign(const X& x) {
    return each_part(x, [](const auto& p) {
        if constexpr (Sign(grade_of(part_dim<std::remove_cvref_t<decltype(p)>>())) > 0)
            return p;
        else
            return as_part(-p);
    });
}

// The adjoint whose product with x is a scalar for every value of x's type,
// if one is: for each pair of parts i < j, the two cross terms B_i·α(B_j) and
// B_j·α(B_i) must cancel. reverse first, then the Clifford conjugate (which
// also covers G(1,0) and the dual numbers).
enum class adjoint_kind : unsigned char { none, reverse, conjugate };
template <class X>
consteval adjoint_kind adjoint_of() {
    constexpr auto d       = dims_of<parts_t<X>>();
    const auto     cancels = [&](int (*sign)(int)) {
        for (std::size_t i = 0; i < d.size(); ++i)
            for (std::size_t j = i + 1; j < d.size(); ++j) {
                const blade x = blade_of(d[i]), y = blade_of(d[j]);
                const int   p = blade_sign(x, y), q = blade_sign(y, x);
                if (p * sign(static_cast<int>(y.size())) + q * sign(static_cast<int>(x.size())) != 0)
                    return false;
            }
        return true;
    };
    if (cancels(reverse_sign))
        return adjoint_kind::reverse;
    if (cancels(conjugate_sign))
        return adjoint_kind::conjugate;
    return adjoint_kind::none;
}
template <adjoint_kind K, class X>
constexpr auto adjoint(const X& x) {
    if constexpr (K == adjoint_kind::reverse)
        return graded_sign<reverse_sign>(x);
    else
        return graded_sign<conjugate_sign>(x);
}
// Σ partᵢ·α(partᵢ): the scalar x·α(x) when the cross terms cancel.
template <adjoint_kind K, class X>
constexpr auto adjoint_norm(const X& x) {
    return each_part(x, [](const auto& p) { return as_part(p * adjoint<K>(p)); });
}
template <class X>
concept invertible = multivector_like<X> && (adjoint_of<std::remove_cvref_t<X>>() != adjoint_kind::none) &&
                     !is_zero_multivector_v<decltype(adjoint_norm<adjoint_of<std::remove_cvref_t<X>>()>(
                         std::declval<const std::remove_cvref_t<X>&>()))>;

// A unit spelled by the basis vectors of d's blade, in name order: its
// canonical blade (magnitude 1).
consteval unit canonical_blade_unit(const dimension& d) {
    unit u{};
    for (const base_dimension* v : blade_of(d))
        unit_add(u, unit_atom{v->Vector, 0, 1});
    return u;
}
// d's blade outside the blade of I (I ∖ A).
consteval dimension blade_rest(const dimension& i, const dimension& a) {
    return dimension_of(canonical_blade_unit(i)) / dimension_of(canonical_blade_unit(a));
}
consteval bool blade_within(const dimension& a, const dimension& i) {
    blade x = blade_of(a);
    blade_mul(x, blade_of(i));
    return x.size() + blade_of(a).size() == blade_of(i).size();
}
// Where the part of type T comes from in the parts PX: its index, or -1.
template <class T, class PX>
consteval int source_of() {
    constexpr auto dx = dims_of<PX>();
    for (std::size_t j = 0; j < dx.size(); ++j)
        if (blade_equal(part_dim<T>(), dx[j]))
            return static_cast<int>(j);
    return -1;
}
// The number type of a part.
template <class T>
struct part_number {
    using type = T;
};
template <reference R, class V>
struct part_number<quantity<R, V>> {
    using type = V;
};
// Whether part T is made from its source in PX (or from 0), with policies.
template <class T, class PX, class... Pol>
consteval bool part_from() {
    constexpr int from = source_of<T, PX>();
    if constexpr (from >= 0)
        return std::constructible_from<T, const std::tuple_element_t<from, PX>&, const Pol&...>;
    else
        return std::constructible_from<T, decltype(just<0>), const Pol&...>;
}
template <class T, class PX>
consteval bool part_from_implicitly() {
    constexpr int from = source_of<T, PX>();
    if constexpr (from >= 0)
        return std::convertible_to<const std::tuple_element_t<from, PX>&, T>;
    else
        return std::convertible_to<decltype(just<0>), typename part_number<T>::type>;
}

// The blade atoms of a unit, unscaled: e₁ e₂ of m e₁ e₂.
consteval unit blade_atoms(const unit& u) {
    unit b{};
    for (std::int8_t i = 0; i < u.Size; ++i)
        if (is_blade_atom(u.Atoms[i]))
            unit_add(b, u.Atoms[i]);
    return b;
}
// Part p in unit U times p's blade: 2 km e₁ in m is 2000 m e₁.
template <unit U, class P>
constexpr auto part_in(const P& p) {
    if constexpr (quantity_like<P>)
        return p.template in<unit_mul(U, blade_atoms(P::reference.U), 1)>();
    else
        return quantity<reference_of(unit{}), P>{p}.template in<U>();
}
template <unit U, class P>
concept part_in_unit = requires(const P& p) { part_in<U>(p); };

// u without its blade atoms (its scale kept), and the sign and size of the
// blade's spelling: u = factor · physical_atoms(u) · canonical blade.
consteval unit physical_atoms(const unit& u) {
    unit r{};
    r.Scale = u.Scale;
    for (std::int8_t i = 0; i < u.Size; ++i)
        if (!is_blade_atom(u.Atoms[i]))
            unit_add(r, u.Atoms[i]);
    return r;
}
consteval grid_rational blade_factor(const unit& u) {
    return (magnitude_of(u) / magnitude_of(physical_atoms(u))).Ratio;
}
template <class P>
consteval unit unit_of_part() {
    if constexpr (quantity_like<P>)
        return P::reference.U;
    else
        return unit{};
}
template <class P>
constexpr const auto& number_of_part(const P& p) {
    if constexpr (quantity_like<P>)
        return p.numerical_value();
    else
        return p;
}
// p's coefficient times F on the canonical blade B, in p's unit without its
// blade: no division, so a blade with a null vector moves too.
template <dimension B, grid_rational F, class P>
constexpr auto move_to_blade(const P& p) {
    static constexpr unit u = unit_of_part<P>();
    constexpr unit        r = [] {
        unit t = physical_atoms(u);
        for (const base_dimension* v : blade_of(B))
            unit_add(t, unit_atom{v->Vector, 0, 1});
        return t;
    }();
    constexpr grid_rational f = blade_factor(u) * F;
    return as_part(make_quantity<reference_of(r)>(number_of_part(p) * just<f>));
}

// A pseudoscalar argument: a unit of a blade only.
template <unit I>
concept pseudoscalar = has_blade(dimension_of(I)) && same_outside_blade(dimension_of(I), dim_one);
} // namespace detail

//---------------------------------------------------------------------------
// multivector<Parts...> — a sum of parts of different blades: insides (the
// scalar part, when it is a plain number) and quantities, in canonical order
// (by grade, then by the names of the basis vectors), all of one dimension
// but for their blades. Results with one part are that part, with none the
// zero multivector<>.
//---------------------------------------------------------------------------
template <class... Parts>
class multivector {
    static_assert(sizeof...(Parts) >= 2,
                  "units: a multivector has two parts or more - one part is the quantity itself");
    static_assert(
        (detail::is_part<Parts>() && ...),
        "units: a multivector's parts are insides and quantities (a plain dimensionless number is an inside)");
    static_assert(
        detail::canonical_parts<Parts...>(),
        "units: a multivector's parts have distinct blades, by grade then by name, and one dimension but for "
        "their blades");

    std::tuple<Parts...> P{};

    template <class X, class... Pol>
    static constexpr std::tuple<Parts...> embed(const X& x, const Pol&... pol) {
        using PX    = detail::parts_t<X>;
        const PX px = detail::parts_of(x);
        return [&]<std::size_t... K>(std::index_sequence<K...>) {
            return std::tuple<Parts...>{[&] {
                using T            = Parts...[K];
                constexpr int from = detail::source_of<T, PX>();
                if constexpr (from >= 0)
                    return T(std::get<from>(px), pol...);
                else
                    return T(just<0>, pol...);
            }()...};
        }(std::make_index_sequence<sizeof...(Parts)>{});
    }

    // Each of X's blades is one of these (of the same dimension), and each
    // part is made from X's part of its blade, or from 0.
    template <class X, class... Pol>
    static consteval bool embeds() {
        using PX = detail::parts_t<X>;
        for (const dimension& d : detail::dims_of<PX>()) {
            bool found = false;
            for (const dimension& m : {detail::part_dim<Parts>()...})
                found = found || (detail::blade_equal(d, m) && detail::same_outside_blade(d, m));
            if (!found)
                return false;
        }
        return (detail::part_from<Parts, PX, Pol...>() && ...);
    }
    template <class X>
    static consteval bool embeds_implicitly() {
        return (detail::part_from_implicitly<Parts, detail::parts_t<X>>() && ...);
    }

  public:
    using parts_type = std::tuple<Parts...>;

    constexpr multivector() = default;

    // From its parts, in order: multivector<inside<…>, quantity<i, …>>{3, 4}.
    template <class... A>
        requires(sizeof...(A) == sizeof...(Parts) && (std::constructible_from<Parts, A> && ...))
    constexpr explicit(!(std::convertible_to<A, Parts> && ...)) multivector(A&&... a)
        : P{Parts(std::forward<A>(a))...} {}

    // From any element whose blades are among these; the parts it lacks are
    // 0. Implicit when each part converts implicitly.
    template <multivector_like X>
        requires(!std::same_as<std::remove_cvref_t<X>, multivector>) && (embeds<X>())
    constexpr explicit(!embeds_implicitly<X>()) multivector(const X& x) : P{embed(x)} {}

    // The same with a one-shot policy for each part's store: {x, clamp}.
    template <multivector_like X, class Pol>
        requires(beman::inside::detail::flag_like<Pol> || beman::inside::detail::policy_like<Pol>) &&
                (embeds<X, Pol>())
    constexpr multivector(const X& x, const Pol& pol) : P{embed(x, pol)} {}

    [[nodiscard]] constexpr const parts_type& parts() const noexcept { return P; }

    // Each part in unit U (a unit without blades), its blade kept, exactly:
    // (V / Z).in(si::ampere).
    template <units::unit U>
        requires(!detail::has_blade(dimension_of(U))) && (detail::part_in_unit<U, Parts> && ...)
    [[nodiscard]] constexpr auto in(unit_t<U>) const {
        return std::apply([](const auto&... p) { return detail::collapse(detail::part_in<U>(p)...); }, P);
    }

    [[nodiscard]] constexpr auto operator+() const { return *this; }
    [[nodiscard]] constexpr auto operator-() const { return detail::mv_negate(*this); }
};

namespace detail {
// The operands of a multivector operation: elements, at least one of them a
// multivector (sums and products of plain parts are the units' own).
template <class A, class B>
concept mv_operands = multivector_like<A> && multivector_like<B> &&
                      (is_multivector_v<std::remove_cvref_t<A>> || is_multivector_v<std::remove_cvref_t<B>>);
template <class A, class B>
concept mv_addable = mergeable<std::remove_cvref_t<A>, std::remove_cvref_t<B>>();
// Subsumes the deleted overloads of units/core.hpp.
template <reference A>
concept geoalg_included = true;
} // namespace detail

//---------------------------------------------------------------------------
// Sums: per blade, the parts of one blade added in their common unit.
//---------------------------------------------------------------------------
template <class A, class B>
    requires detail::mv_operands<A, B> && detail::mv_addable<A, B>
[[nodiscard]] constexpr auto operator+(const A& a, const B& b) {
    return detail::mv_add(a, b);
}
template <class A, class B>
    requires detail::mv_operands<A, B> && detail::mv_addable<A, B>
[[nodiscard]] constexpr auto operator-(const A& a, const B& b) {
    return detail::mv_add(a, detail::mv_negate(b));
}
template <class A, class B>
    requires detail::mv_operands<A, B> && detail::mv_addable<A, B>
[[nodiscard]] constexpr bool operator==(const A& a, const B& b) {
    return detail::mv_equal(a, b);
}

template <class A, class B>
    requires detail::mv_operands<A, B> && (!detail::mv_addable<A, B>)
void operator+(const A&, const B&) = delete ("units: adding multivectors of different dimensions (outside their "
                                             "blades), or parts of one blade that do not add");
template <class A, class B>
    requires detail::mv_operands<A, B> && (!detail::mv_addable<A, B>)
void operator-(const A&, const B&) = delete ("units: subtracting multivectors of different dimensions (outside "
                                             "their blades), or parts of one blade that do not add");

// A number and a quantity, as elements: i·i == just<-1>, (2 e₁)² == just<4>.
template <insidable V, reference A, class VA>
    requires detail::mv_addable<V, quantity<A, VA>>
[[nodiscard]] constexpr bool operator==(const V& v, const quantity<A, VA>& q) {
    return detail::mv_equal(v, q);
}

// Two quantities of different blades, and a number and a quantity of a
// blade: 3 + 4 i, 2 m e₁ + 1 m e₂.
template <reference A, class VA, reference B, class VB>
    requires detail::blade_sum<A, B> && detail::geoalg_included<A>
[[nodiscard]] constexpr auto operator+(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::mv_add(a, b);
}
template <reference A, class VA, reference B, class VB>
    requires detail::blade_sum<A, B> && detail::geoalg_included<A>
[[nodiscard]] constexpr auto operator-(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::mv_add(a, -b);
}
template <reference A, class VA, reference B, class VB>
    requires detail::blade_sum<A, B> && detail::geoalg_included<A>
[[nodiscard]] constexpr bool operator==(const quantity<A, VA>& a, const quantity<B, VB>& b) {
    return detail::mv_equal(a, b);
}
template <insidable V, reference A, class VA>
    requires detail::number_blade_sum<A> && detail::geoalg_included<A>
[[nodiscard]] constexpr auto operator+(const V& v, const quantity<A, VA>& a) {
    return detail::mv_add(v, a);
}
template <reference A, class VA, insidable V>
    requires detail::number_blade_sum<A> && detail::geoalg_included<A>
[[nodiscard]] constexpr auto operator+(const quantity<A, VA>& a, const V& v) {
    return detail::mv_add(a, v);
}
template <insidable V, reference A, class VA>
    requires detail::number_blade_sum<A> && detail::geoalg_included<A>
[[nodiscard]] constexpr auto operator-(const V& v, const quantity<A, VA>& a) {
    return detail::mv_add(v, -a);
}
template <reference A, class VA, insidable V>
    requires detail::number_blade_sum<A> && detail::geoalg_included<A>
[[nodiscard]] constexpr auto operator-(const quantity<A, VA>& a, const V& v) {
    return detail::mv_add(a, detail::as_part(-v));
}

//---------------------------------------------------------------------------
// Products. The geometric product of two quantities is the units' own;
// these take multivectors.
//---------------------------------------------------------------------------
template <class A, class B>
    requires detail::mv_operands<A, B>
[[nodiscard]] constexpr auto operator*(const A& a, const B& b) {
    return detail::mv_product<detail::product_rule::geometric>(a, b);
}
// Each part times a unit: (3 + 4 i) * si::ohm.
template <class... P, class Tag>
    requires detail::is_reference_tag_v<Tag>
[[nodiscard]] constexpr auto operator*(const multivector<P...>& x, Tag t) {
    return detail::each_part(x, [t](const auto& p) { return detail::as_part(p * t); });
}

// The outer product a ∧ b: the pairs of blades with no basis vector in common.
template <multivector_like A, multivector_like B>
[[nodiscard]] constexpr auto wedge(const A& a, const B& b) {
    return detail::mv_product<detail::product_rule::wedge>(a, b);
}
// The left contraction a ⌋ b (the pairs with A ⊆ B, of grade |B| - |A|) and
// the right one a ⌊ b (B ⊆ A).
template <multivector_like A, multivector_like B>
[[nodiscard]] constexpr auto left_contraction(const A& a, const B& b) {
    return detail::mv_product<detail::product_rule::left_contraction>(a, b);
}
template <multivector_like A, multivector_like B>
[[nodiscard]] constexpr auto right_contraction(const A& a, const B& b) {
    return detail::mv_product<detail::product_rule::right_contraction>(a, b);
}
// The scalar product ⟨a b⟩₀.
template <multivector_like A, multivector_like B>
[[nodiscard]] constexpr auto scalar_product(const A& a, const B& b) {
    return detail::mv_product<detail::product_rule::scalar>(a, b);
}

//---------------------------------------------------------------------------
// Involutions, grades and parts.
//---------------------------------------------------------------------------
// The reverse x̃ (grade k times (-1)^(k(k-1)/2)): the conjugate of a complex
// number or a quaternion.
template <multivector_like X>
[[nodiscard]] constexpr auto reverse(const X& x) {
    return detail::graded_sign<detail::reverse_sign>(x);
}
// The grade involution x̂ ((-1)^k).
template <multivector_like X>
[[nodiscard]] constexpr auto involute(const X& x) {
    return detail::graded_sign<detail::involute_sign>(x);
}
// The Clifford conjugate x̄ ((-1)^(k(k+1)/2)).
template <multivector_like X>
[[nodiscard]] constexpr auto conjugate(const X& x) {
    return detail::graded_sign<detail::conjugate_sign>(x);
}
// The parts of grade K.
template <int K, multivector_like X>
[[nodiscard]] constexpr auto grade(const X& x) {
    return detail::each_part(x, [](const auto& p) {
        if constexpr (detail::grade_of(detail::part_dim<std::remove_cvref_t<decltype(p)>>()) == K)
            return p;
        else
            return multivector<>{};
    });
}
// The coefficient of blade u (a unit of a blade only, or `one`): the part of
// that blade divided by u, so 2 m for 2 m e₁ and u = e1, the sign of u's
// spelling included. The zero when x has no such part.
template <multivector_like X, unit U>
    requires(detail::same_outside_blade(dimension_of(U), dim_one))
[[nodiscard]] constexpr auto part(const X& x, unit_t<U>) {
    return detail::each_part(x, [](const auto& p) {
        if constexpr (detail::blade_equal(detail::part_dim<std::remove_cvref_t<decltype(p)>>(), dimension_of(U)))
            return detail::move_to_blade<dim_one, grid_rational{1} / magnitude_of(U).Ratio>(p);
        else
            return multivector<>{};
    });
}

//---------------------------------------------------------------------------
// Norms, inverses and division.
//---------------------------------------------------------------------------
// ⟨x x̃⟩₀ = Σ partᵢ·reverse(partᵢ): |z|² of a complex number z, |q|² of a
// quaternion, |v|² of a Euclidean vector.
template <multivector_like X>
[[nodiscard]] constexpr auto squared_norm(const X& x) {
    return detail::adjoint_norm<detail::adjoint_kind::reverse>(x);
}

// x⁻¹ = α(x) / (x α(x)), α the reverse or the Clifford conjugate: offered when
// x α(x) is a scalar for every value of x's type (complex numbers,
// quaternions, rotors, vectors, dual numbers). Like `/`, an expected when
// the norm's grid holds 0.
template <multivector_like X>
    requires detail::invertible<X>
[[nodiscard]] constexpr auto inverse(const X& x) {
    constexpr auto k = detail::adjoint_of<std::remove_cvref_t<X>>();
    return detail::divide_parts(detail::adjoint<k>(x), detail::adjoint_norm<k>(x));
}
template <multivector_like X>
    requires(!detail::invertible<X>)
void inverse(const X&) = delete ("units: x times its reverse (or conjugate) is no scalar for every value of this type "
                                 "- no inverse of the form x~ / (x x~)");

// a / b: by a part (each part divided), or by a multivector (a · b⁻¹).
template <class A, class B>
    requires detail::mv_operands<A, B>
[[nodiscard]] constexpr auto operator/(const A& a, const B& b) {
    if constexpr (!detail::is_multivector_v<B>)
        return detail::divide_parts(a, b);
    else {
        static_assert(detail::invertible<B>, "units: the divisor has no inverse of the form x~ / (x x~)");
        const auto inv = inverse(b);
        if constexpr (detail::is_expected_v<std::remove_cvref_t<decltype(inv)>>)
            return inv.transform([&](const auto& i) { return a * i; });
        else
            return a * inv;
    }
}

//---------------------------------------------------------------------------
// Duals.
//---------------------------------------------------------------------------
// x · I⁻¹ for a pseudoscalar unit I that squares to ±1: the cross product is
// dual(u ∧ v, e₁₂₃) up to its sign convention.
template <multivector_like X, unit I>
    requires detail::pseudoscalar<I>
[[nodiscard]] constexpr auto dual(const X& x, unit_t<I> i) {
    static_assert(detail::blade_square(dimension_of(I)) != 0,
                  "units: dual - the pseudoscalar squares to 0 (a null vector): use complement(x, I)");
    return detail::each_part(x, [i](const auto& p) { return detail::as_part(p / i); });
}

// The right complement within the blade of I, without the metric: A ∧ c(A) = I
// for each basis blade A of I. With null vectors (PGA) it stands in for the
// dual.
template <multivector_like X, unit I>
    requires detail::pseudoscalar<I>
[[nodiscard]] constexpr auto complement(const X& x, unit_t<I>) {
    return detail::each_part(x, [](const auto& p) {
        constexpr dimension a = detail::part_dim<std::remove_cvref_t<decltype(p)>>();
        static_assert(detail::blade_within(a, dimension_of(I)), "units: complement - a part outside I's blade");
        constexpr dimension rest = detail::blade_rest(dimension_of(I), a);
        return detail::move_to_blade<rest, magnitude_of(I).Ratio * grid_rational{detail::blade_sign(a, rest)}>(p);
    });
}
// The inverse of complement: uncomplement(complement(x, I), I) == x.
template <multivector_like X, unit I>
    requires detail::pseudoscalar<I>
[[nodiscard]] constexpr auto uncomplement(const X& x, unit_t<I>) {
    return detail::each_part(x, [](const auto& p) {
        constexpr dimension b = detail::part_dim<std::remove_cvref_t<decltype(p)>>();
        static_assert(detail::blade_within(b, dimension_of(I)), "units: uncomplement - a part outside I's blade");
        constexpr dimension a = detail::blade_rest(dimension_of(I), b);
        return detail::
            move_to_blade<a, grid_rational{1} / (magnitude_of(I).Ratio * grid_rational{detail::blade_sign(a, b)})>(p);
    });
}
// The regressive product a ∨ b = c⁻¹(c(a) ∧ c(b)): the join of PGA.
template <multivector_like A, multivector_like B, unit I>
    requires detail::pseudoscalar<I>
[[nodiscard]] constexpr auto regressive(const A& a, const B& b, unit_t<I> i) {
    return uncomplement(wedge(complement(a, i), complement(b, i)), i);
}

} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_GEOALG_HPP
