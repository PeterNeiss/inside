// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// beman.inside.differential: prints random cases of every family for
// check.py, which runs it.
//   beman.inside.differential <seed> <n>      n cases per grid and family

#include "dump.hpp"

#include <string>

int main(int argc, char** argv) {
    const std::uint64_t seed = argc > 1 ? std::stoull(argv[1]) : 1;
    const int           n    = argc > 2 ? std::stoi(argv[2]) : 200;
    diff::rng_t         g{seed};
    std::cout.sync_with_stdio(false);
    diff::dump_arith(g, n);
    diff::dump_round(g, n);
    diff::dump_bounded(g, n);
    diff::dump_float(g, n);
    diff::dump_pi(g, n);
    return 0;
}
