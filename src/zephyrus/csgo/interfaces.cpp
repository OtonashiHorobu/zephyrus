// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/csgo/interfaces.hpp>

#include <dlfcn.h>

namespace zephyrus::csgo {
std::expected<create_interface_fn, std::string>
interface_factory(const hook::module_info &module) noexcept {
    if (module.base == nullptr || module.path.empty()) {
        return std::unexpected{std::string{"invalid module"}};
    }

    // The host opened the module itself; RTLD_NOLOAD only finds the existing
    // handle (a new copy is never loaded). The symbol stays valid after
    // dlclose() because the host keeps the module mapped.
    void *handle{dlopen(module.path.c_str(), RTLD_LAZY | RTLD_NOLOAD)};
    if (handle == nullptr) {
        const char *error{dlerror()};
        return std::unexpected{"failed to find loaded module " + module.path +
                               " (" + (error != nullptr ? error : "unknown") + ")"};
    }

    auto symbol{hook::resolve_from(handle, "CreateInterface")};
    dlclose(handle);
    if (!symbol) {
        return std::unexpected{"CreateInterface not found in " + module.path};
    }
    return reinterpret_cast<create_interface_fn>(*symbol);
}

std::expected<void *, std::string>
create_interface(const hook::module_info &module,
                 std::string_view name) noexcept {
    auto factory{interface_factory(module)};
    if (!factory) {
        return std::unexpected{factory.error()};
    }

    const std::string interface_name{name};
    void *interface{(*factory)(interface_name.c_str(), nullptr)};
    if (interface == nullptr) {
        return std::unexpected{"interface not found: " + interface_name};
    }
    return interface;
}
} // namespace zephyrus::csgo
