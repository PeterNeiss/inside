// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Carry cascades with `wrap` and `on_wrap`: a field that overflows hands the
// carry to the next one up, so the cascade is one line per field. Each field
// is a quantity in its own unit (s, min, h; d and units of our own for the
// month and the year), so a carry of n periods is just n of the next unit.
//   1. A 24-hour clock: seconds → minutes → hours, wrapping at 24. It
//      converts to and from seconds since midnight, and the cascade is checked
//      against that count.
//   2. A calendar (30-day months for clarity): days → months → years, plus a
//      day of the week by divmod with a week (7 d), whose grid excludes 0.

#include <array>
#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
// Calendar units of our own: a 30-day month, a 12-month year, a 7-day week.
inline constexpr named_unit month_def{"mo", mag<30> * si::day};
inline constexpr auto       month = unit_c<month_def>;
inline constexpr named_unit year_def{"yr", mag<12> * month};
inline constexpr auto       year = unit_c<year_def>;
inline constexpr named_unit week_def{"wk", mag<7> * si::day};
inline constexpr auto       week = unit_c<week_def>;
} // namespace

struct clock24 {
    using hour_t   = quantity<si::hour, inside<{0, 24, bounds::half_open}, wrap>>;
    using minute_t = quantity<si::minute, inside<{0, 60, bounds::half_open}, wrap>>;
    using second_t = quantity<si::second, inside<{0, 60, bounds::half_open}, wrap>>;

    hour_t   hours{0};
    minute_t minutes{0};
    second_t seconds{0};

    clock24() = default;
    clock24(int h, int m, int s) : hours(h), minutes(m), seconds(s) {}

    // Each field takes any duration: += converts it exactly (90 min is
    // 5400 s), and a wrap past 60 s hands its count of periods on as minutes.
    void add_seconds(const auto& d) {
        seconds.on_wrap([&](auto&, auto carry) { add_minutes(carry * si::minute); }) += d;
    }

    void add_minutes(const auto& d) {
        minutes.on_wrap([&](auto&, auto carry) { add_hours(carry * si::hour); }) += d;
    }

    void add_hours(const auto& d) { hours += d; }

    // A time of day from a duration: whole days wrap away (the `wrap` flag on
    // the store), then one divmod per field.
    using day_seconds_t = quantity<si::second, inside<{0, 86400, bounds::half_open}>>;
    static clock24 from(const auto& since_midnight) {
        const day_seconds_t s = try_value_cast<day_seconds_t, wrap>(since_midnight).value();
        const auto [h, rest]  = divmod(s, just<1> * si::hour);
        const auto [m, sec]   = divmod(rest, just<1> * si::minute);
        clock24 c;
        c.hours   = h * si::hour;
        c.minutes = m * si::minute;
        c.seconds = sec;
        return c;
    }

    // …and back: h + min + s runs in seconds, on [0, 86399], so it is a
    // day_seconds_t as it stands.
    [[nodiscard]] day_seconds_t seconds_since_midnight() const { return hours + minutes + seconds; }

    friend std::ostream& operator<<(std::ostream& os, const clock24& c) {
        auto pad = [&](auto n) {
            if (n < 10)
                os << '0';
            os << n;
        };
        pad(c.hours.numerical_value_in(si::hour));
        os << ':';
        pad(c.minutes.numerical_value_in(si::minute));
        os << ':';
        pad(c.seconds.numerical_value_in(si::second));
        return os;
    }
};

// Days 1–30, months 1–12; the year takes the final carry.
struct date {
    quantity<si::day, inside<{1, 30}, wrap>> day{1};
    quantity<month, inside<{1, 12}, wrap>>   months{1};
    quantity<year, inside<{1900, 2200}>>     years{2000};

    void add_days(const auto& d) {
        day.on_wrap([&](auto&, auto carry) { add_months(carry * month); }) += d;
    }
    void add_months(const auto& d) {
        months.on_wrap([&](auto&, auto carry) { years += carry * year; }) += d;
    }

    friend std::ostream& operator<<(std::ostream& os, const date& d) {
        const auto m  = d.months.numerical_value_in(month);
        const auto dd = d.day.numerical_value_in(si::day);
        return os << d.years.numerical_value_in(year) << '-' << (m < 10 ? "0" : "") << m << '-' << (dd < 10 ? "0" : "")
                  << dd;
    }
};

int main() {
    clock24 t(23, 59, 45);
    std::cout << "start:      " << t << "\n";

    // Each delta is a quantity, so the wrap carry threaded through the
    // sec→min→hour cascade is a count of the next unit up.
    t.add_seconds(just<20> * si::second);
    std::cout << "+20 s:      " << t << "\n";

    t.add_minutes(just<90> * si::minute);
    std::cout << "+90 min:    " << t << "\n";

    t.add_seconds(just<1> * si::hour + just<30> * si::minute + just<30> * si::second); // 5430 s
    std::cout << "+1 h 30 min 30 s: " << t << "\n";
    if (to_string(t.hours) != "3 h" || to_string(t.minutes) != "0 min" || to_string(t.seconds) != "35 s")
        return 1;

    // The same steps as one duration: the cascade agrees with it, and the
    // clock converts both ways.
    {
        const auto end = just<23> * si::hour + just<59> * si::minute + just<45> * si::second + just<20> * si::second +
                         just<90> * si::minute + just<5430> * si::second; // past midnight
        if (t.seconds_since_midnight() != end % (just<1> * si::day))
            return 1;
        const clock24 back = clock24::from(t.seconds_since_midnight());
        if (back.hours != t.hours || back.minutes != t.minutes || back.seconds != t.seconds)
            return 1;
        std::cout << "from count: " << clock24::from(end) << " (" << end << ", whole days wrap away)\n";
    }

    date d;
    d.day    = just<15> * si::day;
    d.months = just<3> * month;
    d.years  = just<2026> * year;
    std::cout << "\nstart:      " << d << "\n";
    d.add_days(just<20> * si::day);
    std::cout << "+20 d:      " << d << "\n";
    d.add_months(just<11> * month);
    std::cout << "+11 mo:     " << d << "\n";
    d.add_days(just<400> * si::day); // cascades through all three fields
    std::cout << "+400 d:     " << d << "\n";
    if (d.years != just<2028> * year || d.months != just<4> * month || d.day != just<15> * si::day)
        return 1;

    // Weeks and day of the week from one division: a week's grid excludes 0,
    // so divmod cannot fail.
    using ordinal_t         = quantity<si::day, inside<{0, 1'000'000}, snap>>;
    const auto [weeks, dow] = divmod(ordinal_t{14003}, just<1> * week);
    std::cout << "day 14003 is " << weeks << " weeks + " << dow << " (0 d = the reference day's weekday)\n";
    if (weeks != 2000 || dow != just<3> * si::day)
        return 1;

    // With 1970-01-01 (a Thursday, 4 days after a Sunday) as day 0: 2008-05-04, a Sunday.
    constexpr std::array<const char*, 7> names{
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const auto our = (just<4> * si::day + dow) % (just<1> * week);
    std::cout << "1970-01-01 + 14003 d is a " << names[our.numerical_value_in(si::day).as<std::size_t>()] << "\n";
    if (our != just<0> * si::day)
        return 1;
    return 0;
}
