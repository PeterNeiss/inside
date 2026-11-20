// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_STORAGE_HPP
#define INSIDE_INSIDE_DETAIL_STORAGE_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/introspection.hpp>

#include <algorithm>
#include <bit>
#include <initializer_list>
#include <utility>

// How an inside's value lives in its raw (the storage concepts), the raw ↔ value plumbing,
// the grid shape predicates, and the integer rounding of quotients the stores share.
namespace ins::detail
{
  // The storage is a function of the grid alone (grid.hpp raw_for).
  // The leaves partition every inside; each names the raw type and what it means:
  //
  //   value_storage — the raw IS the value
  //     rational_storage        a 64-bit rational
  //     fraction_storage        a wide_rational (continuous, limits past 64 bits)
  //     integer_value_storage   a builtin integer
  //   index_storage — the raw is a 0-based slot index; value = Lower + raw·Notch
  //     point_storage           empty (point_slot): one slot, the value is the type
  //     integer_index_storage   a builtin integer
  //     wide_index_storage      a wide_int (more than 2^64 slots)
  //   integer_storage = index_storage | integer_value_storage (an integer raw)
  //
  // Concepts, so the groups subsume their leaves and can constrain helpers.
  template <typename B>
  concept rational_storage = insidable<B> && std::same_as<raw_t<B>, rational>;

  // A continuous grid whose limits pass 64 bits: a reduced exact fraction of as many limbs as the limits need.
  template <typename B>
  concept fraction_storage = insidable<B> && wide_rational_like<raw_t<B>>;

  // Whether an integer raw holds the value itself (else a slot index):
  // the whole-number grids where that is free (grid.hpp deduces_value).
  template <insidable B>
  inline constexpr bool integer_raw_holds_value = deduces_value<grid_of<B>>;

  template <typename B>
  concept integer_value_storage = insidable<B> && std::integral<raw_t<B>> && integer_raw_holds_value<B>;

  template <typename B>
  concept integer_index_storage = insidable<B> && std::integral<raw_t<B>> && !integer_raw_holds_value<B>;

  // A point's raw acts as index slot 0.
  template <typename B>
  concept point_storage = insidable<B> && std::same_as<raw_t<B>, point_slot>;

  // Its values need the exact wide paths (inside/detail/wide_value.hpp): a 64-bit rational or int64 cannot hold them.
  template <typename B>
  concept wide_index_storage = insidable<B> && wide_integer<raw_t<B>>;

  template <typename B>
  concept value_storage = rational_storage<B> || fraction_storage<B> || integer_value_storage<B>;

  template <typename B>
  concept index_storage = point_storage<B> || integer_index_storage<B> || wide_index_storage<B>;

  template <typename B>
  concept integer_storage = index_storage<B> || integer_value_storage<B>;

  // Same raw type and same encoding (value vs index): only then does one inside's raw mean the same as another's.
  // Two insides on one grid always share it; two grids may not ({5, 100} holds values, {5, 1000} an index).
  template <insidable L, insidable R>
  inline constexpr bool same_storage = std::is_same_v<raw_t<L>, raw_t<R>> && index_storage<L> == index_storage<R>;

  // Grid shape and magnitude:
  // - notched: Notch != 0, the values sit on a lattice (a point may still have Notch 0).
  // - point_grid: Lower == Upper, one value. Its raw is point_storage.
  // - wide_valued: values past 64 bits, a wide raw, or grid numbers (a limit or the notch) past 64 bits
  //   even with few slots (wide_grid_numbers). They take the exact paths of inside/detail/wide_value.hpp.
  template <insidable B>
  inline constexpr bool notched = notch_of<B> != 0;

  template <insidable B>
  inline constexpr bool point_grid = lower_of<B> == upper_of<B>;

  // The lattice passes through 0 (grid::anchored):
  // every value is a whole number of notches, the value index J = value/Notch an integer.
  // Unanchored grids ({{0.5, 10.5}, 1}) take the paths that work from Lower.
  template <insidable B>
  inline constexpr bool anchored = grid_of<B>.anchored();

  template <insidable B>
  inline constexpr bool wide_grid_numbers =
    !fits_rational(grid_of<B>.Interval.Lower) || !fits_rational(grid_of<B>.Interval.Upper)
    || !fits_rational(grid_of<B>.Notch);

  // A lattice whose values pass the 64-bit rational between limits that fit:
  // on {{-1.2e19, 1.2e19}, 1.5} the half-integers need numerators to 2.4e19.
  template <insidable B>
  inline constexpr bool lattice_past_rational =
    grid_of<B>.Notch != 0
    && (grid_int{std::numeric_limits<std::int64_t>::max()} < den_bound(grid_of<B>)
        || grid_int{std::numeric_limits<std::uint64_t>::max()} < num_bound(grid_of<B>));

  // Values past the 64-bit rational: a wide index, grid numbers past 64 bits, a lattice past it,
  // or a fraction raw (a bounded continuous grid can need one).
  template <insidable B>
  inline constexpr bool wide_valued =
    wide_index_storage<B> || wide_grid_numbers<B> || lattice_past_rational<B> || fraction_storage<B>;

  // Whether an inside converts to the 64-bit rational:
  // a lattice past it still does, for its values that fit (the conversion reports the others).
  template <insidable B>
  inline constexpr bool rational_readable =
    !wide_valued<B>
    || (lattice_past_rational<B> && !wide_index_storage<B> && !wide_grid_numbers<B> && !fraction_storage<B>);

  template <insidable B>
  constexpr auto exact_of(const B& b); // wide_value.hpp

