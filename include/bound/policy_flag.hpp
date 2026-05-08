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

  // runtime checking — opt-in
  inline static constexpr policy_flag checked{1ull << 34}; // enable runtime domain/overflow checks

  // unary — mutually exclusive
  inline static constexpr policy_flag clamp   {1ull << 32}; // saturate to boundary
  inline static constexpr policy_flag wrap    {1ull << 33}; // modular arithmetic
  inline static constexpr policy_flag sentinel{1ull << 35}; // overflow -> sentinel (nullopt)

  // opt-out of the default `checked` policy: no domain / round / overflow
  // checks. Division-by-zero is still reported (use `ignore_zero` to suppress
  // that too). Includes `ignore_round` so notch-incompatible assigns compile and
  // native-integer div/mod paths fire.
  inline static constexpr policy_flag unsafe
    {(1ull << 36) | ignore_domain | ignore_round};

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
  template<typename F> constexpr auto on_clamp(F&& fn)
  { return on_clamp_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> constexpr auto on_wrap(F&& fn)
  { return on_wrap_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> constexpr auto on_error(F&& fn)
  { return on_error_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> constexpr auto on_sentinel(F&& fn)
  { return on_sentinel_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> constexpr auto on_overflow(F&& fn)
  { return on_overflow_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }

  template<typename T> inline constexpr bool is_clamp_action    = false;
  template<typename F> inline constexpr bool is_clamp_action<on_clamp_t<F>> = true;
  template<typename T> inline constexpr bool is_wrap_action     = false;
  template<typename F> inline constexpr bool is_wrap_action<on_wrap_t<F>> = true;
  template<typename T> inline constexpr bool is_error_action    = false;
  template<typename F> inline constexpr bool is_error_action<on_error_t<F>> = true;
  template<typename T> inline constexpr bool is_sentinel_action = false;
  template<typename F> inline constexpr bool is_sentinel_action<on_sentinel_t<F>> = true;
  template<typename T> inline constexpr bool is_overflow_action = false;
  template<typename F> inline constexpr bool is_overflow_action<on_overflow_t<F>> = true;

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
  // packs without rewriting every is_*_action<plain<A>> check.
  //
  // Class-template wrappers around the variable-template traits so they can be
  // passed as template-template parameters. The trait wrappers stay private to
  // pack helpers; existing variable-template `is_*_action<T>` keep working.
  //---------------------------------------------------------------------------
  template<typename T> struct is_clamp_action_pred    : std::bool_constant<is_clamp_action<T>>    {};
  template<typename T> struct is_wrap_action_pred     : std::bool_constant<is_wrap_action<T>>     {};
  template<typename T> struct is_error_action_pred    : std::bool_constant<is_error_action<T>>    {};
  template<typename T> struct is_sentinel_action_pred : std::bool_constant<is_sentinel_action<T>> {};
  template<typename T> struct is_overflow_action_pred : std::bool_constant<is_overflow_action<T>> {};

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
