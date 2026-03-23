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
  inline static constexpr waiver_flag round{1ull << 1};
  inline static constexpr waiver_flag wrap{1ull << 1};
  inline static constexpr waiver_flag saturate{1ull << 1};

  template<waiver_flag W = none>
  struct waiver_type
  {
    static constexpr bool test(waiver_flag w) 
    { return W & w; }

    constexpr explicit operator bool() const { return W != 0; }
  };

  template<waiver_flag W>
  inline constexpr waiver_type<W> waiver;
} // namespace bnd

#endif // BNDwaiverHPP
