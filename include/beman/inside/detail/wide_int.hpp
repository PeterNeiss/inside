// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_WIDE_INT_HPP
#define BEMAN_INSIDE_DETAIL_WIDE_INT_HPP

#include <beman/inside/math.hpp>            // umax, constexpr ldexp
#include <beman/inside/detail/debug.hpp>    // errc, raise, constexpr_error

#include <bit>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

//---------------------------------------------------------------------------
// wide_int — a fixed-width two's-complement integer of N limbs, for raws and
// intermediates wider than 64 bits. It behaves like a builtin integer of
// N·limb_bits bits: + − × wrap, / and % truncate toward zero, >> is arithmetic
// when signed, and conversions to and from builtin integers truncate or
// sign-extend as the builtin conversions do. Unlike a builtin, overflow and
// out-of-range shifts are defined (wrap, and 0 or the sign fill).
//
// Structural (a public C array) and trivially copyable, so it can be an NTTP
// member and an inside raw. Every operation is constexpr and allocation-free.
//
// The limb type is a template parameter so tests can run wide_int over uint8_t
// limbs exhaustively against builtin oracles; the library uses umax limbs.
// The limb kernels below are also used by the C++26 big_rational.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  namespace limb
  {
    template <std::unsigned_integral L>
    inline constexpr int bits = std::numeric_limits<L>::digits;

    template <std::unsigned_integral L>
    struct pair { L Hi; L Lo; };

    // a + b + carry; carry is 0 or 1 in and out.
    template <std::unsigned_integral L>
    constexpr L add_carry(L a, L b, L& carry) noexcept
    {
      const L s  = static_cast<L>(a + b);
      const L s2 = static_cast<L>(s + carry);
      carry = static_cast<L>((s < a) + (s2 < s));
      return s2;
    }

    // a − b − borrow; borrow is 0 or 1 in and out.
    template <std::unsigned_integral L>
    constexpr L sub_borrow(L a, L b, L& borrow) noexcept
    {
      const L d  = static_cast<L>(a - b);
      const L d2 = static_cast<L>(d - borrow);
      borrow = static_cast<L>((a < b) + (d < borrow));
      return d2;
    }

    // Full product a·b as {hi, lo}. Native where the target has unsigned
    // __int128; else a schoolbook 32-bit split (32-bit targets).
    template <std::unsigned_integral L>
    constexpr pair<L> mul(L a, L b) noexcept
    {
      if constexpr (bits<L> <= 32)
      {
        const std::uint64_t p = std::uint64_t{a} * b;
        return {static_cast<L>(p >> bits<L>), static_cast<L>(p)};
      }
      else
      {
        static_assert(bits<L> == 64, "wide_int: limbs are at most 64 bits");
#if defined(__SIZEOF_INT128__)
        const unsigned __int128 p = static_cast<unsigned __int128>(a) * b;
        return {static_cast<L>(p >> 64), static_cast<L>(p)};
#else
        const L al = a & 0xffffffffu, ah = a >> 32;
        const L bl = b & 0xffffffffu, bh = b >> 32;
        const L ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
        const L mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
        return {hh + (lh >> 32) + (hl >> 32) + (mid >> 32), (ll & 0xffffffffu) | (mid << 32)};
#endif
      }
    }

    // a·b <=> c·d, exactly (the native 128-bit compare where available).
    constexpr std::strong_ordering mul_compare(umax a, umax b, umax c, umax d) noexcept
    {
#if defined(__SIZEOF_INT128__)
      return static_cast<unsigned __int128>(a) * b <=> static_cast<unsigned __int128>(c) * d;
#else
      const pair<umax> x = mul(a, b), y = mul(c, d);
      return x.Hi != y.Hi ? x.Hi <=> y.Hi : x.Lo <=> y.Lo;
#endif
    }

    // {hi, lo} / d and the remainder. Requires hi < d, so the quotient fits L.
    template <std::unsigned_integral L>
    constexpr pair<L> div(L hi, L lo, L d) noexcept   // {quotient, remainder}
    {
      if constexpr (bits<L> <= 32)
      {
        const std::uint64_t n = (std::uint64_t{hi} << bits<L>) | lo;
        return {static_cast<L>(n / d), static_cast<L>(n % d)};
      }
      else
      {
        if (hi == 0) return {static_cast<L>(lo / d), static_cast<L>(lo % d)};   // one machine division
#if defined(__SIZEOF_INT128__)
        const unsigned __int128 n = (static_cast<unsigned __int128>(hi) << 64) | lo;
        return {static_cast<L>(n / d), static_cast<L>(n % d)};
#else
        // Restoring shift-subtract; `top` keeps the bit shifted out of r.
        L r = hi, q = 0;
        for (int i = 63; i >= 0; --i)
        {
          const bool top = (r >> 63) != 0;
          r = (r << 1) | ((lo >> i) & 1u);
          q <<= 1;
          if (top || r >= d) { r -= d; q |= 1u; }
        }
        return {q, r};
#endif
      }
    }

    // Knuth's algorithm D (TAOCP 4.3.1). u has m limbs, v has n limbs with
    // v[n−1] != 0 and m >= n. Writes the m−n+1 quotient limbs to q and the n
    // remainder limbs to r. The caller provides scratch: un (m+1) and vn (n).
    template <std::unsigned_integral L>
    constexpr void divmod(const L* u, std::size_t m, const L* v, std::size_t n,
                          L* q, L* r, L* un, L* vn) noexcept
    {
      constexpr int B = bits<L>;
      if (n == 1)
      {
        L rem = 0;
        for (std::size_t i = m; i-- > 0;)
        {
          const pair<L> d = div(rem, u[i], v[0]);
          q[i] = d.Hi;
          rem = d.Lo;
        }
        r[0] = rem;
        return;
      }
      // Normalise: shift so the divisor's top limb has its high bit set.
      const int s = std::countl_zero(v[n - 1]);
      const auto shl = [&](L hi, L lo) -> L
      { return s == 0 ? hi : static_cast<L>(static_cast<L>(hi << s) | static_cast<L>(lo >> (B - s))); };
      for (std::size_t i = n - 1; i > 0; --i) vn[i] = shl(v[i], v[i - 1]);
      vn[0] = static_cast<L>(v[0] << s);
      un[m] = (s == 0) ? L{0} : static_cast<L>(u[m - 1] >> (B - s));
      for (std::size_t i = m - 1; i > 0; --i) un[i] = shl(u[i], u[i - 1]);
      un[0] = static_cast<L>(u[0] << s);

      for (std::size_t j = m - n + 1; j-- > 0;)
      {
        // Estimate q̂ from the top two dividend limbs, then correct it with the
        // next limb (at most two steps).
        L qhat, rhat;
        bool rhat_big = false;                          // r̂ ≥ base: skip the test
        if (un[j + n] == vn[n - 1])
        {
          qhat = static_cast<L>(~L{0});
          rhat = static_cast<L>(un[j + n - 1] + vn[n - 1]);
          rhat_big = rhat < vn[n - 1];
        }
        else
        {
          const pair<L> d = div(un[j + n], un[j + n - 1], vn[n - 1]);
          qhat = d.Hi;
          rhat = d.Lo;
        }
        while (!rhat_big)
        {
          const pair<L> p = mul(qhat, vn[n - 2]);
          if (p.Hi < rhat || (p.Hi == rhat && p.Lo <= un[j + n - 2])) break;
          --qhat;
          const L old = rhat;
          rhat = static_cast<L>(rhat + vn[n - 1]);
          rhat_big = rhat < old;
        }
        // Multiply and subtract q̂·vn from un[j .. j+n].
        L carry = 0, borrow = 0;
        for (std::size_t i = 0; i < n; ++i)
        {
          const pair<L> p = mul(qhat, vn[i]);
          L c = 0;
          const L lo = add_carry(p.Lo, carry, c);
          carry = static_cast<L>(p.Hi + c);
          un[i + j] = sub_borrow(un[i + j], lo, borrow);
        }
        un[j + n] = sub_borrow(un[j + n], carry, borrow);
        if (borrow != 0)                                // q̂ was one too large: add back
        {
          --qhat;
          L c = 0;
          for (std::size_t i = 0; i < n; ++i) un[i + j] = add_carry(un[i + j], vn[i], c);
          un[j + n] = static_cast<L>(un[j + n] + c);
        }
        q[j] = qhat;
      }
      // Denormalise the remainder.
      for (std::size_t i = 0; i + 1 < n; ++i)
        r[i] = (s == 0) ? un[i]
                        : static_cast<L>(static_cast<L>(un[i] >> s) | static_cast<L>(un[i + 1] << (B - s)));
      r[n - 1] = static_cast<L>(un[n - 1] >> s);
    }
  } // namespace limb

  template <std::size_t N, bool Signed, std::unsigned_integral L = umax>
  struct wide_int
  {
    static_assert(N >= 1, "wide_int: at least one limb");
    static constexpr int limb_bits = limb::bits<L>;
    static constexpr int bits = static_cast<int>(N) * limb_bits;
    static constexpr bool is_signed = Signed;

    L Word[N];                                          // little-endian limbs

    wide_int() = default;

    // From a builtin integer: sign-extends a negative signed value, as the
    // builtin conversions do.
    template <std::integral T>
    constexpr wide_int(T v) noexcept
    {
#if defined(__SIZEOF_INT128__)
      using W = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
      using W = std::uint64_t;
#endif
      using S = std::make_signed_t<W>;
      constexpr int wbits = std::numeric_limits<W>::digits;
      const W w = std::is_signed_v<T> ? static_cast<W>(static_cast<S>(v)) : static_cast<W>(v);
      const L fill = (std::is_signed_v<T> && v < 0) ? static_cast<L>(~L{0}) : L{0};
      for (std::size_t i = 0; i < N; ++i)
        Word[i] = (static_cast<int>(i) * limb_bits < wbits)
                ? static_cast<L>(w >> (static_cast<int>(i) * limb_bits)) : fill;
    }

    // Between widths: sign-extends a signed source, truncates a wider one.
    // Implicit only when it widens.
    template <std::size_t M, bool S2>
      requires (M != N || S2 != Signed)
    constexpr explicit(M >= N) wide_int(wide_int<M, S2, L> const& o) noexcept
    {
      const L fill = o.negative() ? static_cast<L>(~L{0}) : L{0};
      for (std::size_t i = 0; i < N; ++i) Word[i] = (i < M) ? o.Word[i] : fill;
    }

    [[nodiscard]] constexpr bool negative() const noexcept
    { return Signed && (Word[N - 1] >> (limb_bits - 1)) != 0; }

    [[nodiscard]] constexpr bool is_zero() const noexcept
    {
      for (std::size_t i = 0; i < N; ++i)
        if (Word[i] != 0) return false;
      return true;
    }

    constexpr explicit operator bool() const noexcept { return !is_zero(); }

    // To a builtin integer: keeps the low bits (two's complement), as the
    // builtin narrowing conversions do.
    template <std::integral T>
    constexpr explicit operator T() const noexcept
    {
#if defined(__SIZEOF_INT128__)
      using W = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
      using W = std::uint64_t;
#endif
      constexpr int wbits = std::numeric_limits<W>::digits;
      W acc = 0;
      for (std::size_t i = 0; i < N && static_cast<int>(i) * limb_bits < wbits; ++i)
        acc |= static_cast<W>(Word[i]) << (static_cast<int>(i) * limb_bits);
      if constexpr (bits < wbits)
        if (negative()) acc |= ~W{0} << bits;
      return static_cast<T>(acc);
    }

    // Nearest double (ties to even).
    constexpr explicit operator double() const noexcept
    {
      if (negative()) return -static_cast<double>(wide_int<N, false, L>(-*this));   // −min reads as 2^(bits−1)
      const int bw = bit_width_of(*this);
      if (bw <= 64) return static_cast<double>(static_cast<std::uint64_t>(*this));
      // Keep the top 64 bits and fold the rest into a sticky bit: 64 > 53 + 2,
      // so the one uint64 → double rounding is the correct one.
      const int sh = bw - 64;
      std::uint64_t top = static_cast<std::uint64_t>(lshr(*this, sh));
      if (!(lshr(shl(*this, bits - sh), bits - sh)).is_zero()) top |= 1u;
      return ldexp(static_cast<double>(top), sh);
    }

    // ---- arithmetic (wraps modulo 2^bits) -------------------------------
    friend constexpr wide_int operator+(wide_int a, wide_int const& b) noexcept
    {
      L c = 0;
      for (std::size_t i = 0; i < N; ++i) a.Word[i] = limb::add_carry(a.Word[i], b.Word[i], c);
      return a;
    }
    friend constexpr wide_int operator-(wide_int a, wide_int const& b) noexcept
    {
      L br = 0;
      for (std::size_t i = 0; i < N; ++i) a.Word[i] = limb::sub_borrow(a.Word[i], b.Word[i], br);
      return a;
    }
    friend constexpr wide_int operator-(wide_int const& a) noexcept { return wide_int{0} - a; }
    friend constexpr wide_int operator+(wide_int const& a) noexcept { return a; }

    friend constexpr wide_int operator*(wide_int const& a, wide_int const& b) noexcept
    {
      wide_int r{0};
      for (std::size_t i = 0; i < N; ++i)
      {
        L carry = 0;
        for (std::size_t j = 0; i + j < N; ++j)
        {
          const limb::pair<L> p = limb::mul(a.Word[i], b.Word[j]);
          L c1 = 0, c2 = 0;
          L s = limb::add_carry(r.Word[i + j], p.Lo, c1);
          s = limb::add_carry(s, carry, c2);
          r.Word[i + j] = s;
          carry = static_cast<L>(p.Hi + c1 + c2);       // fits: (b−1)² + 2(b−1) < b²
        }
      }
      return r;
    }

    struct divmod_result;
    // Truncating division, as the builtin operators. Division by zero fails
    // the build in constant evaluation and reports errc::division_by_zero at
    // runtime.
    [[nodiscard]] static constexpr divmod_result divmod(wide_int const& a, wide_int const& b) noexcept;

    friend constexpr wide_int operator/(wide_int const& a, wide_int const& b) noexcept
    { return divmod(a, b).Quotient; }
    friend constexpr wide_int operator%(wide_int const& a, wide_int const& b) noexcept
    { return divmod(a, b).Remainder; }

    // ---- bitwise ---------------------------------------------------------
    friend constexpr wide_int operator~(wide_int a) noexcept
    {
      for (std::size_t i = 0; i < N; ++i) a.Word[i] = static_cast<L>(~a.Word[i]);
      return a;
    }
    friend constexpr wide_int operator&(wide_int a, wide_int const& b) noexcept
    { for (std::size_t i = 0; i < N; ++i) a.Word[i] &= b.Word[i]; return a; }
    friend constexpr wide_int operator|(wide_int a, wide_int const& b) noexcept
    { for (std::size_t i = 0; i < N; ++i) a.Word[i] |= b.Word[i]; return a; }
    friend constexpr wide_int operator^(wide_int a, wide_int const& b) noexcept
    { for (std::size_t i = 0; i < N; ++i) a.Word[i] ^= b.Word[i]; return a; }

    // Shifts by s >= bits give 0 (<<) or the sign fill (>>); negative s is a
    // precondition violation, as for builtins.
    friend constexpr wide_int operator<<(wide_int const& a, int s) noexcept { return shl(a, s); }
    friend constexpr wide_int operator>>(wide_int const& a, int s) noexcept
    {
      const L fill = a.negative() ? static_cast<L>(~L{0}) : L{0};
      return shr(a, s, fill);
    }

    // ---- comparison --------------------------------------------------------
    // Written out: GCC 15/16 miscompare a defaulted == over an array member in
    // constant evaluation (`a == x && b == y` can come out true with b != y).
    friend constexpr bool operator==(wide_int const& a, wide_int const& b) noexcept
    {
      for (std::size_t i = 0; i < N; ++i)
        if (a.Word[i] != b.Word[i]) return false;
      return true;
    }
    friend constexpr std::strong_ordering operator<=>(wide_int const& a, wide_int const& b) noexcept
    {
      if (a.negative() != b.negative())
        return a.negative() ? std::strong_ordering::less : std::strong_ordering::greater;
      for (std::size_t i = N; i-- > 0;)
        if (a.Word[i] != b.Word[i]) return a.Word[i] <=> b.Word[i];
      return std::strong_ordering::equal;
    }

    // ---- compound forms ----------------------------------------------------
    constexpr wide_int& operator+=(wide_int const& b) noexcept { return *this = *this + b; }
    constexpr wide_int& operator-=(wide_int const& b) noexcept { return *this = *this - b; }
    constexpr wide_int& operator*=(wide_int const& b) noexcept { return *this = *this * b; }
    constexpr wide_int& operator/=(wide_int const& b) noexcept { return *this = *this / b; }
    constexpr wide_int& operator%=(wide_int const& b) noexcept { return *this = *this % b; }
    constexpr wide_int& operator&=(wide_int const& b) noexcept { return *this = *this & b; }
    constexpr wide_int& operator|=(wide_int const& b) noexcept { return *this = *this | b; }
    constexpr wide_int& operator^=(wide_int const& b) noexcept { return *this = *this ^ b; }
    constexpr wide_int& operator<<=(int s) noexcept { return *this = *this << s; }
    constexpr wide_int& operator>>=(int s) noexcept { return *this = *this >> s; }
    constexpr wide_int& operator++() noexcept { return *this += wide_int{1}; }
    constexpr wide_int& operator--() noexcept { return *this -= wide_int{1}; }
    constexpr wide_int operator++(int) noexcept { wide_int t = *this; ++*this; return t; }
    constexpr wide_int operator--(int) noexcept { wide_int t = *this; --*this; return t; }

    // Number of significant bits of a non-negative value (0 for 0).
    [[nodiscard]] friend constexpr int bit_width_of(wide_int const& a) noexcept
    {
      for (std::size_t i = N; i-- > 0;)
        if (a.Word[i] != 0)
          return static_cast<int>(i) * limb_bits + (limb_bits - std::countl_zero(a.Word[i]));
      return 0;
    }

  private:
    static constexpr wide_int shl(wide_int const& a, int s) noexcept
    {
      wide_int r{0};
      if (s >= bits) return r;
      const std::size_t ws = static_cast<std::size_t>(s / limb_bits);
      const int bs = s % limb_bits;
      for (std::size_t i = ws; i < N; ++i)
      {
        L w = static_cast<L>(a.Word[i - ws] << bs);
        if (bs != 0 && i > ws) w |= static_cast<L>(a.Word[i - ws - 1] >> (limb_bits - bs));
        r.Word[i] = w;
      }
      return r;
    }
    static constexpr wide_int shr(wide_int const& a, int s, L fill) noexcept
    {
      wide_int r;
      if (s >= bits)
      {
        for (std::size_t i = 0; i < N; ++i) r.Word[i] = fill;
        return r;
      }
      const std::size_t ws = static_cast<std::size_t>(s / limb_bits);
      const int bs = s % limb_bits;
      const auto word = [&](std::size_t k) { return k < N ? a.Word[k] : fill; };
      for (std::size_t i = 0; i < N; ++i)
      {
        L w = static_cast<L>(word(i + ws) >> bs);
        if (bs != 0) w |= static_cast<L>(word(i + ws + 1) << (limb_bits - bs));
        r.Word[i] = w;
      }
      return r;
    }
    static constexpr wide_int lshr(wide_int const& a, int s) noexcept { return shr(a, s, L{0}); }
  };

  template <std::size_t N, bool Signed, std::unsigned_integral L>
  struct wide_int<N, Signed, L>::divmod_result { wide_int Quotient; wide_int Remainder; };

  template <std::size_t N, bool Signed, std::unsigned_integral L>
  constexpr auto wide_int<N, Signed, L>::divmod(wide_int const& a, wide_int const& b) noexcept
    -> divmod_result
  {
    if (b.is_zero())
    {
      if consteval { constexpr_error<"wide_int: division by zero">(); }
      raise(errc::division_by_zero, "wide_int: division by zero");
    }
    // Divide the magnitudes; −min reads correctly as an unsigned magnitude.
    const bool na = a.negative(), nb = b.negative();
    const wide_int ua = na ? -a : a, ub = nb ? -b : b;
    std::size_t m = N, n = N;
    while (m > 0 && ua.Word[m - 1] == 0) --m;
    while (ub.Word[n - 1] == 0) --n;
    divmod_result res{wide_int{0}, wide_int{0}};
    if (m < n)
      res.Remainder = ua;
    else
    {
      L un[N + 1]{}, vn[N]{};
      limb::divmod(ua.Word, m, ub.Word, n, res.Quotient.Word, res.Remainder.Word, un, vn);
    }
    if (na != nb) res.Quotient = -res.Quotient;
    if (na) res.Remainder = -res.Remainder;
    return res;
  }

  template <std::size_t N, std::unsigned_integral L = umax>
  using wide_uint = wide_int<N, false, L>;
  template <std::size_t N, std::unsigned_integral L = umax>
  using wide_sint = wide_int<N, true, L>;

  template <typename T>
  inline constexpr bool is_wide_int_v = false;
  template <std::size_t N, bool S, std::unsigned_integral L>
  inline constexpr bool is_wide_int_v<wide_int<N, S, L>> = true;
} // namespace beman::inside::detail

template <std::size_t N, bool Signed, std::unsigned_integral L>
struct std::numeric_limits<beman::inside::detail::wide_int<N, Signed, L>>
{
private:
  using W = beman::inside::detail::wide_int<N, Signed, L>;
  static constexpr W top_bit() noexcept { return W{1} << (W::bits - 1); }
public:
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = Signed;
  static constexpr bool is_integer = true;
  static constexpr bool is_exact = true;
  static constexpr bool has_infinity = false;
  static constexpr bool has_quiet_NaN = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr bool is_bounded = true;
  static constexpr bool is_modulo = !Signed;
  static constexpr int  radix = 2;
  static constexpr int  digits = W::bits - (Signed ? 1 : 0);
  static constexpr int  digits10 = digits * 30103 / 100000;   // ⌊digits·log10 2⌋
  static constexpr W min() noexcept { return Signed ? top_bit() : W{0}; }
  static constexpr W lowest() noexcept { return min(); }
  static constexpr W max() noexcept { return Signed ? ~top_bit() : ~W{0}; }
};

#endif // BEMAN_INSIDE_DETAIL_WIDE_INT_HPP
