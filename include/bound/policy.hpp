//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpolicyHPP
#define BNDpolicyHPP

#include "bound/assignment.hpp"
#include "bound/overflow.hpp"
#include "bound/policy_flag.hpp"

#include <system_error>

namespace bnd
{
  //---------------------------------------------------------------------------
  // policy
  //---------------------------------------------------------------------------
  struct empty_ref{ };
  struct error_ref
  {
    constexpr error_ref(std::error_code& ec):Code{ec} {}
    std::error_code& Code;
  };

  template<policy_flag W = none, typename E = empty_ref>
  struct policy: E
  {
    constexpr policy() = default;
    constexpr policy(std::error_code& ec) requires std::same_as<E, error_ref>
    :E(ec) { }

    static constexpr bool test(policy_flag w)
    { return (W & w) == w; }

    static constexpr bool domain_check()
    {
      if consteval { return true; }
      return test(checked) && not test(ignore_domain);
    }

    static constexpr bool round_check()
    {
      if consteval { return true; }
      return test(checked) && not test(ignore_round);
    }

    void report(errc code, std::string what)
    {
      if constexpr (std::is_same_v<E, error_ref>)
        E::Code = E::Code ? E::Code : make_error_code(code);
      else
      {
#ifdef BOUND_HAS_STACKTRACE
        what += ": \n" + std::to_string(std::stacktrace::current());
#endif
        throw std::system_error(make_error_code(code), what);
      }
    }
  };

  policy(std::error_code&) -> policy<none, error_ref>;

  //---------------------------------------------------------------------------
  // is_policy_v — true for policy<F,E> specializations, false otherwise.
  // Used to gate free-fn overloads so they don't accidentally bind P = action tag.
  //---------------------------------------------------------------------------
  template<typename T>             inline constexpr bool is_policy_v = false;
  template<policy_flag F, typename E> inline constexpr bool is_policy_v<policy<F,E>> = true;

  // Concept form of is_policy_v — pulls cvref off so the constraint matches
  // forwarded `policy<F,E>` references in template parameters.
  template<typename T>
  concept policy_like = is_policy_v<std::remove_cvref_t<T>>;

  // True for policy specializations that carry an std::error_code& reference.
  // Free-fn arithmetic uses this to decide whether to call policy.report on
  // failure (which sets ec) vs. returning silent nullopt (no-arg form).
  template<typename T>             inline constexpr bool uses_error_ref_v = false;
  template<policy_flag F>          inline constexpr bool uses_error_ref_v<policy<F, error_ref>> = true;

  //---------------------------------------------------------------------------
  // make_policy
  //---------------------------------------------------------------------------
  template<policy_flag F = none>
  constexpr auto make_policy()
  { return policy<F,empty_ref>{}; }

  template<policy_flag F = none>
  constexpr auto make_policy(std::error_code& ec)
  { return policy<F,error_ref>{ec}; }

  //---------------------------------------------------------------------------
  // Named convenience policies — let user code skip `make_policy<F>()` entirely
  // for the per-call flag form on free arithmetic functions.
  //---------------------------------------------------------------------------
  inline constexpr auto truncated        = make_policy<ignore_round>();
  inline constexpr auto round_to_nearest = make_policy<round_nearest>();
  inline constexpr auto clamped          = make_policy<clamp>();
  inline constexpr auto wrapped          = make_policy<wrap>();

  //---------------------------------------------------------------------------
  // policy_ref
  //
  // Variadic in actions: stores a `std::tuple<As...>`. Single-action callers
  // (the bound::on_* methods) instantiate with a 1-element pack and aggregate-
  // init the tuple from the tag. The bound::with(...) entry point and the
  // free-fn pack overloads instantiate with N-element packs.
  //
  // assignment::assign and addition/multiplication/etc. keep their single-A
  // signatures unchanged; policy_ref pre-picks the appropriate action for each
  // call path. The compound-op payoff: the imax-probe stage and the narrowing
  // stage can each fire a different action (e.g. on_overflow + on_clamp).
  //---------------------------------------------------------------------------
  template<boundable B, typename P, typename... As>
  struct policy_ref
  {
    private:
    // Conflict diagnostics: at most one assignment-time tag (clamp / wrap /
    // sentinel / error), at most one of each kind, no clamp+wrap.
    static constexpr unsigned _clamp_count    = count_action_matches<is_clamp_action_pred,    As...>;
    static constexpr unsigned _wrap_count     = count_action_matches<is_wrap_action_pred,     As...>;
    static constexpr unsigned _sentinel_count = count_action_matches<is_sentinel_action_pred, As...>;
    static constexpr unsigned _error_count    = count_action_matches<is_error_action_pred,    As...>;
    static constexpr unsigned _overflow_count = count_action_matches<is_overflow_action_pred, As...>;

    static_assert(_clamp_count + _wrap_count + _sentinel_count + _error_count <= 1,
      "on_clamp / on_wrap / on_sentinel / on_error are mutually exclusive in a single policy_ref");
    static_assert(_clamp_count    <= 1, "duplicate on_clamp");
    static_assert(_wrap_count     <= 1, "duplicate on_wrap");
    static_assert(_sentinel_count <= 1, "duplicate on_sentinel");
    static_assert(_error_count    <= 1, "duplicate on_error");
    static_assert(_overflow_count <= 1, "duplicate on_overflow");

