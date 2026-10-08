// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>
#include <zephyrus/log.hpp>

#include <Zydis/Zydis.h>
#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#if !defined(MAP_FIXED_NOREPLACE)
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace zephyrus::hook {
namespace detail {
namespace {
thread_local int detour_depth{};
} // namespace
int &detour_depth_ref() noexcept { return detour_depth; }
} // namespace detail

int active_detour_depth() noexcept { return detail::detour_depth_ref(); }

namespace {
constexpr std::uintptr_t rel32_limit{0x7FFF0000};
constexpr std::size_t trampoline_capacity{64};
constexpr std::size_t island_capacity{16};
constexpr std::size_t minimum_prologue{5};

[[nodiscard]] std::size_t page_size() noexcept {
    static const std::size_t value{
        static_cast<std::size_t>(sysconf(_SC_PAGESIZE))};
    return value;
}

struct arena {
    std::byte *base{};
    std::size_t used{};
    std::size_t capacity{};
};

std::mutex arena_mutex;
std::vector<arena> arenas;

[[nodiscard]] void *map_near(std::uintptr_t target) noexcept {
    const std::size_t page{page_size()};
    constexpr std::uintptr_t step{0x10'0000};

    for (std::uintptr_t offset{step}; offset < rel32_limit; offset += step) {
        for (const int direction : {1, -1}) {
            const std::uintptr_t raw{direction > 0 ? target + offset
                                                   : target - offset};
            const std::uintptr_t candidate{raw & ~(page - 1)};
            if (direction < 0 && candidate < page) {
                continue;
            }
            void *mapped{mmap(reinterpret_cast<void *>(candidate), page,
                              PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_PRIVATE | MAP_ANONYMOUS |
                                  MAP_FIXED_NOREPLACE,
                              -1, 0)};
            if (mapped != MAP_FAILED) {
                return mapped;
            }
        }
    }
    return nullptr;
}

[[nodiscard]] std::byte *allocate_near(std::uintptr_t target,
                                       std::size_t size) noexcept {
    std::lock_guard lock{arena_mutex};

    for (auto &arena : arenas) {
        const auto base{reinterpret_cast<std::uintptr_t>(arena.base)};
        const auto distance{base > target ? base - target : target - base};
        if (distance < rel32_limit && arena.used + size <= arena.capacity) {
            std::byte *result{arena.base + arena.used};
            arena.used += size;
            return result;
        }
    }

    void *mapped{map_near(target)};
    if (mapped == nullptr) {
        return nullptr;
    }
    arenas.push_back(
        arena{.base = static_cast<std::byte *>(mapped),
              .used = size,
              .capacity = page_size()});
    return static_cast<std::byte *>(mapped);
}

/// Unmap every trampoline/island page. Only safe once no hook is installed:
/// restored targets no longer reference them.
void release_arenas() noexcept {
    std::lock_guard lock{arena_mutex};
    for (const arena &entry : arenas) {
        if (entry.base != nullptr) {
            munmap(entry.base, entry.capacity);
        }
    }
    arenas.clear();
}

[[nodiscard]] result_type write_code(void *address, const void *bytes,
                                     std::size_t size) noexcept {
    const std::size_t page{page_size()};
    const auto start{reinterpret_cast<std::uintptr_t>(address)};
    const auto page_start{start & ~(page - 1)};
    const std::size_t span{start + size - page_start};
    const std::size_t length{(span + page - 1) & ~(page - 1)};

    auto *region{reinterpret_cast<void *>(page_start)};
    if (mprotect(region, length, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return std::unexpected{"mprotect(RWX) failed"};
    }

    std::memcpy(address, bytes, size);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    if (mprotect(region, length, PROT_READ | PROT_EXEC) != 0) {
        return std::unexpected{"mprotect(RX) failed"};
    }
    return {};
}

// Prevent two hooks from fighting over the same target.
std::mutex registry_mutex;
std::vector<void *> hooked_targets;

[[nodiscard]] bool registry_add(void *target) noexcept {
    std::lock_guard lock{registry_mutex};
    for (void *existing : hooked_targets) {
        if (existing == target) {
            return false;
        }
    }
    hooked_targets.push_back(target);
    return true;
}

void registry_remove(void *target) noexcept {
    std::lock_guard lock{registry_mutex};
    for (auto it{hooked_targets.begin()}; it != hooked_targets.end(); ++it) {
        if (*it == target) {
            hooked_targets.erase(it);
            break;
        }
    }
    if (hooked_targets.empty()) {
        // Nothing is patched any more: reclaim the executable pages so
        // repeated unload/reload cycles do not leak RWX memory.
        release_arenas();
    }
}

struct prologue {
    std::vector<std::byte> bytes;   // original instructions (for restoration)
    std::size_t length{};           // stolen length in the target
    std::size_t trampoline_length{}; // relocated size (may grow when widened)
};

[[nodiscard]] std::expected<prologue, std::string>
relocate_prologue(const std::byte *target, std::byte *trampoline) {
    ZydisDecoder decoder{};
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64,
                                       ZYDIS_STACK_WIDTH_64))) {
        return std::unexpected{"failed to initialize zydis decoder"};
    }

    prologue result;
    std::size_t offset{};
    std::size_t trampoline_offset{};
    while (offset < minimum_prologue) {
        if (offset >= 15) {
            return std::unexpected{"prologue needs more than 15 bytes"};
        }

        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, target + offset,
                                                 15 - offset, &instruction,
                                                 operands))) {
            return std::unexpected{"zydis failed to decode prologue"};
        }
        if (instruction.length == 0 ||
            offset + instruction.length > 15) {
            return std::unexpected{"inconsistent prologue instruction"};
        }

        result.bytes.insert(result.bytes.end(), target + offset,
                            target + offset + instruction.length);
        std::memcpy(trampoline + trampoline_offset, target + offset,
                    instruction.length);
        std::size_t new_length{instruction.length};

        const auto instruction_address{
            reinterpret_cast<std::uintptr_t>(target + offset)};
        const auto new_instruction_address{
            reinterpret_cast<std::uintptr_t>(trampoline + trampoline_offset)};

        // RIP-relative memory operands must be re-based on the trampoline.
        for (std::size_t i{}; i < instruction.operand_count_visible; ++i) {
            const auto &operand{operands[i]};
            if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY ||
                operand.mem.base != ZYDIS_REGISTER_RIP) {
                continue;
            }

            const auto &displacement{instruction.raw.disp};
            if (displacement.size != 32) {
                return std::unexpected{
                    "unsupported RIP-relative displacement size"};
            }

            const std::int64_t absolute{static_cast<std::int64_t>(
                instruction_address + instruction.length +
                static_cast<std::uintptr_t>(displacement.value))};
            const std::int64_t adjusted{
                absolute - static_cast<std::int64_t>(
                               new_instruction_address + instruction.length)};
            if (adjusted < INT32_MIN || adjusted > INT32_MAX) {
                return std::unexpected{
                    "RIP-relative operand cannot reach the trampoline"};
            }
            const auto value{static_cast<std::int32_t>(adjusted)};
            std::memcpy(trampoline + trampoline_offset + displacement.offset,
                        &value, sizeof(value));
        }

        // Relative branches (call/jmp/jcc) must be re-targeted as well.
        const auto category{instruction.meta.category};
        const bool branch{category == ZYDIS_CATEGORY_COND_BR ||
                          category == ZYDIS_CATEGORY_UNCOND_BR ||
                          category == ZYDIS_CATEGORY_CALL};
        if (branch) {
            const auto &immediate{instruction.raw.imm[0]};
            if (immediate.is_relative) {
                const std::int64_t absolute{
                    static_cast<std::int64_t>(instruction_address +
                                              instruction.length) +
                    immediate.value.s};

                if (immediate.size == 8) {
                    // Short branches cannot reach a trampoline more than
                    // +-127 bytes away, so widen them to their rel32 form.
                    const auto opcode{
                        static_cast<unsigned char>(target[offset])};
                    if (category == ZYDIS_CATEGORY_COND_BR && opcode >= 0x70 &&
                        opcode <= 0x7F) {
                        new_length = 6;
                        trampoline[trampoline_offset] = std::byte{0x0F};
                        trampoline[trampoline_offset + 1] =
                            static_cast<std::byte>(0x80 + (opcode - 0x70));
                        const std::int64_t adjusted{
                            absolute - static_cast<std::int64_t>(
                                           new_instruction_address + 6)};
                        if (adjusted < INT32_MIN || adjusted > INT32_MAX) {
                            return std::unexpected{
                                "widened branch cannot reach its target"};
                        }
                        const auto value{static_cast<std::int32_t>(adjusted)};
                        std::memcpy(trampoline + trampoline_offset + 2, &value,
                                    sizeof(value));
                    } else if (category == ZYDIS_CATEGORY_UNCOND_BR &&
                               opcode == 0xEB) {
                        new_length = 5;
                        trampoline[trampoline_offset] = std::byte{0xE9};
                        const std::int64_t adjusted{
                            absolute - static_cast<std::int64_t>(
                                           new_instruction_address + 5)};
                        if (adjusted < INT32_MIN || adjusted > INT32_MAX) {
                            return std::unexpected{
                                "widened branch cannot reach its target"};
                        }
                        const auto value{static_cast<std::int32_t>(adjusted)};
                        std::memcpy(trampoline + trampoline_offset + 1, &value,
                                    sizeof(value));
                    } else {
                        return std::unexpected{
                            "unsupported short branch in prologue"};
                    }
                } else if (immediate.size == 32) {
                    const std::int64_t adjusted{
                        absolute - static_cast<std::int64_t>(
                                       new_instruction_address +
                                       instruction.length)};
                    if (adjusted < INT32_MIN || adjusted > INT32_MAX) {
                        return std::unexpected{
                            "branch cannot reach the trampoline"};
                    }
                    const auto value{static_cast<std::int32_t>(adjusted)};
                    std::memcpy(trampoline + trampoline_offset +
                                    immediate.offset,
                                &value, sizeof(value));
                } else {
                    return std::unexpected{"unsupported relative branch size"};
                }
            }
        }

        offset += instruction.length;
        trampoline_offset += new_length;
    }

    result.length = offset;
    result.trampoline_length = trampoline_offset;
    return result;
}

