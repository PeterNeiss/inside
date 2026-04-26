//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpolicyHPP
#define BNDpolicyHPP

#include "bound/assignment.hpp"
#include "bound/policy_flag.hpp"

#include <system_error>

namespace bnd
{
  //---------------------------------------------------------------------------
  // policy
  //---------------------------------------------------------------------------
  struct empty_ref
  {
  };
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
    { return W & w; }

    static constexpr bool domain_check()
    {
      if consteval { return true; }
      return test(checked) && not test(ignore_domain);
    }

    void report(errc code, std::string what)
    {
      if constexpr (std::is_same_v<E, error_ref>)
      {
        E::Code = E::Code ? E::Code : make_error_code(code);
      }
      else
      {
#ifdef BOUND_HAS_STACKTRACE
        what += ": \n" + std::to_string(std::stacktrace::current());
#endif
        throw std::system_error(make_error_code(code), what);
      }
    }
  };

  //---------------------------------------------------------------------------
  // make_policy
  //---------------------------------------------------------------------------
  template<policy_flag F = none>
  constexpr auto make_policy()
  {
    return policy<F,empty_ref>{};
  }

  template<policy_flag F = none>
  constexpr auto make_policy(std::error_code& ec)
  {
    return policy<F,error_ref>{ec};
  }

  //---------------------------------------------------------------------------
  // policy_ref
  //---------------------------------------------------------------------------
  template<boundable B, typename P, typename A = no_action>
  struct policy_ref
  {
    B& Ref;
    P Policy;
    [[no_unique_address]] A Action;

    template <numeric C>
    constexpr B& operator=(C const& other)
    { return assignment<B, C>::assign(Ref, other, Policy, Action); }

    template <arithmetic C>
    constexpr B& operator+=(C rhs)
    {
      return assignment<B, imax>::assign(Ref,
        to_value(Ref) + static_cast<imax>(rhs), Policy, Action);
    }
  };

} // namespace bnd

#endif // BNDpolicyHPP
