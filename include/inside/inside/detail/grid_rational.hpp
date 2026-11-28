// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_GRID_RATIONAL_HPP
#define INSIDE_INSIDE_DETAIL_GRID_RATIONAL_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/rational.hpp>
#include <inside/inside/detail/number_text.hpp> // the literal grammar (_g)
#include <inside/inside/detail/big_rational.hpp>

// The grid-number vocabulary.
// A grid's limits and notch (what interval and grid are made of) are grid_numbers of any size:
// inline while they fit the 64-bit rational, else an interned big_rational
// (C++26 static reflection, std::define_static_object), so equal values stay the same template argument.
//   grid_number                            the type of a grid's limits and notch
//   grid_int                               exact integers for grid computations (step counts, value indices),
//                                          wide enough for every grid
//   grid_numerator(r), grid_denominator(r) a grid number's signed numerator and positive denominator as grid_int
//                                          (also for a rational)
//   grid_divides_evenly                    a / n is an integer (true for n == 0)
//   grid_same_lattice                      (a − b) / n is an integer (true for n == 0)
//   fits_rational(r), to_rational(r)       whether, and as which 64-bit rational, a grid number serves the 64-bit
//                                          fast paths
// + − × /, abs and gcd are grid_number's own (hidden friends).
namespace ins::detail
{
  // (a − b) / n is an integer (true for n == 0), for 64-bit rationals.
  // A difference past the rational range is tested in four limbs:
  // (a − b)/n = (a_n·b_d − b_n·a_d)·n_d / (a_d·b_d·n_n), whose products of three 64-bit magnitudes fit.
  constexpr bool same_lattice64(const rational& a, const rational& b, const rational& n)
  {
    if (n == 0)
      return true;
    if (const auto d = try_sub(a, b)) [[likely]]
      return divides_evenly(*d, n);
    using W = wide_sint<4>;
    auto num_of = [](const rational& r) { return signed_numerator<W>(r); };
    auto den_of = [](const rational& r) { return W{abs_den(r.Den)}; };
    const W num{(num_of(a) * den_of(b) - num_of(b) * den_of(a)) * den_of(n)};
    const W den{den_of(a) * den_of(b) * num_of(n)};
    return num % den == W{0};
  }

  using grid_int = big_int;
} // namespace ins::detail

namespace ins
{
  // grid_number: an exact rational of any size, and a structural type,
  // the type of a grid's limits, notch and bound, of _g and of lower_of, upper_of and notch_of.
  // A value that fits the 64-bit rational is held inline, as detail::rational holds it
  // (the numerator's magnitude, and the denominator with the sign),
  // so a grid's type reads short: grid_number{1, 100, 0} is 1/100.
  // Zero is all zero bits, grid_number{0, 0, 0}, which a mangled name leaves out
  // (and with it the MaxDenominator of every grid but a continuous one).
  // A larger value is an interned big_rational, which exists only at compile time, marked by Den == 0 and Num == 1
  // (tested instead of the pointer: under -fsanitize=null GCC cannot compare an interned pointer with nullptr
  // in a constant expression).
  // Zero takes the 64-bit paths without a big value: it is decided inside the Den == 0 branch,
  // so the paths of the other inline values are as before.
  // One form per value, reduced and inline whenever it fits, so equal values are the same template argument.
  struct grid_number
  {
    std::uint64_t               Num{0};
    std::int64_t                Den{0};
    const detail::big_rational* Big{nullptr};

