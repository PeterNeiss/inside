// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_INTROSPECTION_HPP
#define INSIDE_INSIDE_DETAIL_INTROSPECTION_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/error.hpp>
#include <inside/inside/grid.hpp>
#include <inside/inside/key.hpp>
#include <inside/inside/policy_flag.hpp>

#include <bit>
#include <cstdint>
#include <limits>

// An inside's template parameters as values (grid_of<B>, policy_of<B>, lower_of<B>, notch_of<B>, …
// and their 64-bit forms), the insidable and numeric concepts,
// and the plumbing every layer uses (plain_t, as_rational, raw_t).
// The storage shapes build on it in inside/detail/storage.hpp.
namespace ins
{
  namespace detail
  {
    template <class>
    inline constexpr bool is_inside_v = false;
    template <key K>
      requires(inside_key<K>)
    inline constexpr bool is_inside_v<id<K>> = true;
  } // namespace detail

  template <typename B>
  concept insidable = detail::is_inside_v<std::remove_cvref_t<B>>;

  // The public introspection of an inside's template parameters.
  // Like std::numeric_limits they report what the grid is, used opaquely
  // (callers never name the rational return type).
  namespace detail
  {
    // The key of an id type: an inside's, a quantity's, a point's.
    template <class T>
    struct key_of_id;
    template <key K>
    struct key_of_id<id<K>>
    {
      static constexpr key value = K;
    };
    template <class T>
    inline constexpr key key_v = key_of_id<std::remove_cvref_t<T>>::value;
  } // namespace detail

  template <insidable B>
  inline constexpr grid grid_of = detail::key_grid<detail::key_v<B>>;

  namespace detail
  {
    template <typename T>
    inline constexpr policy_flag policy_of_impl = none;
    template <key K>
      requires(inside_key<K>)
    inline constexpr policy_flag policy_of_impl<id<K>> = key_policy<K>;
    template <policy_flag V>
    inline constexpr policy_flag policy_of_impl<flag<V>> = V;
  } // namespace detail

  // The policy flags of an inside, of a one-shot call_policy<F, E> (inside/detail/call_policy.hpp)
  // or of a named flag; none for anything else, such as an action.
  template <typename X>
  inline constexpr policy_flag policy_of = detail::policy_of_impl<std::remove_cvref_t<X>>;

  namespace detail
  {
    // B's grid under the flags F: the type a store under one-shot flags is checked against.
    template <insidable B, policy_flag F>
    using with_flags_t = id<key{grid_of<B>, F}>;
    // B's policy without a cursor's mark: the policy its values and its results carry.
    template <class B>
    inline constexpr policy_flag value_policy = policy_of<B> & ~cursor_marker;
  } // namespace detail

  namespace detail
  {
    // The policy of an arithmetic result (and of the common type): the one both operands share,
    // where a default-policy point (just<…>, 0_ins) is neutral, so (a + 0_ins) / 2_ins rounds as a / 2_ins does.
    // Different policies give the default. A cursor's marker never carries over.
    template <insidable L, insidable R>
    inline constexpr policy_flag result_flags = []
    {
      constexpr auto neutral = []<class B>(std::type_identity<B>)
      {
        return grid_of<B>.Lower == grid_of<B>.Upper && policy_of<B> == 0;
      };
      constexpr policy_flag f{neutral(std::type_identity<L>{}) ? policy_of<R>
                            : neutral(std::type_identity<R>{}) ? policy_of<L>
                            : policy_of<L> == policy_of<R>     ? policy_of<L>
                            : policy_flag{0}};
      return f & ~cursor_marker;
    }();
  } // namespace detail

  template <typename T>
  inline constexpr interval interval_of = {0, 0};

  template <insidable B>
  inline constexpr interval interval_of<B> = grid_of<B>.interval();

  template <std::integral I>
  inline constexpr interval interval_of<I> = {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

  template <insidable B>
  inline constexpr grid_number lower_of = grid_of<B>.Lower;
  template <insidable B>
  inline constexpr grid_number upper_of = grid_of<B>.Upper;
  template <insidable B>
  inline constexpr grid_number notch_of = grid_of<B>.Notch;

  namespace detail
  {
    // The same as 64-bit rationals, for the paths that work in 64 bits (the integer fast paths, the math engines).
    // A grid number past 64 bits fails the build here, never truncates.
    template <insidable B>
    inline constexpr rational lower64 = to_rational(lower_of<B>);
    template <insidable B>
    inline constexpr rational upper64 = to_rational(upper_of<B>);
    template <insidable B>
    inline constexpr rational notch64 = to_rational(notch_of<B>);

    // B's notch as P/Q in the int64 the integer fast paths compute in (Q > 0), and Q's shift when Q = 2^QShift
    // (else -1). Fits: P below 2^63 (Q always is); a fast path that reads P asserts it.
    struct notch_int64_t
    {
      std::int64_t P{0};
      std::int64_t Q{1};
      int          QShift{-1};
      bool         Fits{false};
    };
    template <insidable B>
    inline constexpr notch_int64_t notch_int64 = []
    {
      constexpr std::uint64_t p{notch64<B>.Num}, q{abs_den(notch64<B>.Den)};
      constexpr bool          fits{p <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())};
      return notch_int64_t{fits ? static_cast<std::int64_t>(p) : 0, static_cast<std::int64_t>(q),
                           std::has_single_bit(q) ? std::countr_zero(q) : -1, fits};
    }();

    // An inside or a plain number (integer, floating point, rational).
    template <typename N>
    concept numeric = insidable<N> or plain_number<N>;
  } // namespace detail

  // The internal plumbing: storage shape, raw and value conversion, dispatch.
  namespace detail
  {
    template <typename T>
    using plain_t = std::remove_cvref_t<T>;

    // The value and raw helpers, told apart. "Value" is the number an inside denotes, "raw" how it is stored
    // (§2, "Storage encoding", in docs/internals.md):
    //   as_rational(x)         value → rational   an exact view of a scalar or an inside
    //   as_double(b)           raw → double       a kind-aware decode; lossy off dyadic grids
    //   to_value(b)            raw → int64        the inside's integer value (decodes an index)
    //   from_value(b, v)       int64 → raw        stores the integer value v into b (the inverse of to_value)
    //   raw_cast<B>(x)         x → raw_t<B>       a type cast only, no value arithmetic
    //   raw_int64(b)           raw → int64        widens the raw bits (not the value of an index raw)
    //   raw_from_offset<B>(o)  index → raw_t<B>   adds raw_lo (Lower for a value raw, 0 for an index)
    //   raw_of_slot<B>(o)      index → raw_t<B>   any storage, any width (inside/detail/wide_value.hpp)

    // A uniform rational view of a scalar or an inside (rational{v} or operator rational()).
    template <numeric N>
    [[nodiscard]] constexpr rational as_rational(N v)
    {
      if constexpr (plain_number<N>)
        return rational{v};
      else
        return v;
    }

    // The zero test of a rational divisor: rational stores zero as {0, 1}, so Num == 0 catches it.
    [[nodiscard]] constexpr bool is_canonical_zero(const rational& v) { return v.Num == 0; }

    // From the grid, not B::raw_type: the storage traits are read inside B's own definition, where B is incomplete.
    template <insidable B>
    using raw_t = raw_for_t<grid_of<B>>;
  } // namespace detail
} // namespace ins

#endif // INSIDE_INSIDE_DETAIL_INTROSPECTION_HPP
