//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpolicyHPP
#define BNDpolicyHPP

#include <system_error>

namespace bnd
{
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

  struct empty_ref
  { 
  };
  struct error_ref
  {
    error_ref(std::error_code& ec):Code{ec} {}
    std::error_code& Code;
  };
 
  template<policy_flag W = none, typename E = empty_ref>
  struct policy: E 
  {
    policy() = default;
    policy(std::error_code& ec) requires std::same_as<E, error_ref>
    :E(ec) { }

    //TODO conditionally make member: error_code* eptr{nullptr};
    static constexpr bool test(policy_flag w) 
    { return W & w; }

    static constexpr bool domain_check()
    {
      if consteval { return true; }
      return not test(ignore_domain);
    }

    void domain_error(std::string const& what) const
    { 
      if constexpr (std::is_same_v<E, error_ref>)
      { E::Code = std::error_code{EDOM, std::generic_category()}; }
      else
        throw std::system_error(EDOM, std::generic_category(), what); 
    }
  };

  template<policy_flag F = none>
  policy<F,empty_ref> make_policy() { return {}; }

  template<policy_flag F = none>
  policy<F, error_ref> make_policy(std::error_code& ec) { return {ec}; }

} // namespace bnd

#endif // BNDwaiverHPP
