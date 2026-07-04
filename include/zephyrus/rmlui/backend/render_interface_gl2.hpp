// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <RmlUi/Core/RenderInterface.h>

namespace zephyrus::backend {
class render_interface_gl2 : public Rml::RenderInterface {
  public:
    render_interface_gl2() = default;

    void set_viewport(int viewport_width, int viewport_height);
    void begin_frame();
    void end_frame();
    void clear();

    Rml::CompiledGeometryHandle
    CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                    Rml::Span<const int> indices) override;

    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    void RenderGeometry(Rml::CompiledGeometryHandle handle,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i &texture_dimensions,
                                   const Rml::String &source) override;

    Rml::TextureHandle
    GenerateTexture(Rml::Span<const Rml::byte> source,
                    Rml::Vector2i source_dimensions) override;

    void ReleaseTexture(Rml::TextureHandle texture_handle) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

    void EnableClipMask(bool enable) override;
    void RenderToClipMask(Rml::ClipMaskOperation operation,
                          Rml::CompiledGeometryHandle geometry,
                          Rml::Vector2f translation) override;

    void SetTransform(const Rml::Matrix4f *transform) override;

    static const auto TextureEnableWithoutBinding{
        static_cast<Rml::TextureHandle>(-1)};

  private:
    struct geometry_view {
        Rml::Span<const Rml::Vertex> vertices;
        Rml::Span<const int> indices;
    };

    int viewport_width_{};
    int viewport_height_{};
    bool transform_enabled_{};
};
} // namespace zephyrus::backend
