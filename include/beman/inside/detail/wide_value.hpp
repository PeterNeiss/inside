// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP
#define BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_int.hpp>

#include <compare>
#include <expected>
#include <utility>

//---------------------------------------------------------------------------
// wide_value — exact values of insides whose raw is a wide index (more than
// 2^64 slots), where the 64-bit `rational` cannot hold every value.
//
// On a valid grid Lower/Notch is an integer m (slot_base), so a value is its
// value index J = m + raw times Notch, exactly. exact_frac carries a value as
// an unreduced fraction of wide integers; comparisons cross-multiply, and a
// store divides by the target notch and rounds by the policy's mode.
//
// exact_int is sized for 64-bit grid numbers: a value numerator J·Notch.num
// stays under ~200 bits, so products and cross terms fit 512 bits.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  using exact_int = wide_sint<8>;

  struct exact_frac
  {
    exact_int Num;
    exact_int Den;                                      // > 0; not reduced

    friend constexpr std::strong_ordering operator<=>(exact_frac const& a, exact_frac const& b) noexcept
    { return a.Num * b.Den <=> b.Num * a.Den; }
    friend constexpr bool operator==(exact_frac const& a, exact_frac const& b) noexcept
    { return a.Num * b.Den == b.Num * a.Den; }

    friend constexpr exact_frac operator+(exact_frac const& a, exact_frac const& b) noexcept
    { return {a.Num * b.Den + b.Num * a.Den, a.Den * b.Den}; }
    friend constexpr exact_frac operator*(exact_frac const& a, exact_frac const& b) noexcept
    { return {a.Num * b.Num, a.Den * b.Den}; }

    constexpr explicit operator double() const noexcept
    { return static_cast<double>(Num) / static_cast<double>(Den); }
  };

  constexpr exact_frac exact_of(rational const& r) noexcept
  { return {exact_int{wide_numerator(r)}, exact_int{wide_denominator(r)}}; }

  template <std::integral T>
  constexpr exact_frac exact_of(T v) noexcept { return {exact_int{v}, exact_int{1}}; }

  // m = Lower/Notch, the value index of slot 0 (0 for a continuous grid).
  template <insidable B>
  inline constexpr grid_wide slot_base = []{
    if constexpr (notch_of<B> == 0)
      return grid_wide{0};
    else
      return wide_numerator(lower_of<B>) * wide_denominator(notch_of<B>)
           / (wide_denominator(lower_of<B>) * wide_numerator(notch_of<B>));
  }();

  template <insidable B>
  constexpr exact_frac exact_of(B const& b)
  {
    if constexpr (wide_raw<B>)
    {
      const exact_int j = exact_int{slot_base<B>} + exact_int{b.raw()};
      return {j * exact_int{wide_numerator(notch_of<B>)}, exact_int{wide_denominator(notch_of<B>)}};
    }
    else
      return exact_of(as_rational(b));
  }

  // Truncation toward zero, as an integer.
  constexpr exact_int trunc(exact_frac const& f) noexcept { return f.Num / f.Den; }

  // The reduced value as a 64-bit rational, or overflow when it does not fit.
  constexpr std::expected<rational, errc> try_rational(exact_frac const& f) noexcept
  {
    const bool neg = f.Num.negative();
    exact_int a = neg ? -f.Num : f.Num, b = f.Den;
    exact_int x = a, y = b;
    while (!y.is_zero()) { const exact_int t = x % y; x = y; y = t; }
    if (!x.is_zero()) { a /= x; b /= x; }
    if (a > exact_int{std::numeric_limits<umax>::max()} || b > exact_int{std::numeric_limits<imax>::max()})
      return std::unexpected{errc::overflow};
    const imax den = static_cast<imax>(b);
    return rational{static_cast<umax>(a), neg ? -den : den};
  }

  // The value index of f on L's lattice (f / Notch) rounded by M, minus the
  // slot base: L's slot offset. Exact is false when f lies between notches.
  struct exact_index_result { exact_int Index; bool Exact; };

  template <insidable L, round_mode M>
  constexpr exact_index_result exact_index(exact_frac const& f) noexcept
  {
    const exact_int n = f.Num * exact_int{wide_denominator(notch_of<L>)};
    const exact_int d = f.Den * exact_int{wide_numerator(notch_of<L>)};   // > 0
    auto [q, r] = exact_int::divmod(n, d);                                 // toward zero
    const bool exact = r.is_zero();
    if (!exact)
    {
      const bool neg = n.negative();
      const exact_int away = neg ? q - exact_int{1} : q + exact_int{1};
      const exact_int r2 = (neg ? -r : r) * exact_int{2};
      if constexpr (M == round_mode::floor)          { if (neg) q = away; }
      else if constexpr (M == round_mode::ceil)      { if (!neg) q = away; }
      else if constexpr (M == round_mode::nearest)   { if (r2 >= d) q = away; }
      else if constexpr (M == round_mode::half_even)
      { if (r2 > d || (r2 == d && (q.Word[0] & 1u) != 0)) q = away; }
    }
    return {q - exact_int{slot_base<L>}, exact};
  }

  // The raw of slot offset `index` (0 .. slot count) in L's encoding.
  template <insidable L>
  constexpr raw_t<L> raw_of_index(exact_int const& index) noexcept
  {
    if constexpr (point_raw<L>)
      return raw_t<L>{};
    else if constexpr (index_raw<L>)
      return static_cast<raw_t<L>>(index);
    else                                                   // value raw: raw == J
      return static_cast<raw_t<L>>(index + exact_int{slot_base<L>});
  }

  //---------------------------------------------------------------------------
  // Wrapping value-index arithmetic — the integer path of + and ×.
  //
  // wrap_work_t<Result> is unsigned and as wide as Result's raw (at least 64
  // bits). + − × wrap modulo 2^bits, so any expression whose true value is a
  // Result raw comes out exact, however far its intermediates wrapped. Each
  // operand enters as its value index J = value/Notch (an index raw plus
  // Lower/Notch, a value raw as is — value storage has notch 1).
  //---------------------------------------------------------------------------
  template <insidable Result>
  using wrap_work_t = std::conditional_t<wide_raw<Result>, raw_t<Result>, umax>;

  // An operand's value-index range in `Unit`s: Lower/Unit .. Upper/Unit.
  template <insidable X, rational Unit>
  inline constexpr grid_wide units_lo = exact_quotient(lower_of<X>, Unit);
  template <insidable X, rational Unit>
  inline constexpr grid_wide units_hi = exact_quotient(upper_of<X>, Unit);

  // The work type of an integer + or ×: signed imax when every value index
  // involved (each operand in its unit, the result in its notch, and the
  // result's slot offsets) provably fits — then nothing wraps, and the
  // compiler keeps the value ranges, as the builtin paths always did — else
  // the wrapping type.
  template <insidable Result, insidable L, rational UL, insidable R, rational UR>
  using index_work_t = std::conditional_t<
      signed_value_bits_of({units_lo<L, UL>, units_hi<L, UL>, units_lo<R, UR>, units_hi<R, UR>,
                            units_lo<Result, notch_of<Result>>, units_hi<Result, notch_of<Result>>,
                            grid_of<Result>.slot_count()}) <= 63,
      imax, wrap_work_t<Result>>;

  // Integer raws: neither fp nor rational (a point's empty raw counts).
  template <insidable B>
  inline constexpr bool integer_raw = !fp_raw<B> && !rational_raw<B>;

  // a / b for grid numbers, known at compile time to be an integer.
  constexpr grid_wide exact_quotient(rational const& a, rational const& b) noexcept
  { return wide_numerator(a) * wide_denominator(b) / (wide_denominator(a) * wide_numerator(b)); }

  template <typename W, insidable X>
  constexpr W value_index(X const& x) noexcept
  {
    if constexpr (index_raw<X>)
      return static_cast<W>(slot_base<X>) + static_cast<W>(x.raw());
    else
      return static_cast<W>(x.raw());
  }

  // x's value in units of the notch `unit` (an integer: the unit divides
  // x's notch, or x's value for a point).
  template <typename W, rational Unit, insidable X>
  constexpr W value_in_units(X const& x) noexcept
  {
    // A point (Lower == Upper) holds its value in the type — even under a
    // width flag, whose raw stores it again.
    if constexpr (lower_of<X> == upper_of<X>)
      return static_cast<W>(exact_quotient(lower_of<X>, Unit));
    else
    {
      constexpr grid_wide scale = exact_quotient(notch_of<X>, Unit);
      if constexpr (scale == grid_wide{1}) return value_index<W>(x);
      else                                 return value_index<W>(x) * static_cast<W>(scale);
    }
  }

  // The Result whose value index is j (taken modulo 2^bits).
  template <insidable Result, typename W>
  constexpr Result from_value_index(W const& j) noexcept
  {
    if constexpr (point_raw<Result>)
      return Result::from_raw(raw_t<Result>{});
    else if constexpr (index_raw<Result>)
      return Result::from_raw(static_cast<raw_t<Result>>(j - static_cast<W>(slot_base<Result>)));
    else
      return Result::from_raw(static_cast<raw_t<Result>>(j));
  }

  // Exact result of grid arithmetic: the value is on the result lattice and
  // inside its interval by construction, so it maps straight to a raw.
  template <insidable Result>
  constexpr Result exact_result(exact_frac const& v) noexcept
  { return Result::from_raw(raw_of_index<Result>(exact_index<Result, round_mode::trunc>(v).Index)); }
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_WIDE_VALUE_HPP
