// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_POLICY_FLAG_HPP
#define BEMAN_INSIDE_POLICY_FLAG_HPP

#include <tuple>
#include <type_traits>
#include <utility>

// BEMAN_INSIDE_MATH_NO_FP — no hardware floating point anywhere: the f64/f32
// storage flags fall back to deduced integer storage, and the math engine's
// double tier compiles out (its integer path computes every result; results
// do not change). Resolved here, in a header every other one includes, so
// storage selection and the math headers always agree. Define it to force the
// FP-free build; it is auto-enabled on freestanding targets
// (__STDC_HOSTED__ == 0). Public API and grid deduction are unchanged.
#if !defined(BEMAN_INSIDE_MATH_NO_FP)
#  if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
#    define BEMAN_INSIDE_MATH_NO_FP
#  endif
#endif

// -ffast-math is not supported. The library's results are exact or correctly
// rounded, and that rests on IEEE arithmetic as written: f64/f32 storage
// detects overflow through infinities and NaN, and the math engine's error
// bounds and error-free sums count every rounding in program order. Fast-math
// lets the compiler assume no NaN or infinity and reassociate, which can
// change results silently, so a build that announces it stops here.
#if defined(__FAST_MATH__) || defined(__ASSOCIATIVE_MATH__) \
    || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
#  error "beman::inside does not support -ffast-math, -fassociative-math or -ffinite-math-only: its results rely on IEEE floating point as written"
#endif

namespace beman::inside
{
  //---------------------------------------------------------------------------
  // policy_flag
  //---------------------------------------------------------------------------
  using policy_flag = unsigned long long;

  // Check model: compile-time checks always run. When success can't be proven
  // statically, compilation fails unless the matching ignore flag is set; else a
  // runtime check is inserted that throws (or reports via an error_code param).
  // Binary operations OR the flags of both operands.
  inline constexpr policy_flag none         {0ull};
  inline constexpr policy_flag ignore_zero  {1ull << 1};
  inline constexpr policy_flag ignore_range{1ull << 2};
  // `snap` — an off-notch value is rounded to fit the grid instead of
  // rejected; on its own truncate-toward-zero. Without it, an off-notch value is
  // a compile/runtime error and div/mod fall through to exact-rational results.
  inline constexpr policy_flag snap     {1ull << 4};
  inline constexpr policy_flag round_nearest {(1ull << 5) | snap};
  // Rounding modes each pick a unique bit and OR in `snap`. Conceptually
  // exclusive; combining two is allowed but dispatch (assignment.hpp) picks the
  // first match: nearest → floor → ceil → half_even → trunc.
  inline constexpr policy_flag round_floor     {(1ull << 6) | snap};
  inline constexpr policy_flag round_ceil      {(1ull << 7) | snap};
  inline constexpr policy_flag round_half_even {(1ull << 8) | snap};

  // runtime checking — on unless the policy carries `unsafe` (see is_checked).
  // Spelling `checked` re-enables the checks alongside `unsafe`.
  inline constexpr policy_flag checked{1ull << 34}; // runtime range/notch/overflow checks

  // unary — mutually exclusive
  inline constexpr policy_flag clamp   {1ull << 32}; // saturate to boundary
  inline constexpr policy_flag wrap    {1ull << 33}; // modular arithmetic

  // Representation flags — select raw storage. Without one, storage is deduced
  // from the grid (notch-0 → rational; unit notch at/below 0 → integer value;
  // else 0-based index). Binary ops OR operand policies; storage resolves
  // widest-wins: exact > f64 > f32 > {width} > direct > indexed > deduced.
  // ({width} = the fixed-width integer flags i8..u64 declared below; they pin the
  // exact backing type rather than letting deduction pick the smallest fit.)
  //
  // `f64` — binary64-backed storage (value held as IEEE-754 double, notch
  // nominal); an ordinary round_nearest integer inside under
  // BEMAN_INSIDE_MATH_NO_FP. Power-of-2 notch + dyadic Lower required so
  // on-grid values are exact in double (see `double_exact`).
  inline constexpr policy_flag f64{(1ull << 37) | round_nearest};

  // `f32` — binary32-backed storage (raw held as IEEE-754 float, notch nominal);
  // the single-precision sibling of `f64`, for float-only FPUs (Cortex-M4F).
  // Power-of-2 notch + dyadic Lower required AND every on-grid value must fit
  // float's 24-bit significand (see `float_exact`). Like `f64` it is an
  // ordinary round_nearest integer inside under BEMAN_INSIDE_MATH_NO_FP.
  inline constexpr policy_flag f32{(1ull << 41) | round_nearest};


