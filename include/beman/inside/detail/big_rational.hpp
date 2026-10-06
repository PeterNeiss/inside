// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_BIG_RATIONAL_HPP
#define BEMAN_INSIDE_DETAIL_BIG_RATIONAL_HPP

#include <beman/inside/detail/rational.hpp>
#include <beman/inside/detail/wide_int.hpp>

#include <compare>
#include <cstddef>
#include <limits>
#include <vector>

//---------------------------------------------------------------------------
// big_int / big_rational — grid numbers of any size (C++26 static reflection).
//
// A grid is a template argument, so its numbers must be structural values.
// A value of at most one limb is held inline (Small); a larger magnitude is
// interned in static storage with std::define_static_array and held by
// pointer. Equal arrays intern to the same object, and every value has one
// canonical form (reduced fraction, no leading zero limbs, inline when it
// fits), so equal values are the same template argument.
//
// Interning is consteval: a value past 64 bits exists only at compile time.
// Small values compute without allocation and work at runtime as well (for
// grid::try_make); a runtime operation whose result would need more than one
// limb reports errc::overflow.
//
// Arithmetic on large values runs on transient std::vector<umax> magnitudes
// with the wide_int limb kernels, during constant evaluation only.
//---------------------------------------------------------------------------
// Included by grid_rational.hpp, which detects BEMAN_INSIDE_BIG_GRIDS.
#if defined(BEMAN_INSIDE_BIG_GRIDS) && BEMAN_INSIDE_BIG_GRIDS
#include <meta>

namespace beman::inside::detail
{
  namespace big
  {
    using mag = std::vector<umax>;                    // little-endian, no top zeros

    constexpr void trim(mag& m) { while (!m.empty() && m.back() == 0) m.pop_back(); }

    constexpr std::strong_ordering compare(mag const& a, mag const& b)
    {
      if (a.size() != b.size()) return a.size() <=> b.size();
      for (std::size_t i = a.size(); i-- > 0;)
        if (a[i] != b[i]) return a[i] <=> b[i];
      return std::strong_ordering::equal;
    }

    constexpr mag add(mag const& a, mag const& b)
    {
      mag r(a.size() > b.size() ? a.size() + 1 : b.size() + 1, 0);
      umax c = 0;
      for (std::size_t i = 0; i + 1 < r.size(); ++i)
        r[i] = limb::add_carry(i < a.size() ? a[i] : umax{0}, i < b.size() ? b[i] : umax{0}, c);
      r.back() = c;
      trim(r);
      return r;
    }

    // a − b for a ≥ b.
    constexpr mag sub(mag const& a, mag const& b)
    {
      mag r(a.size(), 0);
      umax br = 0;
      for (std::size_t i = 0; i < a.size(); ++i)
        r[i] = limb::sub_borrow(a[i], i < b.size() ? b[i] : umax{0}, br);
      trim(r);
      return r;
    }

    constexpr mag mul(mag const& a, mag const& b)
    {
      if (a.empty() || b.empty()) return {};
      mag r(a.size() + b.size(), 0);
      for (std::size_t i = 0; i < a.size(); ++i)
      {
        umax carry = 0;
        for (std::size_t j = 0; j < b.size(); ++j)
        {
          const limb::pair<umax> p = limb::mul(a[i], b[j]);
          umax c1 = 0, c2 = 0;
          umax s = limb::add_carry(r[i + j], p.Lo, c1);
          s = limb::add_carry(s, carry, c2);
          r[i + j] = s;
          carry = p.Hi + c1 + c2;
        }
        r[i + b.size()] = carry;
      }
      trim(r);
      return r;
    }

    // {a / b, a % b} for b ≠ 0.
    struct divmod_result { mag Quotient; mag Remainder; };
    constexpr divmod_result divmod(mag const& a, mag const& b)
    {
      if (compare(a, b) < 0) return {{}, a};
      mag q(a.size() - b.size() + 1, 0), r(b.size(), 0), un(a.size() + 1, 0), vn(b.size(), 0);
      limb::divmod(a.data(), a.size(), b.data(), b.size(), q.data(), r.data(), un.data(), vn.data());
      trim(q);
      trim(r);
      return {q, r};
    }