  // The value indices Lower/Notch and Upper/Notch of a notched grid (0 on a continuous one), exactly,
  // and as 64-bit rationals (for the 64-bit paths) where both fit.
  template <insidable B>
  inline constexpr grid_number lower_index_of = notched<B> ? lower_of<B> / notch_of<B> : grid_number{0};
  template <insidable B>
  inline constexpr grid_number upper_index_of = notched<B> ? upper_of<B> / notch_of<B> : grid_number{0};
  template <insidable B>
  inline constexpr bool value_indices_fit64 =
    notched<B> && fits_rational(lower_index_of<B>) && fits_rational(upper_index_of<B>);
  template <insidable B>
  inline constexpr rational lower_index64 = to_rational(lower_index_of<B>);
  // Lower/Notch as an int64 value index: an integer within int64.
  template <insidable B>
  inline constexpr bool lower_index_fits_int64 =
    notched<B> && grid_denominator(lower_index_of<B>) == grid_int{1}
    && !(lower_index_of<B> < grid_number{std::numeric_limits<std::int64_t>::min()})
    && !(grid_number{std::numeric_limits<std::int64_t>::max()} < lower_index_of<B>);
  template <insidable B>
  inline constexpr rational upper_index64 = to_rational(upper_index_of<B>);
  // Every value index of an anchored grid (value/Notch, an integer) fits int64:
  // value_index<std::int64_t> is exact. (uint64's do not.)
  template <insidable B>
  inline constexpr bool value_indices_fit_int64 =
    notched<B> && anchored<B> && !(lower_index_of<B> < grid_number{std::numeric_limits<std::int64_t>::min()})
    && !(grid_number{std::numeric_limits<std::int64_t>::max()} < upper_index_of<B>);

  // Index raw → double in one IEEE operation.
  // On an anchored grid with Notch p/q the value is J·p/q, J = raw + Lower/Notch.
  // With |J·p| and q at most 2^53 both are exact doubles,
  // so double(J·p) / double(q) is the correctly rounded quotient, what the rational decode gives
  // (a power-of-two q compiles to a multiply).
  struct index_double_codec
  {
    bool         Ok;
    std::int64_t LowerIndex; // Lower / Notch
    std::int64_t P;          // Notch numerator
    double       Q;          // Notch denominator
  };
  template <insidable B>
  inline constexpr index_double_codec index_double = []
  {
    if constexpr (!integer_index_storage<B> || !anchored<B> || wide_valued<B> || !value_indices_fit64<B>)
      return index_double_codec{};
    else
    {
      constexpr std::uint64_t k53{std::uint64_t{1} << 53};
      const rational          n{notch64<B>};
      const auto              lo{lower_index64<B>}, hi{upper_index64<B>};
      if (abs_den(n.Den) > k53)
        return index_double_codec{};
      const std::uint64_t m{lo.Num > hi.Num ? lo.Num : hi.Num}; // max |J|
      if (m > k53 / n.Num)
        return index_double_codec{};
      return index_double_codec{
        true, signed_numerator(lo), static_cast<std::int64_t>(n.Num), static_cast<double>(abs_den(n.Den))};
    }
  }();

  // True when R's interval cannot contain zero,
  // so a / b can return a plain inside instead of expected<inside, error> (see inside/detail/division.hpp).
  // A point grid at 0 is not excluded.
  template <insidable R>
  inline constexpr bool divisor_excludes_zero = (lower_of<R> > 0) || (upper_of<R> < 0);

  // The interval's endpoints truncated toward zero, modulo 2^64 (exact on the grids within int64 that read them).
  // Used by from_value, raw_lo and the fast paths.
  template <insidable B>
  inline constexpr std::int64_t lower_int64 = static_cast<std::int64_t>(lower64<B>);

  template <insidable B>
  inline constexpr std::int64_t upper_int64 = static_cast<std::int64_t>(upper64<B>);

  // The largest index, detail::max_index.
  // Overflow-safe: 0 when it doesn't fit uint64, for grids with a wide index, which take the exact wide paths.
  template <insidable B>
  inline constexpr std::uint64_t max_index_of = max_index(grid_of<B>);

  // Every value, and for index storage every slot, fits int64.
  // It gates the integer fast paths that work in int64 (raw_int64, to_value, raw_lo, raw_hi);
  // a grid reaching past int64 (e.g. {0, 2^64−1} in a uint64) takes the exact rational or uint64 paths instead.
  template <insidable B>
  inline constexpr bool values_fit_int64 =
    !wide_valued<B> && fits_int64(interval_of<B>)
    && (value_storage<B> || std::in_range<std::int64_t>(max_index_of<B>));

  // Every value's reduced numerator and denominator fit int64 too:
  // a value within int64 can still have a numerator past it ((2^63 + 1)/2).
  template <insidable B>
  inline constexpr bool fraction_fits_int64 = []
  {
    if constexpr (!values_fit_int64<B>)
      return false;
    else
      return !(grid_int{std::numeric_limits<std::int64_t>::max()} < num_bound(grid_of<B>));
  }();

  // Notch a non-zero integer and Lower an integer: every value is an integer
  // ({{0.5, 10.5}, 1} has an integer notch but not integer values).
  template <grid G>
  inline constexpr bool integer_notch =
    grid_denominator(G.Notch) == grid_int{1} && G.Notch != 0 && grid_denominator(G.Interval.Lower) == grid_int{1};

  // An index grid: every value an integer from 0 up, within int64.
  template <grid G>
  inline constexpr bool index_shaped =
    (integer_notch<G>
     || (G.Interval.Lower == G.Interval.Upper && grid_denominator(G.Interval.Lower) == grid_int{1}))
    && !(G.Interval.Lower < 0) && fits_int64(G.Interval);

  // Every value an integer: an integer lattice or an integer point (see integer_limits below).
  template <insidable B>
  inline constexpr bool integer_lattice =
    integer_notch<grid_of<B>> || (lower_of<B> == upper_of<B> && grid_denominator(lower_of<B>) == grid_int{1});

  // Only the type conversion, no change of value representation.
  template <insidable B>
  [[nodiscard]] constexpr raw_t<B> raw_cast(auto value) noexcept
  {
    return static_cast<raw_t<B>>(value);
  }