  // Fixed-width integer raw storage — pin the exact backing type instead of
  // letting deduction pick the smallest fit. A bare width flag means *value*
  // storage (raw == value, like `direct`, so Notch == 1 and the value range must
  // fit the type); OR in `indexed` for 0-based notch-index storage. `storage_pick`
  // static_asserts the type is big enough for the grid (no silent widening). One
  // width flag at a time. Unlike `f32`/`f64` these carry no `round_nearest` — they
  // are plain integer storage, like `direct`/`indexed`.
  inline constexpr policy_flag i8 {1ull << 42};
  inline constexpr policy_flag u8 {1ull << 43};
  inline constexpr policy_flag i16{1ull << 44};
  inline constexpr policy_flag u16{1ull << 45};
  inline constexpr policy_flag i32{1ull << 46};
  inline constexpr policy_flag u32{1ull << 47};
  inline constexpr policy_flag i64{1ull << 48};
  inline constexpr policy_flag u64{1ull << 49};

  // OR of every fixed-width flag — lets storage_pick test "any width pinned" and
  // count set bits (exactly one allowed) in a single mask.
  inline constexpr policy_flag raw_width_mask
    {i8 | u8 | i16 | u16 | i32 | u32 | i64 | u64};

  // `exact` — force rational raw storage on any grid. Values still obey the grid;
  // exact fractions, no notch-count limit, no double. Slowest; overflow-checked
  // rational math. Identical under both engines.
  inline constexpr policy_flag exact{1ull << 38};

  // `direct` — force raw == value (plain integer) where deduction would pick a
  // 0-based index (inside<{5,100}> stores 5..100). Wire/debugger value for interop.
  // Requires Notch == 1.
  inline constexpr policy_flag direct{1ull << 39};

  // `indexed` — force raw == 0-based notch index where deduction would pick
  // direct storage (inside<{-5,5}> stores 0..10). Dense unsigned layout. Requires
  // Notch != 0.
  inline constexpr policy_flag indexed{1ull << 40};

  // opt-out of `checked`: no domain/round/overflow/div-by-zero checks (reading
  // out-of-range or dividing by zero is UB; `/= 0` no-ops, `a / 0` skips the
  // check). Includes `snap` so notch-incompatible assigns compile.
  namespace detail { inline constexpr policy_flag unsafe_marker{1ull << 36}; }
  inline constexpr policy_flag unsafe
    {detail::unsafe_marker | ignore_range | snap | ignore_zero};

  //---------------------------------------------------------------------------
  // Flag-set membership predicates. `has_flag(set, flag)` is true iff EVERY bit
  // of `flag` is present in `set` — reads better than the raw `(set & flag) ==
  // flag` and is correct for composite flags (e.g. `round_nearest` carries
  // `snap`, `f64` carries `round_nearest`), where a bare `set & flag`
  // truthy test would misfire. `has_any_flag` tests for any overlap.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr bool has_flag(policy_flag set, policy_flag flag) noexcept
  { return (set & flag) == flag; }

  [[nodiscard]] constexpr bool has_any_flag(policy_flag set, policy_flag flags) noexcept
  { return (set & flags) != none; }

  // Runtime checks run unless the policy opts out with `unsafe`; an explicit
  // `checked` wins over `unsafe`. So `inside<G, round_nearest>` and
  // `inside<G, f64>` are checked, exactly like the default `inside<G>`.
  [[nodiscard]] constexpr bool is_checked(policy_flag set) noexcept
  { return has_flag(set, checked) || !has_flag(set, detail::unsafe_marker); }

  namespace detail
  {
    // The rounding mode a flag set selects — the ONE precedence every rounding
    // path uses (integer, rational and fp storage, division, math stores).
    // An explicit directional or half-even mode beats round_nearest (which f64 /
    // f32 carry by default, so `f64 | round_floor` floors); `snap` alone, or no
    // rounding flag at all, truncates toward zero. Ties of `nearest` go half
    // away from zero.
    enum class round_mode { trunc, nearest, floor, ceil, half_even };

    [[nodiscard]] constexpr round_mode rounding_of(policy_flag f) noexcept
    {
      if (has_flag(f, round_floor))     return round_mode::floor;
      if (has_flag(f, round_ceil))      return round_mode::ceil;
      if (has_flag(f, round_half_even)) return round_mode::half_even;
      if (has_flag(f, round_nearest))   return round_mode::nearest;
      return round_mode::trunc;
    }
  }

  //---------------------------------------------------------------------------
  // no_action — zero-overhead default for overflow callbacks
  //---------------------------------------------------------------------------
  struct no_action {};

  //---------------------------------------------------------------------------
  // tagged actions — opt-in callbacks for each failure path.
  // The lambda receives the inside by mutable reference as its first argument,
  // so the handler can override the value the policy was about to store.
  //---------------------------------------------------------------------------
  template<typename F> struct on_clamp_t    { [[no_unique_address]] F Fn; };
  template<typename F> struct on_wrap_t     { [[no_unique_address]] F Fn; };
  template<typename F> struct on_error_t    { [[no_unique_address]] F Fn; };
  template<typename F> struct on_overflow_t { [[no_unique_address]] F Fn; };