    constexpr grid_number() = default;
    template <std::integral T>
      requires(std::numeric_limits<T>::digits <= 64)
    constexpr grid_number(T v) noexcept
      : Num{std::is_signed_v<T> && v < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(v)
                                         : static_cast<std::uint64_t>(v)},
        Den{v == 0 ? 0 : std::is_signed_v<T> && v < 0 ? -1 : 1} {}
    template <std::integral T>
      requires(std::numeric_limits<T>::digits > 64)
    constexpr grid_number(T v) : grid_number(detail::big_rational{v}) {}
    constexpr grid_number(detail::big_int n, detail::big_int d = detail::big_int{1})
      : grid_number(detail::big_rational{n, d}) {}
    // A 64-bit rational, in lowest terms whether or not it was.
    constexpr grid_number(const detail::rational& r)
      : grid_number(detail::big_rational{detail::big_int::of_limb(r.Num, r.Den < 0),
                                         detail::big_int{detail::abs_den(r.Den)}}) {}
    // Anything else that converts to the 64-bit rational (an inside: -1.2_q) takes that one conversion,
    // so it can be a grid limit.
    template <class T>
      requires(std::is_class_v<T> && !std::same_as<T, detail::rational> && !std::same_as<T, grid_number>
               && !std::same_as<T, detail::big_rational> && std::convertible_to<const T&, detail::rational>)
    constexpr grid_number(const T& v) : grid_number(detail::rational(v)) {}
    constexpr grid_number(std::floating_point auto d) : grid_number(detail::big_rational{d}) {} // exact
    constexpr grid_number(const detail::big_rational& b)
    {
      if (b.fits_rational())
      {
        if (b.Num.Small == 0)
          return; // zero: all zero bits
        Num = b.Num.Small;
        const std::int64_t d{static_cast<std::int64_t>(b.Den.Small)};
        // A denominator is positive: the optimizer then knows Den != 0 here, so big() never reads Big (null).
        [[assume(d > 0)]];
        Den = b.Num.negative() ? -d : d;
        return;
      }
      if consteval
      {
        Num = 1;
        Den = 0;
        Big = std::define_static_object(b);
      }
      else
      {
        detail::fail_hard<"grid number past 64 bits at run time">(error::overflow);
      }
    }

    // The value as a big_rational: a copy of the interned one, or built from the inline parts, in lowest terms.
    [[nodiscard]] constexpr detail::big_rational big() const
    {
      if (Den == 0)
        return Num == 0 ? detail::big_rational{} : *Big;
      detail::big_rational r;
      r.Num = detail::big_int::of_limb(Num, Den < 0);
      r.Den = detail::big_int{detail::abs_den(Den)};
      return r;
    }
    // The numerator (signed) and the denominator (> 0), in lowest terms.
    [[nodiscard]] constexpr detail::big_int numerator() const { return big().Num; }
    [[nodiscard]] constexpr detail::big_int denominator() const { return big().Den; }

    [[nodiscard]] constexpr bool fits_rational() const noexcept { return Den != 0 || Num == 0; }
    [[nodiscard]] constexpr bool negative() const noexcept
    {
      return Den != 0 ? Den < 0 : Num != 0 && Big->Num.negative();
    }
    [[nodiscard]] constexpr bool is_zero() const noexcept { return Den == 0 && Num == 0; }
    [[nodiscard]] constexpr bool is_integer() const noexcept
    {
      return Den != 0 ? (Den == 1 || Den == -1) : Num == 0 || Big->is_integer();
    }

    // The 64-bit rational of a value that fits (fits_rational()).
    [[nodiscard]] constexpr detail::rational small() const noexcept
    {
      return Den != 0 ? detail::rational_from_parts(Num, Den) : detail::rational{};
    }

    // Implicit, so every 64-bit path keeps working on a small grid.
    // A value past 64 bits fails the build when such a path meets it.
    constexpr operator detail::rational() const
    {
      if (Den == 0)
      {
        if (Num == 0)
          return detail::rational{};
        detail::fail_hard<"grid number past 64 bits on a path that needs a 64-bit rational">(error::overflow);
      }
      return detail::rational_from_parts(Num, Den);
    }
    constexpr explicit operator double() const { return static_cast<double>(big()); }
    constexpr explicit operator float() const { return static_cast<float>(big()); }

    // By content, never by address (see Den): both inline (zero too), or both interned.
    friend constexpr bool operator==(const grid_number& a, const grid_number& b)
    {
      if (a.Den != b.Den || a.Num != b.Num)
        return false;
      return a.Den != 0 || a.Num == 0 || *a.Big == *b.Big;
    }
    friend constexpr std::strong_ordering operator<=>(const grid_number& a, const grid_number& b)
    {
      if (a.fits_rational() && b.fits_rational())
        return a.small() <=> b.small();
      return a.big() <=> b.big();
    }

