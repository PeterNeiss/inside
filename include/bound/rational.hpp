//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrationalHPP
#define BNDrationalHPP

#include "bound/math.hpp"            // umax/imax, arithmetic, rational fwd
#include "bound/lift.hpp"            // lift, is_slim_optional_v, unwrap_t, slim::optional
#include "bound/detail/overflow.hpp" // add/sub/mul_overflow
#include "bound/detail/debug.hpp"    // errc, make_error_code, <system_error>

#include "slim/expected.hpp"     // slim::expected, slim::unexpected

#include <numeric>
#include <compare>
#include <cmath>
#include <limits>
#include <tuple>
#include <type_traits>      // std::is_constant_evaluated

namespace bnd::detail { struct rational; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::detail::rational>
  {
    protected:
      static constexpr bnd::detail::rational sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::detail::rational& v) noexcept;
  };
} // namespace slim

namespace bnd::detail
{
  constexpr umax abs_den(imax d) { return (d >= 0) ? static_cast<umax>(d) : -static_cast<umax>(d); }

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

  constexpr slim::optional<rational> operator+(rational const&, rational const&);
  constexpr slim::optional<rational> operator/(rational const&, rational const&);
  constexpr slim::optional<rational> operator-(rational const&, rational const&);

  constexpr slim::optional<rational> operator*(rational const&, rational const&);
  constexpr auto     operator<=>(rational, rational) -> std::strong_ordering;

  //---------------------------------------------------------------------------
  // Overflow / malformed-literal signalling
  //---------------------------------------------------------------------------
  // Failure aborts constant evaluation directly via `throw`: the `consteval`
  // `_b`/`_r` literal parsers throw on malformed input, and the checked
  // arithmetic paths throw under `if (std::is_constant_evaluated())`. A `throw`
  // reached during constant evaluation hard-fails the build with the message;
  // at runtime the arithmetic paths never enter that branch and fall through to
  // `nullopt` instead. There is deliberately no dedicated always-throwing
  // helper — as a `consteval`/`constexpr` function its body could never be a
  // constant expression, which is ill-formed NDR and a hard error on some
  // toolchains (e.g. the Xilinx aarch64 GCC).

  //---------------------------------------------------------------------------
  // rational
  //---------------------------------------------------------------------------
  // Must be a structural type for template NTTP (only public members)
  //---------------------------------------------------------------------------
  // Sign is encoded in the denominator (negative denominator = negative rational)
  // Numerator is unsigned to represent e.g. umax itself
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

    // operator== by default for structural type
    constexpr bool operator==(const rational&) const = default;
    template <arithmetic T>
    constexpr bool operator==(T value) const { return operator==(rational{value}); }

    constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr slim::expected<T, errc> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T () const
    {
      if (Denominator < 0)
        throw std::system_error(make_error_code(errc::domain_error), "cannot convert negative rational to unsigned");
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
    constexpr rational operator+() const { return *this; }

    // Compound-assign operators. Each forwards to the checked binary op and
    // unwraps the resulting optional via .value() — overflow surfaces as
    // slim::bad_optional_access rather than as a return value, matching the
    // in-place idiom (no error channel available).
    constexpr rational& operator+=(rational const& rhs);
    constexpr rational& operator-=(rational const& rhs);
    constexpr rational& operator*=(rational const& rhs);
    constexpr rational& operator/=(rational const& rhs);

    // -1 / 0 / +1 — single source of truth for the sign convention
    // (sign lives in Denominator; canonical zero is {0, 1}).
    [[nodiscard]] constexpr int sign() const noexcept
    {
      if (Numerator == 0) return 0;
      return (Denominator < 0) ? -1 : 1;
    }

    // The sentinel slot is {N, 0}; only make_sentinel() can produce one.
    [[nodiscard]] constexpr bool is_sentinel() const noexcept
    { return Denominator == 0; }

    // Named integer reductions — explicit, lossy, intent-clear alternatives
    // to `static_cast<imax>(r)`. trunc rounds toward zero (matches operator T);
    // floor rounds toward -inf; ceil rounds toward +inf; round goes
    // half-away-from-zero.
    [[nodiscard]] constexpr imax trunc() const
    {
      umax q = Numerator / abs_den(Denominator);
      return (Denominator < 0) ? -q : q;
    }

    [[nodiscard]] constexpr imax floor() const
    {
      umax ad = abs_den(Denominator);
      umax q = Numerator / ad;
      umax rem = Numerator % ad;
      // negative with non-zero remainder: step one further toward -inf
      if (Denominator < 0 && rem != 0)
        return -q - 1;
      return (Denominator < 0) ? -q : q;
    }

    [[nodiscard]] constexpr imax ceil() const
    {
      umax ad  = abs_den(Denominator);
      umax q   = Numerator / ad;
      umax rem = Numerator % ad;
      // negative value: ceiling toward +inf coincides with truncation toward zero
      if (Denominator < 0)
        return -q;
      // positive with non-zero remainder: step one further toward +inf
      return q + (rem != 0 ? 1 : 0);
    }

    [[nodiscard]] constexpr imax round() const
    {
      umax ad = abs_den(Denominator);
      umax q = Numerator / ad;
      umax rem = Numerator % ad;
      // half-away-from-zero: bump magnitude when 2*rem >= ad
      if (rem * 2 >= ad) ++q;
      return (Denominator < 0) ? -q : q;
    }

    [[nodiscard]] static constexpr rational make_sentinel() noexcept
    { rational r; r.Numerator = 1; r.Denominator = 0; return r; }

    // Unchecked arithmetic — caller takes responsibility for non-overflow
    // (and non-zero operand for div_unchecked / inv_unchecked).
    static constexpr rational add_unchecked(rational, rational);
    static constexpr rational mul_unchecked(rational, rational);
    static constexpr rational div_unchecked(rational, rational);
    static constexpr rational inv_unchecked(rational);

    static constexpr slim::optional<rational> add(rational a, rational b)
    { return a + b; }
    static constexpr slim::optional<rational> inv(rational);

    // Shared algorithm bodies. Checked=true returns slim::optional<rational>
    // and reports overflow (a compile-time `throw` under is_constant_evaluated
    // or nullopt at runtime). Checked=false silently overflows; the caller
    // must guarantee its absence.
    template <bool Checked> static constexpr auto add_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto mul_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto div_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto inv_impl(rational const&);

  private:
    // Domain check + canonical-zero + gcd reduction; used by the integral ctors.
    // (Private member function; non-static data members above remain public so
    // rational stays a structural type usable as an NTTP.)
    //
    // Two domain errors:
    //  - Denominator == 0           → undefined rational (also reserved as the
    //                                 sentinel slot, so user values can't land
    //                                 there)
    //  - Denominator == imax_min    → cannot be negated without UB; every
    //                                 sign-flip in the file (`operator-`,
    //                                 `abs`, the trim sign-write) assumes
    //                                 `-Denominator` is well-defined
    static constexpr void canonicalize(umax& num, imax& den)
    {
      if (den == 0)
        throw std::system_error(make_error_code(errc::domain_error),
                                "Denominator of Zero is invalid");
      if (den == std::numeric_limits<imax>::min())
        throw std::system_error(make_error_code(errc::domain_error),
                                "Denominator imax_min is invalid (cannot be negated)");
      if (num == 0) den = 1;
      trim(num, den);
    }

    // Computes the signed-encoded denominator for the signed ctor, validating
    // BEFORE the negation so that `-den` is never UB. Throws on den == 0 or
    // den == imax_min — the canonicalize() running in the ctor body would
    // also catch these, but only after the mem-init has already evaluated
    // `-den` and stepped on UB.
    static constexpr imax signed_den_from(std::signed_integral auto num, imax den)
    {
      if (den == 0)
        throw std::system_error(make_error_code(errc::domain_error),
                                "Denominator of Zero is invalid");
      if (den == std::numeric_limits<imax>::min())
        throw std::system_error(make_error_code(errc::domain_error),
                                "Denominator imax_min is invalid (cannot be negated)");
      return (num < 0) ? -den : den;
    }
  };


  [[nodiscard]] constexpr slim::optional<rational> gcd(rational const&, rational const&);
  [[nodiscard]] constexpr rational abs(rational);

  [[nodiscard]] constexpr bool divides_evenly(rational const&, rational const&);

  //---------------------------------------------------------------------------
  // abs
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr rational abs(rational v)
  { if (v.Denominator < 0) v.Denominator = -v.Denominator; return v; }

