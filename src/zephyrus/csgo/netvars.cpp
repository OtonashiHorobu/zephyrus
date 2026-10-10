// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#include <zephyrus/csgo/netvars.hpp>
#include <zephyrus/log.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace zephyrus::csgo {
namespace {
struct recv_table;

struct recv_prop {
    char *name;
    int type;
    int flags;
    int string_buffer_size;
    int inside_array;
    const void *extra_data;
    recv_prop *array_prop;
    void *array_length_proxy;
    void *proxy;
    void *data_table_proxy;
    recv_table *data_table;
    int offset;
    int element_stride;
    int element_count;
    const char *parent_array_prop_name;
};

struct recv_table {
    recv_prop *props;
    int prop_count;
    void *decoder;
    char *net_table_name;
    bool is_initialized;
    bool is_in_main_list;
};

struct client_class {
    void *create_function;
    void *create_event_function;
    char *network_name;
    recv_table *table;
    client_class *next;
    int class_id;
};

std::vector<std::pair<std::uint32_t, std::uint32_t>> offsets;

void walk_table(const char *network_name, recv_table *table,
                const std::uint32_t offset = 0) noexcept {
    for (int i{}; i < table->prop_count; ++i) {
        const auto &prop{table->props[i]};

        if (fnv1a(prop.name) == fnv1a("baseclass")) {
            continue;
        }

        if (prop.type == 6 && prop.data_table != nullptr &&
            prop.data_table->net_table_name[0] == 'D') {
            walk_table(network_name, prop.data_table,
                       static_cast<std::uint32_t>(prop.offset) + offset);
        }

        const std::string path{std::string{network_name} + "->" + prop.name};
        offsets.emplace_back(fnv1a(path),
                             static_cast<std::uint32_t>(prop.offset) + offset);
    }
}
} // namespace

void netvars::init(void *client) noexcept {
    if (client == nullptr) {
        return;
    }

    auto **vtable{*reinterpret_cast<void ***>(client)};
    using get_all_classes_fn = client_class *(*)(void *);
    auto get_all_classes{reinterpret_cast<get_all_classes_fn>(vtable[8])};

    for (client_class *client_class{get_all_classes(client)};
         client_class != nullptr; client_class = client_class->next) {
        walk_table(client_class->network_name, client_class->table);
    }

    std::ranges::sort(offsets, {}, &std::pair<std::uint32_t, std::uint32_t>::first);
    offsets.shrink_to_fit();
    ZEPHYRUS_INFO("netvars: {} properties", offsets.size());
}

void netvars::shutdown() noexcept { offsets.clear(); }

std::uint32_t netvars::get(const std::uint32_t hash) noexcept {
    const auto it{std::ranges::lower_bound(
        offsets, hash, {}, &std::pair<std::uint32_t, std::uint32_t>::first)};
    if (it != offsets.end() && it->first == hash) {
        return it->second;
    }
    return 0;
}
} // namespace zephyrus::csgo
