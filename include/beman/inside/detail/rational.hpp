// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_RATIONAL_HPP
#define BEMAN_INSIDE_DETAIL_RATIONAL_HPP

#include <beman/inside/math.hpp>            // umax/imax, arithmetic, rational fwd
#include <beman/inside/lift.hpp>            // lift, is_expected_v, unwrap_t
#include <beman/inside/detail/overflow.hpp> // add/sub/mul_overflow
#include <beman/inside/detail/debug.hpp>    // errc, detail::raise, detail::constexpr_error

#include <expected>   // std::expected, std::unexpected

#include <numeric>
#include <compare>
#include <limits>
#include <tuple>
#include <type_traits>

namespace beman::inside::detail
{
  [[nodiscard]] constexpr umax abs_den(imax d) noexcept { return (d >= 0) ? static_cast<umax>(d) : umax{0} - static_cast<umax>(d); }

  // 64×64 → 128-bit unsigned product, as {hi, lo}. Native where the target has
  // unsigned __int128; else a schoolbook 32-bit split (32-bit targets) — the
  // same construction trusted in cmath.hpp's fmul, so both are bit-exact.
  struct u128 { umax Hi; umax Lo; };
  constexpr u128 umul(umax a, umax b)
  {
#if defined(__SIZEOF_INT128__)
    const unsigned __int128 p = static_cast<unsigned __int128>(a) * b;
    return {static_cast<umax>(p >> 64), static_cast<umax>(p)};
#endif
    umax al = a & 0xffffffffu, ah = a >> 32;
    umax bl = b & 0xffffffffu, bh = b >> 32;
    umax ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    umax mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
    return { hh + (lh >> 32) + (hl >> 32) + (mid >> 32),
             (ll & 0xffffffffu) | (mid << 32) };
  }
  constexpr std::strong_ordering cmp128(u128 a, u128 b)
  { return (a.Hi != b.Hi) ? (a.Hi <=> b.Hi) : (a.Lo <=> b.Lo); }

  // 128×64 product with an overflow flag (result beyond 128 bits).
  struct mul128_result { u128 Value; bool Overflowed; };
  constexpr mul128_result mul128(u128 a, umax b)
  {
    const u128 low  = umul(a.Lo, b);
    const u128 high = umul(a.Hi, b);
    const umax hi_sum = high.Lo + low.Hi;
    return {u128{hi_sum, low.Lo}, high.Hi != 0 || hi_sum < low.Hi};
  }

  // Quotient/remainder of a 128-bit dividend by a 64-bit divisor. Requires
  // 1 <= d <= imax_max (the rational-denominator domain) so the portable
  // partial remainder can never overflow when shifted.
  struct divmod128_result { u128 Quotient; umax Remainder; };
  constexpr divmod128_result divmod128(u128 n, umax d)
  {
#if defined(__SIZEOF_INT128__)
    using u128n = unsigned __int128;
    const u128n wide = (static_cast<u128n>(n.Hi) << 64) | n.Lo;
    const u128n q = wide / d;
    return {u128{static_cast<umax>(q >> 64), static_cast<umax>(q)},
            static_cast<umax>(wide % d)};
#else
    // Portable (no __int128, 32-bit targets): restoring shift-subtract divide — the same construction
    // as cmath.hpp's to_fixed fallback.
    u128 q{0, 0};
    umax r = 0;
    for (int i = 127; i >= 0; --i)
    {
      r = (r << 1) | ((i >= 64 ? (n.Hi >> (i - 64)) : (n.Lo >> i)) & 1u);
      q.Hi = (q.Hi << 1) | (q.Lo >> 63);
      q.Lo <<= 1;
      if (r >= d) { r -= d; q.Lo |= 1; }
    }
    return {q, r};
#endif
  }

  //---------------------------------------------------------------------------
  // trim
  //---------------------------------------------------------------------------
  inline constexpr void trim(umax& numerator, imax& denominator)
  {
    umax ad = abs_den(denominator);
    if (ad <= 1)
      return;
    auto g = std::gcd(numerator, ad);
    if (g <= 1)
      return;
    numerator /= g;
    ad /= g;
    denominator = (denominator < 0) ? -ad : ad;
  }

  inline constexpr void trim(umax& a, umax& b)
  {
    if (a <= 1 || b <= 1)
      return;
    auto g = std::gcd(a, b);
    if (g <= 1)
      return;
    a /= g;
    b /= g;
  }

  [[nodiscard]] constexpr std::expected<rational, errc> operator+(rational const&, rational const&);
  [[nodiscard]] constexpr std::expected<rational, errc> operator/(rational const&, rational const&);
  [[nodiscard]] constexpr std::expected<rational, errc> operator-(rational const&, rational const&);

