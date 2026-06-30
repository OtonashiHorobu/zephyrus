// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <mutex>
#include <unordered_set>

#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Types.h>
#include <cmrc/cmrc.hpp>

namespace zephyrus::rmlui {
struct cmrc_file_handle_data;

/**
 * @brief RmlUi file interface for CMRC embedded file systems
 *
 */
class cmrc_file_interface : public Rml::FileInterface {
  public:
    explicit cmrc_file_interface(const cmrc::embedded_filesystem &fs) noexcept;
    virtual ~cmrc_file_interface() noexcept;

    Rml::FileHandle Open(const Rml::String &path) noexcept override;
    void Close(Rml::FileHandle file) noexcept override;
    size_t Read(void *buffer, size_t size,
                Rml::FileHandle file) noexcept override;
    bool Seek(Rml::FileHandle file, long offset, int origin) noexcept override;
    size_t Tell(Rml::FileHandle file) noexcept override;

  private:
    cmrc::embedded_filesystem fs_;
    std::unordered_set<cmrc_file_handle_data *> open_files_;
    mutable std::mutex mutex_;
};
} // namespace zephyrus::rmlui
