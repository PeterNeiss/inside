#include <format>
#include <iostream>

int main()
{
  std::cout << std::format("{} {}\n", "bound", "test");
  return 0;
}
