// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_UNITS_MDSPAN_HPP
#define BEMAN_INSIDE_UNITS_MDSPAN_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/mdspan.hpp>
#include <beman/inside/units.hpp>

#include <cstddef>
#include <mdspan>
#include <ranges>
#include <type_traits>
#include <utility>

//---------------------------------------------------------------------------
// std::mdspan with quantities (opt-in). Includes inside/mdspan.hpp and makes
// its accessors and algorithms take quantities:
//   value_accessor<Q, T>, raw_accessor<Q>   a buffer of numbers or raws seen
//                                          as quantities of Q
//   sum, dot, matrix_vector_product, matrix_product
//                                          exact totals in the elements' (or
//                                          products') unit, converted to the
//                                          target's unit and checked once
//   units::view_in(m, u)                    the same quantities read in unit u:
//                                          q.in<U>(), the raw unchanged
//---------------------------------------------------------------------------
namespace beman::inside::detail {
// A quantity's number is its rep. Its total into Q: the exact sum of the
// numbers in the elements' unit, on Q's range in that unit, then converted
// to Q (exact, or rounded and checked once by Q's policy).
template <units::quantity_like Q>
struct element_traits<Q> {
    using number = typename Q::rep;
    template <insidable N>
    using rebind = units::quantity<Q::reference, N>;
    static constexpr const number& number_of(const Q& q) noexcept { return units::detail::value_of(q); }
    static constexpr Q             make(const number& n) { return units::detail::quantity_access::make<Q>(n); }
    template <class Rng>
    static constexpr Q total(Rng&& r) {
        using P = std::remove_cvref_t<std::ranges::range_reference_t<Rng>>;
        static_assert(units::quantity_like<P>, "sum: a quantity total of plain numbers - give them a unit");
        using N       = units::detail::rescaled_t<Q::reference, P::reference, number>;
        const N total = sum<N>(std::forward<Rng>(r) |
                               std::views::transform([](const P& p) { return units::detail::value_of(p); }));
        return Q{units::detail::quantity_access::make<units::quantity<P::reference, N>>(total)};
    }
};
} // namespace beman::inside::detail

namespace beman::inside::units {
namespace detail {
// An element of From read in unit U, written back exactly.
template <class From, unit U>
using in_unit_t = decltype(std::declval<const std::remove_cv_t<From>&>().template in<U>());

template <class From, unit U>
class in_unit_ref {
    From* P;

  public:
    using value_type = in_unit_t<From, U>;
    constexpr explicit in_unit_ref(From* p) noexcept : P{p} {}
    [[nodiscard]] constexpr value_type get() const { return P->template in<U>(); }
    constexpr                          operator value_type() const { return get(); }
    constexpr const in_unit_ref&       operator=(const value_type& v) const {
        *P = From{v};
        return *this;
    }
    constexpr const in_unit_ref& operator=(const in_unit_ref& o) const { return *this = o.get(); }
};

template <class From, unit U>
struct in_unit_accessor {
    using element_type     = in_unit_t<From, U>;
    using data_handle_type = From*;
    using offset_policy    = in_unit_accessor;
    using reference        = std::conditional_t<std::is_const_v<From>, element_type, in_unit_ref<From, U>>;

    [[nodiscard]] constexpr reference access(data_handle_type p, std::size_t i) const {
        if constexpr (std::is_const_v<From>)
            return p[i].template in<U>();
        else
            return reference{p + i};
    }
    [[nodiscard]] constexpr data_handle_type offset(data_handle_type p, std::size_t i) const noexcept { return p + i; }
};
} // namespace detail

// m's quantities read in unit u, over the same storage: an element is
// q.in(u) (exact, the raw unchanged), and a store converts back exactly.
template <unit U, class T, class E, class L>
    requires quantity_like<std::remove_cv_t<T>> && requires(const std::remove_cv_t<T>& q) { q.template in<U>(); }
[[nodiscard]] constexpr auto view_in(const std::mdspan<T, E, L, std::default_accessor<T>>& m, unit_t<U>) {
    using A = detail::in_unit_accessor<T, U>;
    return inside_mdspan<typename A::element_type, E, L, A>{m.data_handle(), m.mapping(), A{}};
}
} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_MDSPAN_HPP
