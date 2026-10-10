// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>

#include <boost/ut.hpp>

#include <cstdint>
#include <cstring>

namespace {
alignas(16) std::uint8_t thunk[6]{0xFF, 0x25, 0, 0, 0, 0};
void *stored{
    reinterpret_cast<void *>(static_cast<std::uintptr_t>(0x1234))};

int target(int value) { return value + 1; }
} // namespace

int main() {
    using namespace boost::ut;

    "pointer_slot resolves an FF 25 thunk to its slot"_test = [] {
        // jmp qword ptr [rip + disp32] pointing at `stored`.
        const auto displacement{static_cast<std::int32_t>(
            reinterpret_cast<std::intptr_t>(&stored) -
            (reinterpret_cast<std::intptr_t>(thunk) + 6))};
        std::memcpy(thunk + 2, &displacement, sizeof(displacement));

        auto slot{zephyrus::hook::pointer_slot::from_function(thunk)};
        expect(slot.has_value()) << slot.error_or("");
        expect(slot->valid());
        expect(slot->address() == &stored);
        expect(slot->value() == stored);

        void *replacement{reinterpret_cast<void *>(&target)};
        slot->set(replacement);
        expect(stored == replacement);

        slot->set(nullptr);
        expect(slot->value() == nullptr);
    };

    "pointer_slot rejects functions that are not thunks"_test = [] {
        auto invalid{zephyrus::hook::pointer_slot::from_function(
            reinterpret_cast<void *>(&target))};
        expect(!invalid.has_value());
        expect(!zephyrus::hook::pointer_slot::from_function(nullptr)
                    .has_value());
    };
}
