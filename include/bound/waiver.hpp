//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDwaiverHPP
#define BNDwaiverHPP

namespace bnd
{
  using waiver_flag = unsigned long long;

  // Do all compile time only checks always
  // if we cannot guarantee success at compile time
  // insert runtime checks, except we ignore the potential error
  // if a error is detected at runtime, we throw an exception by default
  // some function may provide a error_code parameter, which replaces the throw
  //
  // binary operations or the flags of both operations
  inline static constexpr waiver_flag none         {0ull};
  inline static constexpr waiver_flag fail_early   {1ull << 0}; // possible fail -> compile time fail
  inline static constexpr waiver_flag ignore_zero  {1ull << 1};
  inline static constexpr waiver_flag ignore_domain{1ull << 2};
  inline static constexpr waiver_flag ignore_range {1ull << 3};
  inline static constexpr waiver_flag ignore_round {1ull << 4};
  inline static constexpr waiver_flag ignore_all   
  {ignore_zero | ignore_domain | ignore_range | ignore_round};

  // unary  
  inline static constexpr waiver_flag saturate{1ull << 32}; // only for assignment, construction 
  inline static constexpr waiver_flag wrap    {1ull << 33}; // only for assignment, construction

  template<waiver_flag W = none>
  struct waiver_type
  {
    //TODO conditionally make member: error_code* eptr{nullptr};
    static constexpr bool test(waiver_flag w) 
    { return W & w; }

    static constexpr bool domain_check()
    {
      if consteval { return true; }
      return not test(ignore_domain);
    }

    [[noreturn]] void domain_error(std::string const& what)
    { throw std::system_error(EDOM, std::generic_category(), what ); }

    constexpr explicit operator bool() const { return W != 0; }

  };

  //TODO CTAD waiver from error_code object

  template<waiver_flag W>
  inline constexpr waiver_type<W> waiver;
} // namespace bnd

#endif // BNDwaiverHPP
