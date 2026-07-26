// Hazard 6: integral promotion. Small types do not stay small, `char`
// signedness is implementation-defined, and the result type of an
// expression is rarely the type of its operands.

#include <cstdint>
#include <cstdio>
#include <type_traits>

int main()
{
  std::uint8_t a = 200;
  std::uint8_t b = 200;

  // Both operands are uint8_t; the product is not.
  std::printf("uint8 * uint8 -> int? %s\n",
              std::is_same_v<decltype(a * b), int> ? "yes" : "no");
  std::printf("200 * 200          = %d\n", a * b);
  std::printf("stored back in u8  = %u   <- truncated\n",
              static_cast<std::uint8_t>(a * b));

  // char signedness is not specified by the standard.
  std::printf("char is signed?    %s (platform-dependent)\n",
              std::is_signed_v<char> ? "yes" : "no");

  // Widths are minimums, not guarantees.
  std::printf("sizeof(int)        = %zu bytes here; the standard promises >= 2\n",
              sizeof(int));
  return 0;
}