  // Widen raw storage to int64.
  // Distinct from to_value(b) for notch-stored grids, where the raw is an index rather than a value:
  // the names keep the two intents apart although both are a static_cast<std::int64_t>.
  template <insidable B>
    requires(!wide_index_storage<B>) // a wide raw does not fit int64: use the exact wide path
  constexpr std::int64_t raw_int64(B b) noexcept
  {
    return static_cast<std::int64_t>(b.raw());
  }

  // ⌊a / b⌋ for grid numbers (b > 0).
  consteval grid_int floor_quotient(const grid_number& a, const grid_number& b)
  {
    const grid_int n{grid_numerator(a) * grid_denominator(b)}, d{grid_denominator(a) * grid_numerator(b)};
    const grid_int q{n / d};
    return (n % d != grid_int{0} && n.negative()) ? q - grid_int{1} : q;
  }

  // Q-format integer fast path: for grids with an integer Lower, a unit-numerator Notch 1/nd,
  // and every value·nd within int64, value ↔ raw is pure integer arithmetic.
  // Shared by operator rational(), from_value and assignment::store.
  template <insidable B>
  inline constexpr bool q_format_fits = []
  {
    if constexpr (!values_fit_int64<B>) // (first: the 64-bit views below need it)
      return false;
    else if constexpr (abs_den(lower64<B>.Den) != 1 || notch64<B>.Num != 1)
      return false;
    else // every value·nd (so Lower·nd and raw + Lower·nd) in int64
    {
      constexpr grid_number nd{grid_denominator(notch_of<B>)};
      return !(lower_of<B> * nd < grid_number{std::numeric_limits<std::int64_t>::min()})
          && !(grid_number{std::numeric_limits<std::int64_t>::max()} < upper_of<B> * nd);
    }
  }();

