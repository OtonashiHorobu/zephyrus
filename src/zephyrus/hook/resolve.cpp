// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <string>

namespace zephyrus::hook {
namespace {
std::string last_dlerror() {
    const char *error{dlerror()};
    return error ? std::string{error} : std::string{"unknown dynamic loader error"};
}

struct module_search {
    const void *address{};
    module_info info{};
    bool found{};
};

[[nodiscard]] std::string_view module_path(const struct dl_phdr_info *info) {
    return info->dlpi_name != nullptr && info->dlpi_name[0] != '\0'
               ? std::string_view{info->dlpi_name}
               : std::string_view{"/proc/self/exe"};
}

[[nodiscard]] std::string_view base_name(std::string_view path) {
    const auto separator{path.find_last_of('/')};
    return separator == std::string_view::npos ? path
                                               : path.substr(separator + 1);
}

[[nodiscard]] std::uintptr_t module_end(const struct dl_phdr_info *info) {
    const auto base{static_cast<std::uintptr_t>(info->dlpi_addr)};
    std::uintptr_t end{base};
    for (int i{}; i < info->dlpi_phnum; ++i) {
        const auto &phdr{info->dlpi_phdr[i]};
        if (phdr.p_type != PT_LOAD) {
            continue;
        }
        end = std::max(end, base + phdr.p_vaddr + phdr.p_memsz);
    }
    return end;
}

int module_callback(struct dl_phdr_info *info, std::size_t /*size*/, void *data) {
    auto &search{*static_cast<module_search *>(data)};

    const auto address{reinterpret_cast<std::uintptr_t>(search.address)};
    const auto base{static_cast<std::uintptr_t>(info->dlpi_addr)};
    const auto end{module_end(info)};

    if (address >= base && address < end) {
        search.info.base = reinterpret_cast<void *>(base);
        search.info.size = end - base;
        search.info.path = std::string{module_path(info)};
        search.found = true;
        return 1; // stop iteration
    }
    return 0;
}

struct module_lookup {
    std::string_view name;
    module_info info{};
    bool found{};
};

int module_lookup_callback(struct dl_phdr_info *info, std::size_t /*size*/,
                           void *data) {
    auto &lookup{*static_cast<module_lookup *>(data)};

    const auto path{module_path(info)};
    if (base_name(path) != lookup.name) {
        return 0;
    }

    const auto base{static_cast<std::uintptr_t>(info->dlpi_addr)};
    lookup.info.base = reinterpret_cast<void *>(base);
    lookup.info.size = module_end(info) - base;
    lookup.info.path = std::string{path};
    lookup.found = true;
    return 1; // stop iteration
}
} // namespace

std::expected<void *, std::string> resolve(std::string_view symbol) noexcept {
    if (symbol.empty()) {
        return std::unexpected{std::string{"empty symbol name"}};
    }
    const std::string name{symbol};
    if (void *address{dlsym(RTLD_DEFAULT, name.c_str())}; address != nullptr) {
        return address;
    }
    return std::unexpected{"symbol not found: " + name + " (" + last_dlerror() + ")"};
}

std::expected<void *, std::string>
resolve_from(void *handle, std::string_view symbol) noexcept {
    if (handle == nullptr || symbol.empty()) {
        return std::unexpected{std::string{"invalid handle or empty symbol name"}};
    }
    const std::string name{symbol};
    if (void *address{dlsym(handle, name.c_str())}; address != nullptr) {
        return address;
    }
    return std::unexpected{"symbol not found: " + name + " (" + last_dlerror() + ")"};
}

std::expected<module_info, std::string>
module_of(const void *address) noexcept {
    if (address == nullptr) {
        return std::unexpected{std::string{"null address"}};
    }
    module_search search{.address = address};
    dl_iterate_phdr(&module_callback, &search);
    if (!search.found) {
        return std::unexpected{std::string{"address not inside any loaded module"}};
    }
    return search.info;
}

std::expected<module_info, std::string>
find_module(std::string_view name) noexcept {
    if (name.empty()) {
        return std::unexpected{std::string{"empty module name"}};
    }
    module_lookup lookup{.name = name};
    dl_iterate_phdr(&module_lookup_callback, &lookup);
    if (!lookup.found) {
        return std::unexpected{"module not loaded: " + std::string{name}};
    }
    return lookup.info;
}
} // namespace zephyrus::hook
