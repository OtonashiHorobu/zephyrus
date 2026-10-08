# Coding Conventions

## Language & Tooling

- **C++ Standard**: C++26 required
- **Build System**: CMake 4.2+
- **Testing**: Boost.UT (micro testing framework)
- **Dependency Management**: Conan 2 (`conanfile.py`, exported local recipes in `conan/recipes/`)
- **Formatting**: clang-format (LLVM-based, 4-space indent)
- **Linting**: `-Wall -Wextra -Werror` on library targets

---

## Naming

| Category | Convention | Example |
|---|---|---|
| Namespace | `snake_case`, C++17 nested syntax | `namespace app::module::submodule {` |
| Class / Struct | `snake_case` | `class file_manager` |
| Function / Method | `snake_case` | `void open_file() noexcept` |
| Local Variable | `snake_case` | `auto file_path{...}` |
| Private Member | `snake_case` with trailing `_` | `std::string name_;` |
| Class Parameter | `snake_case` | `void set_name(std::string name)` |
| Macro | `SCREAMING_SNAKE_CASE` with project prefix | `PROJECT_NAME_MAJOR_VERSION` |
| Enum Type | `snake_case` | `enum class backend_mode { ... }` |
| Enumerator | `snake_case` | `backend_mode::standalone` |
| Template Parameter | `PascalCase` | `template <typename Derived>` |
| Type Alias | `snake_case` | `using result_type = ...` |
| CMake Target | `kebab-case` | `my-project-core` |
| CMake Alias | `PascalCase::PascalCase` | `MyProject::Core` |

**Exception**: Overrides of external framework virtual methods retain that framework's naming convention (e.g., `PascalCase` for RmlUi overrides like `Open`, `Close`, `Read`).

---

## File Organization

```
project-root/
├── include/
│   └── project/          # Public headers; mirrors namespace hierarchy
│       ├── module/
│       │   └── sub.hpp   # namespace project::module
│       └── util.hpp      # Convenience header at namespace root
├── src/
│   ├── project/          # Implementation; mirrors include/ layout
│   │   └── module/
│   │       └── sub.cpp
│   └── main.cpp          # Entry points at src/ root
├── test/
│   └── project/          # Mirrors src/ layout
│       └── module/
│           └── test_sub.cpp
├── asset/                # Embedded resources (CMRC)
├── cmake/                # CMake infrastructure scripts
├── doc/                  # Documentation
├── CMakeLists.txt
├── .clang-format
└── .clangd
```

- **Public headers** live in `include/project/`.
- **Implementation files** mirror the header path in `src/project/`.
- **Entry points** (`main`, library constructors) live at `src/` root.
- **Test files** mirror source layout under `test/`, named `test_<module>.cpp`.

---

## Includes

- **Angle brackets** for all includes (both project and external).
- **One include per line**.
- **Group ordering** (groups separated by blank lines):
  1. Project headers (`<project/...>`)
  2. External/third-party headers (`<fmt/...>`, `<boost/...>`)
  3. Standard library (`<string>`, `<vector>`, etc.)
- **`#pragma once`** for header guards (no `#ifndef`/`#define`/`#endif`).

```cpp
#include <project/module/sub.hpp>
#include <project/util.hpp>

#include <fmt/format.h>
#include <boost/di.hpp>

#include <expected>
#include <string>
```

---

## License Header

Every source file must begin with an SPDX license header:

```cpp
// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) YYYY Author Name
```

For CMake files:

```cmake
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) YYYY Author Name
```

---

## Formatting

- **Indentation**: 4 spaces, no tabs.
- **Braces**: K&R / attached style (opening brace on same line).
- **Access specifiers**: 1-space indent from class body.
- **`switch`/`case`**: `case` labels at same indent level as `switch`.
- **Single-line bodies**: allowed for simple functions/methods.

```cpp
class example {
public:
    [[nodiscard]] auto get_value() const noexcept -> int {
        return value_;
    }

    void set_value(int value) noexcept { value_ = value; }

private:
    int value_{};
};
```

---

## Variables