  [[nodiscard]] constexpr std::expected<rational, errc> operator*(rational const&, rational const&);
  [[nodiscard]] constexpr auto     operator<=>(rational, rational) -> std::strong_ordering;

  //---------------------------------------------------------------------------
  // Overflow / malformed-literal signalling
  //---------------------------------------------------------------------------
  // Failure aborts constant evaluation via `detail::constexpr_error<Msg>()` — a
  // non-constexpr [[noreturn]] helper carrying the message in an NTTP (literal
  // parsers and the checked paths' `fail<"...">` under `if consteval`),
  // hard-failing the build with the text in the diagnostic; at runtime those
  // paths fall through to `std::unexpected`. No `throw`, so it is -fno-exceptions clean.

  //---------------------------------------------------------------------------
  // rational — structural type for NTTP (public members only). Sign is encoded
  // in the denominator (negative = negative rational); numerator is unsigned to
  // represent e.g. umax itself.
  //---------------------------------------------------------------------------
  struct rational
  {
    umax Numerator;
    imax Denominator;

    constexpr rational() = default;
    constexpr rational(std::floating_point  auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1)
     :Numerator{num}, Denominator{den}
    { canonicalize(Numerator, Denominator); }

    // Two-unsigned overload: lets `rational{i, N}` accept two `size_t`
    // operands without forcing the caller to `static_cast<imax>` the
    // numerator. Numerator is unsigned anyway; the cast on `den` is
    // safe because the unsigned-integral concept exclude negative inputs.
    template <std::unsigned_integral N, std::unsigned_integral D>
    constexpr rational(N num, D den)
     :Numerator{num}, Denominator{static_cast<imax>(den)}
    { canonicalize(Numerator, Denominator); }

    // Implicit unwrap of a checked result, so coefficient expressions read as
    // plain arithmetic (`rational two_pi = 2 * pi;`); an error (overflow) is a
    // compile error in constant evaluation, a throw at runtime. same_as-constrained
    // (not a plain `rational(expected<rational, errc>)`) because expected's own
    // converting ctor is gated on `is_constructible_v<rational, U>` — a
    // non-template overload would make that trait depend on itself.
    template <class O>
      requires std::same_as<std::remove_cvref_t<O>, std::expected<rational, errc>>
    constexpr rational(O&& o) : rational(o.value()) {}

    // operator== by default for structural type
    [[nodiscard]] constexpr bool operator==(const rational&) const = default;
    template <arithmetic T>
    [[nodiscard]] constexpr bool operator==(T value) const { return operator==(rational{value}); }

    [[nodiscard]] constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr std::expected<T, errc> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T () const
    {
      if (Denominator < 0)
        raise(errc::domain_error, "cannot convert negative rational to unsigned");
      return Numerator / abs_den(Denominator);
    }

    template <std::signed_integral T>
    explicit constexpr operator T () const
    {
      umax q = Numerator / abs_den(Denominator);
      return (Denominator < 0) ? -q : q;
    }

    template <std::floating_point T>
    explicit constexpr operator T () const
    {
      T q = static_cast<T>(Numerator) / static_cast<T>(abs_den(Denominator));
      return (Denominator < 0) ? -q : q;
    }

    // allow unary+ for generic programming
    [[nodiscard]] constexpr rational operator+() const { return *this; }

    // Compound-assign: forward to the checked binary op and unwrap via .value()
    // — overflow surfaces as std::bad_expected_access (no error channel here).
    constexpr rational& operator+=(rational const& rhs);
    constexpr rational& operator-=(rational const& rhs);
    constexpr rational& operator*=(rational const& rhs);
    constexpr rational& operator/=(rational const& rhs);

    // Unchecked arithmetic — caller takes responsibility for non-overflow
    // (and non-zero operand for div_unchecked / inv_unchecked).
    static constexpr rational add_unchecked(rational, rational);
    static constexpr rational mul_unchecked(rational, rational);
    static constexpr rational div_unchecked(rational, rational);
    static constexpr rational inv_unchecked(rational);

    static constexpr std::expected<rational, errc> add(rational a, rational b)
    { return a + b; }
    static constexpr std::expected<rational, errc> inv(rational);

    // Shared algorithm bodies. Checked=true returns expected<rational, errc>,
    // reporting overflow / division_by_zero (a compile error at compile time,
    // std::unexpected at runtime); Checked=false
    // silently overflows — the caller must guarantee its absence.
    template <bool Checked> static constexpr auto add_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto mul_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto div_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto inv_impl(rational const&);

