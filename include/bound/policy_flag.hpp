//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpolicyflagHPP
#define BNDpolicyflagHPP

#include <tuple>
#include <type_traits>
#include <utility>

namespace bnd
{
  //---------------------------------------------------------------------------
  // policy_flag
  //---------------------------------------------------------------------------
  using policy_flag = unsigned long long;

  // Check model: compile-time checks always run. When success can't be proven
  // statically, compilation fails unless the matching ignore flag is set; else a
  // runtime check is inserted that throws (or reports via an error_code param).
  // Binary operations OR the flags of both operands.
  inline static constexpr policy_flag none         {0ull};
  inline static constexpr policy_flag ignore_zero  {1ull << 1};
  inline static constexpr policy_flag ignore_domain{1ull << 2};
  // `snap` — an off-notch value is rounded to fit the grid instead of
  // rejected; on its own truncate-toward-zero. Without it, an off-notch value is
  // a compile/runtime error and div/mod fall through to exact-rational results.
  inline static constexpr policy_flag snap     {1ull << 4};
  inline static constexpr policy_flag round_nearest {(1ull << 5) | snap};
  // Rounding modes each pick a unique bit and OR in `snap`. Conceptually
  // exclusive; combining two is allowed but dispatch (assignment.hpp) picks the
  // first match: nearest → floor → ceil → half_even → trunc.
  inline static constexpr policy_flag round_floor     {(1ull << 6) | snap};
  inline static constexpr policy_flag round_ceil      {(1ull << 7) | snap};
  inline static constexpr policy_flag round_half_even {(1ull << 8) | snap};

  // runtime checking — opt-in
  inline static constexpr policy_flag checked{1ull << 34}; // enable runtime domain/overflow checks

  // unary — mutually exclusive
  inline static constexpr policy_flag clamp   {1ull << 32}; // saturate to boundary
  inline static constexpr policy_flag wrap    {1ull << 33}; // modular arithmetic
  inline static constexpr policy_flag sentinel{1ull << 35}; // overflow -> sentinel (nullopt)

  // Representation flags — select raw storage. Without one, storage is deduced
  // from the grid (notch-0 → rational; unit notch at/below 0 → integer value;
  // else 0-based index). Binary ops OR operand policies; storage resolves
  // widest-wins: exact > real > direct > indexed > deduced.
  //
  // `real` — math operand. Double-backed storage under the default engine (value
  // held as IEEE-754 double, notch nominal); an ordinary round_nearest integer
  // bound under BND_MATH_FIXED. Power-of-2 notch + dyadic Lower required so
  // on-grid values are exact in double.
  inline static constexpr policy_flag real{(1ull << 37) | round_nearest};

  // `exact` — force rational raw storage on any grid. Values still obey the grid;
  // exact fractions, no notch-count limit, no double. Slowest; overflow-checked
  // rational math. Identical under both engines.
  inline static constexpr policy_flag exact{1ull << 38};

  // `direct` — force raw == value (plain integer) where deduction would pick a
  // 0-based index (bound<{5,100}> stores 5..100). Wire/debugger value for interop.
  // Requires Notch == 1.
  inline static constexpr policy_flag direct{1ull << 39};

  // `indexed` — force raw == 0-based notch index where deduction would pick
  // direct storage (bound<{-5,5}> stores 0..10). Dense unsigned layout. Requires
  // Notch != 0.
  inline static constexpr policy_flag indexed{1ull << 40};

  // opt-out of `checked`: no domain/round/overflow/div-by-zero checks (reading
  // out-of-range or dividing by zero is UB; `/= 0` no-ops, `a / 0` skips the
  // check). Includes `snap` so notch-incompatible assigns compile.
  inline static constexpr policy_flag unsafe
    {(1ull << 36) | ignore_domain | snap | ignore_zero};

  //---------------------------------------------------------------------------
  // Flag-set membership predicates. `has_flag(set, flag)` is true iff EVERY bit
  // of `flag` is present in `set` — reads better than the raw `(set & flag) ==
  // flag` and is correct for composite flags (e.g. `round_nearest` carries
  // `snap`, `real` carries `round_nearest`), where a bare `set & flag`
  // truthy test would misfire. `has_any_flag` tests for any overlap.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr bool has_flag(policy_flag set, policy_flag flag) noexcept
  { return (set & flag) == flag; }

  [[nodiscard]] constexpr bool has_any_flag(policy_flag set, policy_flag flags) noexcept
  { return (set & flags) != none; }

