//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDwaiverHPP
#define BNDwaiverHPP

namespace bnd
{
  using waiver_flag = unsigned long long;

  inline static constexpr waiver_flag none  {0ull};
  inline static constexpr waiver_flag casting{1ull};
  inline static constexpr waiver_flag no_runtime_check{1ull << 1};

  template<waiver_flag W = none>
  struct waiver_type
  {
    static constexpr bool test(waiver_flag w) 
    { return W & w; }

    constexpr explicit operator bool() const { return W != 0; }
  };

  template<waiver_flag W>
  inline constexpr waiver_type<W> waiver;


  //inline static constexpr type no_runtime_check  {1ull << 0};
  //inline static constexpr type change_return_type {1ull << 1};
  
} // namespace bnd

#endif // BNDwaiverHPP
