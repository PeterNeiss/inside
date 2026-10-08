// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Carry cascades with `wrap` and `on_wrap`: a field that overflows hands the
// carry to the next one up, so the cascade is one line per field.
//   1. A 24-hour clock: seconds → minutes → hours, wrapping at 24.
//   2. A calendar (30-day months for clarity): days → months → years, plus a
//      day of the week by `%` on a zero-free divisor (a plain value).

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

struct clock24 {
    using hour_t   = inside<{0, 23}, wrap>;
    using minute_t = inside<{0, 59}, wrap>;
    using second_t = inside<{0, 59}, wrap>;

    hour_t   hours{0};
    minute_t minutes{0};
    second_t seconds{0};

    clock24() = default;
    clock24(int h, int m, int s) : hours(h), minutes(m), seconds(s) {}

    void add_seconds(auto n) {
        seconds.on_wrap([&](auto, auto carry) { add_minutes(carry); }) += n;
    }

    void add_minutes(auto n) {
        minutes.on_wrap([&](auto, auto carry) { add_hours(carry); }) += n;
    }

    void add_hours(auto n) { hours += n; }

    friend std::ostream& operator<<(std::ostream& os, const clock24& c) {
        auto pad = [&](auto b) {
            if (b < 10)
                os << '0';
            os << b;
        };
        pad(c.hours);
        os << ':';
        pad(c.minutes);
        os << ':';
        pad(c.seconds);
        return os;
    }
};

// Days 1–30, months 1–12; the year takes the final carry.
struct date {
    inside<{1, 30}, wrap> day{1};
    inside<{1, 12}, wrap> month{1};
    inside<{1900, 2200}>  year{2000};

    void add_days(numeric auto n) {
        day.on_wrap([&](auto&, auto carry) { add_months(carry); }) += n;
    }
    void add_months(numeric auto n) {
        month.on_wrap([&](auto&, auto carry) { year += carry; }) += n;
    }

    friend std::ostream& operator<<(std::ostream& os, const date& d) {
        return os << d.year << '-' << (d.month < 10 ? "0" : "") << d.month << '-' << (d.day < 10 ? "0" : "") << d.day;
    }
};

int main() {
    clock24 t(23, 59, 45);
    std::cout << "start:      " << t << "\n";

    // Pass the delta as an inside (`_ins` literal): the `+=` is inside-RHS, so the wrap
    // carry threaded through the sec→min→hour cascade is itself an inside, not a raw
    // integer — the whole cascade stays in inside-space. (Large deltas land on a
    // disjoint sub-grid, which wrap accepts.)
    t.add_seconds(20_ins);
    std::cout << "+20 sec:    " << t << "\n";

    t.add_minutes(90_ins);
    std::cout << "+90 min:    " << t << "\n";

    t.add_seconds(just<3600 + 1800 + 30>);
    std::cout << "+5430 sec:  " << t << "\n";
    if (t.hours != 3 || t.minutes != 0 || t.seconds != 35)
        return 1;

    date d;
    d.day   = 15;
    d.month = 3;
    d.year  = 2026;
    std::cout << "\nstart:      " << d << "\n";
    d.add_days(20_ins);
    std::cout << "+20 days:   " << d << "\n";
    d.add_months(11_ins);
    std::cout << "+11 months: " << d << "\n";
    d.add_days(400_ins); // cascades through all three fields
    std::cout << "+400 days:  " << d << "\n";
    if (d.year != 2028 || d.month != 4 || d.day != 15)
        return 1;

    // Day of the week: `seven`'s grid excludes 0, so `%` cannot fail.
    using ordinal_t = inside<{0, 1'000'000}, snap>;
    constexpr inside<{1, 7}, snap> seven{7};
    const auto                     dow = ordinal_t{14003} % seven;
    std::cout << "day 14003 is weekday " << dow << " (0 = the reference day's)\n";
    if (dow != 3)
        return 1;
    return 0;
}