    public:
    B& Ref;
    P Policy;
    [[no_unique_address]] std::tuple<As...> Actions;

    private:
    // Pre-pick the assignment-time action that matches the policy_ref's pack,
    // and forward to the single-action assignment::assign. At most one of the
    // four assignment-time tags is in the pack (enforced by static_assert), so
    // exactly one branch fires; the rest fall through to no-action.
    template <numeric C>
    constexpr B& assign_with_picked(C const& other)
    {
      if constexpr (has_action<is_clamp_action_pred, As...>)
        return assignment<B, C>::assign(Ref, other, Policy,
          pick_action_in<is_clamp_action_pred>(Actions));
      else if constexpr (has_action<is_wrap_action_pred, As...>)
        return assignment<B, C>::assign(Ref, other, Policy,
          pick_action_in<is_wrap_action_pred>(Actions));
      else if constexpr (has_action<is_sentinel_action_pred, As...>)
        return assignment<B, C>::assign(Ref, other, Policy,
          pick_action_in<is_sentinel_action_pred>(Actions));
      else if constexpr (has_action<is_error_action_pred, As...>)
        return assignment<B, C>::assign(Ref, other, Policy,
          pick_action_in<is_error_action_pred>(Actions));
      else if constexpr (sizeof...(As) == 1
                         && !has_action<is_overflow_action_pred, As...>)
        // Legacy single-callable form (e.g. `s.policy<wrap>([](auto c){...})`)
        // — the action is a plain callable, not a tagged action; assignment's
        // apply_clamp / apply_wrap branches invoke it directly.
        return assignment<B, C>::assign(Ref, other, Policy, std::get<0>(Actions));
      else
        return assignment<B, C>::assign(Ref, other, Policy);
    }

    public:
    template <numeric C>
    constexpr B& operator=(C const& other)
    { return assign_with_picked(other); }

    private:
    constexpr void report_zero(errc code, const char* what)
    {
      if constexpr (has_action<is_error_action_pred, As...>)
        pick_action_in<is_error_action_pred>(Actions).fn(Ref, code, std::string_view(what));
      else if constexpr (!HasPolicy<B, P, ignore_zero>)
        Policy.report(code, what);
    }

    public:
    template <std::integral C>
    constexpr B& operator+=(C rhs)
    {
      imax l = to_value(Ref), r = static_cast<imax>(rhs), result;
      if constexpr (has_action<is_overflow_action_pred, As...>
                 || has_action<is_error_action_pred,    As...>)
      {
        if (add_overflow(l, r, &result))
        {
          if constexpr (has_action<is_overflow_action_pred, As...>)
            pick_action_in<is_overflow_action_pred>(Actions).fn(Ref, errc::overflow);
          else
            pick_action_in<is_error_action_pred>(Actions)
              .fn(Ref, errc::overflow, std::string_view("operator+= overflow"));
          return Ref;
        }
        return assign_with_picked(result);
      }
      else
        return assign_with_picked(static_cast<imax>(l + r));
    }

    template <std::integral C>
    constexpr B& operator-=(C rhs)
    {
      imax l = to_value(Ref), r = static_cast<imax>(rhs), result;
      if constexpr (has_action<is_overflow_action_pred, As...>
                 || has_action<is_error_action_pred,    As...>)
      {
        if (sub_overflow(l, r, &result))
        {
          if constexpr (has_action<is_overflow_action_pred, As...>)
            pick_action_in<is_overflow_action_pred>(Actions).fn(Ref, errc::overflow);
          else
            pick_action_in<is_error_action_pred>(Actions)
              .fn(Ref, errc::overflow, std::string_view("operator-= overflow"));
          return Ref;
        }
        return assign_with_picked(result);
      }
      else
        return assign_with_picked(static_cast<imax>(l - r));
    }

    template <std::integral C>
    constexpr B& operator*=(C rhs)
    {
      imax l = to_value(Ref), r = static_cast<imax>(rhs), result;
      if constexpr (has_action<is_overflow_action_pred, As...>
                 || has_action<is_error_action_pred,    As...>)
      {
        if (mul_overflow(l, r, &result))
        {
          if constexpr (has_action<is_overflow_action_pred, As...>)
            pick_action_in<is_overflow_action_pred>(Actions).fn(Ref, errc::overflow);
          else
            pick_action_in<is_error_action_pred>(Actions)
              .fn(Ref, errc::overflow, std::string_view("operator*= overflow"));
          return Ref;
        }
        return assign_with_picked(result);
      }
      else
        return assign_with_picked(static_cast<imax>(l * r));
    }

    template <std::integral C>
    constexpr B& operator/=(C rhs)
    {
      if (rhs == 0)
      {
        report_zero(errc::division_by_zero, "operator/= division by zero");
        return Ref;
      }
      return assign_with_picked(static_cast<imax>(to_value(Ref) / static_cast<imax>(rhs)));
    }

    template <std::integral C>
    constexpr B& operator%=(C rhs)
    {
      if (rhs == 0)
      {
        report_zero(errc::division_by_zero, "operator%= division by zero");
        return Ref;
      }
      return assign_with_picked(static_cast<imax>(to_value(Ref) % static_cast<imax>(rhs)));
    }
  };

} // namespace bnd

#endif // BNDpolicyHPP