  private:
    // Domain check + canonical-zero + gcd reduction; used by the integral ctors.
    // Two domain errors: Denominator == 0 (undefined)
    // and Denominator == imax_min (cannot be negated without UB, which every
    // sign-flip in the file assumes is well-defined).
    static constexpr void canonicalize(umax& num, imax& den)
    {
      if (den == 0)
        raise(errc::domain_error, "Denominator of Zero is invalid");
      if (den == std::numeric_limits<imax>::min())
        raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
      if (num == 0) den = 1;
      trim(num, den);
    }

    // Signed-encoded denominator for the signed ctor, validated BEFORE the
    // negation so `-den` is never UB (the ctor-body canonicalize() would catch
    // these too, but only after the mem-init already evaluated `-den`).
    static constexpr imax signed_den_from(std::signed_integral auto num, imax den)
    {
      if (den == 0)
        raise(errc::domain_error, "Denominator of Zero is invalid");
      if (den == std::numeric_limits<imax>::min())
        raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
      return (num < 0) ? -den : den;
    }
  };


  [[nodiscard]] constexpr std::expected<rational, errc> gcd(rational const&, rational const&);
  [[nodiscard]] constexpr rational abs(rational);

  [[nodiscard]] constexpr bool divides_evenly(rational const&, rational const&);

  //---------------------------------------------------------------------------
  // sign / named integer reductions — free functions over the public
  // numerator/denominator (structural type), siblings of abs/gcd. Reductions
  // are explicit, lossy alternatives to `static_cast`:
  // trunc → 0; floor → -inf; ceil → +inf; round → half-away-from-zero.
  //---------------------------------------------------------------------------
  // -1 / 0 / +1 — single source of truth for the sign convention (sign lives in
  // Denominator; canonical zero is {0, 1}).
  [[nodiscard]] constexpr int sign(rational v) noexcept
  {
    if (v.Numerator == 0) return 0;
    return (v.Denominator < 0) ? -1 : 1;
  }

  // A rational from already-canonical parts (magnitude, signed denominator):
  // no gcd, no domain checks.
  [[nodiscard]] constexpr rational make_raw(umax num, imax den) noexcept
  {
    rational r;
    r.Numerator   = num;
    r.Denominator = den;
    return r;
  }

  // A checked op failed: during constant evaluation that is a compile error
  // naming the cause; at runtime it is an error value.
  template <fixed_string Msg>
  constexpr std::unexpected<errc> fail(errc code)
  {
    if consteval { constexpr_error<Msg>(); }
    return std::unexpected{code};
  }

  // The numerator with the value's sign, as imax (callers ensure it fits).
  [[nodiscard]] constexpr imax signed_numerator(rational v) noexcept
  {
    const imax n = static_cast<imax>(v.Numerator);
    return (v.Denominator < 0) ? -n : n;
  }

  [[nodiscard]] constexpr imax trunc(rational v)
  {
    umax q = v.Numerator / abs_den(v.Denominator);
    return (v.Denominator < 0) ? -q : q;
  }

  [[nodiscard]] constexpr imax floor(rational v)
  {
    umax ad = abs_den(v.Denominator);
    umax q = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // negative with non-zero remainder: step one further toward -inf
    if (v.Denominator < 0 && rem != 0)
      return -q - 1;
    return (v.Denominator < 0) ? -q : q;
  }

  [[nodiscard]] constexpr imax ceil(rational v)
  {
    umax ad  = abs_den(v.Denominator);
    umax q   = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // negative value: ceiling toward +inf coincides with truncation toward zero
    if (v.Denominator < 0)
      return -q;
    // positive with non-zero remainder: step one further toward +inf
    return q + (rem != 0 ? 1 : 0);
  }

  [[nodiscard]] constexpr imax round(rational v)
  {
    umax ad = abs_den(v.Denominator);
    umax q = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // half-away-from-zero: bump magnitude when 2*rem >= ad
    if (rem * 2 >= ad) ++q;
    return (v.Denominator < 0) ? -q : q;
  }

  //---------------------------------------------------------------------------
  // abs
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr rational abs(rational v)
  { if (v.Denominator < 0) v.Denominator = -v.Denominator; return v; }

