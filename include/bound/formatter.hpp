//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDformatterHPP
#define BNDformatterHPP

#include "bound/format.hpp"
#include "bound/print.hpp"

#include <version>          // __cpp_lib_format feature-test macro

// std::format integration is gated on a working <format> (libstdc++ ships it
// from GCC 13; GCC 12 / C++20 builds compile this header as a no-op and rely
// on to_string()/operator<< from format.hpp/print.hpp instead).
#ifdef __cpp_lib_format

#include <format>
#include <type_traits>

//---------------------------------------------------------------------------
// formatter — `std::formatter` specializations for `bound<G, P>` and
// `rational`.
//
// Default spec (`{}`) preserves exact-rational output via `bnd::to_string`
// — `"21.5"`, `"2 1/3"`, etc.
//
// Non-empty specs route by storage shape:
//   - IsIntegerAligned<bound> → std::formatter<imax> over `to_value(b)`.
//     User gets `{:5}`, `{:+}`, `{:#x}`, `{:o}`, `{:b}`, `{:c}` etc.
//   - otherwise              → std::formatter<double> over `double(b)`
//     (via implicit rational then explicit cast). User gets `{:.2f}`,
//     `{:e}`, `{:g}`. This is a lossy display path — user opts in by
//     writing a numeric spec.
//
// For `rational`: empty spec → `to_string`, non-empty → double formatter.
//---------------------------------------------------------------------------
namespace bnd::detail
{
  // Shared spec handling for the bound / rational std::formatter
  // specializations: an empty `{}` spec is left to the derived format() (which
  // prints the exact to_string form); a non-empty spec is parsed and later
  // applied by `numeric_` (a std::formatter<imax> or std::formatter<double>).
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

template <bnd::grid G, bnd::policy_flag P>
struct std::formatter<bnd::bound<G, P>>
  : bnd::detail::numeric_spec_formatter<
      std::conditional_t<bnd::detail::IsIntegerAligned<bnd::bound<G, P>>,
                         std::formatter<bnd::imax>,
                         std::formatter<double>>>
{
  using B = bnd::bound<G, P>;
  static constexpr bool integer_path = bnd::detail::IsIntegerAligned<B>;

  template <typename Ctx>
  auto format(B const& b, Ctx& ctx) const
  {
    if constexpr (integer_path)
      return this->numeric_.format(bnd::detail::to_value(b), ctx);
    else if (this->has_spec_)
      return this->numeric_.format(
          static_cast<double>(bnd::detail::rational{b}), ctx);
    else
      return std::format_to(ctx.out(), "{}", bnd::to_string(b));
  }
};

template <>
struct std::formatter<bnd::detail::rational>
  : bnd::detail::numeric_spec_formatter<std::formatter<double>>
{
  template <typename Ctx>
  auto format(bnd::detail::rational const& r, Ctx& ctx) const
  {
    if (has_spec_)
      return numeric_.format(static_cast<double>(r), ctx);
    return std::format_to(ctx.out(), "{}", bnd::to_string(r));
  }
};

#endif // __cpp_lib_format

#endif // BNDformatterHPP