    constexpr mag gcd(mag a, mag b)
    {
      while (!b.empty()) { mag t = divmod(a, b).Remainder; a = std::move(b); b = std::move(t); }
      return a;
    }

    constexpr int bit_width(mag const& m)
    {
      return m.empty() ? 0
           : static_cast<int>(m.size() - 1) * 64 + (64 - std::countl_zero(m.back()));
    }
  } // namespace big

  //---------------------------------------------------------------------------
  // big_int — a signed integer of any size; structural.
  //---------------------------------------------------------------------------
  struct big_int
  {
    umax        Small = 0;          // the magnitude when Size == 0
    const umax* Limbs = nullptr;    // interned magnitude, Size ≥ 2 limbs
    std::size_t Size = 0;           // tested instead of Limbs: under -fno-delete-null-pointer-checks
                                    // (implied by -fsanitize=null) GCC cannot compare an interned
                                    // pointer with nullptr in a constant expression
    bool        Negative = false;   // never set for zero

    constexpr big_int() = default;

    template <std::integral T>
    constexpr big_int(T v) noexcept
      : Small{std::is_signed_v<T> && v < 0 ? umax{0} - static_cast<umax>(v) : static_cast<umax>(v)},
        Negative{std::is_signed_v<T> && v < 0} {}

    // From a wide_int (sign-extending a signed one).
    template <std::size_t N, bool S, std::unsigned_integral L>
    constexpr big_int(wide_int<N, S, L> const& w)
    {
      const bool neg = w.negative();
      const wide_int<N, false, L> m(neg ? -w : w);
      big::mag v;
      for (std::size_t i = 0; i < N; ++i)
      {
        if constexpr (std::numeric_limits<L>::digits == 64) v.push_back(m.Word[i]);
        else static_assert(std::numeric_limits<L>::digits == 64, "big_int: umax limbs only");
      }
      *this = from_mag(std::move(v), neg);
    }

    // By value, not by pointer (see Size): equal magnitudes intern to the
    // same array, so this matches the defaulted comparison.
    friend constexpr bool operator==(big_int const& a, big_int const& b) noexcept
    {
      if (a.Size != b.Size || a.Small != b.Small || a.Negative != b.Negative) return false;
      for (std::size_t i = 0; i < a.Size; ++i)
        if (a.Limbs[i] != b.Limbs[i]) return false;
      return true;
    }

    [[nodiscard]] constexpr bool is_zero() const noexcept { return Size == 0 && Small == 0; }
    [[nodiscard]] constexpr bool negative() const noexcept { return Negative; }
    [[nodiscard]] constexpr bool fits_limb() const noexcept { return Size == 0; }

    [[nodiscard]] constexpr big::mag magnitude() const
    {
      if (Size != 0) return big::mag(Limbs, Limbs + Size);
      if (Small) return big::mag{Small};
      return {};
    }

    // The canonical big_int of a magnitude and sign: inline when it fits one
    // limb, else interned (constant evaluation only).
    static constexpr big_int from_mag(big::mag m, bool neg)
    {
      big::trim(m);
      big_int r;
      if (m.size() <= 1)
      {
        r.Small = m.empty() ? umax{0} : m[0];
        r.Negative = neg && r.Small != 0;
        return r;
      }
      if consteval
      {
        const auto s = std::define_static_array(m);
        r.Limbs = s.data();
        r.Size = s.size();
        r.Negative = neg;
        return r;
      }
      else
      {
        raise(errc::overflow, "big_int: a value past 64 bits exists only at compile time");
      }
    }

    [[nodiscard]] constexpr int bit_width() const
    { return Size != 0 ? big::bit_width(magnitude()) : (Small ? 64 - std::countl_zero(Small) : 0); }

    // Truncating conversion to a builtin integer or wide_int (two's complement).
    template <typename T>
      requires (std::integral<T> || is_wide_int_v<T>)
    constexpr explicit operator T() const
    {
      if constexpr (std::integral<T>)
      {
        const umax low = Size != 0 ? Limbs[0] : Small;
        return static_cast<T>(Negative ? umax{0} - low : low);
      }
      else
      {
        T w{0};
        for (std::size_t i = 0; i < sizeof(w.Word) / sizeof(w.Word[0]); ++i)
          w.Word[i] = Size != 0 ? (i < Size ? Limbs[i] : 0) : (i == 0 ? Small : 0);
        return Negative ? -w : w;
      }
    }

