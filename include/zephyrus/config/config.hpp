// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/input/keybind.hpp>

#include <toml++/toml.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace zephyrus::config {
/// TOML-backed configuration document with debounced autosave.
///
/// The document itself is a plain `toml::table` tree; the mapping between the
/// tree and the feature options lives with the owner of those options (the
/// overlay). `tick()` writes the file back once the tree has been stable for
/// `autosave_delay` (mark_dirty is implicit through `replace_root`).
class document {
  public:
    document() noexcept = default;
    ~document() = default;

    void set_path(std::filesystem::path path) noexcept;
    [[nodiscard]] const std::filesystem::path &path() const noexcept {
        return path_;
    }

    /// Parse the file if it exists; missing/broken files leave an empty tree.
    [[nodiscard]] bool load() noexcept;
    /// Serialize and write the tree atomically.
    [[nodiscard]] bool save() const noexcept;
    /// Serialized text of the current tree (used for change detection).
    [[nodiscard]] std::string dump() const;

    /// Replace the tree after external modification.
    void replace_root(toml::table root) noexcept;
    [[nodiscard]] toml::table &root() noexcept { return root_; }
    [[nodiscard]] const toml::table &root() const noexcept { return root_; }

    /// Note an external change; the autosave timer restarts.
    void mark_dirty() noexcept;
    /// Call once per frame; writes when the debounce window elapsed.
    void tick() noexcept;
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void clear_dirty() noexcept { dirty_ = false; }

    /// `~/.config/zephyrus/latest.toml`, or
    /// `~/.config/zephyrus/<game>/latest.toml` when `game` is set.
    /// The empty game keeps the legacy top-level location (recovery snapshot,
    /// written on changes but never loaded implicitly).
    [[nodiscard]] static std::filesystem::path
    default_path(std::string_view game = {});
    /// `~/.config/zephyrus/<game>/profiles` (legacy top-level `profiles` when
    /// `game` is empty) for user-managed profiles.
    [[nodiscard]] static std::filesystem::path
    profiles_dir(std::string_view game = {});

    /// One-shot migration of the pre-split top-level `latest.toml` and
    /// `profiles/` into `<game>/`. Never overwrites an existing target and
    /// never removes the source when moving fails; safe to call repeatedly.
    static void migrate_legacy(std::string_view game) noexcept;

    static constexpr std::chrono::milliseconds autosave_delay{2000};

  private:
    std::filesystem::path path_;
    toml::table root_{};
    bool dirty_{};
    std::chrono::steady_clock::time_point changed_at_{};
};

/// Nested-table navigation with `a.b.c` paths. `ensure` creates missing
/// levels; the const variant returns nullptr when a level is absent.
[[nodiscard]] toml::table *table_at(toml::table &root, std::string_view path);
[[nodiscard]] const toml::table *table_at(const toml::table &root,
                                          std::string_view path);
[[nodiscard]] toml::table &ensure_table(toml::table &root,
                                        std::string_view path);

// Scalar readers with defaults.
[[nodiscard]] bool read_bool(const toml::table &table, std::string_view key,
                             bool fallback) noexcept;
[[nodiscard]] int64_t read_int(const toml::table &table, std::string_view key,
                               int64_t fallback) noexcept;
[[nodiscard]] double read_float(const toml::table &table, std::string_view key,
                                double fallback) noexcept;
[[nodiscard]] std::string read_string(const toml::table &table,
                                      std::string_view key,
                                      std::string fallback) noexcept;

/// Keybind as `{ key = "X", mode = "toggle" }` (keycode fallback for unknown
/// names).
void write_keybind(toml::table &table, std::string_view key,
                   const keybind &bind);
void read_keybind(const toml::table &table, std::string_view key,
                  keybind &bind);
} // namespace zephyrus::config