  //---------------------------------------------------------------------------
  // gcd
  //---------------------------------------------------------------------------
  // Returns nullopt if `lcm(|lhs.Denominator|, |rhs.Denominator|)` would
  // exceed imax_max (the sign bit reservation). Compute lcm as
  // `(a / gcd(a, b)) * b` and trap the multiplication overflow on the
  // `mul_overflow` builtin, then a final range check before the cast to imax.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr slim::optional<rational> gcd(rational const& lhs, rational const& rhs)
  {
    umax a = abs_den(lhs.Denominator);
    umax b = abs_den(rhs.Denominator);
    umax g = std::gcd(a, b);

    umax denominator;
    if (mul_overflow(a / g, b, &denominator))
      return slim::nullopt;
    if (denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      return slim::nullopt;

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
    if (not std::isfinite(value))
      throw std::system_error(make_error_code(errc::domain_error), "non-finite double");

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
  constexpr slim::expected<T, errc> rational::to() const
  {
    if (is_sentinel())   return slim::unexpected{errc::overflow};
    if (Denominator < 0) return slim::unexpected{errc::domain_error};
    return static_cast<T>(Numerator / abs_den(Denominator));
  }

  //---------------------------------------------------------------------------
  // _b / _r literal parser — shared between bound.hpp's `_b` and `_r` below.
  // Accepts:
  //   integer:           5, 1'000
  //   decimal:           1.25, .5
  //   decimal scientific 1.5e2, 2.5e-1
  //   hex integer:       0xff
  //   binary integer:    0b1010
  //   hex float (Q-fmt): 0x1p15, 0x1p-15, 0x1.8p3
  // Exact (no double round-trip). Overflow -> consteval throw.
  //---------------------------------------------------------------------------
  namespace _detail
  {
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
    consteval rational parse_b_literal()
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
          throw ("_b/_r literal: invalid char in exponent");
        }

        if (c == '.')
        {
          if (in_frac) throw ("_b/_r literal: multiple '.'");
          if (base == 2) throw ("_b/_r literal: '.' not allowed in binary");
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
        if (d < 0) throw ("_b/_r literal: invalid digit for radix");

        umax base_u = base;
        if (num > (~umax{0} - d) / base_u)
          throw ("_b/_r literal: numerator overflow");
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
            throw ("_b/_r literal: denominator overflow");
          den *= 10u;
        }
      }
      else if (base == 16)
      {
        int shift = 4 * frac_len;
        if (shift >= 64) throw ("_b/_r literal: hex fraction too long");
        den <<= shift;
      }

      // Apply binary exponent (hex floats, `p`).
      if (has_p_exp)
      {
        if (exp >= 63) throw ("_b/_r literal: p exponent too large");
        if (!exp_neg) num <<= exp;
        else
        {
          if (den > (~umax{0}) >> exp)
            throw ("_b/_r literal: p exponent denominator overflow");
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
              throw ("_b/_r literal: e exponent numerator overflow");
            num *= 10u;
          }
          else
          {
            if (den > (~umax{0}) / 10u)
              throw ("_b/_r literal: e exponent denominator overflow");
            den *= 10u;
          }
        }
      }