  //---------------------------------------------------------------------------
  // no_action — zero-overhead default for overflow callbacks
  //---------------------------------------------------------------------------
  struct no_action {};

  //---------------------------------------------------------------------------
  // tagged actions — opt-in callbacks for each failure path.
  // The lambda receives the bound by mutable reference as its first argument,
  // so the handler can override the value the policy was about to store.
  //---------------------------------------------------------------------------
  template<typename F> struct on_clamp_t    { [[no_unique_address]] F fn; };
  template<typename F> struct on_wrap_t     { [[no_unique_address]] F fn; };
  template<typename F> struct on_error_t    { [[no_unique_address]] F fn; };
  template<typename F> struct on_sentinel_t { [[no_unique_address]] F fn; };
  template<typename F> struct on_overflow_t { [[no_unique_address]] F fn; };

  //---------------------------------------------------------------------------
  // CTAD-style factories — drop the on_overflow_t{lambda} brace-init.
  //---------------------------------------------------------------------------
  template<typename F> [[nodiscard]] constexpr auto on_clamp(F&& fn)
  { return on_clamp_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_wrap(F&& fn)
  { return on_wrap_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_error(F&& fn)
  { return on_error_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_sentinel(F&& fn)
  { return on_sentinel_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_overflow(F&& fn)
  { return on_overflow_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }

  namespace detail
  {
  // Action detection: the `*Pred` struct is the primary detector; the concept
  // derives from it and strips cvref so the ref form matches the value form.
  template<typename T> struct IsClampActionPred    : std::false_type {};
  template<typename F> struct IsClampActionPred<on_clamp_t<F>>    : std::true_type {};
  template<typename T> struct IsWrapActionPred     : std::false_type {};
  template<typename F> struct IsWrapActionPred<on_wrap_t<F>>     : std::true_type {};
  template<typename T> struct IsErrorActionPred    : std::false_type {};
  template<typename F> struct IsErrorActionPred<on_error_t<F>>    : std::true_type {};
  template<typename T> struct IsSentinelActionPred : std::false_type {};
  template<typename F> struct IsSentinelActionPred<on_sentinel_t<F>> : std::true_type {};
  template<typename T> struct IsOverflowActionPred : std::false_type {};
  template<typename F> struct IsOverflowActionPred<on_overflow_t<F>> : std::true_type {};

  template<typename T> concept clamp_action    = IsClampActionPred   <std::remove_cvref_t<T>>::value;
  template<typename T> concept wrap_action     = IsWrapActionPred    <std::remove_cvref_t<T>>::value;
  template<typename T> concept error_action    = IsErrorActionPred   <std::remove_cvref_t<T>>::value;
  template<typename T> concept sentinel_action = IsSentinelActionPred<std::remove_cvref_t<T>>::value;
  template<typename T> concept overflow_action = IsOverflowActionPred<std::remove_cvref_t<T>>::value;

  //---------------------------------------------------------------------------
  // implied_flags<A> — single source of truth for "this action requires these
  // policy bits". Used by bound::on_* and the action-first free-fn overloads.
  //---------------------------------------------------------------------------
  template<typename T> inline constexpr policy_flag implied_flags = none;
  template<typename F> inline constexpr policy_flag implied_flags<on_clamp_t<F>>    = clamp;
  template<typename F> inline constexpr policy_flag implied_flags<on_wrap_t<F>>     = wrap;
  template<typename F> inline constexpr policy_flag implied_flags<on_error_t<F>>    = checked;
  template<typename F> inline constexpr policy_flag implied_flags<on_sentinel_t<F>> = sentinel;
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

  // OR of implied_flags<plain<A>> across the pack.
  template<typename... As>
  inline constexpr policy_flag merged_implied_flags =
    (none | ... | implied_flags<std::remove_cvref_t<As>>);

  // pick_action<Trait>(actions...) returns a reference to the first pack element
  // matching the trait, or a static `no_action` fallback if none does. Conflict
  // diagnostics elsewhere ensure at most one match.
  namespace _detail
  {
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
  }

  template<template<typename> class Trait, typename... As>
  constexpr auto& pick_action(As&... as)
  {
    if constexpr (sizeof...(As) == 0) return _detail::pick_action_fallback<Trait>();
    else return _detail::pick_action_impl<Trait>(as...);
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
} // namespace bnd

#endif // BNDpolicyflagHPP
