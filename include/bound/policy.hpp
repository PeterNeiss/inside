//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpolicyHPP
#define BNDpolicyHPP

#include "bound/print.hpp"
#include "bound/assignment.hpp"

#include <system_error>

namespace bnd
{
  //---------------------------------------------------------------------------
  // policy_flag
  //---------------------------------------------------------------------------
  using policy_flag = unsigned long long;

  // Do all compile time only checks always
  // if we cannot guarantee success at compile time
  // insert runtime checks, except we ignore the potential error
  // if a error is detected at runtime, we throw an exception by default
  // some function may provide a error_code parameter, which replaces the throw
  //
  // binary operations or the flags of both operations
  inline static constexpr policy_flag none         {0ull};
  inline static constexpr policy_flag fail_early   {1ull << 0}; // possible fail -> compile time fail
  inline static constexpr policy_flag ignore_zero  {1ull << 1};
  inline static constexpr policy_flag ignore_domain{1ull << 2};
  inline static constexpr policy_flag ignore_range {1ull << 3};
  inline static constexpr policy_flag ignore_round {1ull << 4};
  inline static constexpr policy_flag ignore_all
  {ignore_zero | ignore_domain | ignore_range | ignore_round};

  // unary
  inline static constexpr policy_flag clamp{1ull << 32}; // only for assignment, construction
  inline static constexpr policy_flag wrap {1ull << 33}; // only for assignment, construction

  //---------------------------------------------------------------------------
  // policy
  //---------------------------------------------------------------------------
  struct handler
  {
    // domain
    using domain_error_t = void (*)(const char*);
    using domain_error_ec_t = void (*)(std::error_code& ec);

    static void default_domain_error(const char* what)
    {
      std::string str{what};
#ifdef BOUND_HAS_STACKTRACE
      str += ": \n" + std::to_string(std::stacktrace::current());
#endif
      throw std::system_error(EDOM, std::generic_category(), str);
    }

    static void default_domain_error_ec(std::error_code& ec)
    { ec = ec ? ec : std::error_code{EDOM, std::generic_category()}; }

    domain_error_t domain_error = default_domain_error;
    domain_error_ec_t domain_error_ec = default_domain_error_ec;
  };

  struct empty_ref
  {
  };
  struct error_ref
  {
    constexpr error_ref(std::error_code& ec):Code{ec} {}
    std::error_code& Code;
  };

  template<policy_flag W = none, typename E = empty_ref, handler H = handler{}>
  struct policy: E
  {
    constexpr policy() = default;
    constexpr policy(std::error_code& ec) requires std::same_as<E, error_ref>
    :E(ec) { }

    //TODO conditionally make member: error_code* eptr{nullptr};
    static constexpr bool test(policy_flag w)
    { return W & w; }

    void set_error(std::error_code ec)
    {
      if constexpr (std::is_same_v<E, error_ref>)
      { E::Code = E::Code ? E::Code : ec; } //only replace success
    }

    static constexpr bool domain_check()
    {
      if consteval { return true; }
      return not test(ignore_domain);
    }

    static constexpr bool round_check()
    {
      if consteval { return true; }
      return not test(ignore_round);
    }

    void domain_error(std::string what)
    {
      if constexpr (std::is_same_v<E, error_ref>)
       H.domain_error_ec(E::Code);
      else
       H.domain_error(what.c_str());
    }

    void round_error(std::string what)
    {
      if constexpr (std::is_same_v<E, error_ref>)
      { set_error({ENOSPC, std::generic_category()}); }
      else
      {
#ifdef BOUND_HAS_STACKTRACE
        what += ": \n" + std::to_string(std::stacktrace::current());
#endif
        throw std::system_error(ENOSPC, std::generic_category(), what); }
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
  template<boundable B, typename P>
  struct policy_ref
  {
    B& Ref;
    P Policy;

    template <numeric C>
    constexpr B& operator=(C const& other)
    { return assignment<B, C>::assign(Ref, other, Policy); }
  };

} // namespace bnd

#endif // BNDpolicyHPP
