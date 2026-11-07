// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_MDSPAN_HPP
#define BEMAN_INSIDE_INSIDE_MDSPAN_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <mdspan>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

//---------------------------------------------------------------------------
// std::mdspan with insides (opt-in: include this header).
//
//   extents_of<I, J>            std::extents from index types: inside<{0, 479}>
//                               is an extent of 480
//   extent_index_t<E, r>        inside<{0, N − 1}> for E's static extent N at rank r
//   indices(m)                  every index tuple, the last index fastest; a
//                               static rank yields extent_index_t insides
//   inside_mdspan<T, E, L, A>   a std::mdspan that also takes inside indices:
//                               one whose grid lies within a static extent
//                               needs no check; any other is held to the
//                               extent by its own policy (clamp pads at the
//                               edges, wrap is periodic, checked reports
//                               overflow, unsafe trusts it)
//   value_accessor<E, T>        E's numbers stored as values of T (a double
//                               or int16_t buffer), read by E's policy
//   raw_accessor<E, R>          E's numbers stored as their raws (fixed-point
//                               words, ADC counts), a raw outside the grid
//                               held by the policy
//   sum(m), dot(x, y)           exact; with static extents the result type is
//                               deduced (N·[Lower, Upper]), else sum<T>(m), dot<T>(x, y)
//   matrix_vector_product(A, x, y), matrix_product(A, B, C)
//                               each output element the exact Σ of products,
//                               stored once by the output's policy
//   transform_into(in, out, f), transform_into(in1, in2, out, f)
//                               out[idx] = f(in[idx]…), stored by out's policy
//                               (not `transform`: ADL on std::mdspan would
//                               find std::transform(first, last, out, f))
// Quantities: <beman/inside/units/mdspan.hpp>.
//---------------------------------------------------------------------------
namespace beman::inside {
namespace detail {
// An index type: an inside on 0, 1, …, N − 1 (or the point 0).
template <class I>
concept index_inside =
    insidable<I> && integer_lattice<I> && lower_of<I> == 0 && (notch_of<I> == 1 || upper_of<I> == 0) &&
    upper_of<I> < grid_rational{std::numeric_limits<imax>::max()};

template <class I>
inline constexpr std::size_t extent_of_index = [] {
    if constexpr (index_inside<I>)
        return static_cast<std::size_t>(grid_numerator(upper_of<I>)) + 1;
    else
        return std::size_t{0};
}();

template <class... Is>
struct extents_of_impl {
    static_assert((index_inside<Is> && ...),
                  "extents_of<I...>: each I is an index type - an inside on 0, 1, ..., N - 1 (inside<{0, 479}>)");
    using type = std::extents<std::size_t, extent_of_index<Is>...>;
};

// 0 … n − 1 in steps of 1 (inside<{0, n − 1}>, also for n = 1).
consteval grid index_range_grid(std::size_t n) {
    return grid{interval{grid_rational{0}, grid_rational{n - 1}}, grid_rational{1}};
}

template <class E>
inline constexpr bool is_extents_v = false;
template <class I, std::size_t... Ns>
inline constexpr bool is_extents_v<std::extents<I, Ns...>> = true;

template <class M>
inline constexpr bool is_mdspan_v = false;
template <class T, class E, class L, class A>
inline constexpr bool is_mdspan_v<std::mdspan<T, E, L, A>> = true;
} // namespace detail

// The extents whose indices are the index types Is.
template <class... Is>
using extents_of = typename detail::extents_of_impl<Is...>::type;

// The index type of E's static extent N ≥ 1 at rank R: inside<{0, N − 1}>.
template <class E, std::size_t R>
    requires detail::is_extents_v<E> && (R < E::rank()) && (E::static_extent(R) != std::dynamic_extent) &&
                 (E::static_extent(R) > 0)
using extent_index_t = inside<detail::index_range_grid(E::static_extent(R))>;

// Any std::mdspan, and so an inside_mdspan, which derives from one.
template <class M>
concept mdspan_like =
    requires(const M& m) { []<class T, class E, class L, class A>(const std::mdspan<T, E, L, A>&) {}(m); };

//---------------------------------------------------------------------------
// indices
//---------------------------------------------------------------------------
namespace detail {
template <class E, std::size_t R>
constexpr auto rank_indices(const E& e) {
    if constexpr (E::static_extent(R) != std::dynamic_extent && E::static_extent(R) > 0) {
        // 0 … N − 1 as index insides (an iota, not values<I>: values may
        // start mid-ring, and that wrap costs every step).
        using I = extent_index_t<E, R>;
        return std::views::iota(std::size_t{0}, E::static_extent(R)) |
               std::views::transform([](std::size_t k) { return I::from_raw(raw_from_offset<I>(k)); });
    } else
        return std::views::iota(typename E::index_type{0}, e.extent(R));
}
} // namespace detail

// Every index tuple of e, the last index fastest (layout_right's order):
// `for (auto [i, j] : indices(m))`. A static rank yields its extent_index_t,
// so indexing an inside_mdspan with them needs no check.
template <class I, std::size_t... Ns>
[[nodiscard]] constexpr auto indices(const std::extents<I, Ns...>& e) {
    using E = std::extents<I, Ns...>;
    return [&]<std::size_t... R>(std::index_sequence<R...>) {
        return std::views::cartesian_product(detail::rank_indices<E, R>(e)...);
    }(std::make_index_sequence<E::rank()>{});
}
template <class T, class E, class L, class A>
[[nodiscard]] constexpr auto indices(const std::mdspan<T, E, L, A>& m) {
    return indices(m.extents());
}

//---------------------------------------------------------------------------
// inside_mdspan
//---------------------------------------------------------------------------
namespace detail {
// An index argument: a builtin integer (std semantics), or an inside on the
// integers whose values fit imax.
template <class I>
concept span_index = std::integral<I> || (insidable<I> && integer_lattice<I> && values_fit_imax<I>);

// The index at rank R of e for i: an integer as is; an inside proven within
// a static extent as its value; any other inside held to [0, extent − 1] by
// its policy.
template <std::size_t R, class E, class I>
constexpr typename E::index_type extent_index(const E& e, const I& i) {
    using index_type = typename E::index_type;
    if constexpr (std::integral<I>)
        return static_cast<index_type>(i);
    else {
        constexpr std::size_t S = E::static_extent(R);
        if constexpr (S != std::dynamic_extent && S > 0 && lower_of<I> >= 0 && upper_of<I> <= grid_rational{S - 1})
            return static_cast<index_type>(to_value(i));
        else {
            const imax v = to_value(i);
            const imax n = static_cast<imax>(e.extent(R));
            if (v >= 0 && v < n) [[likely]]
                return static_cast<index_type>(v);
            if constexpr (has_flag(flags_of<I>, unsafe))
                return static_cast<index_type>(v);
            else {
                if constexpr (has_flag(flags_of<I>, clamp)) {
                    if (n > 0)
                        return static_cast<index_type>(v < 0 ? 0 : n - 1);
                } else if constexpr (has_flag(flags_of<I>, wrap)) {
                    if (n > 0) {
                        const imax r = v % n;
                        return static_cast<index_type>(r < 0 ? r + n : r);
                    }
                }
                raise(errc::overflow, "inside_mdspan: an index past the extent");
            }
        }
    }
}
} // namespace detail

template <class T, class Extents, class Layout = std::layout_right, class Accessor = std::default_accessor<T>>
class inside_mdspan : public std::mdspan<T, Extents, Layout, Accessor> {
    using base = std::mdspan<T, Extents, Layout, Accessor>;