    friend constexpr std::strong_ordering operator<=>(big_int const& a, big_int const& b)
    {
      if (a.Negative != b.Negative) return a.Negative ? std::strong_ordering::less : std::strong_ordering::greater;
      const std::strong_ordering m = (a.Size != 0 || b.Size != 0) ? big::compare(a.magnitude(), b.magnitude())
                                                          : a.Small <=> b.Small;
      return a.Negative ? 0 <=> m : m;
    }

    friend constexpr big_int operator-(big_int a) { a.Negative = !a.Negative && !a.is_zero(); return a; }

    friend constexpr big_int operator+(big_int const& a, big_int const& b)
    {
      if (a.Size == 0 && b.Size == 0)
      {
        if (a.Negative == b.Negative)
        {
          umax c = 0;
          const umax s = limb::add_carry(a.Small, b.Small, c);
          if (c == 0) { big_int r; r.Small = s; r.Negative = a.Negative && s != 0; return r; }
        }
        else
        {
          const bool a_big = a.Small >= b.Small;
          big_int r;
          r.Small = a_big ? a.Small - b.Small : b.Small - a.Small;
          r.Negative = (a_big ? a.Negative : b.Negative) && r.Small != 0;
          return r;
        }
      }
      const big::mag ma = a.magnitude(), mb = b.magnitude();
      if (a.Negative == b.Negative) return from_mag(big::add(ma, mb), a.Negative);
      return big::compare(ma, mb) >= 0 ? from_mag(big::sub(ma, mb), a.Negative)
                                       : from_mag(big::sub(mb, ma), b.Negative);
    }
    friend constexpr big_int operator-(big_int const& a, big_int const& b) { return a + (-b); }

    friend constexpr big_int operator*(big_int const& a, big_int const& b)
    {
      if (a.Size == 0 && b.Size == 0)
      {
        const limb::pair<umax> p = limb::mul(a.Small, b.Small);
        if (p.Hi == 0) { big_int r; r.Small = p.Lo; r.Negative = (a.Negative != b.Negative) && p.Lo != 0; return r; }
      }
      return from_mag(big::mul(a.magnitude(), b.magnitude()), a.Negative != b.Negative);
    }

    // Truncating division, as the builtin operators. Pre: b ≠ 0.
    struct divmod_result;
    static constexpr divmod_result divmod(big_int const& a, big_int const& b);
    friend constexpr big_int operator/(big_int const& a, big_int const& b);
    friend constexpr big_int operator%(big_int const& a, big_int const& b);


    // a · 2^k (k ≥ 0).
    friend constexpr big_int operator<<(big_int const& a, int k)
    {
      if (a.Size == 0 && k < 64 && (k == 0 || (a.Small >> (64 - k)) == 0))
      { big_int r = a; r.Small = a.Small << k; return r; }
      big::mag m = a.magnitude();
      const std::size_t words = static_cast<std::size_t>(k / 64);
      const int bits = k % 64;
      big::mag r(m.size() + words + 1, 0);
      for (std::size_t i = 0; i < m.size(); ++i)
      {
        r[i + words] |= m[i] << bits;
        if (bits != 0) r[i + words + 1] |= m[i] >> (64 - bits);
      }
      return from_mag(std::move(r), a.Negative);
    }
  };

  struct big_int::divmod_result { big_int Quotient; big_int Remainder; };

  // (Free functions, not hidden friends: callers spell detail::abs / gcd.)
  constexpr big_int abs(big_int a) { a.Negative = false; return a; }

  constexpr auto big_int::divmod(big_int const& a, big_int const& b) -> divmod_result
  {
    if (b.is_zero())
    {
      if consteval { constexpr_error<"big_int: division by zero">(); }
      raise(errc::division_by_zero, "big_int: division by zero");
    }
    if (a.Size == 0 && b.Size == 0)
    {
      big_int q, r;
      q.Small = a.Small / b.Small; q.Negative = (a.Negative != b.Negative) && q.Small != 0;
      r.Small = a.Small % b.Small; r.Negative = a.Negative && r.Small != 0;
      return {q, r};
    }
    big::divmod_result m = big::divmod(a.magnitude(), b.magnitude());
    return {from_mag(std::move(m.Quotient), a.Negative != b.Negative), from_mag(std::move(m.Remainder), a.Negative)};
  }
  constexpr big_int operator/(big_int const& a, big_int const& b) { return big_int::divmod(a, b).Quotient; }
  constexpr big_int operator%(big_int const& a, big_int const& b) { return big_int::divmod(a, b).Remainder; }

