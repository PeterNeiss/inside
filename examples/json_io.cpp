// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// JSON numbers in and out, exactly. A JSON reader that parses numbers as
// double cannot hold an ID past 2^53 or the cents of a price; here each number
// goes from its text straight into its field's grid with parse_exact — no
// double in between, and nothing rounded or clamped whatever the field's
// policy — so a bad field is reported by name and cause. to_string writes the
// values back as the same decimal text, so the document round-trips byte for
// byte. (A value with no finite decimal, such as 1/3, prints as N/D: not JSON.)
// The fields carry units — a price in USD/item, a quantity in items — so the
// total is USD by its own arithmetic; the JSON text holds the bare numbers,
// read and written in those units.

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_io.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension dollar_dim{"dollar"};
inline constexpr quantity_spec  dollars_def{"dollars", dollar_dim};
inline constexpr named_unit     usd_def{"USD", spec_c<dollars_def>};
inline constexpr auto           USD = unit_c<usd_def>;
inline constexpr base_dimension count_dim{"count"};
inline constexpr quantity_spec  items_def{"items", count_dim};
inline constexpr named_unit     item_def{"item", spec_c<items_def>};
inline constexpr auto           item = unit_c<item_def>;
} // namespace

using order_id   = inside<{0, 9'000'000'000'000'000'000}>;                                  // 64-bit IDs
using price_t    = quantity<USD / item, inside<{{0, 1'000'000}, per<100>}, round_nearest>>; // to the cent
using quantity_t = quantity<item, inside<{1, 1000}>>;                                       // whole items
using ratio_t    = inside<{{0, 1}, per<100>}>;                                              // a discount, 1 %
using total_t    = quantity<USD, inside<{{0, 1'000'000'000}, per<100>}, round_nearest>>;    // rounded once

// A flat JSON object of numbers: key → the number's text. Minimal on purpose
// (no nesting, strings or escapes) — just enough to feed parse.
std::map<std::string, std::string_view> scan(std::string_view json) {
    std::map<std::string, std::string_view> fields;
    std::size_t                             i = 0;
    while ((i = json.find('"', i)) != std::string_view::npos) {
        const std::size_t k_end = json.find('"', i + 1);
        const std::string key{json.substr(i + 1, k_end - i - 1)};
        const std::size_t v     = json.find_first_not_of(" :", k_end + 1);
        const std::size_t v_end = json.find_first_of(",}", v);
        fields[key]             = json.substr(v, json.find_last_not_of(' ', v_end - 1) + 1 - v);
        i                       = v_end;
    }
    return fields;
}

struct order {
    order_id   id;
    price_t    price;
    quantity_t quantity;
    ratio_t    discount;
};

// Every field parsed; the errors, by key, if any failed.
std::expected<order, std::vector<std::pair<std::string, errc>>> read_order(std::string_view json) {
    const auto                                fields = scan(json);
    std::vector<std::pair<std::string, errc>> errors;
    auto                                      get = [&]<typename B>(const char* key, B& out) {
        const auto it = fields.find(key);
        const auto v  = it == fields.end() ? std::expected<B, errc>{std::unexpected(errc::invalid_format)}
                                           : parse_exact<B>(it->second);
        if (v)
            out = *v;
        else
            errors.emplace_back(key, v.error());
    };
    order o{order_id{0}, price_t{0}, quantity_t{1}, ratio_t{0}};
    get("id", o.id);
    get("price", o.price);
    get("quantity", o.quantity);
    get("discount", o.discount);
    if (!errors.empty())
        return std::unexpected(errors);
    return o;
}

std::string write_order(const order& o) {
    return R"({"id": )" + to_string(o.id) + R"(, "price": )" + to_string(o.price.numerical_value_in(USD / item)) +
           R"(, "quantity": )" + to_string(o.quantity.numerical_value_in(item)) + R"(, "discount": )" +
           to_string(o.discount) + "}";
}

int main() {
    // 1. A valid order: 2^53 + 1 survives; the total is exact until one rounding.
    constexpr std::string_view good = R"({"id": 9007199254740993, "price": 19.99, "quantity": 3, "discount": 0.15})";
    const auto                 o    = read_order(good);
    if (!o)
        return 1;
    const total_t total{o->price * o->quantity * (just<1> - o->discount)}; // 50.9745 → 50.97
    std::cout << "read   " << good << "\n"
              << "write  " << write_order(*o) << "\n"
              << "total  " << total << "\n"
              << "as double the id is " << static_cast<long long>(std::stod("9007199254740993")) << "\n";
    if (write_order(*o) != good || to_string(total) != "50.97 USD")
        return 1;

    // 2. A bad order: each failing field reported with its cause.
    constexpr std::string_view bad = R"({"id": 12x, "price": 19.999, "quantity": 0, "discount": 0.15})";
    const auto                 b   = read_order(bad);
    std::cout << "\nread   " << bad << "\n";
    for (const auto& [key, e] : b.error())
        std::cout << "  " << key << ": " << errc_message(e) << "\n";
    return !b && b.error().size() == 3 ? 0 : 1; // id malformed; price off the cent grid; quantity below 1
}