- **Brace initialization** universally preferred (`auto x{value}`, not `auto x = value`).
- **Zero-initialization** with empty braces: `int count{};`.
- **`const`** on local variables wherever possible.
- **`constexpr`** where appropriate.

```cpp
auto file_path{std::string{"data.txt"}};
const auto max_count{items.size()};
bool running{true};
int counter{};
constexpr double pi{3.1415926535};
```

---

## Functions & Methods

- **`noexcept`** on most method implementations unless the function may legitimately throw.
- **`[[nodiscard]]`** on functions returning values that must not be ignored (especially `std::expected`).
- **`[[maybe_unused]]`** on intentionally unused parameters.
- **`explicit`** on single-argument constructors.
- **`= default`** for trivial special member functions.
- **`= delete`** for forbidden special member functions.

```cpp
[[nodiscard]] auto open_file(const std::string &path) noexcept
    -> std::expected<void, std::string>;

explicit file_manager(std::string root) noexcept;

file_manager(const file_manager &) = delete;
auto operator=(const file_manager &) = delete;
```

---

## Classes

- **CRTP (Curiously Recurring Template Pattern)** for base classes providing template-method lifecycle:

```cpp
template <typename Derived>
class manager {
public:
    [[nodiscard]] auto init() noexcept -> std::expected<void, std::string> {
        if (initialized_.exchange(true)) {
            return std::unexpected{"already initialized"};
        }
        return static_cast<Derived *>(this)->init_impl();
    }

    void shutdown() noexcept {
        if (initialized_.exchange(false)) {
            static_cast<Derived *>(this)->shutdown_impl();
        }
    }

    manager() noexcept = default;
    manager(manager &&other) noexcept
        : initialized_{other.initialized_.exchange(false)} {}

    virtual ~manager() noexcept { shutdown(); }

private:
    std::atomic<bool> initialized_{};
};
```

- **Public API / private data** split.
- **Pimpl-like pattern** via anonymous namespace struct for implementation details.

---

## Error Handling

- **`std::expected`** (C++23) for all recoverable errors — no exceptions.
- Success: `return {};`
- Failure: `return std::unexpected{"error message"};`
- Check pattern: `if (auto result{expr}; !result) { handle_error(result.error()); }`

```cpp
auto load_file(const std::string &path) noexcept
    -> std::expected<std::string, std::string>
{
    if (path.empty()) {
        return std::unexpected{"empty path"};
    }
    // ...
    return content;
}

if (auto result{load_file("data.txt")}; !result) {
    fmt::println(stderr, "error: {}", result.error());
    return;
}
```

---

## Logging

- **spdlog** with custom macros wrapping `SPDLOG_LOGGER_*`:

```cpp
#if !defined(NDEBUG)
#define PROJECT_LOG_TRACE(...) ::spdlog::trace(__VA_ARGS__)
#define PROJECT_LOG_DEBUG(...) ::spdlog::debug(__VA_ARGS__)
#else
#define PROJECT_LOG_TRACE(...) (void)0
#define PROJECT_LOG_DEBUG(...) (void)0
#endif

#define PROJECT_LOG_INFO(...)  ::spdlog::info(__VA_ARGS__)
#define PROJECT_LOG_WARN(...)  ::spdlog::warn(__VA_ARGS__)
#define PROJECT_LOG_ERROR(...) ::spdlog::error(__VA_ARGS__)
```

- Lifecycle events: `INFO` level.
- Detailed operations: `TRACE` level.
- Validation warnings: `DEBUG` level.
- Caught errors: `ERROR` level.
- `NDEBUG` controls active level: `TRACE` in debug, `INFO` in release.

---

## Templates

- **CRTP** for base/derived static polymorphism.
- **C++23 deducing `this`** for macros generating member functions:

```cpp
#define PROJECT_GETTER(field)                                                  \
    template <typename Self> auto &field(this Self &&self) noexcept {          \
        return self.field##_;                                                  \
    }
```

- **`if constexpr`** for compile-time branching (e.g., platform-specific code).

---

## Namespaces