    // The arithmetic by name, for run-time paths on 64-bit values.
    static constexpr grid_number neg(const grid_number& a)
    {
      if (a.Den == 0)
        return a.Num == 0 ? a : grid_number{detail::big_rational::neg(*a.Big)};
      grid_number r{a};
      r.Den = -r.Den;
      return r;
    }
    static constexpr grid_number add(const grid_number& a, const grid_number& b)
    {
      return detail::big_rational::add(a.big(), b.big());
    }
    static constexpr grid_number sub(const grid_number& a, const grid_number& b)
    {
      return detail::big_rational::sub(a.big(), b.big());
    }
    static constexpr grid_number mul(const grid_number& a, const grid_number& b)
    {
      return detail::big_rational::mul(a.big(), b.big());
    }
    // Pre: b ≠ 0.
    static constexpr grid_number div(const grid_number& a, const grid_number& b)
    {
      return detail::big_rational::div(a.big(), b.big());
    }

    // Found by argument-dependent lookup, from every namespace.
    friend constexpr grid_number abs(grid_number a)
    {
      if (a.Den == 0)
        return a.Num == 0 ? a : grid_number{detail::abs(*a.Big)};
      if (a.Den < 0)
        a.Den = -a.Den;
      return a;
    }
    // The gcd of two fractions: the gcd of the numerators over the lcm of the denominators.
    friend constexpr grid_number gcd(const grid_number& a, const grid_number& b)
    {
      return detail::gcd(a.big(), b.big());
    }

    friend consteval grid_number operator-(const grid_number& a) { return neg(a); }
    friend consteval grid_number operator+(const grid_number& a, const grid_number& b) { return add(a, b); }
    friend consteval grid_number operator-(const grid_number& a, const grid_number& b) { return sub(a, b); }
    friend consteval grid_number operator*(const grid_number& a, const grid_number& b) { return mul(a, b); }
    friend consteval grid_number operator/(const grid_number& a, const grid_number& b) { return div(a, b); }

    // Mixed with an integer or the 64-bit rational:
    // each converts to both grid_number and rational, so these exact overloads keep the operators unambiguous.
    // (A comparison with the 64-bit rational compares in place:
    // it needs no reduced form, so a run-time range check forms no big value.)
    template <detail::big_operand T>
    friend constexpr std::strong_ordering operator<=>(const grid_number& a, const T& b)
    {
      if constexpr (std::same_as<T, detail::rational>)
      {
        if (a.fits_rational())
          return a.small() <=> b;
        return *a.Big <=> detail::big_rational{b};
      }
      else
        return a <=> grid_number{b};
    }
    template <detail::big_operand T>
    friend constexpr bool operator==(const grid_number& a, const T& b)
    {
      return (a <=> b) == 0;
    }
    template <detail::big_operand T>
    friend consteval grid_number operator+(const grid_number& a, const T& b)
    {
      return a + grid_number{b};
    }
    template <detail::big_operand T>
    friend consteval grid_number operator+(const T& a, const grid_number& b)
    {
      return grid_number{a} + b;
    }
    template <detail::big_operand T>
    friend consteval grid_number operator-(const grid_number& a, const T& b)
    {
      return a - grid_number{b};
    }
    template <detail::big_operand T>
    friend consteval grid_number operator-(const T& a, const grid_number& b)
    {
      return grid_number{a} - b;
    }
    template <detail::big_operand T>
    friend consteval grid_number operator*(const grid_number& a, const T& b)
    {
      return a * grid_number{b};
    }
    template <detail::big_operand T>
    friend consteval grid_number operator*(const T& a, const grid_number& b)
    {
      return grid_number{a} * b;
    }
    template <detail::big_operand T>
    friend consteval grid_number operator/(const grid_number& a, const T& b)
    {
      return a / grid_number{b};
    }
    template <detail::big_operand T>
    friend consteval grid_number operator/(const T& a, const grid_number& b)
    {
      return grid_number{a} / b;
    }
  };
} // namespace ins

