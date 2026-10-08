// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>

#include <boost/ut.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

int main() {
    using namespace boost::ut;

    const std::vector<std::byte> data{
        std::byte{0x90}, std::byte{0x55}, std::byte{0x48},
        std::byte{0x89}, std::byte{0xE5}, std::byte{0x8B},
        std::byte{0x45}, std::byte{0x08}, std::byte{0xC3}};

    "pattern finds an exact match"_test = [&] {
        auto found{zephyrus::hook::find_pattern(data, "55 48 89 E5")};
        expect(found.has_value()) << found.error_or("");
        expect(found.has_value() &&
               *found == data.data() + 1);
    };

    "pattern supports wildcards"_test = [&] {
        auto found{
            zephyrus::hook::find_pattern(data, "55 ?? 89 ?? 8B")};
        expect(found.has_value()) << found.error_or("");
        expect(found.has_value() &&
               *found == data.data() + 1);
    };

    "pattern reports a miss"_test = [&] {
        auto found{zephyrus::hook::find_pattern(data, "DE AD BE EF")};
        expect(!found.has_value());
    };

    "pattern rejects malformed input"_test = [&] {
        expect(!zephyrus::hook::find_pattern(data, "").has_value());
        expect(!zephyrus::hook::find_pattern(data, "5").has_value());
        expect(!zephyrus::hook::find_pattern(data, "GG").has_value());
        expect(!zephyrus::hook::find_pattern(
                    data, "55 48 89 E5 8B 45 08 C3 FF FF")
                   .has_value());
    };
}