  //---------------------------------------------------------------------------
  // CTAD-style factories — drop the on_overflow_t{lambda} brace-init.
  //---------------------------------------------------------------------------
  template<typename F> [[nodiscard]] constexpr auto on_clamp(F&& fn)
  { return on_clamp_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_wrap(F&& fn)
  { return on_wrap_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_error(F&& fn)
  { return on_error_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_overflow(F&& fn)
  { return on_overflow_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }

  namespace detail
  {
  // Action detection: the `*Pred` struct is the primary detector; the concept
  // derives from it and strips cvref so the ref form matches the value form.
  template<typename T> struct is_clamp_action    : std::false_type {};
  template<typename F> struct is_clamp_action<on_clamp_t<F>>    : std::true_type {};
  template<typename T> struct is_wrap_action     : std::false_type {};
  template<typename F> struct is_wrap_action<on_wrap_t<F>>     : std::true_type {};
  template<typename T> struct is_error_action    : std::false_type {};
  template<typename F> struct is_error_action<on_error_t<F>>    : std::true_type {};
  template<typename T> struct is_overflow_action : std::false_type {};
  template<typename F> struct is_overflow_action<on_overflow_t<F>> : std::true_type {};

  template<typename T> concept clamp_action    = is_clamp_action   <std::remove_cvref_t<T>>::value;
  template<typename T> concept wrap_action     = is_wrap_action    <std::remove_cvref_t<T>>::value;
  template<typename T> concept error_action    = is_error_action   <std::remove_cvref_t<T>>::value;
  template<typename T> concept overflow_action = is_overflow_action<std::remove_cvref_t<T>>::value;

  //---------------------------------------------------------------------------
  // implied_flags<A> — single source of truth for "this action requires these
  // policy bits". Used by inside::on_* and the action-first free-fn overloads.
  //---------------------------------------------------------------------------
  template<typename T> inline constexpr policy_flag implied_flags = none;
  template<typename F> inline constexpr policy_flag implied_flags<on_clamp_t<F>>    = clamp;
  template<typename F> inline constexpr policy_flag implied_flags<on_wrap_t<F>>     = wrap;
  template<typename F> inline constexpr policy_flag implied_flags<on_error_t<F>>    = checked;
  template<typename F> inline constexpr policy_flag implied_flags<on_overflow_t<F>> = checked;

  //---------------------------------------------------------------------------
  // Pack helpers — let policy_ref/assignment/arithmetic accept Actions... packs.
  // The `*Pred` structs are reused as template-template parameters (concepts
  // can't be passed as such in C++23).
  //---------------------------------------------------------------------------

  // True if any element of the pack matches the trait.
  template<template<typename> class Trait, typename... As>
  inline constexpr bool has_action = (Trait<std::remove_cvref_t<As>>::value || ... || false);

  // How many pack elements match.
  template<template<typename> class Trait, typename... As>
  inline constexpr unsigned count_action_matches =
    (0u + ... + (Trait<std::remove_cvref_t<As>>::value ? 1u : 0u));

  // OR of implied_flags<plain_t<A>> across the pack.
  template<typename... As>
  inline constexpr policy_flag merged_implied_flags =
    (none | ... | implied_flags<std::remove_cvref_t<As>>);

  // pick_action<Trait>(actions...) returns a reference to the first pack element
  // matching the trait, or a static `no_action` fallback if none does. Conflict
  // diagnostics elsewhere ensure at most one match.
    template<template<typename> class Trait>
    inline no_action& pick_action_fallback()
    { static no_action n; return n; }

    template<template<typename> class Trait, typename A, typename... Rest>
    constexpr auto& pick_action_impl(A& a, Rest&... rest)
    {
      if constexpr (Trait<std::remove_cvref_t<A>>::value) return a;
      else if constexpr (sizeof...(Rest) > 0) return pick_action_impl<Trait>(rest...);
      else return pick_action_fallback<Trait>();
    }

  template<template<typename> class Trait, typename... As>
  constexpr auto& pick_action(As&... as)
  {
    if constexpr (sizeof...(As) == 0) return pick_action_fallback<Trait>();
    else return pick_action_impl<Trait>(as...);
  }

  // Same, but operating on a tuple (lvalue or rvalue ref).
  template<template<typename> class Trait, typename Tuple>
  constexpr auto& pick_action_in(Tuple& t)
  {
    return std::apply(
      [](auto&... as) -> auto& { return pick_action<Trait>(as...); },
      t);
  }

  } // namespace detail
} // namespace beman::inside

#endif // BEMAN_INSIDE_POLICY_FLAG_HPP
