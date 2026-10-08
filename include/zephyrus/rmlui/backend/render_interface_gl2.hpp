// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace zephyrus::backend {
/// OpenGL 2.x fixed-function render interface for RmlUi.
///
/// In addition to the basic geometry/texture path this backend implements the
/// optional layer API (FBO backed), which is required for `box-shadow`,
/// `filter` and `mask-image` effects. Filters other than masks are not
/// compiled, `box-shadow` renders without the optional blur pass.
class render_interface_gl2 : public Rml::RenderInterface {
  public:
    render_interface_gl2() noexcept = default;
    ~render_interface_gl2() override;

    render_interface_gl2(const render_interface_gl2 &) = delete;
    render_interface_gl2 &operator=(const render_interface_gl2 &) = delete;

    void set_viewport(int viewport_width, int viewport_height);
    void begin_frame();
    void end_frame();
    void clear();

    /// Free all GPU resources; must be called while a context is current.
    void release_gpu_resources();

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

    // Layer API (FBO backed).
    Rml::LayerHandle PushLayer() override;
    void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination,
                         Rml::BlendMode blend_mode,
                         Rml::Span<const Rml::CompiledFilterHandle> filters) override;
    void PopLayer() override;
    Rml::TextureHandle SaveLayerAsTexture() override;

    // Filter API. Filters are applied by rendering the layer through small
    // GLSL programs into post-process targets (see `apply_filters`).
    Rml::CompiledFilterHandle
    CompileFilter(const Rml::String &name,
                  const Rml::Dictionary &parameters) override;
    void ReleaseFilter(Rml::CompiledFilterHandle filter) override;

    static const auto TextureEnableWithoutBinding{
        static_cast<Rml::TextureHandle>(-1)};

  private:
    struct geometry_view {
        Rml::Span<const Rml::Vertex> vertices;
        Rml::Span<const int> indices;
    };

    struct layer_entry {
        unsigned int framebuffer{};
        unsigned int texture{};
        unsigned int depth_stencil{};
        int width{};
        int height{};
    };

    /// One compiled `filter` property value.
    struct filter_entry {
        enum class kind {
            passthrough,
            blur,
            drop_shadow,
            color_matrix
        } type{kind::passthrough};
        /// Passthrough: alpha multiplier. Blur/drop-shadow: blur sigma in px.
        float amount{1.0F};
        /// Drop-shadow colour (premultiplied) and screen space offset.
        Rml::ColourbPremultiplied color{};
        Rml::Vector2f drop_offset{};
        /// Colour matrix and additive term used by the CSS colour filters.
        Rml::Matrix4f matrix{Rml::Matrix4f::Identity()};
        Rml::Vector4f matrix_offset{};
    };

    /// GL 2.0 shader entry points, resolved lazily like the FBO ones.
    struct shader_api {
        bool available{};
        unsigned int (*create_shader)(unsigned int type){};
        void (*shader_source)(unsigned int shader, int count,
                              const char *const *string,
                              const int *length){};
        void (*compile_shader)(unsigned int shader){};
        void (*get_shader_iv)(unsigned int shader, unsigned int pname,
                              int *params){};
        void (*get_shader_info_log)(unsigned int shader, int max_length,
                                    int *length, char *info_log){};
        void (*delete_shader)(unsigned int shader){};
        unsigned int (*create_program)(){};
        void (*attach_shader)(unsigned int program, unsigned int shader){};
        void (*link_program)(unsigned int program){};
        void (*get_program_iv)(unsigned int program, unsigned int pname,
                               int *params){};
        void (*get_program_info_log)(unsigned int program, int max_length,
                                     int *length, char *info_log){};
        void (*delete_program)(unsigned int program){};
        void (*use_program)(unsigned int program){};
        int (*get_uniform_location)(unsigned int program, const char *name){};
        void (*uniform1f)(int location, float v0){};
        void (*uniform2f)(int location, float v0, float v1){};
        void (*uniform4f)(int location, float v0, float v1, float v2,
                          float v3){};
        void (*uniform1fv)(int location, int count, const float *value){};
        void (*uniform_matrix4fv)(int location, int count, unsigned char transpose,
                                  const float *value){};
    };

    /// Shader programs used by the filter chain.
    struct filter_programs {
        unsigned int blur{};
        unsigned int color_matrix{};
        int blur_texture{-1};
        int blur_direction{-1};
        int blur_bounds_min{-1};
        int blur_bounds_max{-1};
        int blur_weights{-1};
        int blur_radius{-1};
        int color_matrix_texture{-1};
        int color_matrix_value{-1};
        int color_matrix_offset{-1};
    };

    [[nodiscard]] static const shader_api &shaders();
    [[nodiscard]] bool ensure_filter_programs();
    void destroy_filter_programs();

    /// Run the filter chain over the source texture. On success the result is
    /// stored in one of the post process targets; `out_u/out_v` give the used
    /// fraction of that texture (down sampled passes only fill the lower left
    /// corner). Returns false when the filter path is unavailable.
    [[nodiscard]] bool
    apply_filters(unsigned int source_texture,
                  Rml::Span<const Rml::CompiledFilterHandle> filters,
                  unsigned int &out_texture, float &out_u, float &out_v);
    [[nodiscard]] bool ensure_post_storage();
    void destroy_post_storage();

    /// Full viewport quad sampling `texture` over
    /// [u_off, u_max + u_off] x [v_off, v_max + v_off]; flipped vertically
    /// like `draw_layer_quad` does.
    void draw_quad_uv(unsigned int texture, float u_max, float v_max,
                      float u_off = 0.0F, float v_off = 0.0F) noexcept;

    /// Current scissor region converted to normalised texture coordinates of
    /// a bottom-up full viewport texture (used to clamp blur taps).
    void read_clip_region(float &u_min, float &v_min, float &u_max,
                          float &v_max) const noexcept;

    /// Resolve the framebuffer entry points lazily; returns false when the
    /// GL driver does not support framebuffer objects.
    [[nodiscard]] bool ensure_layer_support();

    [[nodiscard]] layer_entry *acquire_layer();
    [[nodiscard]] bool ensure_layer_storage(layer_entry &layer);
    void destroy_layer(layer_entry &layer);
    void sync_layer_pool();
    void bind_layer_target(const layer_entry *layer);

    /// Draw a full-viewport textured quad into the currently bound target,
    /// used to composite layers. Assumes the frame's orthographic projection.
    void draw_layer_quad(unsigned int texture, int x, int y, int width,
                         int height, bool flip_vertical);

    void restore_frame_projection();

    int viewport_width_{};
    int viewport_height_{};
    int layer_viewport_width_{};
    int layer_viewport_height_{};
    unsigned int main_framebuffer_{};
    bool transform_enabled_{};

    std::vector<std::unique_ptr<layer_entry>> layers_;
    std::vector<layer_entry *> layer_stack_;
    std::vector<layer_entry *> free_layers_;
    bool layer_support_checked_{};
    bool layer_support_{};

    /// Post-process ping-pong targets, one pair per down sample level (level 0
    /// is the full viewport size, each following level halves it). The filter
    /// passes always fill a whole target, which keeps the texture coordinates
    /// independent of the viewport (layer contents are positioned in screen
    /// space, not in a corner of the texture).
    static constexpr int post_levels{4};
    layer_entry post_[post_levels][2];
    bool post_storage_ready_{};
    filter_programs programs_{};
    bool programs_ready_{};
    bool programs_failed_{};
};
} // namespace zephyrus::backend
