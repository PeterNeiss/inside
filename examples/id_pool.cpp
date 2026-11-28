// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A bounded ID allocator on std::unordered_set<inside>:
// std::hash and std::numeric_limits of an inside, values<T>.size() as the capacity,
// and conversion_overflows as a fail-fast gate for untrusted input.

#include <iostream>
#include <unordered_set>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/predicates.hpp>

using namespace ins;

using pool_id = id<"[0..999]">;

struct id_pool
{
  std::unordered_set<pool_id> allocated;
  static constexpr std::size_t capacity = values<pool_id>.size();

  bool allocate(int request)
  {
    // The gate at the trust boundary (IDs parsed from a wire format): reject before anything is stored.
    if (conversion_overflows<pool_id>(request))
    {
      std::cout << "  reject (out of range): " << request << "\n";
      return false;
    }
    // The gate checked it, so the store needs no check of its own: unsafe states that precondition.
    auto [_, inserted] = allocated.insert(pool_id{request, unsafe});
    if (!inserted)
      std::cout << "  reject (duplicate):  " << request << "\n";
    return inserted;
  }

  void release(int id)
  {
    if (auto m{pool_id::try_make(id)}; m)
      allocated.erase(*m);
  }
};

int main()
{
  id_pool pool;
  std::cout << "pool capacity: " << id_pool::capacity << " IDs"
            << "  (min=" << std::numeric_limits<pool_id>::min() << ", max=" << std::numeric_limits<pool_id>::max()
            << ")\n\n";

  int requests[] = {42, 7, 999, 42, 100, -1, 1000, 555}; // in range, duplicates, out of range
  std::cout << "allocation pass:\n";
  for (int r : requests)
    pool.allocate(r);

  std::cout << "\nallocated " << pool.allocated.size() << " IDs:\n";
  for (auto id : pool.allocated)
    std::cout << "  " << id << "\n";

  pool.release(42);
  std::cout << "\nafter release(42): " << pool.allocated.size() << " IDs\n";

  // try_make returns std::expected<inside, error>: the value, or why it failed. Nothing throws.
  auto bad {pool_id::try_make(2000)};
  auto good{pool_id::try_make(123)};
  std::cout << "\ntry_make(2000) -> " << (bad  ? to_string(*bad)  : error_message(bad.error()))  << "\n";
  std::cout << "try_make( 123) -> "   << (good ? to_string(*good) : error_message(good.error())) << "\n";
  return 0;
}