  constexpr big_int gcd(big_int a, big_int b)
  {
    a = abs(a); b = abs(b);
    while (!b.is_zero()) { const big_int t = a % b; a = b; b = t; }
    return a;
  }

  constexpr int bit_width_of(big_int const& v) { return v.bit_width(); }

  //---------------------------------------------------------------------------
  // big_rational — an exact fraction of any size; structural and canonical
  // (reduced, positive denominator).
  //---------------------------------------------------------------------------
  struct big_rational
  {
    big_int Num{};
    big_int Den{1};

    constexpr big_rational() = default;
    template <std::integral T>
    constexpr big_rational(T v) noexcept : Num{v} {}
    constexpr big_rational(big_int n, big_int d = big_int{1}) : Num{n}, Den{d} { normalize(); }
    constexpr big_rational(rational const& r)
      : Num{r.Numerator}, Den{abs_den(r.Denominator)}
    { if (r.Denominator < 0) Num = -Num; }
    constexpr big_rational(std::floating_point auto d)                 // exact binary value
    {
      if (d != d || d - d != 0)
      {
        if consteval { constexpr_error<"big_rational: not a finite number">(); }
        raise(errc::not_finite, "big_rational: not a finite number");
      }
      int e = 0;
      double m = frexp(static_cast<double>(d), &e);                     // d = m·2^e, |m| in [0.5, 1)
      const bool neg = m < 0;
      if (neg) m = -m;
      const umax mant = static_cast<umax>(ldexp(m, 53));               // exact: 53 bits
      e -= 53;
      big_int n{mant}, den{1};
      for (; e > 0; --e) n = n * big_int{2};
      for (; e < 0; ++e) den = den * big_int{2};
      Num = neg ? -n : n;
      Den = den;
      normalize();
    }

    friend constexpr bool operator==(big_rational const&, big_rational const&) = default;

    constexpr void normalize()
    {
      if (Den.is_zero())
      {
        if consteval { constexpr_error<"big_rational: zero denominator">(); }
        raise(errc::division_by_zero, "big_rational: zero denominator");
      }
      if (Den.negative()) { Num = -Num; Den = -Den; }
      if (Num.is_zero()) { Den = big_int{1}; return; }
      const big_int g = gcd(Num, Den);
      if (!(g == big_int{1})) { Num = Num / g; Den = Den / g; }
    }

    // The 64-bit rational, when the value fits it.
    [[nodiscard]] constexpr bool fits_rational() const noexcept
    { return Num.fits_limb() && Den.fits_limb() && Den.Small <= static_cast<umax>(std::numeric_limits<imax>::max()); }

    // Implicit: lets every 64-bit path keep working on a small grid. A value
    // past 64 bits fails the build when such a path meets it.
    constexpr operator rational() const
    {
      if (!fits_rational())
      {
        if consteval { constexpr_error<"grid number past 64 bits on a path that needs a 64-bit rational">(); }
        raise(errc::overflow, "big_rational: value past 64 bits");
      }
      const imax d = static_cast<imax>(Den.Small);
      return rational{Num.Small, Num.negative() ? -d : d};
    }

    friend constexpr std::strong_ordering operator<=>(big_rational const& a, big_rational const& b)
    {
      // One-limb parts compare by a 128-bit cross product, forming no big
      // value: runtime comparisons (a store's range check) stay allocation-
      // and error-free.
      if (a.Num.fits_limb() && a.Den.fits_limb() && b.Num.fits_limb() && b.Den.fits_limb())
      {
        if (a.Num.negative() != b.Num.negative())
          return a.Num.negative() ? std::strong_ordering::less : std::strong_ordering::greater;
        const std::strong_ordering m = limb::mul_compare(a.Num.Small, b.Den.Small, b.Num.Small, a.Den.Small);
        return a.Num.negative() ? 0 <=> m : m;
      }
      return a.Num * b.Den <=> b.Num * a.Den;
    }

