// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/config/config.hpp>

#include <zephyrus/log.hpp>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace zephyrus::config {
namespace {
std::vector<std::string_view> split_path(std::string_view path) {
    std::vector<std::string_view> parts;
    std::size_t start{};
    while (start <= path.size()) {
        const std::size_t dot{path.find('.', start)};
        const std::size_t end{dot == std::string_view::npos ? path.size()
                                                            : dot};
        if (end > start) {
            parts.push_back(path.substr(start, end - start));
        }
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return parts;
}

std::filesystem::path config_root() {
    if (const char *xdg{std::getenv("XDG_CONFIG_HOME")};
        xdg != nullptr && xdg[0] != '\0') {
        return std::filesystem::path{xdg} / "zephyrus";
    }
    if (const char *home{std::getenv("HOME")};
        home != nullptr && home[0] != '\0') {
        return std::filesystem::path{home} / ".config" / "zephyrus";
    }
    return std::filesystem::path{"."};
}
} // namespace

void document::set_path(std::filesystem::path path) noexcept {
    path_ = std::move(path);
}

bool document::load() noexcept {
    if (path_.empty()) {
        return false;
    }
    std::error_code error;
    if (!std::filesystem::exists(path_, error)) {
        return false;
    }
    try {
        root_ = toml::parse_file(path_.string());
        ZEPHYRUS_INFO("config: loaded {}", path_.string());
        return true;
    } catch (const toml::parse_error &failure) {
        ZEPHYRUS_WARN("config: parse error in {}: {}", path_.string(),
                      std::string{failure.description()});
        root_ = toml::table{};
        return false;
    }
}

std::string document::dump() const {
    std::ostringstream stream;
    stream << toml::toml_formatter{root_};
    return stream.str();
}

bool document::save() const noexcept {
    if (path_.empty()) {
        return false;
    }
    std::error_code error;
    if (path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path(), error);
    }
    const std::filesystem::path temporary{path_.string() + ".tmp"};
    {
        std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
        if (!stream) {
            ZEPHYRUS_WARN("config: cannot open {} for writing",
                          temporary.string());
            return false;
        }
        const std::string text{dump()};
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        if (!stream) {
            ZEPHYRUS_WARN("config: write failed for {}", temporary.string());
            return false;
        }
    }
    std::filesystem::rename(temporary, path_, error);
    if (error) {
        ZEPHYRUS_WARN("config: rename failed: {}", error.message());
        return false;
    }
    ZEPHYRUS_TRACE("config: saved {}", path_.string());
    return true;
}

void document::replace_root(toml::table root) noexcept {
    root_ = std::move(root);
    mark_dirty();
}

void document::mark_dirty() noexcept {
    dirty_ = true;
    changed_at_ = std::chrono::steady_clock::now();
}

void document::tick() noexcept {
    if (!dirty_) {
        return;
    }
    if (std::chrono::steady_clock::now() - changed_at_ < autosave_delay) {
        return;
    }
    if (save()) {
        dirty_ = false;
    }
}

std::filesystem::path document::default_path(std::string_view game) {
    std::filesystem::path base{config_root()};
    if (!game.empty()) {
        base /= game;
    }
    return base / "latest.toml";
}

std::filesystem::path document::profiles_dir(std::string_view game) {
    std::filesystem::path base{config_root()};
    if (!game.empty()) {
        base /= game;
    }
    return base / "profiles";
}

void document::migrate_legacy(std::string_view game) noexcept {
    if (game.empty()) {
        return;
    }
    std::error_code error;
    const std::filesystem::path target_root{config_root() / game};
    std::filesystem::create_directories(target_root, error);

    const auto move_if_missing = [&error](const std::filesystem::path &from,
                                          const std::filesystem::path &to) {
        if (!std::filesystem::exists(from, error) ||
            std::filesystem::exists(to, error)) {
            return;
        }
        std::filesystem::rename(from, to, error);
        if (!error) {
            ZEPHYRUS_INFO("config: migrated {} -> {}", from.string(),
                          to.string());
            return;
        }
        error.clear();
        std::filesystem::copy(
            from, to,
            std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::skip_existing,
            error);
        if (!error) {
            ZEPHYRUS_INFO("config: copied {} -> {}", from.string(),
                          to.string());
        } else {
            ZEPHYRUS_WARN("config: cannot migrate {}: {}", from.string(),
                          error.message());
        }
    };

    move_if_missing(config_root() / "latest.toml",
                    target_root / "latest.toml");
    move_if_missing(config_root() / "profiles", target_root / "profiles");
}