- **C++17 nested namespace syntax**: `namespace a::b::c {`.
- **Anonymous namespaces** for file-local linkage instead of `static`.
- **Closing comment**: `} // namespace a::b::c` (or `} // namespace` for anonymous).

```cpp
namespace app::module {

// ...

} // namespace app::module


namespace {

struct internal_data { /* ... */ };

} // namespace
```

---

## Tests

- **Boost.UT** framework with `"category/test name"_test` naming.
- Single `main()` function containing all test lambdas.
- Assertions via `expect()` with Boost.UT operators.

```cpp
#include <boost/ut.hpp>

int main() {
    using namespace boost::ut;
    using namespace std::literals;

    "open/existing file returns nonzero handle"_test = [&] {
        auto handle{iface.Open("/hello.txt")};
        expect(handle != 0);
    };

    "read/full file content"_test = [&] {
        auto handle{iface.Open("/hello.txt")};
        char buf[64]{};
        auto n{iface.Read(handle, buf, sizeof(buf))};
        expect(std::string_view{buf, n} == "Hello, world!"sv);
    };
}
```

- Test file naming: `test_<module>.cpp` (underscores replaced by hyphens in CMake target names).

---

## Platform Detection

Centralized detection via macros (no raw `#ifdef` scattered across the codebase):

```cpp
// platform/detection.hpp

// OS
#if defined(__linux__)
#define PROJECT_OS_LINUX 1
#elif defined(_WIN32) || defined(_WIN64)
#define PROJECT_OS_WINDOWS 1
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define PROJECT_OS_MACOS 1
#endif
#endif

// Architecture
#if defined(__x86_64__) || defined(_M_X64)
#define PROJECT_ARCH_X86_64 1
#elif defined(__i386__) || defined(_M_IX86)
#define PROJECT_ARCH_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define PROJECT_ARCH_ARM64 1
#elif defined(__arm__)
#define PROJECT_ARCH_ARM 1
#endif

// Compiler
#if defined(__clang__)
#define PROJECT_CC_CLANG 1
#elif defined(__GNUC__)
#define PROJECT_CC_GCC 1
#elif defined(_MSC_VER)
#define PROJECT_CC_MSVC 1
#endif
```

Usage: `#if PROJECT_OS_LINUX` / `#elif PROJECT_OS_WINDOWS`.

---

## CMake Conventions

- **Target naming**: `kebab-case` for actual targets, `PascalCase::PascalCase` for aliases.
- **C++26 required**: `set(CMAKE_CXX_STANDARD 26)`.
- **LTO configurable** via the `lto` Conan option (default off).
- **Position-independent code**: `set(CMAKE_POSITION_INDEPENDENT_CODE ON)`.
- **Warnings as errors**: `target_compile_options(target PRIVATE -Werror)`.
- **Global warnings**: `add_compile_options(-Wall -Wextra)`.
- **Release builds**: `-fvisibility=hidden` on Linux.
- **All dependencies** come from Conan 2 (`conan install`); the injected target also hides statically linked symbols via `-Wl,--exclude-libs,ALL`.
- **Test executables** auto-generated from `test/**/*.cpp` (underscores → hyphens for target names).

---

## Resources (CMRC)

- **cmrc_add_resource_library** for embedding assets as C++ filesystems.
- Declare with `CMRC_DECLARE(namespace)` in headers.
- Namespace convention mirrors project structure: `project::module`.
- Aggregate INTERFACE library collects all resource sub-libraries.
- Test assets use a separate resource library.

---

## General Principles

1. **Consistency over personal preference** — follow existing patterns.
2. **Explicitness** — prefer `auto` with brace init; avoid implicit conversions.
3. **Const correctness** — mark everything `const` that can be.
4. **No exceptions** — use `std::expected` for error handling.
5. **RAII** — resources owned by smart pointers or scoped objects.
6. **Thread safety** — atomic flags for initialization state; mutexes for shared mutable state.
7. **No raw `new`/`delete`** — use `std::make_unique`, `std::make_shared`.
8. **Include what you use** — no transitive dependency on includes.
9. **One responsibility per file** — a header/source pair corresponds to one class or closely related group.