    friend constexpr big_rational operator-(big_rational a) { a.Num = -a.Num; return a; }
    friend constexpr big_rational operator+(big_rational const& a, big_rational const& b)
    { return {a.Num * b.Den + b.Num * a.Den, a.Den * b.Den}; }
    friend constexpr big_rational operator-(big_rational const& a, big_rational const& b) { return a + (-b); }
    friend constexpr big_rational operator*(big_rational const& a, big_rational const& b)
    { return {a.Num * b.Num, a.Den * b.Den}; }
    // Pre: b ≠ 0.
    friend constexpr big_rational operator/(big_rational const& a, big_rational const& b)
    { return {a.Num * b.Den, a.Den * b.Num}; }


    [[nodiscard]] constexpr bool is_integer() const noexcept { return Den == big_int{1}; }

    // With an integer: exact, and unambiguous (an int converts to both).
    template <std::integral T>
    friend constexpr bool operator==(big_rational const& a, T b) { return a.Den == big_int{1} && a.Num == big_int{b}; }
    template <std::integral T>
    friend constexpr std::strong_ordering operator<=>(big_rational const& a, T b) { return a <=> big_rational{b}; }

    // The nearest double, by way of the top 64 bits of each part.
    constexpr explicit operator double() const
    {
      auto to_double = [](big_int const& v) {
        const big::mag m = v.magnitude();
        const int bw = big::bit_width(m);
        double d;
        if (bw <= 64) d = static_cast<double>(m.empty() ? umax{0} : m[0]);
        else
        {
          // top 64 bits plus a sticky bit for everything below
          const int sh = bw - 64;
          const std::size_t w = static_cast<std::size_t>(sh / 64);
          const int b = sh % 64;
          umax top = m[w] >> b;
          if (b != 0 && w + 1 < m.size()) top |= m[w + 1] << (64 - b);
          bool sticky = b != 0 && (m[w] << (64 - b)) != 0;
          for (std::size_t i = 0; i < w && !sticky; ++i) sticky = m[i] != 0;
          d = ldexp(static_cast<double>(top | (sticky ? 1u : 0u)), sh);
        }
        return v.negative() ? -d : d;
      };
      return to_double(Num) / to_double(Den);
    }
    constexpr explicit operator float() const { return static_cast<float>(static_cast<double>(*this)); }

    // Mixed with the 64-bit rational: each converts to the other, so these
    // exact overloads keep the operators unambiguous.
    friend constexpr bool operator==(big_rational const& a, rational const& b) { return a == big_rational{b}; }
    friend constexpr std::strong_ordering operator<=>(big_rational const& a, rational const& b) { return a <=> big_rational{b}; }
    friend constexpr big_rational operator+(big_rational const& a, rational const& b) { return a + big_rational{b}; }
    friend constexpr big_rational operator+(rational const& a, big_rational const& b) { return big_rational{a} + b; }
    friend constexpr big_rational operator-(big_rational const& a, rational const& b) { return a - big_rational{b}; }
    friend constexpr big_rational operator-(rational const& a, big_rational const& b) { return big_rational{a} - b; }
    friend constexpr big_rational operator*(big_rational const& a, rational const& b) { return a * big_rational{b}; }
    friend constexpr big_rational operator*(rational const& a, big_rational const& b) { return big_rational{a} * b; }
    friend constexpr big_rational operator/(big_rational const& a, rational const& b) { return a / big_rational{b}; }
    friend constexpr big_rational operator/(rational const& a, big_rational const& b) { return big_rational{a} / b; }
  };
  constexpr big_rational abs(big_rational a) { a.Num = abs(a.Num); return a; }

  // gcd of two fractions: gcd of numerators over lcm of denominators.
  constexpr big_rational gcd(big_rational const& a, big_rational const& b)
  {
    const big_int dg = gcd(a.Den, b.Den);
    return {gcd(a.Num, b.Num), a.Den / dg * b.Den};
  }
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_BIG_GRIDS
#endif // BEMAN_INSIDE_DETAIL_BIG_RATIONAL_HPP
