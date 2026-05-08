// Error handling strategies: throw, error_code, optional.
// Use the `checked` policy flag to enable runtime domain checks.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

using checked_100 = bound<{0, 100}, checked>;

int main()
{
  // 1. Checked: throws on out-of-range
  try
  {
    checked_100 x = 200;
    (void)x;
  }
  catch (std::system_error& e)
  {
    std::cout << "throw:    " << e.code().message() << std::endl;
  }

  // 2. Error code: no exception, error reported via ec
  std::error_code ec;
  checked_100 y(200, ec);
  std::cout << "ec:       " << (ec ? ec.message() : "no error") << std::endl;

  // 3. Per-operation error code
  ec.clear();
  checked_100 z(50);
  z.policy(ec) = 200;
  std::cout << "policy ec:" << (ec ? ec.message() : "no error")
            << " (z=" << z << ")" << std::endl;

  // 4. Per-operation on_error: lambda runs instead of throw / ec, may overwrite
  checked_100 r(50);
  r.on_error([](auto& self, errc, std::string_view msg) {
    std::cout << "on_error: " << msg << " (recover -> 0)" << std::endl;
    self = 0;
  }) = 200;
  std::cout << "         (r=" << r << ")" << std::endl;

  // 5. Optional: returns nullopt on failure
  auto maybe = checked_100::try_make(200);
  std::cout << "optional: " << (maybe.has_value() ? "has value" : "nullopt") << std::endl;

  auto ok = checked_100::try_make(50);
  std::cout << "ok:       " << *ok << std::endl;

  // 5. Unchecked: no runtime domain check (default)
  bound<{0, 100}> w(50);
  w.policy<ignore_domain>() = 200;
  std::cout << "ignored:  raw=" << w << " (domain check skipped)" << std::endl;

  return 0;
}
