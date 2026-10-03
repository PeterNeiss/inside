//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
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
#include <ostream>
#include <version>          // __cpp_lib_format feature-test macro

namespace beman::inside
{
  //-------------------------------------------------------------------------
  // to_string — pretty-prints `rational`, `interval`, `grid`, plus a fallback
  // for plain arithmetic types and the exact-rational form for insidables.
  //-------------------------------------------------------------------------
  inline std::string to_string(beman::inside::detail::rational r)
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

  inline std::string to_string(interval ival)
  {
    std::string str{"["};

    str += beman::inside::to_string(ival.Lower);
    str += "..";
    str += beman::inside::to_string(ival.Upper);
    str += "]";
    return str;
  }

  inline std::string to_string(grid g)
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
  auto to_string(V value)
  { return std::to_string(value); }

  // `real` (double-backed) and `exact` (rational-backed) bounds: render the
  // exact rational form. (Without this overload a real inside would fall to the
  // generic `std::to_string(double)` and print a lossy 6-digit form, and a
  // rational-raw inside has no std::to_string at all.) A continuous (Notch == 0)
  // real inside prints the double.
  template <insidable B>
    requires (detail::fp_raw<B> || detail::rational_raw<B>)
  inline std::string to_string(B b)
  {
    if constexpr (detail::fp_raw<B> && Notch<B> == beman::inside::detail::rational{0})
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
      return "unknown";
    }
  } // namespace detail

  //-------------------------------------------------------------------------
  // to_string / to_string_debug / operator<< for insidables and rational.
  // The debug form also prints the raw value, raw type, and grid — useful when
  // inspecting failing tests or storage choices.
  //-------------------------------------------------------------------------
  template <insidable B>
  inline std::string to_string(B b)
  { return beman::inside::to_string(detail::as_rational(b)); }

  template <insidable B>
  inline std::string to_string_debug(B b)
  {
    std::string str;
    str += beman::inside::to_string(detail::as_rational(b));
    str += " {";
    str += beman::inside::to_string(+b.raw());
    str += "[" + std::string(detail::type_name<detail::raw_t<B>>());
    str += " Max:" + beman::inside::to_string(+detail::NotchCount<B>) + "] ";
    str += beman::inside::to_string(Grid<B>);
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

} // namespace beman::inside

//---------------------------------------------------------------------------
// std::format integration is gated on a working <format> (libstdc++ ships it
// from GCC 13; GCC 12 / C++20 builds compile this as a no-op and rely on
// to_string()/operator<< instead).
//---------------------------------------------------------------------------
#ifdef __cpp_lib_format

#include <format>
#include <type_traits>

namespace beman::inside::detail
{
  // Shared spec handling: an empty `{}` is left to the derived format() (exact
  // to_string); a non-empty spec is parsed and applied by `numeric_`.
  template <class Inner>
  struct numeric_spec_formatter
  {
    Inner numeric_{};
    bool  has_spec_ = false;

    constexpr auto parse(std::format_parse_context& ctx)
    {
      auto it = ctx.begin();
      if (it != ctx.end() && *it != '}')
      {
        has_spec_ = true;
        return numeric_.parse(ctx);
      }
      return it;
    }
  };
}

template <beman::inside::grid G, beman::inside::policy_flag P>
struct std::formatter<beman::inside::inside<G, P>>
  : beman::inside::detail::numeric_spec_formatter<
      std::conditional_t<beman::inside::detail::IsIntegerAligned<beman::inside::inside<G, P>>,
                         std::formatter<beman::inside::imax>,
                         std::formatter<double>>>
{
  using B = beman::inside::inside<G, P>;
  static constexpr bool integer_path = beman::inside::detail::IsIntegerAligned<B>;

  template <typename Ctx>
  auto format(B const& b, Ctx& ctx) const
  {
    if constexpr (integer_path)
      return this->numeric_.format(beman::inside::detail::to_value(b), ctx);
    else if (this->has_spec_)
      return this->numeric_.format(
          static_cast<double>(beman::inside::detail::rational{b}), ctx);
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
    if (has_spec_)
      return numeric_.format(static_cast<double>(r), ctx);
    return std::format_to(ctx.out(), "{}", beman::inside::to_string(r));
  }
};

#endif // __cpp_lib_format

#endif // BEMAN_INSIDE_IO_HPP
