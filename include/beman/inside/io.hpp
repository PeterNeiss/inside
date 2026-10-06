// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// io — ALL of the library's string / stream / std::format support, gathered
// into one opt-in header. The core (inside.hpp and everything it pulls) never
// includes this, so a freestanding / bare-metal build that never includes
// "beman/inside/io.hpp" pays zero <string>/<ostream>/<format> cost. In the single-
// header amalgamation this whole region is wrapped in `#ifndef BEMAN_INSIDE_NO_STRING`,
// so defining BEMAN_INSIDE_NO_STRING drops it (and its heavy includes) wholesale.
//
// Provides:
//   * to_string(rational | interval | grid | insidable | arithmetic)
//   * to_string_debug(insidable)        — value + raw + raw-type + grid
//   * operator<<(std::ostream&, ...)
//   * std::formatter<inside<G,P>> / std::formatter<rational>   (when <format>)
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_IO_HPP
#define BEMAN_INSIDE_IO_HPP

#include <beman/inside/inside.hpp>

#include <string>
#include <string_view>
#include <istream>
#include <ostream>
#include <version>          // __cpp_lib_format feature-test macro

namespace beman::inside
{
  //-------------------------------------------------------------------------
  // to_string — pretty-prints `rational`, `interval`, `grid`, plus a fallback
  // for plain arithmetic types and the exact-rational form for insidables.
  //-------------------------------------------------------------------------
  [[nodiscard]] inline std::string to_string(beman::inside::detail::rational r)
  {
    std::string str;
    if (r.Denominator < 0)
      str = "-";

    umax ad = detail::abs_den(r.Denominator);
    if (ad == 1)
      return str += std::to_string(r.Numerator);

    // power-of-2 or power-of-10: decimal output
    // find smallest 10^k divisible by ad
    umax pow10 = 1;
    unsigned digits = 0;
    bool is_decimal = false;
    for (unsigned k = 0; k < 20; ++k)
    {
      if (pow10 % ad == 0)
      { is_decimal = true; digits = k; break; }
      pow10 *= 10;
    }

    if (is_decimal)
    {
      umax scale = pow10 / ad;
      umax total;
      if (!mul_overflow(r.Numerator, scale, &total))
      {
        umax int_part = total / pow10;
        umax frac_part = total % pow10;
        str += std::to_string(int_part);
        if (digits > 0)
        {
          str += ".";
          auto frac_str = std::to_string(frac_part);
          // zero-pad
          for (unsigned i = 0; i < digits - frac_str.size(); ++i)
            str += "0";
          str += frac_str;
        }
        return str;
      }
      // Decimal expansion would overflow the umax scratch buffer. Fall back
      // silently to the mixed-number/fraction form — `to_string` must always
      // produce *some* readable output, never an error.
    }

    // mixed number for improper fractions
    umax int_part = r.Numerator / ad;
    umax remainder = r.Numerator % ad;
    if (int_part > 0)
    {
      str += std::to_string(int_part);
      if (remainder > 0)
      {
        str += " ";
        str += std::to_string(remainder);
        str += "/";
        str += std::to_string(ad);
      }
    }
    else
    {
      str += std::to_string(r.Numerator);
      str += "/";
      str += std::to_string(ad);
    }
    return str;
  }

#if BEMAN_INSIDE_BIG_GRIDS
  // A grid number: the rational form when it fits 64 bits, else num/den.
  [[nodiscard]] inline std::string to_string(detail::big_rational const& r);
#endif

  [[nodiscard]] inline std::string to_string(interval ival)
  {
    std::string str{"["};

    str += beman::inside::to_string(ival.Lower);
    str += "..";
    str += beman::inside::to_string(ival.Upper);
    str += "]";
    return str;
  }

  [[nodiscard]] inline std::string to_string(grid g)
  {
    std::string str{"{"};

    str += beman::inside::to_string(g.Interval);
    str += ", ";
    str += beman::inside::to_string(g.Notch);
    str += "}";
    return str;
  }

  // delegate to std::to_string
  template <typename V>
  [[nodiscard]] auto to_string(V value)
  { return std::to_string(value); }

  // `f64` (double-backed) and `exact` (rational-backed) insides: render the
  // exact rational form. (Without this overload a f64 inside would fall to the
  // generic `std::to_string(double)` and print a lossy 6-digit form, and a
  // rational-raw inside has no std::to_string at all.) A continuous (Notch == 0)
  // f64 inside prints the double.
  template <insidable B>
    requires (detail::fp_raw<B> || detail::rational_raw<B>)
  [[nodiscard]] inline std::string to_string(B b)
  {
    if constexpr (detail::fp_raw<B> && detail::notch64<B> == beman::inside::detail::rational{0})
      return std::to_string(detail::as_double(b));
    else
      return to_string(beman::inside::detail::as_rational(b));
  }

