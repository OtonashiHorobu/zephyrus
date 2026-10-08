// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace zephyrus::hook {
using result_type = std::expected<void, std::string>;

/// Resolve a symbol from the process-global dynamic symbol table
/// (`RTLD_DEFAULT`), which includes the main executable and every library
/// loaded with global scope.
[[nodiscard]] std::expected<void *, std::string>
resolve(std::string_view symbol) noexcept;

/// Resolve a symbol from a specific `dlopen` handle.
[[nodiscard]] std::expected<void *, std::string>
resolve_from(void *handle, std::string_view symbol) noexcept;

struct module_info {
    void *base{};
    std::size_t size{};
    std::string path;
};

/// Locate the loaded module that contains `address`.
[[nodiscard]] std::expected<module_info, std::string>
module_of(const void *address) noexcept;

/// IDA-style byte pattern scanning. Tokens are two hex digits separated by
/// whitespace; `?` and `??` are wildcards, e.g. "55 48 89 E5 ?? 8B".
[[nodiscard]] std::expected<const std::byte *, std::string>
find_pattern(std::span<const std::byte> range,
             std::string_view pattern) noexcept;

[[nodiscard]] std::expected<const std::byte *, std::string>
find_pattern(const module_info &module, std::string_view pattern) noexcept;

/// Low-level x86-64 inline hook with value semantics.
///
/// The target's first instructions (>= 5 bytes) are displaced into a
/// trampoline reachable through `trampoline()`. A 5-byte `jmp rel32` in the
/// prologue redirects to an island allocated within +-2 GiB of the target,
/// which performs an absolute jump to the detour. This keeps working even
/// when the detour lives in a `dlopen`ed library far outside rel32 range of
/// the game executable.
///
/// Installation is not thread-safe against threads that may be executing the
/// target concurrently; install hooks from a stopped/safe point (e.g. the
/// first `SDL_GL_SwapBuffers` callback) or for functions only used by the
/// main thread.
class inline_hook {
  public:
    inline_hook() noexcept = default;
    inline_hook(inline_hook &&other) noexcept;
    inline_hook &operator=(inline_hook &&other) noexcept;
    ~inline_hook() noexcept;

    inline_hook(const inline_hook &) = delete;
    inline_hook &operator=(const inline_hook &) = delete;

    [[nodiscard]] static std::expected<inline_hook, std::string>
    install(void *target, void *detour) noexcept;

    /// Entry point of the displaced original code.
    [[nodiscard]] void *trampoline() const noexcept;

    template <typename Fn> [[nodiscard]] Fn *original() const noexcept {
        return reinterpret_cast<Fn *>(trampoline());
    }

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] void *target() const noexcept;

    /// Restore the original bytes. Idempotent.
    result_type uninstall() noexcept;

  private:
    struct impl;
    explicit inline_hook(impl *state) noexcept;
    impl *state_{};
};

namespace detail {
/// Reference to the calling thread's detour depth counter; owned by the hook
/// engine (`inline_hook.cpp`).
[[nodiscard]] int &detour_depth_ref() noexcept;

/// Bounded, per-signature detour slots so arbitrary callables (including
/// capturing lambdas) can be used as detours.
template <typename R, typename... Args> class detour_slots {
  public:
    static constexpr std::size_t npos{static_cast<std::size_t>(-1)};

    [[nodiscard]] static std::expected<std::size_t, std::string>
    acquire(std::move_only_function<R(Args...)> callable) noexcept {
        std::lock_guard lock{mutex()};
        for (std::size_t i{}; i < capacity; ++i) {
            if (!used()[i]) {
                used()[i] = true;
                slots()[i] = std::move(callable);
                return i;
            }
        }
        return std::unexpected{"detour slot table exhausted for signature"};
    }

    static void release(std::size_t index) noexcept {
        if (index >= capacity) {
            return;
        }
        std::lock_guard lock{mutex()};
        slots()[index] = nullptr;
        used()[index] = false;
    }

    [[nodiscard]] static void *function_pointer(std::size_t index) noexcept {
        return thunks()[index];
    }

  private:
    static constexpr std::size_t capacity{64};

    static std::mutex &mutex() noexcept {
        static std::mutex value;
        return value;
    }

    static std::array<std::move_only_function<R(Args...)>, capacity> &
    slots() noexcept {
        static std::array<std::move_only_function<R(Args...)>, capacity> value{};
        return value;
    }

    static std::array<bool, capacity> &used() noexcept {
        static std::array<bool, capacity> value{};
        return value;
    }

    template <std::size_t Index> static R thunk(Args... args) {
        struct depth_guard {
            depth_guard() noexcept { ++detour_depth_ref(); }
            ~depth_guard() noexcept { --detour_depth_ref(); }
        } guard;
        if constexpr (std::is_void_v<R>) {
            slots()[Index](std::forward<Args>(args)...);
        } else {
            return slots()[Index](std::forward<Args>(args)...);
        }
    }

    template <std::size_t... Index>
    static auto make_thunks(std::index_sequence<Index...>)
        -> std::array<void *, capacity> {
        return {reinterpret_cast<void *>(&thunk<Index>)...};
    }

    static const std::array<void *, capacity> &thunks() noexcept {
        static const auto value{
            make_thunks(std::make_index_sequence<capacity>{})};
        return value;
    }
};
} // namespace detail

