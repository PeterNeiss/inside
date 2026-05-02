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
  // if we cannot guarantee success at compile time, fail compilation if ignore flag is not set
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
  inline static constexpr policy_flag ignore_round  {1ull << 4};
  inline static constexpr policy_flag round_nearest {(1ull << 5) | ignore_round};
  inline static constexpr policy_flag ignore_all
  {ignore_zero | ignore_domain | ignore_range | ignore_round};

  // runtime checking — opt-in
  inline static constexpr policy_flag checked{1ull << 34}; // enable runtime domain/overflow checks

  // unary — mutually exclusive
  inline static constexpr policy_flag clamp   {1ull << 32}; // saturate to boundary
  inline static constexpr policy_flag wrap    {1ull << 33}; // modular arithmetic
  inline static constexpr policy_flag sentinel{1ull << 35}; // overflow -> sentinel (nullopt)

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

} // namespace bnd

#endif // BNDpolicyflagHPP
