//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrationalHPP
#define BNDrationalHPP

#include "bound/generic.hpp"
#include "bound/math.hpp"
#include "bound/overflow.hpp"

#include <numeric>
#include <compare>
#include <cmath>
#include <tuple>

namespace bnd { struct rational; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::rational>
  {
    protected:
      static constexpr bnd::rational sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::rational& v) noexcept;
  };
} // namespace slim

namespace bnd
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
    denominator = (denominator < 0) ? -static_cast<imax>(ad) : static_cast<imax>(ad);
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

  constexpr slim::optional<rational> operator+(const rational&, const rational&);
  constexpr slim::optional<rational> operator/(const rational&, const rational&);
  constexpr slim::optional<rational> operator-(const rational&, const rational&);

  constexpr slim::optional<rational> operator*(const rational&, const rational&);
  constexpr auto     operator<=>(rational, rational) -> std::strong_ordering;

  //---------------------------------------------------------------------------
  // rational_overflow — fails constant evaluation with the operator name
  //---------------------------------------------------------------------------
  [[noreturn]] inline consteval void rational_overflow(char const* op)
  { throw op; }

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
    {
      if (Denominator == 0)
        throw std::system_error(make_error_code(errc::domain_error), "Denominator of Zero is invalid");
      if (Numerator == 0)
        Denominator = 1;
      trim(Numerator, Denominator);
    }

    // operator== be default for structural type
    constexpr bool operator==(const rational&) const = default;
    template <arithmetic T>
    constexpr bool operator==(T value) const { return operator==(rational{value}); }

    constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr slim::optional<T> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T () const
    {
      if (Denominator < 0)
        throw std::system_error(make_error_code(errc::domain_error), "cannot convert negative rational to unsigned");
      return static_cast<T>(Numerator/static_cast<umax>(Denominator));
    }

    template <std::signed_integral T>
    explicit constexpr operator T () const
    {
      if (Denominator < 0)
        return -static_cast<T>(Numerator / static_cast<umax>(-Denominator));
      return static_cast<T>(Numerator / static_cast<umax>(Denominator));
    }

    template <std::floating_point T>
    explicit constexpr operator T () const
    {
      if (Denominator < 0)
        return -static_cast<T>(Numerator) / static_cast<T>(static_cast<umax>(-Denominator));
      return static_cast<T>(Numerator) / static_cast<T>(static_cast<umax>(Denominator));
    }

    // allow unary+ for generic programming
    constexpr rational operator+() const { return *this; }

    // Named integer reductions — explicit, lossy, intent-clear alternatives
    // to `static_cast<imax>(r)`. trunc rounds toward zero (matches operator T);
    // floor rounds toward -inf; round goes half-away-from-zero.
    [[nodiscard]] constexpr imax trunc() const
    {
      umax q = Numerator / static_cast<umax>(abs_den(Denominator));
      return (Denominator < 0) ? -static_cast<imax>(q) : static_cast<imax>(q);
    }

    [[nodiscard]] constexpr imax floor() const
    {
      umax ad = static_cast<umax>(abs_den(Denominator));
      umax q = Numerator / ad;
      umax rem = Numerator % ad;
      // negative with non-zero remainder: step one further toward -inf
      if (Denominator < 0 && rem != 0)
        return -static_cast<imax>(q) - 1;
      return (Denominator < 0) ? -static_cast<imax>(q) : static_cast<imax>(q);
    }

    [[nodiscard]] constexpr imax round() const
    {
      umax ad = static_cast<umax>(abs_den(Denominator));
      umax q = Numerator / ad;
      umax rem = Numerator % ad;
      // half-away-from-zero: bump magnitude when 2*rem >= ad
      if (rem * 2 >= ad) ++q;
      return (Denominator < 0) ? -static_cast<imax>(q) : static_cast<imax>(q);
    }

    static constexpr rational make_sentinel() noexcept
    { rational r; r.Numerator = 1; r.Denominator = 0; return r; }

    // Unchecked arithmetic — caller takes responsibility for non-overflow
    // (and non-zero divisor for div_unchecked). No optional, no failure path.
    static constexpr rational add_unchecked(rational, rational);
    static constexpr rational mul_unchecked(rational, rational);
    static constexpr rational div_unchecked(rational, rational);

    static constexpr slim::optional<rational> add(rational a, rational b)
    { return a + b; }

    // Shared algorithm bodies. Checked=true returns slim::optional<rational>
    // and reports overflow (via consteval rational_overflow at compile-time
    // or nullopt at runtime). Checked=false silently overflows; the caller
    // must guarantee its absence.
    template <bool Checked> static constexpr auto add_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto mul_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto div_impl(rational const&, rational const&);
  };


  constexpr rational gcd(const rational&, const rational&);
  constexpr rational abs(rational);

  constexpr bool divides_evenly(const rational&, const rational&);

  //---------------------------------------------------------------------------
  // abs
  //---------------------------------------------------------------------------
  constexpr rational abs(rational value)
  { return (value.Denominator < 0) ? -value : value; }

  //---------------------------------------------------------------------------
  // gcd
  //---------------------------------------------------------------------------
  constexpr rational gcd(const rational& lhs, const rational& rhs)
  {
    auto numerator   = std::gcd(lhs.Numerator, rhs.Numerator);
    auto denominator = std::lcm(abs_den(lhs.Denominator), abs_den(rhs.Denominator));
    return rational{numerator, static_cast<imax>(denominator)};
  }

  //---------------------------------------------------------------------------
  // rational::rational
  //---------------------------------------------------------------------------
  constexpr rational::rational(std::signed_integral auto num, imax den)
   :Numerator{safe_abs(num)},
    Denominator{ (num < 0) ? -den : den }
  {
    if (den == 0)
      throw std::system_error(make_error_code(errc::domain_error), "Denominator of Zero is invalid");
    if (Numerator == 0)
      Denominator = 1;
    trim(Numerator, Denominator);
  }

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
    Denominator = neg ? -static_cast<imax>(den) : static_cast<imax>(den);
    // trim not needed, because abs_fraction already trims in its special case
  }

  //---------------------------------------------------------------------------
  // to
  //---------------------------------------------------------------------------
  template <std::unsigned_integral T>
  constexpr slim::optional<T> rational::to() const
  { return (Denominator <= 0) ? slim::nullopt : slim::make_optional<T>(static_cast<T>(Numerator/static_cast<umax>(Denominator))); }

  //---------------------------------------------------------------------------
  // user defined literals for rational
  //---------------------------------------------------------------------------
  constexpr rational operator ""_r(unsigned long long int numerator)
  { return rational{numerator}; }

  constexpr rational operator ""_r(long double value)
  { return rational{static_cast<double>(value)}; }

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
            if consteval { rational_overflow("rational +: numerator overflow (same denominator)"); }
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
      r.Denominator = r_neg ? -static_cast<imax>(a_ad) : static_cast<imax>(a_ad);
      trim(r.Numerator, r.Denominator);
      return ret_t{r};
    }

    umax denominator;
    umax A;
    umax B;

    if constexpr (Checked)
    {
      if (mul_overflow(a_ad, b_ad, &denominator) ||
          mul_overflow(a.Numerator, b_ad, &A)    ||
          mul_overflow(b.Numerator, a_ad, &B))
      {
        if consteval { rational_overflow("rational +: cross-multiplication overflow"); }
        return ret_t{slim::nullopt};
      }
    }
    else
    {
      denominator = a_ad * b_ad;
      A = a.Numerator * b_ad;
      B = b.Numerator * a_ad;
    }

    if (a_neg == b_neg)
    {
      umax numerator;
      if constexpr (Checked)
      {
        if (add_overflow(A, B, &numerator))
        {
          if consteval { rational_overflow("rational +: numerator sum overflow"); }
          return ret_t{slim::nullopt};
        }
      }
      else
        numerator = A + B;

      imax signed_den = a_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator);
      return ret_t{rational{numerator, signed_den}};
    }

    umax numerator = (A > B) ? (A - B) : (B - A);
    bool r_neg = a_neg ? (A > B) : (B > A);
    imax signed_den = r_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator);
    return ret_t{rational{numerator, signed_den}};
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
          if consteval { rational_overflow("rational *: numerator overflow"); }
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
          mul_overflow(a_ad, b_ad, &denominator))
      {
        if consteval { rational_overflow("rational *: numerator or denominator overflow"); }
        return ret_t{slim::nullopt};
      }
    }
    else
    {
      numerator = a.Numerator * b.Numerator;
      denominator = a_ad * b_ad;
    }

    imax signed_den = r_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator);
    return ret_t{rational{numerator, signed_den}};
  }

  template <bool Checked>
  inline constexpr auto rational::div_impl(rational const& a_in, rational const& b_in)
  {
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;
    rational a = a_in, b = b_in;

    if constexpr (Checked)
      if (b.Numerator == 0) return ret_t{slim::nullopt};

    if (a.Numerator == 0) return ret_t{0_r};

    bool r_neg = (a.Denominator < 0) != (b.Denominator < 0);
    umax a_ad = abs_den(a.Denominator);
    umax b_ad = abs_den(b.Denominator);

    if (a_ad == 1 && b_ad == 1)
    {
      trim(a.Numerator, b.Numerator);
      rational r;
      r.Numerator = a.Numerator;
      r.Denominator = r_neg ? -static_cast<imax>(b.Numerator) : static_cast<imax>(b.Numerator);
      return ret_t{r};
    }

    trim(a.Numerator, b.Numerator);
    trim(a_ad, b_ad);

    umax numerator;
    umax denominator;
    if constexpr (Checked)
    {
      if (mul_overflow(a.Numerator, b_ad, &numerator) ||
          mul_overflow(b.Numerator, a_ad, &denominator))
      {
        if consteval { rational_overflow("rational /: cross-multiplication overflow"); }
        return ret_t{slim::nullopt};
      }
    }
    else
    {
      numerator = a.Numerator * b_ad;
      denominator = b.Numerator * a_ad;
    }

    imax signed_den = r_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator);
    return ret_t{rational{numerator, signed_den}};
  }

  //---------------------------------------------------------------------------
  // unchecked rational arithmetic — caller takes responsibility
  //---------------------------------------------------------------------------
  inline constexpr rational rational::add_unchecked(rational a, rational b)
  { return add_impl<false>(a, b); }

  inline constexpr rational rational::mul_unchecked(rational a, rational b)
  { return mul_impl<false>(a, b); }

  inline constexpr rational rational::div_unchecked(rational a, rational b)
  { return div_impl<false>(a, b); }

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
    bool lhs_neg = lhs.Denominator < 0;
    bool rhs_neg = rhs.Denominator < 0;
    int lhs_sign = (lhs.Numerator == 0) ? 0 : (lhs_neg ? -1 : 1);
    int rhs_sign = (rhs.Numerator == 0) ? 0 : (rhs_neg ? -1 : 1);

    if (lhs_sign != rhs_sign)
      return lhs_sign <=> rhs_sign;

    if (lhs_sign == 0)
      return std::strong_ordering::equal;

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
      if consteval { rational_overflow("rational <=>: cross-multiplication overflow"); }
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

  // optional<T> operand — propagate via lift
  template <typename T>
  inline constexpr auto operator*(slim::optional<T> const& lhs, rational const& rhs)
  { return lift([](rational a, rational b){ return a * b; }, lhs, rhs); }

  template <typename T>
  inline constexpr auto operator*(rational const& lhs, slim::optional<T> const& rhs)
  { return lift([](rational a, rational b){ return a * b; }, lhs, rhs); }

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

  template <typename T>
  inline constexpr auto operator/(slim::optional<T> const& lhs, rational const& rhs)
  { return lift([](rational a, rational b){ return a / b; }, lhs, rhs); }

  template <typename T>
  inline constexpr auto operator/(rational const& lhs, slim::optional<T> const& rhs)
  { return lift([](rational a, rational b){ return a / b; }, lhs, rhs); }

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

  template <typename T>
  inline constexpr auto operator+(slim::optional<T> const& lhs, rational const& rhs)
  { return lift([](rational a, rational b){ return a + b; }, lhs, rhs); }

  template <typename T>
  inline constexpr auto operator+(rational const& lhs, slim::optional<T> const& rhs)
  { return lift([](rational a, rational b){ return a + b; }, lhs, rhs); }

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

  template <typename T>
  inline constexpr auto operator-(slim::optional<T> const& lhs, rational const& rhs)
  { return lift([](rational a, rational b){ return a - b; }, lhs, rhs); }

  template <typename T>
  inline constexpr auto operator-(rational const& lhs, slim::optional<T> const& rhs)
  { return lift([](rational a, rational b){ return a - b; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // divides_evenly
  //---------------------------------------------------------------------------
  inline constexpr bool divides_evenly(const rational& dividend, const rational& divisor)
  {
    return (divisor == 0_r) ? true : abs_den((dividend / divisor).value().Denominator) == 1;
  }

} // namespace bnd

namespace slim
{
  constexpr bnd::rational sentinel_traits<bnd::rational>::sentinel() noexcept { return bnd::rational::make_sentinel(); }
  constexpr bool sentinel_traits<bnd::rational>::is_sentinel(const bnd::rational& v) noexcept
  { return v.Denominator == 0; }
} // namespace slim

#endif // BNDrationalHPP