  public:
    using typename base::accessor_type;
    using typename base::data_handle_type;
    using typename base::element_type;
    using typename base::extents_type;
    using typename base::index_type;
    using typename base::layout_type;
    using typename base::mapping_type;
    using typename base::reference;
    using typename base::value_type;

    using base::base;
    constexpr inside_mdspan() = default;
    constexpr inside_mdspan(const base& m) : base(m) {}

    // Builtin integer indices: std::mdspan's (a precondition, no check).
    using base::operator[];

    // Inside indices (builtin integers may mix in): proven within a static
    // extent, no check; else held to the extent by each index's policy.
    template <class... Is>
        requires(sizeof...(Is) == Extents::rank()) && (insidable<Is> || ...) && (detail::span_index<Is> && ...)
    [[nodiscard]] constexpr reference operator[](const Is&... is) const {
        return [&]<std::size_t... R>(std::index_sequence<R...>) -> reference {
            return base::operator[](detail::extent_index<R>(this->extents(), is)...);
        }(std::index_sequence_for<Is...>{});
    }
#define BEMAN_INSIDE_WHY_span_index \
    "inside_mdspan: an index is a builtin integer or an inside on the integers within int64"
    template <class... Is>
        requires(sizeof...(Is) == Extents::rank()) && (insidable<Is> || ...) && (!(detail::span_index<Is> && ...)) &&
                    ((insidable<Is> || std::integral<Is>) && ...)
    reference operator[](const Is&...) const = delete (BEMAN_INSIDE_WHY_span_index);
#undef BEMAN_INSIDE_WHY_span_index
};

template <class ElementType, class... Integrals>
    requires(std::is_convertible_v<Integrals, std::size_t> && ...) && (sizeof...(Integrals) > 0)
explicit inside_mdspan(ElementType*, Integrals...)
    -> inside_mdspan<ElementType, std::dextents<std::size_t, sizeof...(Integrals)>>;
template <class ElementType, class IndexType, std::size_t... Ns>
inside_mdspan(ElementType*, const std::extents<IndexType, Ns...>&)
    -> inside_mdspan<ElementType, std::extents<IndexType, Ns...>>;
template <class ElementType, class Mapping>
    requires requires { typename Mapping::layout_type; }
inside_mdspan(ElementType*, const Mapping&)
    -> inside_mdspan<ElementType, typename Mapping::extents_type, typename Mapping::layout_type>;
template <class Mapping, class Accessor>
inside_mdspan(const typename Accessor::data_handle_type&, const Mapping&, const Accessor&)
    -> inside_mdspan<typename Accessor::element_type,
                     typename Mapping::extents_type,
                     typename Mapping::layout_type,
                     Accessor>;
// (By value: a prvalue mdspan, such as submdspan's, deduces too.)
template <class T, class E, class L, class A>
inside_mdspan(std::mdspan<T, E, L, A>) -> inside_mdspan<T, E, L, A>;

//---------------------------------------------------------------------------
// Accessors: E (an inside, or a quantity with units/mdspan.hpp) viewed over
// storage that is not E.
//---------------------------------------------------------------------------
namespace detail {
// What an element is made of: its number (an inside), read and rebuilt;
// rebind<N> is the same kind of element on the number N; total<Target>(r)
// is the exact total of a range of such elements into Target, checked once.
// Specialized for quantities in units/mdspan.hpp.
template <class E>
struct element_traits;
template <insidable E>
struct element_traits<E> {
    using number = E;
    template <insidable N>
    using rebind = N;
    static constexpr const E& number_of(const E& e) noexcept { return e; }
    static constexpr E        make(const number& n) noexcept { return n; }
    template <class Rng>
    static constexpr E total(Rng&& r) {
        return sum<E>(std::forward<Rng>(r));
    }
};
template <class E>
concept mdspan_element = requires { typename element_traits<std::remove_cv_t<E>>::number; };
template <class E>
using element_number_t = typename element_traits<std::remove_cv_t<E>>::number;

// How the storage is read and written: as plain values, or as raws.
enum class storage_kind : unsigned char { value, raw };

// The range of a raw type: a builtin's limits, or a wide_int's bits.
template <class R>
inline constexpr grid_int raw_type_min = [] {
    if constexpr (std::integral<R>)
        return grid_int{std::numeric_limits<R>::min()};
    else
        return R::is_signed ? -(grid_int{1} << (R::bits - 1)) : grid_int{0};
}();
template <class R>
inline constexpr grid_int raw_type_max = [] {
    if constexpr (std::integral<R>)
        return grid_int{std::numeric_limits<R>::max()};
    else
        return (grid_int{1} << (R::is_signed ? R::bits - 1 : R::bits)) - grid_int{1};
}();

template <class E, class T, storage_kind K>
constexpr E read_stored(const T& t) {
    using tr = element_traits<E>;
    using N  = typename tr::number;
    if constexpr (K == storage_kind::value)
        return tr::make(N{t}); // a store by N's policy
    else {
        // A raw outside the grid is held by N's policy; skipped when every
        // raw of the type is a point of the grid. The test runs in a work
        // integer holding the raw type's range and the grid's (a uint64 raw
        // passes imax; a wide raw is a wide_int).
        using R                 = raw_t<N>;
        constexpr grid_int rmin = raw_type_min<R>, rmax = raw_type_max<R>;
        constexpr grid_int lo = raw_lo_exact<N>, hi = raw_hi_exact<N>;
        using W = work_int_t<signed_value_bits({rmin, rmax, lo, hi, hi - lo + grid_int{1}})>;
        if constexpr (rmin == lo && rmax == hi)
            return tr::make(N::from_raw(t));
        else {
            constexpr W wlo = static_cast<W>(lo), whi = static_cast<W>(hi);
            const W     r = static_cast<W>(t);
            if (!(r < wlo) && !(whi < r)) [[likely]]
                return tr::make(N::from_raw(t));
            if constexpr (has_flag(flags_of<N>, unsafe))
                return tr::make(N::from_raw(t));
            else if constexpr (has_flag(flags_of<N>, clamp))
                return tr::make(N::from_raw(static_cast<R>(r < wlo ? wlo : whi)));
            else if constexpr (has_flag(flags_of<N>, wrap))
                return tr::make(N::from_raw(static_cast<R>(wlo + floor_divmod(r - wlo, whi - wlo + W{1}).Rem)));
            else
                raise(errc::overflow, "raw_accessor: a raw outside the grid");
        }
    }
}

template <class T, storage_kind K, class E>
constexpr T write_stored(const E& e) {
    const auto& n = element_traits<E>::number_of(e);
    if constexpr (K == storage_kind::raw)
        return n.raw();
    else if constexpr (std::floating_point<T>)
        return n.template to<T>(); // rounded once
    else
        return n.template as<T>(); // checked: a value past T is reported
}

// A writable element of E stored as T: reads and assigns through E.
template <class E, class T, storage_kind K>
class stored_ref {
    T* P;