  //---------------------------------------------------------------------------
  // gcd
  //---------------------------------------------------------------------------
  // Returns errc::overflow if the combined denominator lcm = (a/gcd)·b would exceed
  // imax_max (sign-bit reservation) — traps the mul_overflow then range-checks.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr std::expected<rational, errc> gcd(rational const& lhs, rational const& rhs)
  {
    umax a = abs_den(lhs.Denominator);
    umax b = abs_den(rhs.Denominator);
    umax g = std::gcd(a, b);

    umax denominator;
    if (mul_overflow(a / g, b, &denominator))
      return std::unexpected{errc::overflow};
    if (denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      return std::unexpected{errc::overflow};

    auto numerator = std::gcd(lhs.Numerator, rhs.Numerator);
    return rational{numerator, denominator};
  }

  //---------------------------------------------------------------------------
  // rational::rational
  //---------------------------------------------------------------------------
  constexpr rational::rational(std::signed_integral auto num, imax den)
   :Numerator{safe_abs(num)},
    Denominator{ signed_den_from(num, den) }
  { canonicalize(Numerator, Denominator); }

  constexpr rational::rational(std::floating_point auto value)
  {
    if (not is_finite(value))
      raise(errc::not_finite, "non-finite double");

    if (value == 0.0)
    {
      Numerator = 0;
      Denominator = 1;
      return;
    }

    bool neg = (value < 0.0);
    if (neg) value = -value;

    auto [num, den] = abs_fraction(value);
    Numerator = num;
    Denominator = neg ? -den : den;
    // trim not needed, because abs_fraction already trims in its special case
  }

  //---------------------------------------------------------------------------
  // to
  //---------------------------------------------------------------------------
  template <std::unsigned_integral T>
  constexpr std::expected<T, errc> rational::to() const
  {
    if (Denominator < 0) return std::unexpected{errc::domain_error};
    return static_cast<T>(Numerator / abs_den(Denominator));
  }

  //---------------------------------------------------------------------------
  // _ins / _r literal parser — shared between inside.hpp's `_ins` and `_r` below.
  // Accepts:
  //   integer:           5, 1'000
  //   decimal:           1.25, .5
  //   decimal scientific 1.5e2, 2.5e-1
  //   hex integer:       0xff
  //   binary integer:    0b1010
  //   hex float (Q-fmt): 0x1p15, 0x1p-15, 0x1.8p3
  // Exact (no double round-trip). Overflow -> consteval throw.
  //---------------------------------------------------------------------------
    consteval int parse_digit(char c, int base)
    {
      if (c >= '0' && c <= '9')
      {
        int d = c - '0';
        return d < base ? d : -1;
      }
      if (base == 16)
      {
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
      }
      return -1;
    }

    template<char... Chars>
    consteval rational parse_ins_literal()
    {
      constexpr char src[] = { Chars..., '\0' };
      constexpr std::size_t N = sizeof...(Chars);

      // Detect radix prefix.
      int base = 10;
      std::size_t i = 0;
      if (N >= 2 && src[0] == '0')
      {
        if (src[1] == 'x' || src[1] == 'X') { base = 16; i = 2; }
        else if (src[1] == 'b' || src[1] == 'B') { base = 2; i = 2; }
      }

      umax num = 0;
      int frac_len = 0;
      bool in_frac = false;
      int exp = 0;
      bool exp_neg = false;
      bool has_p_exp = false;  // 2^exp (hex floats)
      bool has_e_exp = false;  // 10^exp (decimal scientific)
      bool in_exp = false;
      bool exp_seen_digit = false;

      for (; i < N; ++i)
      {
        char c = src[i];
        if (c == '\'') continue;

        if (in_exp)
        {
          if (!exp_seen_digit && (c == '+' || c == '-'))
          {
            exp_neg = (c == '-');
            continue;
          }
          if (c >= '0' && c <= '9')
          {
            exp = exp * 10 + (c - '0');
            exp_seen_digit = true;
            continue;
          }
          constexpr_error<"_ins/_r literal: invalid char in exponent">();
        }

        if (c == '.')
        {
          if (in_frac) constexpr_error<"_ins/_r literal: multiple '.'">();
          if (base == 2) constexpr_error<"_ins/_r literal: '.' not allowed in binary">();
          in_frac = true;
          continue;
        }

        if ((c == 'p' || c == 'P') && base == 16)
        {
          has_p_exp = true;
          in_exp = true;
          continue;
        }

        if ((c == 'e' || c == 'E') && base == 10)
        {
          has_e_exp = true;
          in_exp = true;
          continue;
        }

        int d = parse_digit(c, base);
        if (d < 0) constexpr_error<"_ins/_r literal: invalid digit for radix">();

        umax base_u = base;
        if (num > (~umax{0} - d) / base_u)
          constexpr_error<"_ins/_r literal: numerator overflow">();
        num = num * base_u + d;
        if (in_frac) ++frac_len;
      }

      // Build denominator from fractional part.
      // For decimal: den = 10^frac_len. For hex: den = 2^(4*frac_len).
      umax den = 1;
      if (base == 10)
      {
        for (int k = 0; k < frac_len; ++k)
        {
          if (den > (~umax{0}) / 10u)
            constexpr_error<"_ins/_r literal: denominator overflow">();
          den *= 10u;
        }
      }
      else if (base == 16)
      {
        int shift = 4 * frac_len;
        if (shift >= 64) constexpr_error<"_ins/_r literal: hex fraction too long">();
        den <<= shift;
      }

      // Apply binary exponent (hex floats, `p`).
      if (has_p_exp)
      {
        if (exp >= 63) constexpr_error<"_ins/_r literal: p exponent too large">();
        if (!exp_neg) num <<= exp;
        else
        {
          if (den > (~umax{0}) >> exp)
            constexpr_error<"_ins/_r literal: p exponent denominator overflow">();
          den <<= exp;
        }
      }

      // Apply decimal exponent (decimal scientific, `e`).
      if (has_e_exp)
      {
        for (int k = 0; k < exp; ++k)
        {
          if (!exp_neg)
          {
            if (num > (~umax{0}) / 10u)
              constexpr_error<"_ins/_r literal: e exponent numerator overflow">();
            num *= 10u;
          }
          else
          {
            if (den > (~umax{0}) / 10u)
              constexpr_error<"_ins/_r literal: e exponent denominator overflow">();
            den *= 10u;
          }
        }
      }

      return rational{num, den};
    }

  template<char... Chars>
  constexpr rational operator ""_r() { return parse_ins_literal<Chars...>(); }

  // notch<N, D> is defined publicly in `namespace beman::inside` (see the re-export block
  // at the end of this header) so consumers spell it without naming the
  // internal representation type.

  //---------------------------------------------------------------------------
  // add_impl / mul_impl / div_impl — shared bodies (Checked toggles overflow)
  //---------------------------------------------------------------------------
  template <bool Checked>
  inline constexpr auto rational::add_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    // (a == −b needs no test: equal denominators, opposite signs → num == 0 below.)
    if (a.Numerator == 0) return ret_t{b};
    if (b.Numerator == 0) return ret_t{a};

    bool a_neg = a.Denominator < 0;
    bool b_neg = b.Denominator < 0;
    umax a_ad = abs_den(a.Denominator);
    umax b_ad = abs_den(b.Denominator);

    if (a_ad == b_ad)
    {
      if (a_neg == b_neg)
      {
        umax numerator;
        if constexpr (Checked)
        {
          if (add_overflow(a.Numerator, b.Numerator, &numerator))
          { return ret_t{fail<"rational +: numerator overflow (same denominator)">(errc::overflow)}; }
        }
        else
          numerator = a.Numerator + b.Numerator;

        rational r;
        r.Numerator = numerator;
        r.Denominator = a.Denominator;
        trim(r.Numerator, r.Denominator);
        return ret_t{r};
      }

      umax num = (a.Numerator > b.Numerator) ? (a.Numerator - b.Numerator)
                                              : (b.Numerator - a.Numerator);
      bool r_neg = a_neg ? (a.Numerator > b.Numerator) : (b.Numerator > a.Numerator);
      if (num == 0) return ret_t{0_r};
      rational r;
      r.Numerator = num;
      r.Denominator = r_neg ? -a_ad : a_ad;
      trim(r.Numerator, r.Denominator);
      return ret_t{r};
    }

    // Common denominator = lcm(a_ad, b_ad) = a_ad·(b_ad/g), not a_ad·b_ad: the
    // reduced cofactors (g = gcd) overflow far less often than the raw product.
    umax g     = std::gcd(a_ad, b_ad);
    umax a_ad_r = a_ad / g;       // = a_ad / gcd; coprime with b_ad_r
    umax b_ad_r = b_ad / g;

    umax denominator;
    umax A;
    umax B;

    if constexpr (Checked)
    {
      if (mul_overflow(a_ad, b_ad_r, &denominator)    ||   // = lcm(a_ad, b_ad)
          denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      { return ret_t{fail<"rational +: denominator overflow">(errc::overflow)}; }
      if (mul_overflow(a.Numerator, b_ad_r, &A) ||
          mul_overflow(b.Numerator, a_ad_r, &B))
      {
        // Mixed signs: |A − B| can fit umax even when a cross-product alone
        // does not (e.g. 1024 − m/2^54 forms 1024·2^54 == 2^64 before the
        // subtraction brings it back in range — the dbl-engine store path hit
        // exactly this). Retry the difference in 128-bit before giving up.
        if (a_neg != b_neg)
        {
          const u128 A128 = umul(a.Numerator, b_ad_r);
          const u128 B128 = umul(b.Numerator, a_ad_r);
          const bool a_bigger = cmp128(A128, B128) > 0;
          const u128 big   = a_bigger ? A128 : B128;
          const u128 small = a_bigger ? B128 : A128;
          const u128 diff{big.Hi - small.Hi - (big.Lo < small.Lo ? 1u : 0u),
                          big.Lo - small.Lo};
          if (diff.Hi == 0)
          {
            rational r;
            r.Numerator   = diff.Lo;
            r.Denominator = (a_neg ? a_bigger : !a_bigger) ? -denominator
                                                           :  denominator;
            trim(r.Numerator, r.Denominator);
            return ret_t{r};
          }
        }
        return ret_t{fail<"rational +: cross-multiplication overflow">(errc::overflow)};
      }
    }
    else
    {
      denominator = a_ad * b_ad_r;
      A = a.Numerator * b_ad_r;
      B = b.Numerator * a_ad_r;
    }

    if (a_neg == b_neg)
    {
      umax numerator;
      if constexpr (Checked)
      {
        if (add_overflow(A, B, &numerator))
        { return ret_t{fail<"rational +: numerator sum overflow">(errc::overflow)}; }
      }
      else
        numerator = A + B;

      // num, den both > 0 here, so the ctor's domain/zero checks are dead;
      // assemble directly and trim.
      rational r;
      r.Numerator   = numerator;
      r.Denominator = a_neg ? -denominator
                             :  denominator;
      trim(r.Numerator, r.Denominator);
      return ret_t{r};
    }

    // numerator == 0 (exact cancellation) is unreachable here: it would
    // require a == -b, which the `a == -b` early-return at the top of
    // add_impl already handles for canonical inputs.
    umax numerator = (A > B) ? (A - B) : (B - A);
    bool r_neg = a_neg ? (A > B) : (B > A);
    rational r;
    r.Numerator   = numerator;
    r.Denominator = r_neg ? -denominator
                           :  denominator;
    trim(r.Numerator, r.Denominator);
    return ret_t{r};
  }

  template <bool Checked>
  inline constexpr auto rational::mul_impl(rational const& a_in, rational const& b_in)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;
    rational a = a_in, b = b_in;

    if (a.Numerator == 0 || b.Numerator == 0) return ret_t{0_r};

    bool r_neg = (a.Denominator < 0) != (b.Denominator < 0);
    umax a_ad = abs_den(a.Denominator);
    umax b_ad = abs_den(b.Denominator);

    if (a_ad == 1 && b_ad == 1)
    {
      umax numerator;
      if constexpr (Checked)
      {
        if (mul_overflow(a.Numerator, b.Numerator, &numerator))
        { return ret_t{fail<"rational *: numerator overflow">(errc::overflow)}; }
      }
      else
        numerator = a.Numerator * b.Numerator;

      return ret_t{make_raw(numerator, r_neg ? imax{-1} : imax{1})};
    }

    trim(a.Numerator, b_ad);
    trim(b.Numerator, a_ad);

    umax numerator;
    umax denominator;
    if constexpr (Checked)
    {
      if (mul_overflow(a.Numerator, b.Numerator, &numerator) ||
          mul_overflow(a_ad, b_ad, &denominator)             ||
          denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      { return ret_t{fail<"rational *: numerator or denominator overflow">(errc::overflow)}; }
    }
    else
    {
      numerator = a.Numerator * b.Numerator;
      denominator = a_ad * b_ad;
    }

    // The cross-trims above guarantee gcd(numerator, denominator) == 1, so
    // bypass rational(num, den) and skip its redundant trim.
    return ret_t{make_raw(numerator, r_neg ? -denominator : denominator)};
  }

  //---------------------------------------------------------------------------
  // inv_impl — multiplicative inverse (1 / a)
  //---------------------------------------------------------------------------
  // a is already trimmed (Numerator and |Denominator| coprime), so the swapped
  // pair is also trimmed. Sign lives in the denominator and 1/(-x) has the same
  // sign as -x, so the sign bit moves with the (now) denominator unchanged.
  template <bool Checked>
  inline constexpr auto rational::inv_impl(rational const& a)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked)
    {
      // a.Numerator goes into the result's Denominator slot, so it must fit in
      // imax (else the umax→imax conversion wraps and a later -Denominator is UB).
      if (a.Numerator == 0)
      { return ret_t{fail<"rational inv: division by zero">(errc::division_by_zero)}; }
      if (a.Numerator > static_cast<umax>(std::numeric_limits<imax>::max()))
      { return ret_t{fail<"rational inv: numerator out of denominator range">(errc::overflow)}; }
    }