void encode_absolute_jump(std::byte *destination, const void *address) {
    // FF 25 00 00 00 00  jmp qword ptr [rip + 0]
    // <8 byte absolute address>
    constexpr std::array<std::uint8_t, 6> opcode{0xFF, 0x25, 0, 0, 0, 0};
    std::memcpy(destination, opcode.data(), opcode.size());
    std::memcpy(destination + opcode.size(), &address, sizeof(address));
}
} // namespace

struct inline_hook::impl {
    void *target{};
    void *detour{};
    std::byte *trampoline{};
    std::vector<std::byte> original;
    bool installed{};

    ~impl() noexcept {
        if (installed && target != nullptr) {
            (void)write_code(target, original.data(), original.size());
            installed = false;
        }
        if (target != nullptr) {
            registry_remove(target);
        }
    }
};

inline_hook::inline_hook(impl *state) noexcept : state_{state} {}

inline_hook::inline_hook(inline_hook &&other) noexcept
    : state_{other.state_} {
    other.state_ = nullptr;
}

inline_hook &inline_hook::operator=(inline_hook &&other) noexcept {
    if (this != &other) {
        delete state_;
        state_ = other.state_;
        other.state_ = nullptr;
    }
    return *this;
}

inline_hook::~inline_hook() noexcept { delete state_; }

