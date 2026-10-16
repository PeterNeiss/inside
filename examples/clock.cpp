// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Carry cascades with `wrap` and `on_wrap`: a field that overflows hands the
// carry to the next one up, so the cascade is one line per field.
//   1. A 24-hour clock: seconds → minutes → hours, wrapping at 24. It converts
//      to and from std::chrono (chrono.hpp's from_duration / to_duration), and
//      the cascade is checked against chrono's own hh_mm_ss.
//   2. A calendar (30-day months for clarity): days → months → years, plus a
//      day of the week by `%` on a zero-free divisor (a plain value), checked
//      against std::chrono::weekday.

#include <array>
#include <chrono>
#include <iostream>

#include <beman/inside/chrono.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

struct clock24 {
    using hour_t   = inside<{0, 24, bounds::half_open}, wrap>;
    using minute_t = inside<{0, 60, bounds::half_open}, wrap>;
    using second_t = inside<{0, 60, bounds::half_open}, wrap>;

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

    // A time of day from std::chrono: whole days wrap away (the `wrap` flag on
    // the store), then one divmod per field.
    using day_seconds_t = inside<{0, 86400, bounds::half_open}>;
    static clock24 from(std::chrono::seconds since_midnight) {
        const day_seconds_t s = from_duration<day_seconds_t, wrap>(since_midnight).value();
        const auto [h, rest]  = divmod(s, 3600_ins);
        const auto [m, sec]   = divmod(rest, 60_ins);
        clock24 c;
        c.hours   = h;
        c.minutes = m;
        c.seconds = sec;
        return c;
    }

    // …and back: the sum's grid is [0, 86399] on whole seconds, so the
    // conversion cannot fail and returns a plain std::chrono::seconds.
    [[nodiscard]] std::chrono::seconds to_chrono() const {
        return to_duration<std::chrono::seconds>(hours * 3600_ins + minutes * 60_ins + seconds);
    }

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

    // The same steps in std::chrono: the cascade agrees with hh_mm_ss, and the
    // clock converts both ways.
    {
        using namespace std::chrono;
        const seconds  end = (23h + 59min + 45s + 20s + 90min + 5430s) % days{1};
        const hh_mm_ss hms{end};
        if (t.to_chrono() != end || t.hours != hms.hours().count() || t.minutes != hms.minutes().count() ||
            t.seconds != hms.seconds().count())
            return 1;
        const clock24 back = clock24::from(t.to_chrono());
        if (back.hours != t.hours || back.minutes != t.minutes || back.seconds != t.seconds)
            return 1;
        std::cout << "chrono:     " << clock24::from(end) << " (hh_mm_ss agrees)\n";

        const auto now = system_clock::now();
        std::cout << "now (UTC):  " << clock24::from(floor<seconds>(now - floor<days>(now))) << "\n";
    }

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

    // Weeks and day of the week from one division: `seven`'s grid excludes 0,
    // so divmod cannot fail.
    using ordinal_t = inside<{0, 1'000'000}, snap>;
    constexpr inside<{1, 7}, snap> seven{7};
    const auto [weeks, dow] = divmod(ordinal_t{14003}, seven);
    std::cout << "day 14003 is " << weeks << " weeks + weekday " << dow << " (0 = the reference day's)\n";
    if (weeks != 2000 || dow != 3)
        return 1;

    // With 1970-01-01 (a Thursday, weekday 4) as day 0, chrono names the day.
    constexpr std::array<const char*, 7> names{
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const std::chrono::weekday wd{std::chrono::sys_days{std::chrono::days{14003}}};
    const auto                 our = (4_ins + dow) % 7_ins;
    std::cout << "1970-01-01 + 14003 days is a " << names[our] << "\n";
    if (our != wd.c_encoding())
        return 1;
    return 0;
}
