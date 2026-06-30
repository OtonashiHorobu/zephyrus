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
    for (auto *const ptr : open_files_) {
        ZEPHYRUS_WARN("cmrc_file_interface: freeing stale file pointer {:#x}",
                      reinterpret_cast<std::uintptr_t>(ptr));
        delete ptr;
    }
}

Rml::FileHandle cmrc_file_interface::Open(const Rml::String &path) noexcept {
    std::lock_guard lock{mutex_};
    try {
        const auto handle{new cmrc_file_handle_data{fs_.open(path)}};
        ZEPHYRUS_TRACE("cmrc_file_interface: opening {} @ {:#x}", path,
                       reinterpret_cast<std::uintptr_t>(handle->file.begin()));
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
    if (!file) {
        ZEPHYRUS_WARN("cmrc_file_interface: null file pointer in Close()");
        return;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (open_files_.erase(handle)) {
        ZEPHYRUS_TRACE("cmrc_file_interface: closing file @ {:#x}",
                       reinterpret_cast<std::uintptr_t>(handle->file.begin()));
        delete handle;
    } else {
        ZEPHYRUS_WARN(
            "cmrc_file_interface: invalid file pointer {:#x} in Close()",
            reinterpret_cast<std::uintptr_t>(handle));
    }
}

size_t cmrc_file_interface::Read(void *const buffer, const size_t size,
                                 const Rml::FileHandle file) noexcept {
    if (!buffer || !file) {
        ZEPHYRUS_WARN(
            "cmrc_file_interface: null buffer or file pointer in Read()");
        return 0;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        ZEPHYRUS_WARN(
            "cmrc_file_interface: invalid file pointer {:#x} in Read()",
            reinterpret_cast<std::uintptr_t>(handle));
        return 0;
    }

    ZEPHYRUS_TRACE("cmrc_file_interface: reading file @ {:#x}",
                   reinterpret_cast<std::uintptr_t>(handle->file.begin()));
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
    if (!file) {
        ZEPHYRUS_WARN("cmrc_file_interface: null file pointer in Seek()");
        return false;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        ZEPHYRUS_WARN(
            "cmrc_file_interface: invalid file pointer {:#x} in Seek()",
            reinterpret_cast<std::uintptr_t>(handle));
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
        ZEPHYRUS_WARN("cmrc_file_interface: invalid origin {} in Seek()",
                      origin);
        return false;
    }

    handle->cursor =
        std::clamp(base + offset, handle->file.begin(), handle->file.end());
    ZEPHYRUS_TRACE("cmrc_file_interface: seeking file @ {:#x} to {:#x}",
                   reinterpret_cast<std::uintptr_t>(handle->file.begin()),
                   reinterpret_cast<std::uintptr_t>(handle->cursor));
    return true;
}

size_t cmrc_file_interface::Tell(const Rml::FileHandle file) noexcept {
    if (!file) {
        ZEPHYRUS_WARN("cmrc_file_interface: null file pointer in Tell()");
        return false;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        ZEPHYRUS_WARN(
            "cmrc_file_interface: invalid file pointer {:#x} in Tell()",
            reinterpret_cast<std::uintptr_t>(handle));
        return 0;
    }

    return static_cast<size_t>(
        std::distance(handle->file.begin(), handle->cursor));
}
} // namespace zephyrus::rmlui