  //-------------------------------------------------------------------------
  // type_name<T>() — short raw-type label for to_string_debug. Lives here (not
  // in the core math header) so the core never pulls <string_view>.
  //-------------------------------------------------------------------------
  namespace detail
  {
    template <typename T>
    constexpr std::string_view type_name()
    {
      if constexpr (std::is_same_v<T, std::uint8_t>)  return "uint8_t";
      if constexpr (std::is_same_v<T, std::uint16_t>) return "uint16_t";
      if constexpr (std::is_same_v<T, std::uint32_t>) return "uint32_t";
      if constexpr (std::is_same_v<T, std::uint64_t>) return "uint64_t";
      if constexpr (std::is_same_v<T, std::int8_t>)   return "int8_t";
      if constexpr (std::is_same_v<T, std::int16_t>)  return "int16_t";
      if constexpr (std::is_same_v<T, std::int32_t>)  return "int32_t";
      if constexpr (std::is_same_v<T, std::int64_t>)  return "int64_t";
      if constexpr (std::is_same_v<T, rational>) return "rational";
      if constexpr (std::is_same_v<T, point_slot>) return "point";
      if constexpr (std::is_same_v<T, wide_uint<2>>) return "wide_uint<2>";
      if constexpr (std::is_same_v<T, wide_uint<3>>) return "wide_uint<3>";
      if constexpr (is_wide_int_v<T>) return "wide_int";
      return "unknown";
    }
  } // namespace detail

  //-------------------------------------------------------------------------
  // to_string / to_string_debug / operator<< for insidables and rational.
  // The debug form also prints the raw value, raw type, and grid — useful when
  // inspecting failing tests or storage choices.
  //-------------------------------------------------------------------------
  namespace detail
  {
    // Decimal digits of a wide integer: 19 digits per division by 10^19.
    template <std::size_t N, bool S>
    std::string wide_to_decimal(wide_int<N, S> v)
    {
      const bool neg = v.negative();
      wide_int<N, false> u(neg ? -v : v);
      const wide_int<N, false> chunk{10'000'000'000'000'000'000ull};
      std::string out;
      do
      {
        const auto [q, r] = wide_int<N, false>::divmod(u, chunk);
        std::string part = std::to_string(static_cast<umax>(r));
        if (!q.is_zero()) part.insert(0, 19 - part.size(), '0');
        out.insert(0, part);
        u = q;
      } while (!u.is_zero());
      return neg ? "-" + out : out;
    }

    // A wide-index value: the usual rational form when it fits, else the
    // reduced fraction num/den in decimal.
    template <std::size_t K>
    std::string exact_to_string(exact_frac<K> f)
    {
      using I = wide_sint<K>;
      if (const auto r = try_rational(f)) return beman::inside::to_string(*r);
      I x = f.Num.negative() ? -f.Num : f.Num, y = f.Den;
      while (!y.is_zero()) { const I t = x % y; x = y; y = t; }
      const I num = f.Num / x, den = f.Den / x;
      return den == I{1} ? wide_to_decimal(num) : wide_to_decimal(num) + "/" + wide_to_decimal(den);
    }
  }

  template <std::size_t N, bool S>
  [[nodiscard]] inline std::string to_string(detail::wide_int<N, S> v) { return detail::wide_to_decimal(v); }

#if BEMAN_INSIDE_BIG_GRIDS
  // A grid number of any size, in decimal (reads the interned limbs; no new
  // big values are formed at runtime).
  [[nodiscard]] inline std::string to_string(detail::big_int const& v)
  {
    detail::big::mag m = v.magnitude();
    std::string out;
    do
    {
      umax rem = 0;                                     // m /= 10^19, rem = m % 10^19
      for (std::size_t i = m.size(); i-- > 0;)
      {
        const auto d = detail::limb::div(rem, m[i], umax{10'000'000'000'000'000'000ull});
        m[i] = d.Hi;
        rem = d.Lo;
      }
      detail::big::trim(m);
      std::string part = std::to_string(rem);
      if (!m.empty()) part.insert(0, 19 - part.size(), '0');
      out.insert(0, part);
    } while (!m.empty());
    return v.negative() ? "-" + out : out;
  }
#endif

#if BEMAN_INSIDE_BIG_GRIDS
  [[nodiscard]] inline std::string to_string(detail::big_rational const& r)
  {
    if (r.fits_rational()) return beman::inside::to_string(static_cast<detail::rational>(r));
    const std::string num = beman::inside::to_string(r.Num);
    return r.is_integer() ? num : num + "/" + beman::inside::to_string(r.Den);
  }
#endif