    return ret_t{make_raw(abs_den(a.Denominator), signed_numerator(a))};
  }

  // div(a, b) = a * inv(b). The checked path goes through inv_impl<true> so
  // the b.Numerator-fits-in-imax check (added there) propagates here too;
  // the unchecked path skips it (caller's contract).
  template <bool Checked>
  inline constexpr auto rational::div_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked)
    {
      auto inv_b = inv_impl<true>(b);
      if (!inv_b.has_value()) return ret_t{std::unexpected{inv_b.error()}};
      return mul_impl<true>(a, *inv_b);
    }
    else
      return mul_impl<false>(a, inv_impl<false>(b));
  }

  //---------------------------------------------------------------------------
  // unchecked rational arithmetic — caller guarantees: no umax overflow on the
  // products, no zero divisor/numerator, and the result Denominator fits in imax.
  // The checked variants enforce all three; unchecked skips them.
  //---------------------------------------------------------------------------
  inline constexpr rational rational::add_unchecked(rational a, rational b)
  { return add_impl<false>(a, b); }

  inline constexpr rational rational::mul_unchecked(rational a, rational b)
  { return mul_impl<false>(a, b); }

  inline constexpr rational rational::div_unchecked(rational a, rational b)
  { return div_impl<false>(a, b); }

  inline constexpr rational rational::inv_unchecked(rational a)
  { return inv_impl<false>(a); }

  inline constexpr std::expected<rational, errc> rational::inv(rational a)
  { return inv_impl<true>(a); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr rational rational::operator-() const
  {
    if (Numerator == 0)
      return *this;

    // Already trimmed; flip the sign-encoding directly without re-running trim.
    return make_raw(Numerator, -Denominator);
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering
  {
    int lhs_sign = sign(lhs);
    int rhs_sign = sign(rhs);

    if (lhs_sign != rhs_sign)
      return lhs_sign <=> rhs_sign;

    if (lhs_sign == 0)
      return std::strong_ordering::equal;

    // signs are equal here (the `lhs_sign != rhs_sign` branch returned above)
    bool lhs_neg = lhs_sign < 0;

    umax lhs_ad = abs_den(lhs.Denominator);
    umax rhs_ad = abs_den(rhs.Denominator);

    // integer comparison: skip cross-multiply entirely
    if (lhs_ad == 1 && rhs_ad == 1)
    {
      if (lhs_neg)
        return rhs.Numerator <=> lhs.Numerator;
      else
        return lhs.Numerator <=> rhs.Numerator;
    }

    // One side is an integer: compare via divmod instead of cross-multiply.
    // Cross-multiplying would otherwise overflow when the non-integer side
    // has a huge denominator (e.g. doubles like 19.99 stored as N/2^48).
    if (rhs_ad == 1)
    {
      umax q = lhs.Numerator / lhs_ad;
      umax r = lhs.Numerator % lhs_ad;
      auto cmp = (q == rhs.Numerator) ? (r == 0 ? std::strong_ordering::equal
                                                : std::strong_ordering::greater)
                                      : (q <=> rhs.Numerator);
      return lhs_neg ? (0 <=> cmp) : cmp;
    }
    if (lhs_ad == 1)
    {
      umax q = rhs.Numerator / rhs_ad;
      umax r = rhs.Numerator % rhs_ad;
      auto cmp = (q == lhs.Numerator) ? (r == 0 ? std::strong_ordering::equal
                                                : std::strong_ordering::less)
                                      : (lhs.Numerator <=> q);
      return lhs_neg ? (0 <=> cmp) : cmp;
    }

    // Cross-multiply in 128-bit: |numerator| and |denominator| are each ≤ 2^64−1,
    // so the products fit exactly in 128 bits — the comparison can never overflow,
    // so no trap is needed.
#if defined(__SIZEOF_INT128__)
    using u128n = unsigned __int128;
    u128n A = static_cast<u128n>(lhs.Numerator) * rhs_ad;
    u128n B = static_cast<u128n>(rhs.Numerator) * lhs_ad;
    return lhs_neg ? (B <=> A) : (A <=> B);
#else
    // Portable path (no __int128): form each product as {hi, lo} and compare lexically.
    const u128 A = umul(lhs.Numerator, rhs_ad);
    const u128 B = umul(rhs.Numerator, lhs_ad);
    return lhs_neg ? cmp128(B, A) : cmp128(A, B);
#endif
  }

  template <typename T>
  [[nodiscard]] inline constexpr auto operator<=>(std::expected<T, errc> const& lhs, const rational& rhs)
  { return rational{lhs.value()} <=> rhs; }

  template <typename T>
  [[nodiscard]] inline constexpr auto operator<=>(rational const& lhs, std::expected<T, errc> const& rhs)
  { return lhs <=> rational{rhs.value()}; }

  template <arithmetic T>
  [[nodiscard]] inline constexpr auto operator<=>(T lhs, const rational& rhs)
  { return rational{lhs} <=> rhs; }

  template <arithmetic T>
  [[nodiscard]] inline constexpr auto operator<=>(rational const& lhs, T rhs)
  { return lhs <=> rational{rhs}; }

  //---------------------------------------------------------------------------
  // Expected-lifting operators — one generic overload per arithmetic operator
  // that engages when an operand is a std::expected, both unwrap to arithmetic,
  // and at least one to rational. Gating on `arithmetic` (not `insidable`, which
  // isn't visible this low) excludes inside operands, so inside-involving expected
  // expressions partition cleanly to arithmetic.hpp's generic instead.
  //---------------------------------------------------------------------------
  template <class L, class R>
  concept rational_lift_operands =
       (is_expected_v<L> || is_expected_v<R>)
    && arithmetic<unwrap_t<L>> && arithmetic<unwrap_t<R>>
    && (std::same_as<unwrap_t<L>, rational> || std::same_as<unwrap_t<R>, rational>);

  //---------------------------------------------------------------------------
  // Binary operators: the checked rational ⋈ rational cores, then per operator
  // the arithmetic-operand forms (direct construction, no lift overhead), the
  // expected-operand form (propagates via lift), and the compound assignments.
  // Compound assignments unwrap with .value() — std::bad_expected_access on
  // overflow; callers needing a non-throwing path use the binary operators.
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr std::expected<rational, errc> operator+(rational const& lhs, rational const& rhs)
  { return rational::add_impl<true>(lhs, rhs); }

  [[nodiscard]] inline constexpr std::expected<rational, errc> operator-(rational const& lhs, rational const& rhs)
  { return operator+(lhs, -rhs); }

  [[nodiscard]] inline constexpr std::expected<rational, errc> operator*(rational const& lhs, rational const& rhs)
  { return rational::mul_impl<true>(lhs, rhs); }

  [[nodiscard]] inline constexpr std::expected<rational, errc> operator/(rational const& lhs, rational const& rhs)
  { return rational::div_impl<true>(lhs, rhs); }

  [[nodiscard]] inline constexpr std::expected<rational, errc> operator-(std::expected<rational, errc> const& v)
  { return lift([](rational r){ return -r; }, v); }

#define BEMAN_INSIDE_RATIONAL_OP(op)                                                   \
  template <arithmetic T>                                                              \
  [[nodiscard]] inline constexpr auto operator op(T lhs, rational const& rhs)          \
  { return rational{lhs} op rhs; }                                                     \
  template <arithmetic T>                                                              \
  [[nodiscard]] inline constexpr auto operator op(rational const& lhs, T rhs)          \
  { return lhs op rational{rhs}; }                                                     \
  template <class L, class R> requires rational_lift_operands<L, R>                    \
  [[nodiscard]] inline constexpr auto operator op(L const& lhs, R const& rhs)          \
  { return lift([](auto const& a, auto const& b){ return a op b; }, lhs, rhs); }       \
  inline constexpr rational& rational::operator op##=(rational const& rhs)             \
  { *this = (*this op rhs).value(); return *this; }                                    \
  template <arithmetic T>                                                              \
  inline constexpr rational& operator op##=(rational& lhs, T rhs)                      \
  { return lhs op##= rational{rhs}; }                                                  \
  inline constexpr rational& operator op##=(rational& lhs, std::expected<rational, errc> const& rhs) \
  { return lhs op##= rhs.value(); }

  BEMAN_INSIDE_RATIONAL_OP(+)
  BEMAN_INSIDE_RATIONAL_OP(-)
  BEMAN_INSIDE_RATIONAL_OP(*)
  BEMAN_INSIDE_RATIONAL_OP(/)
#undef BEMAN_INSIDE_RATIONAL_OP

  //---------------------------------------------------------------------------
  // divides_evenly
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr bool divides_evenly(rational const& dividend, rational const& divisor)
  {
    if (divisor == 0) return true;            // convention: everything divides 0 evenly
    if (dividend.Numerator == 0) return true; // 0 / anything is the integer 0

    // dividend/divisor ∈ ℤ without forming the (possibly umax-overflowing)
    // quotient numerator. In lowest terms dividend = p/q, divisor = r/s; the
    // quotient p·s/(q·r) is integral iff q | s and r | p (rationals are
    // canonicalized, so gcd(p,q)=gcd(r,s)=1). All checks are on single fields.
    const umax p = dividend.Numerator, q = abs_den(dividend.Denominator);
    const umax r = divisor.Numerator,  s = abs_den(divisor.Denominator);
    return (s % q == 0) && (p % r == 0);
  }

} // namespace beman::inside::detail

namespace beman::inside
{
  // `rational` is internal, but the grid-building `notch<N,D>` / `frac<N,D>`
  // literals are public — they never expose the type.
  template <umax N, imax D = 1>
  inline constexpr detail::rational notch = detail::rational{N, D};

  // frac<N, D> — exact fractional grid value (signed numerator), companion to
  // notch<N,D> for non-dyadic endpoints not writable as a float literal
  // (e.g. `frac<-6, 5>` for -1.2).
  template <imax N, imax D = 1>
  inline constexpr detail::rational frac = detail::rational{N, D};
} // namespace beman::inside

#endif // BEMAN_INSIDE_DETAIL_RATIONAL_HPP