  public:
    constexpr explicit stored_ref(T* p) noexcept : P{p} {}
    [[nodiscard]] constexpr E   get() const { return read_stored<E, T, K>(*P); }
    constexpr                   operator E() const { return get(); }
    constexpr const stored_ref& operator=(const E& e) const {
        *P = write_stored<T, K>(e);
        return *this;
    }
    constexpr const stored_ref& operator=(const stored_ref& o) const { return *this = o.get(); }
    template <class X>
        requires(!std::same_as<std::remove_cvref_t<X>, E> && !std::same_as<std::remove_cvref_t<X>, stored_ref> &&
                 std::constructible_from<E, const X&>)
    constexpr const stored_ref& operator=(const X& x) const {
        return *this = E(x);
    }
};

template <class E, class T, storage_kind K>
struct storage_accessor {
    static_assert(mdspan_element<E>, "accessor: E is an inside (or, with units/mdspan.hpp, a quantity)");
    using element_type     = E;
    using data_handle_type = T*;
    using offset_policy    = storage_accessor;
    // Read-only storage gives the element by value, so every operator applies.
    using reference =
        std::conditional_t<std::is_const_v<T>, std::remove_cv_t<E>, stored_ref<std::remove_cv_t<E>, T, K>>;

    constexpr storage_accessor() noexcept = default;
    [[nodiscard]] constexpr reference access(data_handle_type p, std::size_t i) const {
        if constexpr (std::is_const_v<T>)
            return read_stored<std::remove_cv_t<E>, std::remove_cv_t<T>, K>(p[i]);
        else
            return reference{p + i};
    }
    [[nodiscard]] constexpr data_handle_type offset(data_handle_type p, std::size_t i) const noexcept { return p + i; }
};

template <class E>
concept raw_viewable =
    mdspan_element<E> && integer_storage<element_number_t<E>> && !point_storage<element_number_t<E>>;
} // namespace detail

// E's numbers stored as plain values of T (const T: a read-only view).
template <class E, class T>
    requires std::is_arithmetic_v<std::remove_const_t<T>>
using value_accessor = detail::storage_accessor<E, T, detail::storage_kind::value>;

// E's numbers stored as their raws (const R: a read-only view).
template <class E, class R = detail::raw_t<detail::element_number_t<E>>>
    requires detail::raw_viewable<E> &&
                 std::same_as<std::remove_const_t<R>, detail::raw_t<detail::element_number_t<E>>>
using raw_accessor = detail::storage_accessor<E, R, detail::storage_kind::raw>;

//---------------------------------------------------------------------------
// Element access shared by the algorithms below
//---------------------------------------------------------------------------
namespace detail {
template <class M>
using mdspan_value_t = std::remove_cv_t<typename M::element_type>;

// m's element at integer indices, as a value (through any accessor).
template <class M, class... I>
constexpr mdspan_value_t<M> element_at(const M& m, I... i) {
    return static_cast<mdspan_value_t<M>>(m[static_cast<typename M::index_type>(i)...]);
}

// Each rank: equal static extents, or one dynamic (checked at run time).
template <class E1, class E2>
inline constexpr bool extents_compatible = [] {
    if constexpr (E1::rank() != E2::rank())
        return false;
    else
        return [&]<std::size_t... R>(std::index_sequence<R...>) {
            return ((E1::static_extent(R) == E2::static_extent(R) || E1::static_extent(R) == std::dynamic_extent ||
                     E2::static_extent(R) == std::dynamic_extent) &&
                    ...);
        }(std::make_index_sequence<E1::rank()>{});
}();
// Extents R1 of E1 and R2 of E2 compatible.
template <class E1, std::size_t R1, class E2, std::size_t R2>
inline constexpr bool extent_compatible =
    E1::static_extent(R1) == E2::static_extent(R2) || E1::static_extent(R1) == std::dynamic_extent ||
    E2::static_extent(R2) == std::dynamic_extent;

constexpr void require_extent(bool same, const char* what) {
    if (!same) [[unlikely]]
        raise(errc::domain_error, what);
}

// Calls f(i0, i1, …) for every integer index tuple of e, the last fastest.
template <std::size_t R = 0, class E, class F, class... I>
constexpr void for_each_index(const E& e, F& f, I... i) {
    if constexpr (R == E::rank())
        f(i...);
    else
        for (typename E::index_type k = 0; k < e.extent(R); ++k)
            for_each_index<R + 1>(e, f, i..., k);
}

template <class E>
inline constexpr bool all_static = E::rank_dynamic() == 0;
template <class E>
inline constexpr std::size_t static_size = [] {
    std::size_t n = 1;
    for (std::size_t r = 0; r < E::rank(); ++r)
        n *= E::static_extent(r);
    return n;
}();

// The sum of n values of g: n·Lower … n·Upper on g's notch.
consteval grid n_fold_grid(const grid& g, std::size_t n) {
    const grid_rational k{n};
    if (n == 0 || g.Interval.Lower == g.Interval.Upper)
        return grid{g.Interval.Lower * k};
    return grid{interval{g.Interval.Lower * k, g.Interval.Upper * k}, g.Notch};
}
template <insidable N, std::size_t Count>
    requires notched<N> || point_grid<N>
using n_fold_sum_t = inside<n_fold_grid(grid_of<N>, Count), flags_of<N> & ~cursor_marker>;
// The same for an element (an inside or a quantity): its kind on that number.
template <class E, std::size_t Count>
using n_fold_t = typename element_traits<E>::template rebind<n_fold_sum_t<element_number_t<E>, Count>>;

// The values of m, as a range: its storage when it is one exhaustive block
// of elements, else each element through m.
template <class M>
constexpr auto element_range(const M& m) {
    using T = typename M::element_type;
    using A = typename M::accessor_type;
    using L = typename M::layout_type;
    if constexpr (std::same_as<A, std::default_accessor<T>> &&
                  (std::same_as<L, std::layout_right> || std::same_as<L, std::layout_left>))
        return std::span<const T>(m.data_handle(), m.size());
    else
        return std::views::iota(std::size_t{0}, m.size()) | std::views::transform([m](std::size_t k) {
                   // k → index tuple, the last index fastest
                   using E = typename M::extents_type;
                   std::array<typename E::index_type, E::rank()> idx{};
                   for (std::size_t r = E::rank(); r-- > 0;) {
                       const auto n = static_cast<std::size_t>(m.extent(r));
                       idx[r]       = static_cast<typename E::index_type>(k % n);
                       k /= n;
                   }
                   return std::apply([&](auto... i) { return element_at(m, i...); }, idx);
               });
}
} // namespace detail

//---------------------------------------------------------------------------
// sum
//---------------------------------------------------------------------------
// The exact total of m's elements into Target (an inside; a quantity with
// units/mdspan.hpp, whose total is converted to Target's unit), checked
// once by Target's policy.
template <class Target, class T, class E, class L, class A>
    requires detail::mdspan_element<T> && detail::mdspan_element<Target>
[[nodiscard]] constexpr Target sum(const std::mdspan<T, E, L, A>& m) {
    return detail::element_traits<Target>::total(detail::element_range(m));
}

// All extents static: the result type is the N-fold sum of the element's
// grid, N the element count, so it holds every total.
template <class T, class E, class L, class A>
    requires detail::mdspan_element<T> && detail::all_static<E> &&
             (detail::notched<detail::element_number_t<T>> || detail::point_grid<detail::element_number_t<T>>)
[[nodiscard]] constexpr auto sum(const std::mdspan<T, E, L, A>& m) {
    return sum<detail::n_fold_t<std::remove_cv_t<T>, detail::static_size<E>>>(m);
}
template <class T, class E, class L, class A>
    requires detail::mdspan_element<T> && detail::all_static<E> &&
                 (!detail::notched<detail::element_number_t<T>> && !detail::point_grid<detail::element_number_t<T>>)
auto sum(const std::mdspan<T, E, L, A>&) =
    delete ("sum(m): a continuous element has no deduced total type - name one: sum<Target>(m)");

//---------------------------------------------------------------------------
// dot, matrix_vector_product, matrix_product
//---------------------------------------------------------------------------
namespace detail {
template <class M>
concept vector_span = mdspan_like<M> && M::extents_type::rank() == 1 && mdspan_element<mdspan_value_t<M>>;
template <class M>
concept matrix_span = mdspan_like<M> && M::extents_type::rank() == 2 && mdspan_element<mdspan_value_t<M>>;

template <class X, class Y>
using product_t = decltype(std::declval<mdspan_value_t<X>>() * std::declval<mdspan_value_t<Y>>());

// Σ_k a(k)·b(k) over k < n, exactly, into Target.
template <class Target, class FA, class FB, class I>
constexpr Target sum_of_products(I n, FA a, FB b) {
    return element_traits<Target>::total(std::views::iota(I{0}, n) |
                                         std::views::transform([&](I k) { return a(k) * b(k); }));
}
} // namespace detail

template <class Target, detail::vector_span X, detail::vector_span Y>
    requires detail::mdspan_element<Target> &&
             detail::extent_compatible<typename X::extents_type, 0, typename Y::extents_type, 0>
[[nodiscard]] constexpr Target dot(const X& x, const Y& y) {
    detail::require_extent(x.extent(0) == y.extent(0), "dot: the extents differ");
    using I = typename X::index_type;
    return detail::sum_of_products<Target>(
        x.extent(0), [&](I k) { return detail::element_at(x, k); }, [&](I k) { return detail::element_at(y, k); });
}
// Static extents: the result type is the N-fold sum of one product's grid.
template <detail::vector_span X, detail::vector_span Y>
    requires detail::all_static<typename X::extents_type> && detail::all_static<typename Y::extents_type> &&
             (X::static_extent(0) == Y::static_extent(0))
[[nodiscard]] constexpr auto dot(const X& x, const Y& y) {
    return dot<detail::n_fold_t<detail::product_t<X, Y>, X::static_extent(0)>>(x, y);
}
template <detail::vector_span X, detail::vector_span Y>
    requires(!detail::extent_compatible<typename X::extents_type, 0, typename Y::extents_type, 0>)
auto dot(const X&, const Y&) = delete ("dot: the vectors' static extents differ");

// y = A·x: y[i] = Σ_j A[i, j]·x[j], each exact, stored once by y's policy.
template <detail::matrix_span MA, detail::vector_span X, detail::vector_span Y>
    requires detail::extent_compatible<typename MA::extents_type, 1, typename X::extents_type, 0> &&
             detail::extent_compatible<typename MA::extents_type, 0, typename Y::extents_type, 0>
constexpr void matrix_vector_product(const MA& a, const X& x, const Y& y) {
    detail::require_extent(a.extent(1) == x.extent(0) && a.extent(0) == y.extent(0),
                           "matrix_vector_product: the extents differ");
    using Out = detail::mdspan_value_t<Y>;
    using I   = typename MA::index_type;
    for (I i = 0; i < a.extent(0); ++i)
        y[static_cast<typename Y::index_type>(i)] = detail::sum_of_products<Out>(
            a.extent(1),
            [&](I j) { return detail::element_at(a, i, j); },
            [&](I j) { return detail::element_at(x, j); });
}

// C = A·B: C[i, j] = Σ_k A[i, k]·B[k, j], each exact, stored once by C's policy.
template <detail::matrix_span MA, detail::matrix_span MB, detail::matrix_span MC>
    requires detail::extent_compatible<typename MA::extents_type, 1, typename MB::extents_type, 0> &&
             detail::extent_compatible<typename MA::extents_type, 0, typename MC::extents_type, 0> &&
             detail::extent_compatible<typename MB::extents_type, 1, typename MC::extents_type, 1>
constexpr void matrix_product(const MA& a, const MB& b, const MC& c) {
    detail::require_extent(a.extent(1) == b.extent(0) && a.extent(0) == c.extent(0) && b.extent(1) == c.extent(1),
                           "matrix_product: the extents differ");
    using Out = detail::mdspan_value_t<MC>;
    using I   = typename MA::index_type;
    using CI  = typename MC::index_type;
    for (I i = 0; i < a.extent(0); ++i)
        for (I j = 0; j < b.extent(1); ++j)
            c[static_cast<CI>(i), static_cast<CI>(j)] = detail::sum_of_products<Out>(
                a.extent(1),
                [&](I k) { return detail::element_at(a, i, k); },
                [&](I k) { return detail::element_at(b, k, j); });
}

//---------------------------------------------------------------------------
// transform_into
//---------------------------------------------------------------------------
// out[idx] = f(in[idx]) for every index, stored by out's element policy.
template <mdspan_like In, mdspan_like Out, class F>
    requires detail::extents_compatible<typename In::extents_type, typename Out::extents_type> &&
             std::invocable<F&, detail::mdspan_value_t<In>>
constexpr void transform_into(const In& in, const Out& out, F f) {
    detail::require_extent(in.extents() == out.extents(), "transform_into: the extents differ");
    auto each = [&](auto... i) {
        out[static_cast<typename Out::index_type>(i)...] = std::invoke(f, detail::element_at(in, i...));
    };
    detail::for_each_index(in.extents(), each);
}
// out[idx] = f(in1[idx], in2[idx]).
template <mdspan_like In1, mdspan_like In2, mdspan_like Out, class F>
    requires detail::extents_compatible<typename In1::extents_type, typename Out::extents_type> &&
             detail::extents_compatible<typename In2::extents_type, typename Out::extents_type> &&
             std::invocable<F&, detail::mdspan_value_t<In1>, detail::mdspan_value_t<In2>>
constexpr void transform_into(const In1& in1, const In2& in2, const Out& out, F f) {
    detail::require_extent(in1.extents() == out.extents() && in2.extents() == out.extents(),
                           "transform_into: the extents differ");
    auto each = [&](auto... i) {
        out[static_cast<typename Out::index_type>(i)...] =
            std::invoke(f, detail::element_at(in1, i...), detail::element_at(in2, i...));
    };
    detail::for_each_index(in1.extents(), each);
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_MDSPAN_HPP