      return rational{num, den};
    }
  }

  template<char... Chars>
  constexpr rational operator ""_r() { return _detail::parse_b_literal<Chars...>(); }

  // notch<N, D> is defined publicly in `namespace bnd` (see the re-export block
  // at the end of this header) so consumers spell it without naming the
  // internal representation type.

  //---------------------------------------------------------------------------
  // add_impl / mul_impl / div_impl — shared bodies (Checked toggles overflow)
  //---------------------------------------------------------------------------
  template <bool Checked>
  inline constexpr auto rational::add_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;

    if (a == -b) return ret_t{0_r};
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
          {
            if (std::is_constant_evaluated()) { throw ("rational +: numerator overflow (same denominator)"); }
            return ret_t{slim::nullopt};
          }
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

    // Use lcm(a_ad, b_ad) as the common denominator instead of a_ad * b_ad.
    // Let g = gcd(a_ad, b_ad); the cofactors a_ad/g and b_ad/g are coprime,
    // and lcm = a_ad * (b_ad/g) = (a_ad/g) * b_ad. The cross-multiplications
    // use the reduced cofactors and so overflow far less often than the
    // unreduced product.
    umax g     = std::gcd(a_ad, b_ad);
    umax a_ad_r = a_ad / g;       // = a_ad / gcd; coprime with b_ad_r
    umax b_ad_r = b_ad / g;

    umax denominator;
    umax A;
    umax B;

    if constexpr (Checked)
    {
      if (mul_overflow(a_ad, b_ad_r, &denominator)    ||   // = lcm(a_ad, b_ad)
          mul_overflow(a.Numerator, b_ad_r, &A)       ||
          mul_overflow(b.Numerator, a_ad_r, &B)       ||
          denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      {
        if (std::is_constant_evaluated()) { throw ("rational +: cross-multiplication overflow"); }
        return ret_t{slim::nullopt};
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
        {
          if (std::is_constant_evaluated()) { throw ("rational +: numerator sum overflow"); }
          return ret_t{slim::nullopt};
        }
      }
      else
        numerator = A + B;

      // numerator and denominator are both > 0 here, so the rational(num, den)
      // ctor's domain-error / canonical-zero checks are dead branches; assemble
      // directly and trim, mirroring the equal-denominator path above.
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
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;
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
        {
          if (std::is_constant_evaluated()) { throw ("rational *: numerator overflow"); }
          return ret_t{slim::nullopt};
        }
      }
      else
        numerator = a.Numerator * b.Numerator;

      rational r;
      r.Numerator = numerator;
      r.Denominator = r_neg ? imax{-1} : imax{1};
      return ret_t{r};
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
      {
        if (std::is_constant_evaluated()) { throw ("rational *: numerator or denominator overflow"); }
        return ret_t{slim::nullopt};
      }
    }
    else
    {
      numerator = a.Numerator * b.Numerator;
      denominator = a_ad * b_ad;
    }

    // The cross-trims above guarantee gcd(numerator, denominator) == 1, so
    // bypass rational(num, den) and skip its redundant trim.
    rational r;
    r.Numerator   = numerator;
    r.Denominator = r_neg ? -denominator
                           :  denominator;
    return ret_t{r};
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
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;

    if constexpr (Checked)
    {
      // a.Numerator goes into the result's Denominator slot, so it must fit
      // in imax (otherwise the umax→imax conversion wraps and any later -Denominator
      // is UB).
      if (a.Numerator == 0 ||
          a.Numerator > static_cast<umax>(std::numeric_limits<imax>::max()))
      {
        if (std::is_constant_evaluated()) { throw ("rational inv: numerator zero or out of denominator range"); }
        return ret_t{slim::nullopt};
      }
    }

    rational r;
    r.Numerator   = abs_den(a.Denominator);
    r.Denominator = (a.Denominator < 0) ? -a.Numerator : a.Numerator;
    return ret_t{r};
  }

  // div(a, b) = a * inv(b). The checked path goes through inv_impl<true> so
  // the b.Numerator-fits-in-imax check (added there) propagates here too;
  // the unchecked path skips it (caller's contract).
  template <bool Checked>
  inline constexpr auto rational::div_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;

    if constexpr (Checked)
    {
      auto inv_b = inv_impl<true>(b);
      if (!inv_b.has_value()) return ret_t{slim::nullopt};
      return mul_impl<true>(a, *inv_b);
    }
    else
      return mul_impl<false>(a, inv_impl<false>(b));
  }

  //---------------------------------------------------------------------------
  // unchecked rational arithmetic — caller takes responsibility for:
  //   - no umax overflow on the numerator/denominator products,
  //   - no zero divisor (div_unchecked) or zero numerator (inv_unchecked),
  //   - the resulting Denominator fitting in imax (i.e., the umax-side
  //     denominator does not exceed imax_max). The checked variants enforce
  //     all three; unchecked skips them all.
  //---------------------------------------------------------------------------
  inline constexpr rational rational::add_unchecked(rational a, rational b)
  { return add_impl<false>(a, b); }

  inline constexpr rational rational::mul_unchecked(rational a, rational b)
  { return mul_impl<false>(a, b); }

  inline constexpr rational rational::div_unchecked(rational a, rational b)
  { return div_impl<false>(a, b); }

  inline constexpr rational rational::inv_unchecked(rational a)
  { return inv_impl<false>(a); }

  inline constexpr slim::optional<rational> rational::inv(rational a)
  { return inv_impl<true>(a); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr rational rational::operator-() const
  {
    if (Numerator == 0)
      return *this;

    // Already trimmed; flip the sign-encoding directly without re-running trim.
    rational r;
    r.Numerator = Numerator;
    r.Denominator = -Denominator;
    return r;
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering
  {
    int lhs_sign = lhs.sign();
    int rhs_sign = rhs.sign();

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

    // cross trim to avoid overflow if possible
    trim(lhs.Numerator, rhs.Numerator);
    trim(lhs_ad, rhs_ad);

    umax A;
    umax B;

    if
    (
      mul_overflow(lhs.Numerator, rhs_ad, &A) ||
      mul_overflow(rhs.Numerator, lhs_ad, &B)
    )
    {
      // operator<=> must return std::strong_ordering — there is no optional
      // form that preserves spaceship syntax. We trap (throw at runtime, fail
      // constant evaluation at compile time), symmetric with the checked
      // arithmetic path's compile-time overflow throw.
      if (std::is_constant_evaluated()) { throw ("rational <=>: cross-multiplication overflow"); }
      overflow_trap("multiplicative overflow");
    }

    if (lhs_neg)
      return B <=> A;
    else
      return A <=> B;
  }

  template <typename T>
  inline constexpr auto operator<=>(slim::optional<T> lhs, const rational& rhs)
  { return rational{lhs.value()} <=> rhs; }

  template <typename T>
  inline constexpr auto operator<=>(rational const& lhs, slim::optional<T> rhs)
  { return lhs <=> rational{rhs.value()}; }

  template <arithmetic T>
  inline constexpr auto operator<=>(T lhs, const rational& rhs)
  { return rational{lhs} <=> rhs; }

  template <arithmetic T>
  inline constexpr auto operator<=>(rational const& lhs, T rhs)
  { return lhs <=> rational{rhs}; }

  //---------------------------------------------------------------------------
  // Optional-lifting operators
  //
  // One generic overload per arithmetic operator replaces what used to be five
  // per-shape overloads each. It engages when at least one operand is a
  // slim::optional, both operands unwrap to an arithmetic type, and at least
  // one unwraps to rational. Gating on `arithmetic` (rather than naming
  // `boundable`, which isn't visible this low in the include graph) excludes
  // bound operands — `boundable` is defined as `!arithmetic` — so bound-involving
  // optional expressions fall to the generic operator in arithmetic.hpp instead,
  // and the two generics partition the space with no ambiguity. The lambda
  // re-enters operator resolution on the unwrapped operands, inheriting the
  // scalar/core overloads below.
  //---------------------------------------------------------------------------
  template <class L, class R>
  concept rational_lift_operands =
       (is_slim_optional_v<L> || is_slim_optional_v<R>)
    && arithmetic<unwrap_t<L>> && arithmetic<unwrap_t<R>>
    && (std::same_as<unwrap_t<L>, rational> || std::same_as<unwrap_t<R>, rational>);

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator*(rational const& lhs, rational const& rhs)
  { return rational::mul_impl<true>(lhs, rhs); }

  // arithmetic operand — direct construction, no lift overhead
  template <arithmetic T>
  inline constexpr auto operator*(T lhs, rational const& rhs)
  { return rational{lhs} * rhs; }

  template <arithmetic T>
  inline constexpr auto operator*(rational const& lhs, T rhs)
  { return lhs * rational{rhs}; }

  // optional operand(s) — propagate via lift
  template <class L, class R> requires rational_lift_operands<L, R>
  inline constexpr auto operator*(L const& lhs, R const& rhs)
  { return lift([](auto const& a, auto const& b){ return a * b; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator/(rational const& lhs, rational const& rhs)
  { return rational::div_impl<true>(lhs, rhs); }

  template <arithmetic T>
  inline constexpr auto operator/(T lhs, rational const& rhs)
  { return rational{lhs} / rhs; }

  template <arithmetic T>
  inline constexpr auto operator/(rational const& lhs, T rhs)
  { return lhs / rational{rhs}; }

  template <class L, class R> requires rational_lift_operands<L, R>
  inline constexpr auto operator/(L const& lhs, R const& rhs)
  { return lift([](auto const& a, auto const& b){ return a / b; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator+(const rational& lhs, const rational& rhs)
  { return rational::add_impl<true>(lhs, rhs); }

  template <arithmetic T>
  inline constexpr auto operator+(T lhs, rational const& rhs)
  { return rational{lhs} + rhs; }

  template <arithmetic T>
  inline constexpr auto operator+(rational const& lhs, T rhs)
  { return lhs + rational{rhs}; }

  template <class L, class R> requires rational_lift_operands<L, R>
  inline constexpr auto operator+(L const& lhs, R const& rhs)
  { return lift([](auto const& a, auto const& b){ return a + b; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator-(const rational& lhs, const rational& rhs)
  { return operator+(lhs, -rhs); }

  template <arithmetic T>
  inline constexpr auto operator-(T lhs, rational const& rhs)
  { return rational{lhs} - rhs; }

  template <arithmetic T>
  inline constexpr auto operator-(rational const& lhs, T rhs)
  { return lhs - rational{rhs}; }

  template <class L, class R> requires rational_lift_operands<L, R>
  inline constexpr auto operator-(L const& lhs, R const& rhs)
  { return lift([](auto const& a, auto const& b){ return a - b; }, lhs, rhs); }

  inline constexpr slim::optional<rational> operator-(slim::optional<rational> const& v)
  { return lift([](rational r){ return -r; }, v); }

  //---------------------------------------------------------------------------
  // Compound-assignment definitions — unwrap the checked binary op result.
  // .value() throws slim::bad_optional_access on overflow; callers that
  // need a non-throwing path must use the binary operators directly.
  //---------------------------------------------------------------------------
  inline constexpr rational& rational::operator+=(rational const& rhs)
  { *this = (*this + rhs).value(); return *this; }

  inline constexpr rational& rational::operator-=(rational const& rhs)
  { *this = (*this - rhs).value(); return *this; }

  inline constexpr rational& rational::operator*=(rational const& rhs)
  { *this = (*this * rhs).value(); return *this; }

  inline constexpr rational& rational::operator/=(rational const& rhs)
  { *this = (*this / rhs).value(); return *this; }

  // Forwarding overloads — accept arithmetic RHS (lifted via rational{}) and
  // slim::optional<rational> RHS (unwrapped via .value()) so callers can
  // chain `r += rational * rational` without a manual unwrap.
  template <arithmetic T>
  inline constexpr rational& operator+=(rational& lhs, T rhs)
  { return lhs += rational{rhs}; }

  template <arithmetic T>
  inline constexpr rational& operator-=(rational& lhs, T rhs)
  { return lhs -= rational{rhs}; }

  template <arithmetic T>
  inline constexpr rational& operator*=(rational& lhs, T rhs)
  { return lhs *= rational{rhs}; }

  template <arithmetic T>
  inline constexpr rational& operator/=(rational& lhs, T rhs)
  { return lhs /= rational{rhs}; }

  inline constexpr rational& operator+=(rational& lhs, slim::optional<rational> const& rhs)
  { return lhs += rhs.value(); }

  inline constexpr rational& operator-=(rational& lhs, slim::optional<rational> const& rhs)
  { return lhs -= rhs.value(); }

  inline constexpr rational& operator*=(rational& lhs, slim::optional<rational> const& rhs)
  { return lhs *= rhs.value(); }

  inline constexpr rational& operator/=(rational& lhs, slim::optional<rational> const& rhs)
  { return lhs /= rhs.value(); }

  //---------------------------------------------------------------------------
  // divides_evenly
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr bool divides_evenly(rational const& dividend, rational const& divisor)
  {
    if (divisor == 0) return true;            // convention: everything divides 0 evenly
    auto q = dividend / divisor;
    return q.has_value() && abs_den(q->Denominator) == 1;
  }

} // namespace bnd::detail

namespace bnd
{
  // `rational` itself is an internal representation type and is not part of the
  // public surface. The grid-building `notch<N,D>` / `frac<N,D>` literals stay
  // public — they never expose the type. The integer/rational helpers
  // (`abs_den`, `trim`, `abs`, `gcd`, `divides_evenly`) stay in `bnd::detail`;
  // the library's own headers spell them `detail::…`.
  template <umax N, imax D = 1>
  inline constexpr detail::rational notch = detail::rational{N, D};

  // frac<N, D> — exact fractional grid value (signed numerator), the companion
  // to notch<N,D> for interval endpoints that are not dyadic and so cannot be
  // written exactly as a floating literal (e.g. `frac<-6, 5>` for -1.2). Keeps
  // exact grids spellable now that the `_r` literal is internal.
  template <imax N, imax D = 1>
  inline constexpr detail::rational frac = detail::rational{N, D};
} // namespace bnd

namespace slim
{
  constexpr bnd::detail::rational sentinel_traits<bnd::detail::rational>::sentinel() noexcept { return bnd::detail::rational::make_sentinel(); }
  constexpr bool sentinel_traits<bnd::detail::rational>::is_sentinel(const bnd::detail::rational& v) noexcept
  { return v.is_sentinel(); }
} // namespace slim

#endif // BNDrationalHPP

