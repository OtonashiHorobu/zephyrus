// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/cmrc_file_interface.hpp>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <vector>

namespace zephyrus::rmlui {
struct cmrc_file_handle_data {
    cmrc::file file;
    std::vector<char> external;
    std::size_t cursor{};
    bool external_file{};

    explicit cmrc_file_handle_data(cmrc::file f) noexcept
        : file{std::move(f)} {}

    explicit cmrc_file_handle_data(std::vector<char> data) noexcept
        : external{std::move(data)}, external_file{true} {}

    [[nodiscard]] std::size_t size() const noexcept {
        if (external_file) {
            return external.size();
        }
        return static_cast<std::size_t>(
            std::distance(file.begin(), file.end()));
    }
};

cmrc_file_interface::cmrc_file_interface(cmrc::embedded_filesystem fs) noexcept
    : fs_{std::move(fs)} {}

cmrc_file_interface::~cmrc_file_interface() noexcept {
    std::lock_guard lock{mutex_};
    for (auto *const ptr : open_files_) {
        ZEPHYRUS_DEBUG("freeing stale file pointer {:#x}",
                       reinterpret_cast<std::uintptr_t>(ptr));
        delete ptr;
    }
}

Rml::FileHandle cmrc_file_interface::Open(const Rml::String &path) noexcept {
    std::lock_guard lock{mutex_};

    try {
        const auto handle{new cmrc_file_handle_data{fs_.open(path)}};
        ZEPHYRUS_TRACE("opening {} @ {:#x}", path,
                       reinterpret_cast<std::uintptr_t>(handle->file.begin()));
        open_files_.insert(handle);
        return reinterpret_cast<Rml::FileHandle>(handle);
    } catch (const std::exception &) { // fall back to the real filesystem
    } catch (...) {
        ZEPHYRUS_ERROR("unknown exception in Open()");
        return 0;
    }

    std::ifstream stream{Rml::String{path}, std::ios::binary};
    if (!stream) {
        ZEPHYRUS_ERROR("file not found: {}", path);
        return 0;
    }

    std::vector<char> data{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
    ZEPHYRUS_TRACE("opening external {} ({} bytes)", path, data.size());
    const auto handle{new cmrc_file_handle_data{std::move(data)}};
    open_files_.insert(handle);
    return reinterpret_cast<Rml::FileHandle>(handle);
}

void cmrc_file_interface::Close(const Rml::FileHandle file) noexcept {
    if (!file) {
        ZEPHYRUS_DEBUG("null file pointer in Close()");
        return;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (open_files_.erase(handle)) {
        delete handle;
    } else {
        ZEPHYRUS_DEBUG("invalid file pointer {:#x} in Close()",
                       reinterpret_cast<std::uintptr_t>(handle));
    }
}

size_t cmrc_file_interface::Read(void *const buffer, const size_t size,
                                 const Rml::FileHandle file) noexcept {
    if (!buffer || !file) {
        ZEPHYRUS_DEBUG("null buffer or file pointer in Read()");
        return 0;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        ZEPHYRUS_DEBUG("invalid file pointer {:#x} in Read()",
                       reinterpret_cast<std::uintptr_t>(handle));
        return 0;
    }

    const std::size_t remaining{handle->size() - handle->cursor};
    const size_t to_read{(std::min)(size, remaining)};
    if (to_read == 0) {
        return 0;
    }

    if (handle->external_file) {
        std::copy_n(handle->external.data() + handle->cursor, to_read,
                    static_cast<char *>(buffer));
    } else {
        std::copy_n(handle->file.begin() + handle->cursor, to_read,
                    static_cast<char *>(buffer));
    }
    handle->cursor += to_read;
    return to_read;
}

bool cmrc_file_interface::Seek(const Rml::FileHandle file, const long offset,
                               const int origin) noexcept {
    if (!file) {
        ZEPHYRUS_DEBUG("null file pointer in Seek()");
        return false;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        ZEPHYRUS_DEBUG("invalid file pointer {:#x} in Seek()",
                       reinterpret_cast<std::uintptr_t>(handle));
        return false;
    }

    const auto size{handle->size()};
    std::ptrdiff_t base{};
    switch (origin) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = static_cast<std::ptrdiff_t>(handle->cursor);
        break;
    case SEEK_END:
        base = static_cast<std::ptrdiff_t>(size);
        break;
    default:
        ZEPHYRUS_DEBUG("invalid origin {} in Seek()", origin);
        return false;
    }

    const auto target{std::clamp<std::ptrdiff_t>(
        base + offset, 0, static_cast<std::ptrdiff_t>(size))};
    handle->cursor = static_cast<std::size_t>(target);
    return true;
}

size_t cmrc_file_interface::Tell(const Rml::FileHandle file) noexcept {
    if (!file) {
        ZEPHYRUS_DEBUG("null file pointer in Tell()");
        return 0;
    }

    std::lock_guard lock{mutex_};
    auto *const handle{reinterpret_cast<cmrc_file_handle_data *>(file)};
    if (!open_files_.contains(handle)) {
        ZEPHYRUS_DEBUG("invalid file pointer {:#x} in Tell()",
                       reinterpret_cast<std::uintptr_t>(handle));
        return 0;
    }

    return handle->cursor;
}
} // namespace zephyrus::rmlui
