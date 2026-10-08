// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>

#include <boost/ut.hpp>
#include <cstdint>

namespace {
volatile int global_value = 41;

[[gnu::noinline]] int add(int a, int b) { return a + b; }

[[gnu::noinline]] int read_global() { return global_value; }

[[gnu::noinline]] int nested_call(int value) { return add(value, 1); }
} // namespace

int main() {
    using namespace boost::ut;

    "inline hook patches, trampolines and restores"_test = [] {
        auto hook{zephyrus::hook::function_hook<int(int, int)>::install(
            reinterpret_cast<void *>(&add),
            [](int a, int b) { return (a + b) * 2; })};
        expect(hook.has_value()) << hook.error_or("");

        expect(eq(add(2, 3), 10));
        expect(eq(hook->call_original(2, 3), 5));

        expect(hook->uninstall().has_value());
        expect(eq(add(2, 3), 5)) << "original bytes restored";
    };

    "inline hook relocates rip-relative operands"_test = [] {
        auto hook{zephyrus::hook::function_hook<int()>::install(
            reinterpret_cast<void *>(&read_global),
            []() { return 1337; })};
        expect(hook.has_value()) << hook.error_or("");

        expect(eq(read_global(), 1337));
        expect(eq(hook->call_original(), 41))
            << "trampoline still reads the original global";

        expect(hook->uninstall().has_value());
        expect(eq(read_global(), 41));
    };

    "inline hook keeps callers of a hooked callee working"_test = [] {
        auto hook{zephyrus::hook::function_hook<int(int, int)>::install(
            reinterpret_cast<void *>(&add),
            [](int a, int b) { return a + b + 100; })};
        expect(hook.has_value()) << hook.error_or("");

        expect(eq(nested_call(4), 105)) << "nested call hits the detour";
        expect(eq(hook->call_original(4, 1), 5));
        expect(hook->uninstall().has_value());
        expect(eq(nested_call(4), 5));
    };

    "double hooking a target is rejected"_test = [] {
        auto first{zephyrus::hook::function_hook<int(int, int)>::install(
            reinterpret_cast<void *>(&add),
            [](int a, int b) { return a + b + 100; })};
        expect(first.has_value()) << first.error_or("");

        auto second{zephyrus::hook::function_hook<int(int, int)>::install(
            reinterpret_cast<void *>(&add),
            [](int a, int b) { return a + b + 200; })};
        expect(!second.has_value());

        expect(first->uninstall().has_value());
    };
}
