// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>

#include <boost/ut.hpp>

#include <string>

namespace {
std::string base_name(std::string_view path) {
    const auto separator{path.find_last_of('/')};
    return separator == std::string_view::npos
               ? std::string{path}
               : std::string{path.substr(separator + 1)};
}
} // namespace

int main() {
    using namespace boost::ut;

    "module_of resolves the main executable"_test = [] {
        auto self{zephyrus::hook::module_of(
            reinterpret_cast<const void *>(&main))};
        expect(self.has_value()) << self.error_or("");
        expect(self->base != nullptr);
        expect(self->size > 0u);
    };

    "find_module locates the main executable by basename"_test = [] {
        auto self{zephyrus::hook::module_of(
            reinterpret_cast<const void *>(&main))};
        expect(self.has_value()) << self.error_or("");

        auto found{zephyrus::hook::find_module(base_name(self->path))};
        expect(found.has_value()) << found.error_or("");
        expect(found->base == self->base);
    };

    "find_module rejects unknown modules"_test = [] {
        auto missing{
            zephyrus::hook::find_module("definitely-not-loaded-xyz.so")};
        expect(!missing.has_value());
    };
}
