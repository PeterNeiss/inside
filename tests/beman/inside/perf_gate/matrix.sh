#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# A commit's Debug matrix, apart from the working tree: check <commit> out in
# a worktree (build/perf-gate/wt) and build and test it twice, Debug with
# -Werror and the examples, and the same with BEMAN_INSIDE_MATH_NO_FP. Logs go
# to build/perf-gate/out/mx-{dbg,nofp}.*.log; the last line sums them up.
#
# Usage: matrix.sh <commit>
# googletest comes from build/p26's FetchContent; set GTEST_SOURCE when it
# is elsewhere.
R=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
G=$R/build/perf-gate
GT=${GTEST_SOURCE:-$R/build/p26/_deps/googletest-src}
mkdir -p "$G/out"
[ -d $G/wt ] || git -C "$R" worktree add -q --detach $G/wt "$1"
cd $G/wt && git checkout -q --detach "$1" || exit 1
for cfg in dbg:OFF nofp:ON; do
  name=${cfg%%:*}; nofp=${cfg##*:}; B=$G/mx-$name
  [ -f $B/CMakeCache.txt ] || cmake -S $G/wt -B $B -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-16 \
     -DCMAKE_CXX_STANDARD=26 -DBEMAN_INSIDE_BUILD_EXAMPLES=ON -DBEMAN_INSIDE_WARNINGS_AS_ERRORS=ON \
     -DBEMAN_INSIDE_MATH_NO_FP=$nofp -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$GT \
     -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=$G/wt/infra/cmake/use-fetch-content.cmake > $G/out/mx-$name.cfg.log 2>&1
  cmake --build $B -j4 > $G/out/mx-$name.build.log 2>&1; echo "build exit $?" >> $G/out/mx-$name.build.log
  ctest --test-dir $B -j4 --output-on-failure > $G/out/mx-$name.test.log 2>&1; echo "ctest exit $?" >> $G/out/mx-$name.test.log
done
echo "matrix $1 done: $(tail -1 $G/out/mx-dbg.build.log) / $(grep -E 'tests passed|tests failed' $G/out/mx-dbg.test.log) | nofp $(tail -1 $G/out/mx-nofp.build.log) / $(grep -E 'tests passed|tests failed' $G/out/mx-nofp.test.log)"
