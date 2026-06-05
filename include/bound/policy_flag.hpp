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

  // Do all compile time only checks always
  // if we cannot guarantee success at compile time, fail compilation if ignore flag is not set
  // insert runtime checks, except we ignore the potential error
  // if a error is detected at runtime, we throw an exception by default
  // some function may provide a error_code parameter, which replaces the throw
  //
  // binary operations or the flags of both operations
  inline static constexpr policy_flag none         {0ull};
  inline static constexpr policy_flag ignore_zero  {1ull << 1};
  inline static constexpr policy_flag ignore_domain{1ull << 2};
  inline static constexpr policy_flag ignore_round  {1ull << 4};
  inline static constexpr policy_flag round_nearest {(1ull << 5) | ignore_round};
  // Additional rounding modes. Each picks a unique mode bit (6/7/8) and ORs
  // in `ignore_round` so users don't have to remember the combination. The
  // four rounding modes are *mutually exclusive in spirit* — combining two
  // is permitted by the type system but the dispatch order in assignment.hpp
  // picks the first match (nearest → floor → ceil → half_even → truncate).
  inline static constexpr policy_flag round_floor     {(1ull << 6) | ignore_round};
  inline static constexpr policy_flag round_ceil      {(1ull << 7) | ignore_round};
  inline static constexpr policy_flag round_half_even {(1ull << 8) | ignore_round};

  // runtime checking — opt-in
  inline static constexpr policy_flag checked{1ull << 34}; // enable runtime domain/overflow checks

  // unary — mutually exclusive
  inline static constexpr policy_flag clamp   {1ull << 32}; // saturate to boundary
  inline static constexpr policy_flag wrap    {1ull << 33}; // modular arithmetic
  inline static constexpr policy_flag sentinel{1ull << 35}; // overflow -> sentinel (nullopt)

  // opt-out of the default `checked` policy: no domain / round / overflow /
  // div-by-zero checks. Reading an out-of-range value is undefined behavior.
  // Division by zero is undefined behavior too — consistently across both
  // forms: compound `/= 0` / `%= 0` silently no-op (the bound is unchanged),
  // and binary `a / 0` / `a % 0` skip the zero check (the `ignore_zero` bit).
  // Includes `ignore_round` so notch-incompatible assigns compile and the
  // native-integer div/mod paths fire.
  inline static constexpr policy_flag unsafe
    {(1ull << 36) | ignore_domain | ignore_round | ignore_zero};

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

  // Action detection. The `*Pred` struct is the primary detector
  // (specialized on the tag type); the concept derives from it and strips
  // cvref so `clamp_action<on_clamp_t<F>&>` matches the same as the value form.
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
  // Pack helpers — let policy_ref / assignment / arithmetic accept Actions...
  // packs. The `*Pred` structs from above are reused here as template-template
  // parameters to `has_action` / `count_action_matches` / `pick_action`
  // (concepts can't be passed as template-template parameters in C++23).
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

} // namespace bnd

#endif // BNDpolicyflagHPP
