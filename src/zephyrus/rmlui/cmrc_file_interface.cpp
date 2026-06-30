// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/cmrc_file_interface.hpp>

namespace zephyrus::rmlui {
struct cmrc_file_handle_data {
    cmrc::file file;
    cmrc::file::iterator cursor;

    explicit cmrc_file_handle_data(const cmrc::file f) noexcept
        : file{f}, cursor{file.begin()} {}
}; // namespace zephyrus::rmlui

cmrc_file_interface::cmrc_file_interface(
    const cmrc::embedded_filesystem &fs) noexcept
    : fs_{fs} {}

cmrc_file_interface::~cmrc_file_interface() noexcept {
    std::lock_guard lock{mutex_};
    ZEPHYRUS_TRACE("cmrc_file_interface: freeing {} file pointers",
                   open_files_.size());
    for (auto *const ptr : open_files_) {
        delete ptr;
    }
}

Rml::FileHandle cmrc_file_interface::Open(const Rml::String &path) noexcept {
    std::lock_guard lock{mutex_};
    try {
        const auto handle{new cmrc_file_handle_data{fs_.open(path)}};
        open_files_.insert(handle);
        return reinterpret_cast<Rml::FileHandle>(handle);
    } catch (const std::exception &e) { // no such file or directory
        ZEPHYRUS_ERROR("cmrc_file_interface: {}", e.what());
        return 0;
    } catch (...) {
        ZEPHYRUS_WARN("cmrc_file_interface: unknown exception");
        return 0;
    }
}

void cmrc_file_interface::Close(const Rml::FileHandle file) noexcept {
    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (open_files_.erase(handle)) {
        delete handle;
    }
}

size_t cmrc_file_interface::Read(void *const buffer, const size_t size,
                                 const Rml::FileHandle file) noexcept {
    if (!buffer) {
        return 0;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        return 0;
    }

    const auto remaining{static_cast<std::size_t>(
        std::distance(handle->cursor, handle->file.end()))};
    const size_t to_read{(std::min)(size, remaining)};
    if (to_read) {
        auto *const dst{static_cast<char *>(buffer)};
        std::copy(handle->cursor, handle->cursor + to_read, dst);
        handle->cursor += to_read;
    }
    return to_read;
}

bool cmrc_file_interface::Seek(const Rml::FileHandle file, const long offset,
                               const int origin) noexcept {
    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        return false;
    }

    cmrc::file::iterator base{};
    switch (origin) {
    case SEEK_SET:
        base = handle->file.begin();
        break;
    case SEEK_CUR:
        base = handle->cursor;
        break;
    case SEEK_END:
        base = handle->file.end();
        break;
    default:
        return false;
    }

    handle->cursor =
        std::clamp(base + offset, handle->file.begin(), handle->file.end());
    return true;
}

size_t cmrc_file_interface::Tell(const Rml::FileHandle file) noexcept {
    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        return 0;
    }

    return static_cast<size_t>(
        std::distance(handle->file.begin(), handle->cursor));
}
} // namespace zephyrus::rmlui
