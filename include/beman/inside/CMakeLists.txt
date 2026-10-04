# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

target_sources(
    beman.inside
    PUBLIC
        FILE_SET HEADERS
            FILES
                arithmetic.hpp
                casts.hpp
                cmath_double.hpp
                cmath_float.hpp
                cmath.hpp
                core.hpp
                detail/addition.hpp
                detail/assignment.hpp
                detail/debug.hpp
                detail/division.hpp
                detail/multiplication.hpp
                detail/overflow.hpp
                detail/rational.hpp
                detail/rep.hpp
                formats.hpp
                generic.hpp
                grid.hpp
                inside.hpp
                interval.hpp
                io.hpp
                lift.hpp
                math.hpp
                numeric_limits.hpp
                policy_flag.hpp
                policy.hpp
                predicates.hpp
                random.hpp
                range.hpp
)
