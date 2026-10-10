// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>

#include <cstdint>
#include <cstring>

namespace zephyrus::hook {
std::expected<pointer_slot, std::string>
pointer_slot::from_function(void *function) noexcept {
    if (function == nullptr) {
        return std::unexpected{std::string{"null function"}};
    }

    const auto *bytes{static_cast<const std::uint8_t *>(function)};
    // FF 25 disp32: jmp qword ptr [rip + disp32]
    if (bytes[0] != 0xFF || bytes[1] != 0x25) {
        return std::unexpected{
            std::string{"not an FF 25 (jmp [rip+disp32]) thunk"}};
    }

    std::int32_t displacement{};
    std::memcpy(&displacement, bytes + 2, sizeof(displacement));

    const auto next_instruction{
        reinterpret_cast<std::intptr_t>(bytes) + 6};
    const auto slot_address{static_cast<std::uintptr_t>(
        next_instruction + static_cast<std::intptr_t>(displacement))};

    // The slot must live inside the same module as the thunk; otherwise the
    // displacement is bogus (or the code is not a thunk at all).
    auto thunk_module{module_of(function)};
    auto slot_module{module_of(reinterpret_cast<const void *>(slot_address))};
    if (!thunk_module || !slot_module || thunk_module->base != slot_module->base) {
        return std::unexpected{
            std::string{"thunk slot outside the thunk's module"}};
    }

    pointer_slot result;
    result.slot_ = reinterpret_cast<void **>(slot_address);
    return result;
}
} // namespace zephyrus::hook
