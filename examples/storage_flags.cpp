// Fixed-width storage flags from <bound/policy_flag.hpp>.
//
// By default a bound picks the *smallest* raw integer that fits its grid. The
// width flags i8/u8/i16/u16/i32/u32/i64/u64 let you pin the exact backing type
// instead — for a fixed wire/register layout — and the library checks at compile
// time that the type is actually big enough. A bare width flag stores the value
// directly (raw == value); add `indexed` to store the 0-based notch index.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"        // to_string / to_string_debug (value + raw + type)

using namespace bnd;

int main()
{
  // 1. Deduced vs pinned width. {0,100} fits a uint8_t, but a fixed protocol
  //    field may need a full 16-bit slot — pin it with `u16`.
  using deduced = bound<{0, 100}>;            // raw: uint8_t (smallest fit)
  using field   = bound<{0, 100}, u16>;       // raw: uint16_t (pinned)
  static_assert(sizeof(deduced) == 1 && sizeof(field) == 2);
  std::cout << "deduced: " << to_string_debug(deduced{42}) << "\n";
  std::cout << "pinned : " << to_string_debug(field{42})   << "\n";

  // 2. Value storage (the default for a bare width flag): raw() IS the value,
  //    not a 0-based offset — handy when the raw bytes go straight onto the wire.
  using reg = bound<{5, 100}, u8>;            // raw uint8_t holds 5..100 directly
  reg r{42};
  std::cout << "value storage: reg{42}.raw() == " << int(r.raw()) << "\n";
  std::cout << "  (deduced {5,100} would store index "
            << int(bound<{5, 100}>{42}.raw()) << ")\n";

  // 3. Signed widths hold negatives directly.
  using temp = bound<{-40, 85}, i8>;          // raw int8_t
  temp t{-12};
  std::cout << "signed storage: temp{-12}.raw() == " << int(t.raw()) << "\n";

  // 4. A notched grid can't store its value as an integer, so pair the width
  //    flag with `indexed` to pin the type for the 0-based notch index.
  using level = bound<{{0, 1}, notch<1, 256>}, u16 | indexed>;
  level half{0.5_b};
  std::cout << "indexed storage: level{0.5}.raw() == " << half.raw()
            << " (value " << to_string(half) << ")\n";

  // 5. The capacity check is a hard compile error — no silent widening. Each of
  //    these is ill-formed (uncomment to see the static_assert fire):
  //
  //    bound<{0, 100000}, u8>            // value range overflows uint8
  //    bound<{-200, 0}, u8>              // unsigned type can't hold negatives
  //    bound<{0, 100}, u8 | u16>         // more than one width flag
  //    bound<{{0,1},notch<1,256>}, u16>  // value storage needs Notch == 1

  return 0;
}
