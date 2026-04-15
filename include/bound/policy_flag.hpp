//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpolicyflagHPP
#define BNDpolicyflagHPP

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

  // unary — mutually exclusive
  inline static constexpr policy_flag clamp{1ull << 32}; // saturate to boundary
  inline static constexpr policy_flag wrap {1ull << 33}; // modular arithmetic

  //---------------------------------------------------------------------------
  // no_action — zero-overhead default for overflow callbacks
  //---------------------------------------------------------------------------
  struct no_action {};

} // namespace bnd

#endif // BNDpolicyflagHPP