namespace ins::detail
{
  // A grid number as a transient fraction, for tests that run at run time too.
  constexpr big::fraction fraction_of(const grid_number& r) { return fraction_of(r.big()); }

  constexpr grid_int grid_numerator(const grid_number& r) { return r.numerator(); }
  constexpr grid_int grid_denominator(const grid_number& r) { return r.denominator(); }

  // (a − b) / n is an integer (true for n == 0): b and a share n's lattice.
  // No big value is formed, so the test runs at run time too.
  constexpr bool grid_same_lattice(const grid_number& a, const grid_number& b, const grid_number& n)
  {
    if (n == 0)
      return true;
    if (a.fits_rational() && b.fits_rational() && n.fits_rational())
      return same_lattice64(a, b, n);
    return big::same_lattice(fraction_of(a), fraction_of(b), fraction_of(n));
  }

  // (The convention, as in divides_evenly: everything divides 0 evenly.)
  constexpr bool grid_divides_evenly(const grid_number& a, const grid_number& n)
  {
    return grid_same_lattice(a, grid_number{0}, n);
  }

  // The 64-bit rational of a grid number, for the 64-bit fast paths.
  constexpr bool fits_rational(const grid_number& r) { return r.fits_rational(); }
  constexpr rational to_rational(const grid_number& r) { return r; }

  constexpr grid_int grid_numerator(const rational& r) noexcept
  {
    const grid_int n{r.Num};
    return r.Den < 0 ? big_int::neg(n) : n;
  }
  constexpr grid_int grid_denominator(const rational& r) noexcept { return grid_int{abs_den(r.Den)}; }

  // The value bits a signed integer needs to hold every value in [lo, hi].
  constexpr int signed_value_bits(const grid_int& lo, const grid_int& hi) noexcept
  {
    auto mag = [](const grid_int& v)
    {
      return bit_width_of(v.negative() ? big_int::neg(big_int::add(v, grid_int{1})) : v);
    };
    const int a{mag(lo)}, b{mag(hi)};
    return a > b ? a : b;
  }

  // r rounded to an integer by M (half away from zero for nearest).
  template <round_mode M>
  constexpr grid_number grid_round(const grid_number& r)
  {
    const grid_int n  {grid_numerator(r)}, d{grid_denominator(r)};
    grid_int       q  {n / d};
    const grid_int rem{n - q * d};
    const bool     neg{n.negative()};
    const bool     odd{!(q / grid_int{2} * grid_int{2} == q)};
    if (rounds_away(M, neg, classify_remainder(M, neg ? -rem : rem, d), odd))
      q = neg ? q - grid_int{1} : q + grid_int{1};
    return grid_number{q};
  }

  // An unsigned number in [first, last) in the whole literal grammar
  // (scan_number: 0x and 0b prefixes, ' separators, a point, an e±n or p±n exponent), taken exactly, of any size:
  // the scan's failure (e_exponent_numerator_overflow: an exponent past max_exp), else none and the value in out.
  // The _g and _q literals (parse_grid_literal) and an id's text (detail/id_text.hpp) read numbers here.
  consteval parse_fail scan_exact(const char* first, const char* last, grid_number& out, int max_exp = 10000)
  {
    // The value is m·B^e (B the radix), times 2^±n or 10^±n.
    // Zero digits are held back (zeros) and multiplied in only before a later nonzero digit,
    // so trailing zeros (1.000…) never grow m.
    grid_int m{0};
    int      e{0}, zeros{0};
    const scanned_number sc{scan_number(
      first, last,
      [&](int d, bool in_frac, int b)
      {
        if (d == 0)
          ++zeros;
        else
        {
          for (; zeros > 0; --zeros)
            m = m * grid_int{b};
          m = m * grid_int{b} + grid_int{d};
        }
        e -= in_frac;
        return true;
      })};
    if (sc.Fail != parse_fail::none)
      return sc.Fail;
    if (sc.Exp > max_exp)
      return parse_fail::e_exponent_numerator_overflow;
    if (m == grid_int{0})
    {
      out = grid_number{0};
      return parse_fail::none;
    }
    grid_int num{m}, den{1};
    const auto scale = [&](int x, int k) // ·x^k
    {
      const grid_int p{pow_by_squaring(grid_int{x}, static_cast<unsigned>(k < 0 ? -k : k))};
      (k < 0 ? den : num) = (k < 0 ? den : num) * p;
    };
    scale(sc.Base, e + zeros);
    if (sc.HasP || sc.HasE)
      scale(sc.HasP ? 2 : 10, sc.ExpNeg ? -sc.Exp : sc.Exp);
    out = grid_number{num, den};
    return parse_fail::none;
  }

