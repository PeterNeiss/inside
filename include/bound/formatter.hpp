//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDformatterHPP
#define BNDformatterHPP

#include "bound/format.hpp"
#include "bound/print.hpp"

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
template <bnd::grid G, bnd::policy_flag P>
struct std::formatter<bnd::bound<G, P>>
{
private:
  using B = bnd::bound<G, P>;
  static constexpr bool integer_path = bnd::IsIntegerAligned<B>;
  using inner = std::conditional_t<integer_path,
                                   std::formatter<bnd::imax>,
                                   std::formatter<double>>;
  inner numeric_{};
  bool  has_spec_ = false;

public:
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

  template <typename Ctx>
  auto format(B const& b, Ctx& ctx) const
  {
    if constexpr (integer_path)
      return numeric_.format(bnd::to_value(b), ctx);
    else if (has_spec_)
      return numeric_.format(
          static_cast<double>(static_cast<bnd::rational>(b)), ctx);
    else
      return std::format_to(ctx.out(), "{}", bnd::to_string(b));
  }
};

template <>
struct std::formatter<bnd::rational>
{
private:
  std::formatter<double> numeric_{};
  bool                   has_spec_ = false;

public:
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

  template <typename Ctx>
  auto format(bnd::rational const& r, Ctx& ctx) const
  {
    if (has_spec_)
      return numeric_.format(static_cast<double>(r), ctx);
    return std::format_to(ctx.out(), "{}", bnd::to_string(r));
  }
};

#endif // BNDformatterHPP
