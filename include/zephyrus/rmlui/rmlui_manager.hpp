// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/abstract/manager.hpp>
#include <zephyrus/rmlui/cmrc_file_interface.hpp>

#include <memory>
#include <string_view>

namespace Rml {
class Context;
class ElementDocument;
}

namespace zephyrus {
enum class backend_mode;
}

namespace zephyrus::rmlui {
class rmlui_manager : public abstract::manager<rmlui_manager> {
  public:
    rmlui_manager() noexcept : manager<rmlui_manager>{} {}
    explicit rmlui_manager(const cmrc::embedded_filesystem fs,
                           const backend_mode mode) noexcept
        : manager<rmlui_manager>{},
          file_interface_{std::make_unique<cmrc_file_interface>(fs)},
          mode_{mode} {}

    result_type init_impl() noexcept;
    void shutdown_impl() noexcept;
    [[nodiscard]] Rml::Context *context() const noexcept { return context_; }

    /// The most recently loaded document, or nullptr.
    [[nodiscard]] Rml::ElementDocument *document() const noexcept {
        return document_;
    }

    /// Follow the host window size (injected mode) or the standalone window.
    void set_dimensions(int width, int height) noexcept;

    /// Advance document animations and timers.
    void update() noexcept;

    /// Render the context with the currently bound OpenGL context.
    void render() noexcept;

    /// Load a document from the configured file interface. An optional id
    /// makes it reachable through `Context::GetDocument`.
    [[nodiscard]] result_type
    load_document(std::string_view path, std::string_view id = {}) noexcept;

  private:
    std::unique_ptr<cmrc_file_interface> file_interface_;
    backend_mode mode_;
    Rml::Context *context_{};
    Rml::ElementDocument *document_{};
    int width_{1280};
    int height_{720};
};
} // namespace zephyrus::rmlui