  template <insidable B>
  [[nodiscard]] inline std::string to_string(B b)
  {
    if constexpr (detail::exact_valued<B>) return detail::exact_to_string(detail::exact_of(b));
    else                               return beman::inside::to_string(detail::as_rational(b));
  }

  template <insidable B>
  [[nodiscard]] inline std::string to_string_debug(B b)
  {
    std::string str;
    str += beman::inside::to_string(b);
    str += " {";
    str += beman::inside::to_string(+b.raw());
    str += "[" + std::string(detail::type_name<detail::raw_t<B>>());
    constexpr auto slots = grid_of<B>.slot_count();
    str += " Max:" + beman::inside::to_string(slots) + "] ";
    str += beman::inside::to_string(grid_of<B>);
    str += "}";
    return str;
  }

  inline std::ostream& operator<<(std::ostream& stream, beman::inside::detail::rational r)
  {
    stream << beman::inside::to_string(r);
    return stream;
  }

  template <insidable B>
  inline std::ostream& operator<<(std::ostream& stream, B b)
  {
    stream << beman::inside::to_string(b);
    return stream;
  }

  // from_chars<B>(text) — the std::string_view form of from_chars<B>(first, last).
  template <insidable B>
  [[nodiscard]] constexpr std::expected<B, errc> from_chars(std::string_view text)
  { return from_chars<B>(text.data(), text.data() + text.size()); }

  // Reads one whitespace-delimited token and parses it with from_chars<B>. On an
  // error the stream's failbit is set and `b` is left unchanged.
  template <insidable B>
  inline std::istream& operator>>(std::istream& stream, B& b)
  {
    std::string token;
    if (!(stream >> token)) return stream;
    if (const auto r = from_chars<B>(token); r) b = *r;
    else stream.setstate(std::ios_base::failbit);
    return stream;
  }

} // namespace beman::inside

//---------------------------------------------------------------------------
// std::format integration is gated on a working <format>; without it this
// compiles as a no-op and to_string()/operator<< remain.
//---------------------------------------------------------------------------
#ifdef __cpp_lib_format

#include <format>
#include <type_traits>

namespace beman::inside::detail
{
  // Shared spec handling: an empty `{}` is left to the derived format() (exact
  // to_string); a non-empty spec is parsed and applied by `Numeric`.
  template <class Inner>
  struct numeric_spec_formatter
  {
    Inner Numeric{};
    bool  HasSpec = false;

    constexpr auto parse(std::format_parse_context& ctx)
    {
      auto it = ctx.begin();
      if (it != ctx.end() && *it != '}')
      {
        HasSpec = true;
        return Numeric.parse(ctx);
      }
      return it;
    }
  };
}

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::formatter<beman::inside::inside<G, P>>
  : beman::inside::detail::numeric_spec_formatter<
      std::conditional_t<beman::inside::detail::is_integer_aligned<beman::inside::inside<G, P>> && G.Notch != 0
                         && beman::inside::detail::values_fit_imax<beman::inside::inside<G, P>>,
                         std::formatter<beman::inside::imax>,
                         std::formatter<double>>>
{
  using B = beman::inside::inside<G, P>;
  // Integer formatting only for a notched integer grid: a continuous grid
  // (notch 0) holds fractions even between integer bounds.
  static constexpr bool integer_path = beman::inside::detail::is_integer_aligned<B> && G.Notch != 0
                                    && beman::inside::detail::values_fit_imax<B>;

  template <typename Ctx>
  auto format(B const& b, Ctx& ctx) const
  {
    if constexpr (integer_path)
      return this->Numeric.format(beman::inside::detail::to_value(b), ctx);
    else if (this->HasSpec)
      return this->Numeric.format(beman::inside::detail::as_double(b), ctx);
    else
      return std::format_to(ctx.out(), "{}", beman::inside::to_string(b));
  }
};

template <>
struct std::formatter<beman::inside::detail::rational>
  : beman::inside::detail::numeric_spec_formatter<std::formatter<double>>
{
  template <typename Ctx>
  auto format(beman::inside::detail::rational const& r, Ctx& ctx) const
  {
    if (HasSpec)
      return Numeric.format(static_cast<double>(r), ctx);
    return std::format_to(ctx.out(), "{}", beman::inside::to_string(r));
  }
};

#endif // __cpp_lib_format

#endif // BEMAN_INSIDE_IO_HPP
