//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDwaiverHPP
#define BNDwaiverHPP

namespace bnd
{
  namespace waive
  {
    struct type
    { 
      unsigned long long Waivers;

      constexpr bool test(type w) const 
      { return (Waivers & w.Waivers) == w.Waivers; }

      constexpr explicit operator bool() const { return Waivers != 0; }
      constexpr friend bool operator==(type, type) = default;

      constexpr friend type operator|(type a, type b) 
      { return {a.Waivers | b.Waivers}; }

      constexpr friend type operator&(type a, type b) 
      { return {a.Waivers & b.Waivers}; }
    };

    inline static constexpr type none  {0ull};
    inline static constexpr type no_runtime_check  {1ull << 0};
    inline static constexpr type change_return_type {1ull << 1};
  }; // namespace waive
  
  using waiver = waive::type;
} // namespace bnd

#endif // BNDwaiverHPP