/// Type-safe inline hook for a function signature.
///
/// ```cpp
/// auto detour = [](int dx, int dy) { /* ... */ };
/// auto hook = function_hook<void(int, int)>::install("_Z9mousemoveii", detour);
/// if (!hook) { /* hook.error() */ }
/// hook->call_original(1, 2);
/// ```
template <typename Signature> class function_hook;

/// Per-thread depth of detour callbacks that are currently executing. Callers
/// that unload hooks or the owning module must not do so while this is
/// non-zero: the detour, its trampoline and the owning object may still be on
/// the stack.
[[nodiscard]] int active_detour_depth() noexcept;

template <typename R, typename... Args> class function_hook<R(Args...)> {
  public:
    function_hook() noexcept = default;
    function_hook(function_hook &&) noexcept = default;
    function_hook &operator=(function_hook &&) noexcept = default;
    ~function_hook() noexcept = default;

    template <typename F>
        requires std::invocable<F &, Args...> &&
                 std::convertible_to<std::invoke_result_t<F &, Args...>, R>
    [[nodiscard]] static std::expected<function_hook, std::string>
    install(void *target, F detour) noexcept {
        auto index{detail::detour_slots<R, Args...>::acquire(std::move(detour))};
        if (!index) {
            return std::unexpected{index.error()};
        }

        auto patch{inline_hook::install(
            target, detail::detour_slots<R, Args...>::function_pointer(*index))};
        if (!patch) {
            detail::detour_slots<R, Args...>::release(*index);
            return std::unexpected{patch.error()};
        }

        return function_hook{std::move(*patch), slot_handle{*index}};
    }

    template <typename F>
        requires std::invocable<F &, Args...> &&
                 std::convertible_to<std::invoke_result_t<F &, Args...>, R>
    [[nodiscard]] static std::expected<function_hook, std::string>
    install(std::string_view symbol, F detour) noexcept {
        auto target{resolve(symbol)};
        if (!target) {
            return std::unexpected{target.error()};
        }
        return install(*target, std::move(detour));
    }

    /// Invoke the original implementation through the trampoline.
    [[nodiscard]] R call_original(Args... args) const noexcept {
        auto *fn{reinterpret_cast<R (*)(Args...)>(patch_.trampoline())};
        if constexpr (std::is_void_v<R>) {
            fn(std::forward<Args>(args)...);
        } else {
            return fn(std::forward<Args>(args)...);
        }
    }

    [[nodiscard]] bool valid() const noexcept { return patch_.valid(); }
    [[nodiscard]] void *target() const noexcept { return patch_.target(); }

    result_type uninstall() noexcept {
        slot_.reset();
        return patch_.uninstall();
    }

  private:
    struct slot_handle {
        static constexpr std::size_t npos{
            detail::detour_slots<R, Args...>::npos};

        slot_handle() noexcept = default;
        explicit slot_handle(std::size_t index) noexcept : index_{index} {}
        slot_handle(slot_handle &&other) noexcept : index_{other.index_} {
            other.index_ = npos;
        }
        slot_handle &operator=(slot_handle &&other) noexcept {
            if (this != &other) {
                reset();
                index_ = other.index_;
                other.index_ = npos;
            }
            return *this;
        }
        ~slot_handle() noexcept { reset(); }

        void reset() noexcept {
            if (index_ != npos) {
                detail::detour_slots<R, Args...>::release(index_);
                index_ = npos;
            }
        }

      private:
        std::size_t index_{npos};
    };

    function_hook(inline_hook &&patch, slot_handle slot) noexcept
        : patch_{std::move(patch)}, slot_{std::move(slot)} {}

    inline_hook patch_;
    slot_handle slot_;
};
} // namespace zephyrus::hook