std::expected<inline_hook, std::string>
inline_hook::install(void *target, void *detour) noexcept {
    if (target == nullptr || detour == nullptr) {
        return std::unexpected{"inline hook requires a target and a detour"};
    }

    const auto target_address{reinterpret_cast<std::uintptr_t>(target)};
    if (!registry_add(target)) {
        return std::unexpected{"target is already hooked"};
    }

    auto fail{[target](std::string message) {
        registry_remove(target);
        return std::unexpected{std::move(message)};
    }};

    auto *island{allocate_near(target_address, island_capacity)};
    auto *trampoline{allocate_near(target_address, trampoline_capacity)};
    if (island == nullptr || trampoline == nullptr) {
        return fail("no executable memory available within +-2GiB of target");
    }

    auto relocated{relocate_prologue(static_cast<const std::byte *>(target),
                                     trampoline)};
    if (!relocated) {
        return fail(relocated.error());
    }

    // Trampoline: displaced instructions followed by a jump back after the
    // patched prologue. Widened instructions make the relocated code longer
    // than the stolen bytes, hence the separate trampoline length.
    const void *continuation{static_cast<std::byte *>(target) +
                             relocated->length};
    encode_absolute_jump(trampoline + relocated->trampoline_length,
                         continuation);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    // Island: absolute jump to the detour (reachable from the target by rel32).
    encode_absolute_jump(island, detour);

    // Patch: E9 rel32 to the island + NOP padding over the stolen bytes.
    std::array<std::byte, 16> patch{};
    patch[0] = std::byte{0xE9};
    const auto relative{
        static_cast<std::int64_t>(
            reinterpret_cast<std::uintptr_t>(island)) -
        static_cast<std::int64_t>(target_address + 5)};
    if (relative < INT32_MIN || relative > INT32_MAX) {
        return fail("island is out of rel32 range of the target");
    }
    const auto relative32{static_cast<std::int32_t>(relative)};
    std::memcpy(patch.data() + 1, &relative32, sizeof(relative32));
    for (std::size_t i{5}; i < relocated->length; ++i) {
        patch[i] = std::byte{0x90};
    }

    auto *state{new impl{.target = target,
                         .detour = detour,
                         .trampoline = trampoline,
                         .original = std::move(relocated->bytes),
                         .installed = false}};

    if (auto written{
            write_code(target, patch.data(), relocated->length)};
        !written) {
        delete state;
        return fail(written.error());
    }

    state->installed = true;
    return inline_hook{state};
}

void *inline_hook::trampoline() const noexcept {
    return state_ != nullptr ? state_->trampoline : nullptr;
}

bool inline_hook::valid() const noexcept { return state_ != nullptr; }

void *inline_hook::target() const noexcept {
    return state_ != nullptr ? state_->target : nullptr;
}

result_type inline_hook::uninstall() noexcept {
    if (state_ == nullptr || !state_->installed) {
        return {};
    }
    auto result{write_code(state_->target, state_->original.data(),
                           state_->original.size())};
    if (result) {
        state_->installed = false;
    }
    return result;
}
} // namespace zephyrus::hook
