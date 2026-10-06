// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The replaceable failure handler (beman/inside/detail/debug.hpp). A checked violation
// funnels through beman::inside::detail::raise -> the installed handler. By default that
// throws beman::inside::inside_error carrying the originating errc; set_error_handler swaps
// in a user handler (e.g. a bare-metal reset/log) without any <system_error>
// dependency. These tests cover both the default and a custom handler.

#include <beman/inside/inside.hpp>
#include <beman/inside/casts.hpp>

#include <gtest/gtest.h>
#include <string_view>
#include <limits>

using namespace beman::inside;

namespace {
// A custom [[noreturn]] handler that records the code, then escapes via a
// throw of its own type so the test can observe it (a f64 bare-metal handler
// would reset/halt instead).
struct handler_fired {
    errc        code;
    const char* what;
};
errc g_seen{};

[[noreturn]] void recording_handler(errc code, const char* what) {
    g_seen = code;
    throw handler_fired{code, what};
}

// RAII guard so a thrown handler still restores the default.
struct scoped_handler {
    error_handler_t prev;
    explicit scoped_handler(error_handler_t h) : prev(set_error_handler(h)) {}
    ~scoped_handler() { set_error_handler(prev); }
};
} // namespace

// default handler throws inside_error carrying the code / overflow on out-of-range assignment
TEST(ErrorHandlerTest, default_handler_throws_inside_error_carrying_the_code__overflow_on_out_of_range_assignment) {
    using c100 = inside<{0, 100}, checked>;

    {
        SCOPED_TRACE("overflow on out-of-range assignment");
        try {
            c100 x{200};
            (void)x;
            FAIL() << "expected throw";
        } catch (const inside_error& e) {
            ASSERT_EQ(e.Code, errc::overflow);
        }
    }

    // what() defaults to the static category message.
    try {
        c100 x{200};
        (void)x;
    } catch (const inside_error& e) {
        ASSERT_EQ(std::string_view{e.what()}, errc_message(errc::overflow));
    }
}

// default handler throws inside_error carrying the code / rounding_error on off-notch checked cast
TEST(ErrorHandlerTest,
     default_handler_throws_inside_error_carrying_the_code__rounding_error_on_off_notch_checked_cast) {
    using c100 = inside<{0, 100}, checked>;

    {
        SCOPED_TRACE("rounding_error on off-notch checked cast");
        using coarse = inside<{{0, 10}, 2}>; // notch 2: 3 doesn't land
        try {
            (void)checked_cast<coarse>(3);
            FAIL() << "expected throw";
        } catch (const inside_error& e) {
            ASSERT_EQ(e.Code, errc::rounding_error);
        }
    }

    // what() defaults to the static category message.
    try {
        c100 x{200};
        (void)x;
    } catch (const inside_error& e) {
        ASSERT_EQ(std::string_view{e.what()}, errc_message(errc::overflow));
    }
}

// default handler throws inside_error carrying the code / not_finite on non-finite f64 input
TEST(ErrorHandlerTest, default_handler_throws_inside_error_carrying_the_code__not_finite_on_non_finite_f64_input) {
    using c100 = inside<{0, 100}, checked>;

    {
        SCOPED_TRACE("not_finite on non-finite f64 input");
        using R = inside<{0.0, 1.0}, f64>;
        try {
            R r{std::numeric_limits<double>::infinity()};
            (void)r;
            FAIL() << "expected throw";
        } catch (const inside_error& e) {
            ASSERT_EQ(e.Code, errc::not_finite);
        }
    }

    // what() defaults to the static category message.
    try {
        c100 x{200};
        (void)x;
    } catch (const inside_error& e) {
        ASSERT_EQ(std::string_view{e.what()}, errc_message(errc::overflow));
    }
}

// set_error_handler redirects failures and is restorable
TEST(ErrorHandlerTest, set_error_handler_redirects_failures_and_is_restorable) {
    using c100 = inside<{0, 100}, checked>;

    error_handler_t before = get_error_handler();
    {
        scoped_handler guard{&recording_handler};
        ASSERT_EQ(get_error_handler(), &recording_handler);

        g_seen = errc{};
        try {
            c100 x{200};
            (void)x;
        } catch (const handler_fired& f) {
            ASSERT_EQ(f.code, errc::overflow);
        }
        ASSERT_EQ(g_seen, errc::overflow);
    }
    // Default restored after the guard.
    ASSERT_EQ(get_error_handler(), before);
    ASSERT_THROW((void)((c100{200})), inside_error);

    // A null handler restores the default too.
    set_error_handler(&recording_handler);
    set_error_handler(nullptr);
    ASSERT_EQ(get_error_handler(), before);
}
