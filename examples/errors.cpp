// Error handling strategies: throw, error_code, optional.

#include <iostream>

#include "bound/bound.hpp"

using namespace bnd;

int main()
{
  // 1. Default: throws on out-of-range
  try
  {
    bound<{0, 100}> x = 200;
    (void)x;
  }
  catch (std::system_error& e)
  {
    std::cout << "throw:    " << e.code().message() << std::endl;
  }

  // 2. Error code: no exception, error reported via ec
  std::error_code ec;
  bound<{0, 100}> y(200, make_policy(ec));
  std::cout << "ec:       " << (ec ? ec.message() : "no error") << std::endl;

  // 3. Per-operation error code
  ec.clear();
  bound<{0, 100}> z(50);
  z.policy(ec) = 200;
  std::cout << "policy ec:" << (ec ? ec.message() : "no error")
            << " (z=" << z << ")" << std::endl;

  // 4. Optional: returns nullopt on failure
  auto maybe = bound<{0, 100}>::try_make(200);
  std::cout << "optional: " << (maybe.has_value() ? "has value" : "nullopt") << std::endl;

  auto ok = bound<{0, 100}>::try_make(50);
  std::cout << "ok:       " << *ok << std::endl;

  // 5. Ignore domain: skips the check entirely
  bound<{0, 100}> w(50);
  w.policy<ignore_domain>() = 200;
  std::cout << "ignored:  raw=" << w.Raw << " (domain check skipped)" << std::endl;

  return 0;
}