  // parse_grid_literal: the exact literal (scan_exact), of any size.
  // The _g literal, and _q (its point); _r parses the same grammar into the 64-bit rational.
  template <char... Chars>
  consteval grid_number parse_grid_literal()
  {
    if constexpr (octal_literal<Chars...>())
      constexpr_error<"_g/_q literal: a leading 0 makes an octal literal; write the number without it">();
    constexpr char text[] = {Chars...};
    grid_number    out;
    const parse_fail f{scan_exact(text, text + sizeof(text), out)};
    if (f == parse_fail::empty)
      constexpr_error<"_g/_q literal: expected digits">();
    if (f == parse_fail::e_exponent_numerator_overflow)
      constexpr_error<"_g/_q literal: exponent too large">();
    if (f != parse_fail::none)
      constexpr_error<"_g/_q literal: digits, a point and an exponent only">();
    return out;
  }
} // namespace ins::detail

namespace ins::detail
{
  // The checked builder of per<D> below, so a static_assert here fires at the user's spelling.
  // D is an integer of at least 1, a builtin or a grid number of any size:
  // 1/D as the 64-bit rational where it fits one, else a grid number.
  template <auto D>
  consteval auto make_per()
  {
    if constexpr (std::integral<decltype(D)>)
    {
      static_assert(D >= 1, "per<D> is the positive step 1/D; a continuous grid is spelled 0");
      if constexpr (std::in_range<std::int64_t>(D))
        return rational{std::int64_t{1}, static_cast<std::int64_t>(D)};
      else
        return grid_number{1} / grid_number{D};
    }
    else
    {
      static_assert(std::same_as<decltype(D), grid_number>, "per<D>: D is an integer");
      static_assert(D >= 1 && grid_denominator(D) == grid_int{1},
                    "per<D> is the positive step 1/D for an integer D; a continuous grid is spelled 0");
      if constexpr (fits_rational(grid_number{1} / D))
        return to_rational(grid_number{1} / D);
      else
        return grid_number{1} / D;
    }
  }
} // namespace ins::detail

namespace ins
{
  // The grid-building helpers.
  // per<D> is the step 1/D, the common notch (per<256> is Q·8; per<2^70>, with D a grid number, is a notch past
  // 64 bits).
  // Any other exact ratio is the point "N/D"_q (inside/detail/core.hpp):
  // a limit like -1.2_q for -1.2, or another step like ("360/4096"_q).
  // (The _r literal, an exact decimal or hex value, 0.1_r exactly 1/10, is in inside/detail/number_text.hpp.)
  // Integer steps are plain integers ({{0, 100}, 5}); grid::validate rejects a negative notch however it is spelled.
  template <auto D>
  inline constexpr auto per = detail::make_per<D>();

  // A grid number of any size: id<"[0..1267650600228229401496703205376]">,
  // an exact decimal notch {{0, 1}, 1e-30_g}, or a binary one 0x1p-100_g.
  inline namespace literals
  {
    template <char... Chars>
    consteval grid_number operator""_g()
    {
      return detail::parse_grid_literal<Chars...>();
    }
  } // namespace literals
} // namespace ins

#endif // INSIDE_INSIDE_DETAIL_GRID_RATIONAL_HPP