  // The integer store of a lattice the Q-format shortcut does not cover
  // (an unanchored one, or a notch that is not 1/K: 60, 5/100):
  // Notch = S/K and Lower = M/K over their common denominator K.
  // MaxDen bounds a source denominator so that num·K, M·aden and aden·S stay in int64
  // (|num| ≤ |value|·aden, and every in-range |value| is at most Mag); Ok when the grid allows any.
  struct lattice_codec_t
  {
    std::int64_t  K, S, M;
    std::uint64_t MaxDen;
    bool          Ok;
  };
  template <insidable L>
  inline constexpr lattice_codec_t lattice_codec = []
  {
    constexpr lattice_codec_t no{0, 0, 0, 0, false};
    if constexpr (!values_fit_int64<L> || !notched<L> || point_storage<L>)
      return no;
    else if constexpr (q_format_fits<L>)
      return no;
    else
    {
      constexpr std::uint64_t cap{static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())};
      const rational          n{notch64<L>}, lo{lower64<L>}, hi{upper64<L>};
      const std::uint64_t     dn{abs_den(n.Den)}, dl{abs_den(lo.Den)};
      std::uint64_t           k;
      if (mul_overflow(dn / std::gcd(dn, dl), dl, &k) || k > cap)
        return no;
      std::uint64_t s, mm;
      if (mul_overflow(n.Num, k / dn, &s) || s > cap || mul_overflow(lo.Num, k / dl, &mm) || mm > cap)
        return no;
      // Mag, a bound on every in-range |value| plus one: ⌈max(|Lower|, |Upper|)⌉ + 1, from the grid numbers
      // (|Lower| may be 2^63, past an int64 ceiling).
      constexpr grid_int mag_g = []
      {
        const grid_number a{abs(lower_of<L>)}, b{abs(upper_of<L>)};
        const grid_number m{a < b ? b : a};
        return floor_quotient(m, grid_number{1}) + grid_int{2}; // ≥ the ceiling + 1
      }();
      if constexpr (!(mag_g < grid_int{std::int64_t{1} << 62}))
        return no;
      const std::uint64_t mag{static_cast<std::uint64_t>(mag_g)};
      std::uint64_t       km, kms;
      if (mul_overflow(k, mag, &km) || mul_overflow(km, std::uint64_t{4}, &kms))
        return no;
      const std::uint64_t by_num{cap / kms}; // num·K and M·aden each below cap/2
      const std::uint64_t by_den{cap / s};   // aden·S
      return lattice_codec_t{static_cast<std::int64_t>(k),
                             static_cast<std::int64_t>(s),
                             lo.Den < 0 ? -static_cast<std::int64_t>(mm) : static_cast<std::int64_t>(mm),
                             by_num < by_den ? by_num : by_den,
                             true};
    }
  }();

  // A signed slot offset folded onto L's slots 0 … max_index (a wrap: the period is Upper − Lower + Notch).
  template <insidable L>
  inline constexpr std::int64_t slot_period = static_cast<std::int64_t>(max_index_of<L>) + 1;
  template <insidable L>
  constexpr std::uint64_t fold_slot(std::int64_t offset) noexcept
  {
    const std::int64_t w{offset % slot_period<L>};
    return static_cast<std::uint64_t>(w < 0 ? w + slot_period<L> : w);
  }

  // A slot offset past int64 (a 128-bit product, say) folded the same way:
  // cold, and by value, so the caller's hot path keeps no frame for it.
  template <insidable L, typename W>
  [[gnu::cold]] [[gnu::noinline]] constexpr std::uint64_t fold_wide_slot(W offset) noexcept
  {
    if constexpr (wide_integer<W>)
    {
      // |offset| by the one-limb period, limb by limb; a negative offset's remainder r folds to period − r.
      constexpr std::size_t   N{sizeof(W) / sizeof(std::uint64_t)};
      constexpr std::uint64_t p{static_cast<std::uint64_t>(slot_period<L>)};
      const bool              neg{offset.negative()};
      const wide_uint<N>      u{static_cast<wide_uint<N>>(offset)};
      const std::uint64_t     r{divmod_small<N>(neg ? wide_uint<N>{0} - u : u, p).Remainder};
      return neg && r != 0 ? p - r : r;
    }
    else
      return static_cast<std::uint64_t>(floor_divmod(offset, static_cast<W>(slot_period<L>)).Rem);
  }

  // value → raw, integer math only.
  template <insidable B>
    requires q_format_fits<B>
  constexpr raw_t<B> q_format_encode(std::int64_t value) noexcept
  {
    constexpr std::int64_t nd = abs_den(notch64<B>.Den);
    return raw_cast<B>((value - lower_int64<B>) * nd);
  }

  // raw → rational, integer math only.
  template <insidable B>
    requires q_format_fits<B>
  constexpr rational q_format_decode(B b) noexcept
  {
    constexpr std::int64_t nd = abs_den(notch64<B>.Den);
    const std::int64_t     n{raw_int64(b) + lower_int64<B> * nd};
    if constexpr (std::has_single_bit(static_cast<std::uint64_t>(nd)))
    {
      // A power-of-two denominator reduces by the common trailing zeros (0 reduces to 0/1).
      const std::uint64_t m{n < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(n) : static_cast<std::uint64_t>(n)};
      const int           s{std::countr_zero(m | static_cast<std::uint64_t>(nd))};
      rational            r;
      r.Num = m >> s;
      r.Den = n < 0 ? -(nd >> s) : nd >> s;
      return r;
    }
    else
      return rational{n, nd};
  }

  // The value of an inside on an integer lattice as int64, which the caller proves holds it
  // (user code reads b.to<T>(), which reports overflow).
  template <insidable B>
    requires integer_lattice<B>
  [[nodiscard]] constexpr std::int64_t to_value(B b) noexcept
  {
    if constexpr (value_storage<B>)
      return raw_int64(b);
    else // in uint64: raw·Notch can pass int64 although the value fits
      return static_cast<std::int64_t>(
        static_cast<std::uint64_t>(lower_int64<B>) + static_cast<std::uint64_t>(raw_int64(b)) * notch64<B>.Num);
  }

  // The callers store an integer of B (a fold count, a native quotient or remainder) on an integer lattice
  // with a nonzero notch (a carry point {2, 2} has notch 1).
  template <insidable B>
  constexpr void from_value(B& b, std::int64_t val)
  {
    static_assert(notch64<B>.Num != 0 && abs_den(notch64<B>.Den) == 1 && abs_den(lower64<B>.Den) == 1);
    if constexpr (value_storage<B>)
      b = B::from_raw(raw_cast<B>(val), unsafe);
    else
      b = B::from_raw(
        raw_cast<B>((static_cast<std::uint64_t>(val) - static_cast<std::uint64_t>(lower_int64<B>)) / notch64<B>.Num),
        unsafe);
  }

  // raw_lo, raw_hi, raw_from_offset: an integer raw's endpoints.
  // An index raw is 0-based (raw_lo == 0); a value raw IS the value (raw_lo == Lower),
  // so an offset needs raw_lo<L> added back before storing.
  // (if constexpr: naming lower_int64 instantiates it, and an index raw's grid may have limits past 64 bits.)
  template <insidable B>
  inline constexpr std::int64_t raw_lo = []
  {
    if constexpr (integer_value_storage<B>)
      return lower_int64<B>;
    else
      return std::int64_t{0};
  }();

  template <insidable B>
  inline constexpr std::int64_t raw_hi = []
  {
    if constexpr (integer_value_storage<B>)
      return upper_int64<B>;
    else
      return static_cast<std::int64_t>(max_index_of<B>);
  }();

  // The exact raw range: 0 .. step count for index storage, Lower .. Upper for value storage (integers there).
  // It sizes the work types below.
  template <insidable B>
  inline constexpr grid_int raw_lo_exact = index_storage<B> ? grid_int{0} : grid_numerator(lower_of<B>);
  template <insidable B>
  inline constexpr grid_int raw_hi_exact = index_storage<B> ? grid_of<B>.step_count() : grid_numerator(upper_of<B>);

  // Value bits for every value in a list (their min .. max).
  constexpr int signed_value_bits(std::initializer_list<grid_int> vals) noexcept
  {
    const auto [mn, mx] = std::minmax(vals);
    return signed_value_bits(mn, mx);
  }

  // Signed work type for an exact intermediate of Bits value bits:
  // int64 for everything within int64 (the builtin fast paths keep their codegen), a wide_int beyond.
  template <int Bits>
  using work_int_t = int_for_bits_t<(Bits < 63 ? 63 : Bits), true>;

  // t = Quot·m + Rem with 0 ≤ Rem < m (m > 0): division rounded toward −∞, the fold of every wrap.
  // A wide int divides once for both parts.
  template <typename W>
  struct floored
  {
    W Quot, Rem;
  };
  template <raw_integer W>
  [[nodiscard]] constexpr floored<W> floor_divmod(const W& t, const W& m) noexcept
  {
    floored<W> f;
    if constexpr (wide_integer<W>)
    {
      auto [q, r] = W::divmod(t, m);
      f = {q, r};
    }
    else
      f = {t / m, t % m};
    if (f.Rem < W{0})
    {
      f.Rem += m;
      f.Quot -= W{1};
    }
    return f;
  }

  // Offset (uint64 or int64) → raw. It adds in uint64: the bits are the same,
  // but a value raw of a grid reaching past int64 (offset + Lower ≥ 2^63) must not overflow int64.
  template <insidable L, std::integral W>
  constexpr raw_t<L> raw_from_offset(W offset) noexcept
  {
    return raw_cast<L>(
      static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(raw_lo<L>)); // raw_lo: 0 for an index raw
  }

  // Raw → offset (slot 0 at Lower), raw_from_offset's inverse, in uint64.
  template <insidable L, std::integral R>
  constexpr std::uint64_t raw_offset(R raw) noexcept
  {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(raw)) - static_cast<std::uint64_t>(raw_lo<L>);
  }

  // The raw of L's Lower (low) or Upper endpoint:
  // the exact constant for a rational raw, slot 0 or max_index_of for an integer one.
  template <insidable L>
  constexpr raw_t<L> endpoint_raw(bool low) noexcept
  {
    if constexpr (rational_storage<L>)
      return low ? lower64<L> : upper64<L>;
    else
      return raw_from_offset<L>(low ? std::uint64_t{0} : max_index_of<L>);
  }

  // A value of a lattice the codec covers, as n/K: n = slot·S + M, in int64
  // (the codec's bound on every in-range |value|·K keeps it there).
  template <insidable B>
  constexpr std::int64_t lattice_numerator(const B& b) noexcept
  {
    constexpr auto c{lattice_codec<B>};
    return static_cast<std::int64_t>(raw_offset<B>(b.raw())) * c.S + c.M;
  }
  // …and exact in double: every |n| below 2^53.
  template <insidable B>
  inline constexpr bool lattice_double = []
  {
    if constexpr (!lattice_codec<B>.Ok)
      return false;
    else
    {
      constexpr auto c{lattice_codec<B>};
      std::int64_t   t;
      return max_index_of<B> < (std::uint64_t{1} << 62)
          && !mul_overflow(static_cast<std::int64_t>(max_index_of<B>), c.S, &t)
          && t + (c.M < 0 ? -c.M : c.M) < (std::int64_t{1} << 53);
    }
  }();

  // Ungated double view of any inside (the public operator double() is gated on a rounding flag;
  // this is always available).
  // Everything but index storage holds the value verbatim; an index decodes through the grid.
  template <insidable B>
  [[nodiscard]] constexpr double as_double(const B& b) noexcept
  {
    if constexpr (wide_valued<B>)
      return static_cast<double>(exact_of(b));
    else if constexpr (point_storage<B>)
      return static_cast<double>(lower64<B>);
    else if constexpr (value_storage<B>)
      return static_cast<double>(b.raw());
    else if constexpr (constexpr auto c = index_double<B>; c.Ok)
      return static_cast<double>((static_cast<std::int64_t>(b.raw()) + c.LowerIndex) * c.P) / c.Q;
    else if constexpr (lattice_double<B>)
      // Any other lattice: (slot·S + M)/K with an exact numerator, one correctly rounded division.
      return static_cast<double>(lattice_numerator(b)) / static_cast<double>(lattice_codec<B>.K);
    else
      return static_cast<double>(as_rational(b)); // exact, even where Raw·Notch passes 64 bits
  }

  // integer_limits vs integer_lattice: easy to confuse, both needed.
  // - integer_limits<B>: Lower and Upper integers (Notch may be fractional, e.g. inside<{0,100}, 1/10>).
  //   It lets Lower and Upper be used as int64 constants.
  // - integer_lattice<B>: every value an integer: an integer Lower and an integer non-zero Notch, or a point.
  //   It implies integer_limits (not the converse). A continuous grid (Notch 0) is not one, whatever its limits.
  template <insidable B>
  inline constexpr bool integer_limits =
    grid_denominator(lower_of<B>) == grid_int{1} && grid_denominator(upper_of<B>) == grid_int{1};

  // Whether a store into B under the call policy P uses flag F: B's own flags or P's.
  // Composite flags (e.g. round_half_away = bit5 | snap) require all their bits set:
  // a subset like just snap does not match.
  template <insidable B, typename P, policy_flag F>
  inline constexpr bool uses_flag = has_flag(policy_of<B>, F) || plain_t<P>::test(F);

  // rounding_of (policy_flag.hpp) over L's type policy and the call's policy P.
  template <insidable L, typename P>
  inline constexpr round_mode rounding_for = uses_flag<L, P, round_floor>     ? round_mode::floor
                                           : uses_flag<L, P, round_ceil>      ? round_mode::ceil
                                           : uses_flag<L, P, round_half_even> ? round_mode::half_even
                                           : uses_flag<L, P, round_half_away> ? round_mode::nearest
                                           : round_mode::trunc;

  // The lattice index of Lower, ⌊Lower/Notch⌋, slot_base on an anchored grid.
  // Its parity plus an offset's says which lattice points are even.
  template <insidable B>
  inline constexpr grid_int lower_index_wide = notched<B> ? floor_quotient(lower_of<B>, notch_of<B>) : grid_int{0};
  template <insidable B>
  inline constexpr bool lower_index_odd = (static_cast<std::uint64_t>(lower_index_wide<B>) & 1) != 0;

  // A double onto a lattice whose values times K stay below 2^51 (2^62 for a power-of-two K),
  // with K, S, M the Q-format's K, 1, Lower·K or the lattice codec's:
  // the value index J = round(v·K / S), from v·K = p + e exactly
  // (e by an fma, or Dekker's product where the target has none),
  // so every rounding mode and every tie is decided on the exact value.
  struct double_lattice_t
  {
    std::int64_t K, S, M;
    bool         Ok;
  };
  template <insidable L>
  inline constexpr double_lattice_t double_lattice = []
  {
    constexpr double_lattice_t no{1, 1, 0, false};
    if constexpr (!values_fit_int64<L> || !notched<L> || point_storage<L> || !integer_storage<L>)
      return no;
    else
    {
      std::int64_t k, sv, m;
      if constexpr (q_format_fits<L>)
      {
        k  = static_cast<std::int64_t>(abs_den(notch64<L>.Den));
        sv = 1;
        m  = lower_int64<L> * k;
      }
      else if constexpr (lattice_codec<L>.Ok)
      {
        k  = lattice_codec<L>.K;
        sv = lattice_codec<L>.S;
        m  = lattice_codec<L>.M;
      }
      else
        return no;
      // every in-range |v|·K below 2^51
      constexpr grid_int mag_g = []
      {
        const grid_number a{abs(lower_of<L>)}, b{abs(upper_of<L>)};
        const grid_number mm{a < b ? b : a};
        return floor_quotient(mm, grid_number{1}) + grid_int{2};
      }();
      if constexpr (!(mag_g < grid_int{std::int64_t{1} << 62}))
        return no;
      const std::uint64_t mag{static_cast<std::uint64_t>(mag_g)};
      std::uint64_t       t;
      // (A power-of-two K multiplies exactly, and a double past 2^52 is an integer: then int64 is the only bound.)
      const bool pow2{std::has_single_bit(static_cast<std::uint64_t>(k))};
      if (mul_overflow(mag, static_cast<std::uint64_t>(k), &t) || t >= (std::uint64_t{1} << (pow2 ? 62 : 51)))
        return no;
      return double_lattice_t{k, sv, m, true};
    }
  }();

  // v·k = p + error exactly.
  constexpr double product_error(double v, double k, double p) noexcept
  {
#if INSIDE_HAS_FMA
    return __builtin_fma(v, k, -p);
#else
    constexpr double split{134217729.0}; // 2^27 + 1
    const double     cv{split * v}, vh{cv - (cv - v)}, vl{v - vh};
    const double     ck{split * k}, kh{ck - (ck - k)}, kl{k - kh};
    return ((vh * kh - p) + vh * kl + vl * kh) + vl * kl;
#endif
  }

  struct double_slot_t
  {
    std::int64_t Slot;
    bool         Exact;
  };
  // The slot of an in-range v rounded by M (an exact v gives Exact).
  // The directed modes need only the floor of v·K and whether anything is left;
  // the nearest ones place the fraction against 1/2 on the magnitude.
  template <insidable L, round_mode M>
  [[gnu::always_inline]] constexpr double_slot_t double_slot(double v) noexcept
  {
    constexpr auto c{double_lattice<L>};
    const double   k{static_cast<double>(c.K)};
    const double   p{v * k};
    const double   e{std::has_single_bit(static_cast<std::uint64_t>(c.K)) ? 0.0 : product_error(v, k, p)};
    if constexpr (M == round_mode::floor || M == round_mode::ceil || M == round_mode::trunc)
    {
      std::int64_t pf{static_cast<std::int64_t>(p)};
      pf -= static_cast<double>(pf) > p;              // ⌊p⌋
      const bool whole{static_cast<double>(pf) == p}; // p an integer
      pf -= whole && e < 0;                           // ⌊v·K⌋
      const bool         frac{!whole || e != 0};      // v·K not an integer
      const std::int64_t a{pf - c.M};
      std::int64_t       q{a / c.S}, mm{a % c.S};
      if (mm < 0)
        mm += c.S, --q;
      const bool rest{frac || mm != 0};
      const bool up{M == round_mode::ceil ? rest : M == round_mode::trunc ? rest && v < 0 : false};
      return {q + up, !rest};
    }
    else
    {
      // On the magnitude |v·K| = mag + ea:
      // mag − ⌊mag⌋ is exact (it keeps mag's own bits), which 1 + p for a negative p would not be.
      const bool      neg{p < 0};
      const double    mag{neg ? -p : p};
      const double    ea{neg ? -e : e};
      std::int64_t    ia{static_cast<std::int64_t>(mag)}; // ⌊mag⌋
      const double    fa{mag - static_cast<double>(ia)};
      remainder_class t; // the fraction of |v·K| against 0 and 1/2
      if (fa == 0)
      {
        if (ea < 0)
        {
          --ia;
          t = remainder_class::above_half; // 1 + ea, |ea| < 1/2
        }
        else
          t = ea > 0 ? remainder_class::below_half : remainder_class::zero;
      }
      else if (fa == 0.5)
        t = ea > 0 ? remainder_class::above_half : ea < 0 ? remainder_class::below_half : remainder_class::half;
      else
        t = fa < 0.5 ? remainder_class::below_half : remainder_class::above_half;
      // v·K = pf + t with 0 ≤ t < 1: for a negative value the floor is one further down and the fraction its
      // complement.
      std::int64_t pf{neg ? -ia : ia};
      if (neg && t != remainder_class::zero)
      {
        --pf;
        t = t == remainder_class::below_half ? remainder_class::above_half
          : t == remainder_class::above_half ? remainder_class::below_half
          : t;
      }
      // The offset (v·K − M)/S: q = ⌊(pf − M)/S⌋, rounded up or not by the remainder r = (pf − M) mod S + t
      // against S/2, in value space (the value's sign, the value index's parity), as round_split.
      const std::int64_t a{pf - c.M};
      std::int64_t       q{a / c.S}, mm{a % c.S};
      if (mm < 0)
        mm += c.S, --q;
      const bool      exact{mm == 0 && t == remainder_class::zero};
      remainder_class r;
      if (exact)
        r = remainder_class::zero;
      else if constexpr (c.S == 1)
        r = t;
      else if constexpr (c.S % 2 == 0)
      {
        constexpr std::int64_t h{c.S / 2};
        r = mm < h ? remainder_class::below_half
                   : (mm == h && t == remainder_class::zero ? remainder_class::half : remainder_class::above_half);
      }
      else
      {
        constexpr std::int64_t h{(c.S - 1) / 2};
        r = mm < h ? remainder_class::below_half
          : mm > h ? remainder_class::above_half
          : (t == remainder_class::half         ? remainder_class::half
             : t == remainder_class::above_half ? remainder_class::above_half
             : remainder_class::below_half);
      }
      const bool up{rounds_up(M, v < 0, r, ((q & 1) != 0) != lower_index_odd<L>)};
      return {q + (up ? 1 : 0), exact};
    }
  }

  // The rounding step onto L's lattice by rounding_for<L, P>:
  // the exact lattice offset q (v/Notch when anchored, (v − Lower)/Notch otherwise) as a base index
  // and whether to step one unit up from it. negative: v < 0.
  // A rational index never wraps, however far past int64 it lies.
  struct lattice_step
  {
    rational Base;
    bool     Step;
  };
  template <insidable L, typename P>
  [[nodiscard]] constexpr lattice_step lattice_step_of(rational q, bool negative)
  {
    constexpr round_mode m{rounding_for<L, P>};
    if constexpr (anchored<L>)
      return {round_to_integral(q, m), false}; // the value index's own sign rules
    else
    {
      const rational k{round_to_integral(q, round_mode::floor)};
      // q − k = r/d in [0, 1): the remainder of q's numerator, by integers.
      const std::uint64_t d{abs_den(q.Den)}, t{q.Num % d};
      const std::uint64_t r{q.Den < 0 && t != 0 ? d - t : t};
      return {k, rounds_up(m, negative, classify_remainder(m, r, d), ((k.Num & 1) != 0) != lower_index_odd<L>)};
    }
  }

  // v rounded onto L's lattice {Lower + k·Notch} by rounding_for<L, P> (value space, ties half away from zero),
  // for any v, not only within [Lower, Upper], so wrap can round first and fold an on-lattice value after:
  // a result or step past the rational range is error::overflow.
  template <insidable L, typename P>
  [[nodiscard]] constexpr std::expected<rational, error> try_round_to_lattice(rational v)
  {
    if constexpr (!notched<L>) // a point: apply_wrap's wrap arm is formed, never run (dispatch rejects it)
      return v;
    else
    {
      const auto q{anchored<L>
                     ? try_div(v, notch64<L>)
                     : try_sub(v, lower64<L>).and_then([](rational off) { return try_div(off, notch64<L>); })};
      if (!q)
        return q;
      const auto [base, step] = lattice_step_of<L, P>(*q, v < 0);
      const auto j{step ? try_add(base, rational{1}) : std::expected<rational, error>{base}};
      if (!j)
        return j;
      const auto rel{try_mul(*j, notch64<L>)};
      return (rel && !anchored<L>) ? try_add(lower64<L>, *rel) : rel;
    }
  }

  // Round, then range-check.
  // Lower and Upper are lattice points, so rounding an in-range value keeps it in range;
  // only an out-of-range value can change outcome.
  // When the policy may round (snap), round_into_range rounds such a value and returns its raw when it lands inside
  // the interval. Only values within one notch of the interval can, so the exact rounding there stays small for
  // huge sources.
  // A bounded continuous grid rounds onto its bound the same way: its limits are values, and fractions k/D lie 1/D
  // apart.
  template <insidable L, typename P>
  inline constexpr bool rounds_before_range_check =
    (notched<L> || grid_of<L>.MaxDenominator != 0) && uses_flag<L, P, snap>;

  // The raw a value rounds to, when it rounds inside the limits (Ok).
  template <insidable L>
  struct rounded_raw
  {
    raw_t<L> Raw;
    bool     Ok;
  };

  // The rounding itself, exact (wide_value.hpp): the slot of a notched L, or the fraction on a continuous L's bound.
  template <insidable L, typename P>
  constexpr rounded_raw<L> exact_round_into_range(const rational& v);

  template <insidable L, typename P>
  [[nodiscard]] constexpr rounded_raw<L> round_into_range(rational v)
  {
    if constexpr (!rounds_before_range_check<L, P>)
      return {raw_t<L>{}, false};
    else
    {
      // A bound past int64: 1/int64 still widens the limits by at least a step
      // (only a prefilter: the exact rounding decides).
      constexpr rational step = []
      {
        if constexpr (notched<L>)
          return notch64<L>;
        else
        {
          constexpr std::uint64_t D{to_rational(grid_of<L>.MaxDenominator).Num};
          constexpr std::uint64_t M{static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())};
          return rational{1, static_cast<std::int64_t>(D < M ? D : M)};
        }
      }();
      constexpr rational lo{try_sub(lower64<L>, step).value_or(lower64<L>)};
      constexpr rational hi{try_add(upper64<L>, step).value_or(upper64<L>)};
      if (v <= lo || v >= hi)
        return {raw_t<L>{}, false};
      return exact_round_into_range<L, P>(v);
    }
  }

  // Store-side form for the assignment paths:
  // when v rounds inside, the raw of the rounded value (an index or rational raw, no further rounding).
  // Cold and out of line, and it returns the raw in registers instead of writing through the caller's inside:
  // - a second call site of the large store functions stops GCC inlining them into the hot path
  //   (~40 instructions per in-range store);
  // - an escaping lhs address turns on the stack protector there (~3).
  template <insidable L, typename P>
  [[gnu::cold]] [[gnu::noinline]] constexpr rounded_raw<L> raw_if_rounds_inside(rational v)
  {
    return round_into_range<L, P>(v);
  }

  // The split offset quotient q + r/den of an unanchored grid rounded in value space:
  // the value Lower + (q + r/den)·Notch is below zero exactly when the offset num = q·den + r is below c·den,
  // c = −Lower/Notch, compared without overflow in wide integers (two limbs when c is a 64-bit rational).
  template <insidable L, typename P>
  [[nodiscard]] constexpr std::uint64_t
  round_offset_unanchored(std::uint64_t q, std::uint64_t r, std::uint64_t den) noexcept
  {
    constexpr round_mode m{rounding_for<L, P>};
    bool                 negative{false};
    if constexpr (lower_of<L> < 0)
    {
      constexpr grid_number c{-lower_index_of<L>}; // > 0
      constexpr grid_int    cn{grid_numerator(c)}, cd{grid_denominator(c)};
      using W = wide_uint<limbs_for_bits(64 + std::max(cn.bit_width(), cd.bit_width()))>;
      negative = W{q * den + r} * static_cast<W>(cd) < static_cast<W>(cn) * W{den};
    }
    return q + rounds_up(m, negative, classify_remainder(m, r, den), ((q & 1) != 0) != lower_index_odd<L>);
  }

  // Round the non-negative offset quotient q + r/den (q, r from one division of the offset by den ≥ 1, r < den)
  // to an integer notch index per L's rounding policy.
  // The tie and sign rules are in value space, not offset space,
  // so assigning a value rounds it the same way dividing down to it does (div_rounded is the reference).
  // The offset is ≥ 0 (the sign is lost by subtracting Lower);
  // with m = Lower/Notch the floor's value index is F = m + q, the value is below zero exactly when F is,
  // and the rounding moves F up by one or not: no second, signed division.
  // m is integral on every anchored grid, of any size; an unanchored one rounds the offset against the value's sign.
  template <insidable L, typename P>
  [[nodiscard]] constexpr std::uint64_t round_split(std::uint64_t q, std::uint64_t r, std::uint64_t den) noexcept
  {
    if constexpr (!anchored<L>)
      return round_offset_unanchored<L, P>(q, r, den);
    else
    {
      // F < 0 exactly when m < 0 and q < −m: every q when −m passes 64 bits.
      constexpr round_mode    mode{rounding_for<L, P>};
      constexpr grid_int      mneg{-lower_index_wide<L>};
      constexpr bool          far_below{grid_int{std::numeric_limits<std::uint64_t>::max()} < mneg};
      constexpr std::uint64_t bound{mneg.negative() || far_below ? std::uint64_t{0}
                                                                 : static_cast<std::uint64_t>(mneg)};
      const bool              negative{far_below || q < bound};
      return q + rounds_up(mode, negative, classify_remainder(mode, r, den), ((q & 1) != 0) != lower_index_odd<L>);
    }
  }

  // The offset quotient num/den rounded: round_split of its one division.
  template <insidable L, typename P>
  [[nodiscard]] constexpr std::uint64_t round_quotient(std::uint64_t num, std::uint64_t den) noexcept
  {
    return round_split<L, P>(num / den, num % den, den);
  }

  // ⌊c⌋, c = −Lower/Notch: the slot offset where L's values stop being negative (round_signed_slot),
  // when it fits int64.
  template <insidable L>
  inline constexpr grid_int zero_slot_floor = notched<L> ? floor_quotient(-lower_of<L>, notch_of<L>) : grid_int{0};
  template <insidable L>
  inline constexpr bool signed_slots_fit =
    notched<L> && !(zero_slot_floor<L> < grid_int{std::numeric_limits<std::int64_t>::min()})
    && !(grid_int{std::numeric_limits<std::int64_t>::max()} < zero_slot_floor<L>);

  // The slot rounding of round_split for any slot offset num/den (den > 0), past either limit too
  // (a clamp or a wrap takes the slot next), anchored or not:
  // the floor q moves up as P's mode decides, with the sign of the value Lower + (num/den)·Notch.
  // That value is below zero exactly when num/den < c:
  // q below ⌊c⌋, or q at ⌊c⌋ and the remainder below c's fraction (0 when anchored).
  template <insidable L, typename P>
    requires signed_slots_fit<L>
  [[nodiscard]] constexpr std::int64_t round_signed_slot(std::int64_t num, std::int64_t den) noexcept
  {
    std::int64_t q{num / den}, r{num % den};
    if (r < 0)
    {
      --q;
      r += den;
    }
    if (r == 0)
      return q;
    constexpr round_mode   m{rounding_for<L, P>};
    constexpr std::int64_t fc{static_cast<std::int64_t>(zero_slot_floor<L>)};
    bool                   negative{q < fc};
    if constexpr (!anchored<L>)
      if (q == fc)
      {
        constexpr grid_number frac{-lower_index_of<L> - grid_number{zero_slot_floor<L>}};
        constexpr grid_int    fn{grid_numerator(frac)}, fd{grid_denominator(frac)};
        using W = wide_uint<limbs_for_bits(64 + fd.bit_width())>;
        negative = W{static_cast<std::uint64_t>(r)} * static_cast<W>(fd)
                 < static_cast<W>(fn) * W{static_cast<std::uint64_t>(den)};
      }
    const auto ur{static_cast<std::uint64_t>(r)}, ud{static_cast<std::uint64_t>(den)};
    return q + rounds_up(m, negative, classify_remainder(m, ur, ud), ((q & 1) != 0) != lower_index_odd<L>);
  }

  // Defined in assignment.hpp.
  template <typename L, typename R>
  struct assignment;

  // A single-point source (Lower == Upper) carries one value,
  // so its notch question is only whether that value lies on L's lattice:
  // it admits 3_ins into {{0,9},3} and rejects 1_ins.
  // (Range is the interval check's job, so clamp and wrap still take an out-of-range point.)
  // The raw Factor says nothing here: a point's notch is 0.
  template <typename L, typename R>
  inline constexpr bool point_on_lattice = grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);

  // The notch half of inside_assignable for an insidable R.
  template <typename L, typename R>
  inline constexpr bool notches_compatible = []
  {
    if constexpr (point_grid<R>)
      return point_on_lattice<L, R>;
    else if constexpr (!notched<L> || !notched<R>)
      // A continuous target holds every value; a continuous source is checked when stored, like a rational scalar.
      return true;
    else
      // Every R value on L's lattice: R's notch a multiple of L's, and R's lattice anchored on L's.
      return grid_divides_evenly(notch_of<R>, notch_of<L>)
          && grid_same_lattice(lower_of<R>, lower_of<L>, notch_of<L>);
  }();

  // Tail of the policy cascade: checked reports.
  // It returns true when a policy handled the failure (the caller returns),
  // and reports through the static category message, no string: the cheap default.
  template <typename P>
  constexpr bool range_fail(P&& policy)
  {
    if (policy.checks_range())
    {
      policy.report(error::overflow);
      return true;
    }
    return false;
  }
} // namespace ins::detail

#endif // INSIDE_INSIDE_DETAIL_STORAGE_HPP