toml::table *table_at(toml::table &root, std::string_view path) {
    toml::table *current{&root};
    for (const std::string_view part : split_path(path)) {
        toml::node *node{current->get(part)};
        if (node == nullptr) {
            return nullptr;
        }
        current = node->as_table();
        if (current == nullptr) {
            return nullptr;
        }
    }
    return current;
}

const toml::table *table_at(const toml::table &root, std::string_view path) {
    const toml::table *current{&root};
    for (const std::string_view part : split_path(path)) {
        const toml::node *node{current->get(part)};
        if (node == nullptr) {
            return nullptr;
        }
        current = node->as_table();
        if (current == nullptr) {
            return nullptr;
        }
    }
    return current;
}

toml::table &ensure_table(toml::table &root, std::string_view path) {
    toml::table *current{&root};
    for (const std::string_view part : split_path(path)) {
        toml::node *node{current->get(part)};
        toml::table *next{node != nullptr ? node->as_table() : nullptr};
        if (next == nullptr) {
            auto inserted{current->insert_or_assign(std::string{part},
                                                    toml::table{})};
            next = inserted.first->second.as_table();
        }
        current = next;
    }
    return *current;
}

bool read_bool(const toml::table &table, std::string_view key,
               bool fallback) noexcept {
    if (const toml::node *node{table.get(key)}; node != nullptr) {
        if (auto value{node->value<bool>()}; value.has_value()) {
            return *value;
        }
    }
    return fallback;
}

int64_t read_int(const toml::table &table, std::string_view key,
                 int64_t fallback) noexcept {
    if (const toml::node *node{table.get(key)}; node != nullptr) {
        if (auto value{node->value<int64_t>()}; value.has_value()) {
            return *value;
        }
    }
    return fallback;
}

double read_float(const toml::table &table, std::string_view key,
                  double fallback) noexcept {
    if (const toml::node *node{table.get(key)}; node != nullptr) {
        if (auto value{node->value<double>()}; value.has_value()) {
            return *value;
        }
        if (auto value{node->value<int64_t>()}; value.has_value()) {
            return static_cast<double>(*value);
        }
    }
    return fallback;
}

std::string read_string(const toml::table &table, std::string_view key,
                        std::string fallback) noexcept {
    if (const toml::node *node{table.get(key)}; node != nullptr) {
        if (auto value{node->value<std::string>()}; value.has_value()) {
            return *value;
        }
    }
    return fallback;
}

void write_keybind(toml::table &table, std::string_view key,
                   const keybind &bind) {
    toml::table entry{};
    const std::string_view name{key_name(bind.key)};
    if (bind.key != 0 && name != "Key" && name != "Unknown") {
        entry.insert("key", std::string{name});
    }
    entry.insert("keycode", static_cast<int64_t>(bind.key));
    entry.insert("mode", std::string{keybind_mode_name(bind.mode)});
    table.insert_or_assign(std::string{key}, std::move(entry));
}

void read_keybind(const toml::table &table, std::string_view key,
                  keybind &bind) {
    const toml::table *entry{table_at(table, key)};
    if (entry == nullptr) {
        return;
    }
    const std::string name{
        read_string(*entry, "key", std::string{key_name(bind.key)})};
    const int code{key_from_name(name)};
    if (code != 0) {
        bind.key = code;
    } else {
        bind.key = static_cast<int>(read_int(*entry, "keycode", bind.key));
    }
    bind.mode = keybind_mode_from_name(
        read_string(*entry, "mode", std::string{keybind_mode_name(bind.mode)}));
    bind.reset();
}
} // namespace zephyrus::config
