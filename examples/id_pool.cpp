// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Bounded ID allocator backed by std::unordered_set<inside>.
//
// Demonstrates:
//   - `std::hash<inside>` integration (via numeric_limits.hpp)
//   - `std::numeric_limits<inside>::max()` capacity query
//   - `try_make` for guarded construction from untrusted input
//   - Conversion predicate `conversion_overflows` to fail fast

#include <iostream>
#include <unordered_set>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/numeric_limits.hpp>
#include <beman/inside/predicates.hpp>

using namespace beman::inside;

using pool_id = inside<{0, 999}>;

struct id_pool {
    std::unordered_set<pool_id> allocated;
    static constexpr imax       capacity =
        imax{std::numeric_limits<pool_id>::max()} - imax{std::numeric_limits<pool_id>::min()} + 1;

    bool allocate(int request) {
        // Bail before construction if the request is out of range — exposes
        // the conversion_overflows predicate as a fail-fast gate at the
        // trust boundary (e.g. parsing IDs from a wire format).
        if (conversion_overflows<pool_id>(request)) {
            std::cout << "  reject (out of range): " << request << "\n";
            return false;
        }
        auto maybe = pool_id::try_make(request);
        if (!maybe)
            return false;
        auto [_, inserted] = allocated.insert(*maybe);
        if (!inserted)
            std::cout << "  reject (duplicate):  " << request << "\n";
        return inserted;
    }

    void release(int id) {
        if (auto m = pool_id::try_make(id); m)
            allocated.erase(*m);
    }
};

int main() {
    id_pool pool;
    std::cout << "pool capacity: " << id_pool::capacity << " IDs"
              << "  (min=" << std::numeric_limits<pool_id>::min() << ", max=" << std::numeric_limits<pool_id>::max()
              << ")\n\n";

    // Mixed input: in-range, duplicates, out-of-range.
    int requests[] = {42, 7, 999, 42, 100, -1, 1000, 555};
    std::cout << "allocation pass:\n";
    for (int r : requests)
        pool.allocate(r);

    std::cout << "\nallocated " << pool.allocated.size() << " IDs:\n";
    for (auto id : pool.allocated)
        std::cout << "  " << id << "\n";

    pool.release(42);
    std::cout << "\nafter release(42): " << pool.allocated.size() << " IDs\n";

    // try_make returns std::expected<inside, errc>: a value on success, the
    // failure reason on out-of-range / rounding / overflow inputs. Either way
    // no exception is thrown.
    auto bad  = pool_id::try_make(2000);
    auto good = pool_id::try_make(123);
    std::cout << "\ntry_make(2000) -> " << (bad ? "has value" : errc_message(bad.error())) << "\n";
    std::cout << "try_make( 123) -> " << (good ? "has value" : errc_message(good.error())) << "\n";

    return 0;
}
