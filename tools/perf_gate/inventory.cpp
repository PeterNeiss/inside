// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The public run-time surface, by reflection: every function or function
// template (and operator) of the public namespaces, and every public member
// function of the public class templates, one line per scope and name.
// perfmap.py checks that each line maps to a family it generates kernels for.
//
// Build: g++-16 -std=c++26 -freflection -I include inventory.cpp
#include <inside/inside.hpp>
#include <inside/inside/formats.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/mdspan.hpp>
#include <inside/inside/random.hpp>
#include <inside/math.hpp>
#include <inside/math/integer.hpp>
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>
#include <inside/units/mdspan.hpp>
#include <inside/units/random.hpp>

#include <algorithm>
#include <cstdio>
#include <meta>
#include <string>
#include <vector>

namespace {
namespace bi             = ins;
namespace units          = ins;
using num                = ins::id<ins::key{ins::grid{{0, 100}, bi::per<16>}}>;
using qty                = ins::id<"[0..100] [m]">;
using pt                 = ins::id<"[0..400] [K] @absolute_zero">;
using cur                = bi::cursor<ins::id<"[0..10]">>;
using rng                = bi::inside_range<bi::grid{0, 10}>;
inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<+1>>;
inline constexpr auto e2 = ins::basis_vector<"e2", ins::geometric<+1>>;
using mv                 = ins::multivector<ins::id<"[0..10]">, ins::id<"[0..10] [e1*e2]">>;

template <std::meta::info Scope, bool Members>
void print(const char* scope) {
    std::vector<std::string> names;
    template for (constexpr auto m :
                  std::define_static_array(std::meta::members_of(Scope, std::meta::access_context::current()))) {
        if constexpr ((std::meta::is_function_template(m) || std::meta::is_function(m)))
            if constexpr (!Members || std::meta::is_public(m)) {
                if constexpr (std::meta::has_identifier(m)) {
                    if (!std::meta::identifier_of(m).starts_with("__"))
                        names.emplace_back(std::meta::identifier_of(m));
                } else if constexpr (std::meta::is_operator_function(m) || std::meta::is_operator_function_template(m))
                    names.emplace_back(std::string{"operator"} +
                                       std::string{std::meta::symbol_of(std::meta::operator_of(m))});
                else if constexpr (std::meta::is_constructor(m) || std::meta::is_constructor_template(m))
                    names.emplace_back("(constructor)");
                else if constexpr (std::meta::is_conversion_function(m) ||
                                   std::meta::is_conversion_function_template(m))
                    names.emplace_back("(conversion)");
            }
    }
    std::ranges::sort(names);
    names.erase(std::ranges::unique(names).begin(), names.end());
    for (const auto& n : names)
        std::printf("%s %s\n", scope, n.c_str());
}
} // namespace

int main() {
    print<^^ins, false>("inside");
    print<^^ins::math, false>("math");
    print<^^ins, false>("units");
    print<^^ins::formats, false>("formats");
    print<^^num, true>("inside::member");
    print<^^qty, true>("quantity::member");
    print<^^pt, true>("point::member");
    print<^^cur, true>("cursor::member");
    print<^^rng, true>("range::member");
    print<^^mv, true>("multivector::member");
}
