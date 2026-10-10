// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/rmlui/backend/render_interface_gl2.hpp>

#include <RmlUi/Core/Colour.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Dictionary.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Math.h>
#include <RmlUi/Core/Matrix4.h>
#include <RmlUi/Core/Platform.h>
#include <SDL/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <vector>

#if defined RMLUI_PLATFORM_WIN32
#include "RmlUi_Include_Windows.h"
#include <gl/Gl.h>
#elif defined RMLUI_PLATFORM_MACOSX
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#elif defined RMLUI_PLATFORM_UNIX
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif

namespace zephyrus::backend {
namespace {
/// Framebuffer entry points resolved at runtime so the injected library does
/// not need to link against GL 3.0 symbol sets.
struct layer_api {
    bool available{};
    void (*gen_framebuffers)(int, unsigned int *){};
    void (*delete_framebuffers)(int, const unsigned int *){};
    void (*bind_framebuffer)(unsigned int target, unsigned int framebuffer){};
    void (*framebuffer_texture2d)(unsigned int target, unsigned int attachment,
                                  unsigned int textarget, unsigned int texture,
                                  int level){};
    unsigned int (*check_framebuffer_status)(unsigned int target){};
    void (*gen_renderbuffers)(int, unsigned int *){};
    void (*delete_renderbuffers)(int, const unsigned int *){};
    void (*bind_renderbuffer)(unsigned int target, unsigned int renderbuffer){};
    void (*renderbuffer_storage)(unsigned int target, unsigned int internalformat,
                                 int width, int height){};
    void (*framebuffer_renderbuffer)(unsigned int target,
                                     unsigned int attachment,
                                     unsigned int renderbuffertarget,
                                     unsigned int renderbuffer){};
};

void *gl_proc(const char *name) {
    if (auto *proc = reinterpret_cast<void *>(glXGetProcAddressARB(
            reinterpret_cast<const GLubyte *>(name)));
        proc != nullptr) {
        return proc;
    }
    return dlsym(RTLD_DEFAULT, name);
}

template <typename T> void load_proc(T &target, const char *name) {
    target = reinterpret_cast<T>(gl_proc(name));
}

/// Entry points needed to protect the host's state (only available with GL 3).
struct state_api {
    void (*bind_vertex_array)(unsigned int){};
    void (*use_program)(unsigned int){};
};

const state_api &state_functions() {
    static const state_api api{[] {
        state_api result{};
        load_proc(result.bind_vertex_array, "glBindVertexArray");
        load_proc(result.use_program, "glUseProgram");
        return result;
    }()};
    return api;
}

const layer_api &layer_functions() {
    static const layer_api api{[] {
        layer_api result{};
        load_proc(result.gen_framebuffers, "glGenFramebuffers");
        load_proc(result.delete_framebuffers, "glDeleteFramebuffers");
        load_proc(result.bind_framebuffer, "glBindFramebuffer");
        load_proc(result.framebuffer_texture2d, "glFramebufferTexture2D");
        load_proc(result.check_framebuffer_status, "glCheckFramebufferStatus");
        load_proc(result.gen_renderbuffers, "glGenRenderbuffers");
        load_proc(result.delete_renderbuffers, "glDeleteRenderbuffers");
        load_proc(result.bind_renderbuffer, "glBindRenderbuffer");
        load_proc(result.renderbuffer_storage, "glRenderbufferStorage");
        load_proc(result.framebuffer_renderbuffer, "glFramebufferRenderbuffer");
        result.available = result.gen_framebuffers != nullptr &&
                           result.delete_framebuffers != nullptr &&
                           result.bind_framebuffer != nullptr &&
                           result.framebuffer_texture2d != nullptr &&
                           result.check_framebuffer_status != nullptr &&
                           result.gen_renderbuffers != nullptr &&
                           result.delete_renderbuffers != nullptr &&
                           result.bind_renderbuffer != nullptr &&
                           result.renderbuffer_storage != nullptr &&
                           result.framebuffer_renderbuffer != nullptr;
        return result;
    }()};
    return api;
}

/// SDL_image entry points resolved at runtime from the host process; the
/// injected library never links SDL itself.
struct sdl_image_api {
    bool available{};
    SDL_Surface *(*load)(const char *file){};
    void (*free_surface)(SDL_Surface *surface){};
    int (*lock_surface)(SDL_Surface *surface){};
    void (*unlock_surface)(SDL_Surface *surface){};
    void (*get_rgba)(unsigned int pixel, const SDL_PixelFormat *format,
                     unsigned char *r, unsigned char *g, unsigned char *b,
                     unsigned char *a){};
};

const sdl_image_api &sdl_image() {
    static const sdl_image_api api{[] {
        sdl_image_api result{};
        load_proc(result.load, "IMG_Load");
        load_proc(result.free_surface, "SDL_FreeSurface");
        load_proc(result.lock_surface, "SDL_LockSurface");
        load_proc(result.unlock_surface, "SDL_UnlockSurface");
        load_proc(result.get_rgba, "SDL_GetRGBA");
        result.available = result.load != nullptr &&
                           result.free_surface != nullptr &&
                           result.lock_surface != nullptr &&
                           result.unlock_surface != nullptr &&
                           result.get_rgba != nullptr;
        return result;
    }()};
    return api;
}
} // namespace

render_interface_gl2::~render_interface_gl2() = default;

void render_interface_gl2::set_viewport(int in_viewport_width,
                                        int in_viewport_height) {
    viewport_width_ = in_viewport_width;
    viewport_height_ = in_viewport_height;
}

void render_interface_gl2::begin_frame() {
    RMLUI_ASSERT(viewport_width_ >= 0 && viewport_height_ >= 0);

    // Capture everything the fixed-function path needs to be alone with the
    // GPU: a host GLSL program would ignore glVertexPointer/glMatrixMode, and
    // depth/cull state would clip the overlay. Restored in end_frame().
    if (!state_saved_) {
        state_saved_ = true;
        glGetIntegerv(GL_CURRENT_PROGRAM, &saved_program_);
        saved_depth_test_ = glIsEnabled(GL_DEPTH_TEST);
        GLboolean depth_mask{GL_TRUE};
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
        saved_depth_mask_ = depth_mask == GL_TRUE ? 1 : 0;
        saved_cull_face_ = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &saved_active_texture_);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved_texture_2d_);
        saved_blend_ = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC_RGB, &saved_blend_src_);
        glGetIntegerv(GL_BLEND_DST_RGB, &saved_blend_dst_);
        saved_stencil_ = glIsEnabled(GL_STENCIL_TEST);
        saved_scissor_ = glIsEnabled(GL_SCISSOR_TEST);
        saved_vertex_array_ = 0;
        if (const state_api &api{state_functions()};
            api.bind_vertex_array != nullptr) {
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &saved_vertex_array_);
            api.bind_vertex_array(0);
        } else {
            saved_vertex_array_ = 0;
        }
    }
    if (saved_program_ != 0) {
        if (const state_api &api{state_functions()};
            api.use_program != nullptr) {
            api.use_program(0);
        }
    }
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    // Remember the target the host is rendering into. The injected overlay
    // must composite back into that same framebuffer: the game may render to
    // its own FBO and present it itself, binding framebuffer 0 here would
    // silently redirect every RmlUi draw into an offscreen target (the UI
    // disappears as soon as a layer, e.g. a box-shadow, is used).
    GLint framebuffer{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    main_framebuffer_ = static_cast<unsigned int>(framebuffer);

    // Layer render targets belong to the host's GL context, which the game
    // may recreate when its video mode changes; drop them on resize so they
    // are rebuilt for the new context.
    if (viewport_width_ != layer_viewport_width_ ||
        viewport_height_ != layer_viewport_height_) {
        layer_viewport_width_ = viewport_width_;
        layer_viewport_height_ = viewport_height_;
        for (auto &layer : layers_) {
            destroy_layer(*layer);
        }
        layers_.clear();
        layer_stack_.clear();
        free_layers_.clear();
        destroy_post_storage();
    }

    glViewport(0, 0, viewport_width_, viewport_height_);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, GLuint(-1));
    glStencilMask(GLuint(-1));
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    Rml::Matrix4f projection = Rml::Matrix4f::ProjectOrtho(
        0, static_cast<float>(viewport_width_),
        static_cast<float>(viewport_height_), 0, -10000, 10000);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection.data());
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    transform_enabled_ = false;

    // All layers are reusable at the start of a frame; RmlUi balances every
    // PushLayer with a PopLayer within the frame.
    if (!layer_stack_.empty()) {
        layer_stack_.clear();
    }
    sync_layer_pool();
}

void render_interface_gl2::end_frame() {
    if (!state_saved_) {
        return;
    }
    state_saved_ = false;

    if (saved_depth_test_ != 0) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(saved_depth_mask_ != 0 ? GL_TRUE : GL_FALSE);
    if (saved_cull_face_ != 0) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }

    if (saved_active_texture_ != 0) {
        glActiveTexture(static_cast<GLenum>(saved_active_texture_));
    }
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(saved_texture_2d_));

    if (saved_blend_ != 0) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    if (saved_blend_src_ != 0 || saved_blend_dst_ != 0) {
        glBlendFunc(static_cast<GLenum>(saved_blend_src_),
                    static_cast<GLenum>(saved_blend_dst_));
    }
    if (saved_stencil_ != 0) {
        glEnable(GL_STENCIL_TEST);
    } else {
        glDisable(GL_STENCIL_TEST);
    }
    if (saved_scissor_ != 0) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }

    if (const state_api &api{state_functions()};
        api.bind_vertex_array != nullptr && saved_vertex_array_ != 0) {
        api.bind_vertex_array(
            static_cast<unsigned int>(saved_vertex_array_));
    }

    if (saved_program_ != 0) {
        if (const state_api &api{state_functions()};
            api.use_program != nullptr) {
            api.use_program(static_cast<unsigned int>(saved_program_));
        }
    }
    saved_program_ = 0;
    saved_vertex_array_ = 0;
}

void render_interface_gl2::clear() {
    glClearStencil(0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

Rml::CompiledGeometryHandle
render_interface_gl2::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                      Rml::Span<const int> indices) {
    auto *data = new geometry_view{vertices, indices};
    return reinterpret_cast<Rml::CompiledGeometryHandle>(data);
}

void render_interface_gl2::ReleaseGeometry(
    Rml::CompiledGeometryHandle geometry) {
    delete reinterpret_cast<geometry_view *>(geometry);
}

void render_interface_gl2::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                          Rml::Vector2f translation,
                                          Rml::TextureHandle texture) {
    const auto *geometry = reinterpret_cast<geometry_view *>(handle);
    const Rml::Vertex *vertices = geometry->vertices.data();
    const int *indices = geometry->indices.data();
    const int num_indices{static_cast<int>(geometry->indices.size())};

    glPushMatrix();
    glTranslatef(translation.x, translation.y, 0);

    glVertexPointer(2, GL_FLOAT, sizeof(Rml::Vertex), &vertices[0].position);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Rml::Vertex),
                   &vertices[0].colour);

    if (!texture) {
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    } else {
        glEnable(GL_TEXTURE_2D);

        if (texture != TextureEnableWithoutBinding)
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));

        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(Rml::Vertex),
                          &vertices[0].tex_coord);
    }

    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, indices);

    glPopMatrix();
}

void render_interface_gl2::EnableScissorRegion(bool enable) {
    if (enable)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}

void render_interface_gl2::SetScissorRegion(Rml::Rectanglei region) {
    glScissor(region.Left(), viewport_height_ - region.Bottom(), region.Width(),
              region.Height());
}

void render_interface_gl2::EnableClipMask(bool enable) {
    if (enable)
        glEnable(GL_STENCIL_TEST);
    else
        glDisable(GL_STENCIL_TEST);
}

void render_interface_gl2::RenderToClipMask(
    Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation) {
    RMLUI_ASSERT(glIsEnabled(GL_STENCIL_TEST));
    using Rml::ClipMaskOperation;

    const bool clear_stencil = (operation == ClipMaskOperation::Set ||
                                operation == ClipMaskOperation::SetInverse);
    if (clear_stencil) {
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    GLint stencil_test_value{};
    glGetIntegerv(GL_STENCIL_REF, &stencil_test_value);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilFunc(GL_ALWAYS, GLint(1), GLuint(-1));

    switch (operation) {
    case ClipMaskOperation::Set: {
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        stencil_test_value = 1;
    } break;
    case ClipMaskOperation::SetInverse: {
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        stencil_test_value = 0;
    } break;
    case ClipMaskOperation::Intersect: {
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
        stencil_test_value += 1;
    } break;
    }

    RenderGeometry(geometry, translation, {});

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_EQUAL, stencil_test_value, GLuint(-1));
}

#pragma pack(1)
struct tga_header {
    char id_length;
    char colour_map_type;
    char data_type;
    short int colour_map_origin;
    short int colour_map_length;
    char colour_map_depth;
    short int x_origin;
    short int y_origin;
    short int width;
    short int height;
    char bits_per_pixel;
    char image_descriptor;
};
#pragma pack()

Rml::TextureHandle
render_interface_gl2::LoadTexture(Rml::Vector2i &texture_dimensions,
                                  const Rml::String &source) {
    // Prefer SDL_image (PNG/JPG/BMP/GIF/...), resolved from the host process.
    // Embedded CMRC assets are not real files; those fall through to the
    // built-in TGA reader below.
    const sdl_image_api &image{sdl_image()};
    if (image.available) {
        SDL_Surface *surface{image.load(source.c_str())};
        if (surface != nullptr) {
            if (image.lock_surface(surface) == 0) {
                const int width{surface->w};
                const int height{surface->h};
                const auto bytes_per_pixel{
                    static_cast<std::size_t>(surface->format->BytesPerPixel)};
                std::vector<Rml::byte> pixels(
                    static_cast<std::size_t>(width) *
                    static_cast<std::size_t>(height) * 4);

                for (int row{}; row < height; ++row) {
                    const auto *line{
                        static_cast<const unsigned char *>(surface->pixels) +
                        static_cast<std::size_t>(row) * surface->pitch};
                    for (int column{}; column < width; ++column) {
                        unsigned int pixel{};
                        std::memcpy(&pixel,
                                    line + static_cast<std::size_t>(column) *
                                               bytes_per_pixel,
                                    bytes_per_pixel);
                        unsigned char r{};
                        unsigned char g{};
                        unsigned char b{};
                        unsigned char a{};
                        image.get_rgba(pixel, surface->format, &r, &g, &b, &a);
                        auto *dest{&pixels[(static_cast<std::size_t>(row) *
                                                static_cast<std::size_t>(width) +
                                            static_cast<std::size_t>(column)) *
                                           4]};
                        dest[0] = static_cast<Rml::byte>((r * a) / 255);
                        dest[1] = static_cast<Rml::byte>((g * a) / 255);
                        dest[2] = static_cast<Rml::byte>((b * a) / 255);
                        dest[3] = static_cast<Rml::byte>(a);
                    }
                }

                image.unlock_surface(surface);
                texture_dimensions = {width, height};
                const Rml::TextureHandle handle{
                    GenerateTexture({pixels.data(), pixels.size()},
                                    texture_dimensions)};
                image.free_surface(surface);
                if (handle) {
                    return handle;
                }
            } else {
                image.free_surface(surface);
            }
        }
    }

    Rml::FileInterface *file_interface = Rml::GetFileInterface();
    Rml::FileHandle file_handle{file_interface->Open(source)};
    if (!file_handle) {
        return false;
    }

    file_interface->Seek(file_handle, 0, SEEK_END);
    const size_t buffer_size{file_interface->Tell(file_handle)};
    file_interface->Seek(file_handle, 0, SEEK_SET);

    if (buffer_size <= sizeof(tga_header)) {
        Rml::Log::Message(
            Rml::Log::LT_ERROR,
            "Texture file size is smaller than TGAHeader, file is not a "
            "valid TGA image.");
        file_interface->Close(file_handle);
        return false;
    }

    using Rml::byte;
    Rml::UniquePtr<byte[]> buffer(new byte[buffer_size]);
    file_interface->Read(buffer.get(), buffer_size, file_handle);
    file_interface->Close(file_handle);

    tga_header header{};
    std::memcpy(&header, buffer.get(), sizeof(tga_header));

    int color_mode{header.bits_per_pixel / 8};
    const auto image_size{
        static_cast<std::size_t>(header.width * header.height * 4)};

    if (header.data_type != 2) {
        Rml::Log::Message(Rml::Log::LT_ERROR,
                          "Only 24/32bit uncompressed TGAs are supported.");
        return false;
    }

    if (color_mode < 3) {
        Rml::Log::Message(Rml::Log::LT_ERROR,
                          "Only 24 and 32bit textures are supported.");
        return false;
    }

    const byte *image_src = buffer.get() + sizeof(tga_header);
    Rml::UniquePtr<byte[]> image_dest_buffer(new byte[image_size]);
    byte *image_dest = image_dest_buffer.get();
    const bool top_to_bottom_order{((header.image_descriptor & 32) != 0)};

    for (long y{}; y < header.height; y++) {
        long read_index{y * header.width * color_mode};
        long write_index = top_to_bottom_order
                               ? (y * header.width * 4)
                               : (header.height - y - 1) * header.width * 4;
        for (long x{}; x < header.width; x++) {
            image_dest[write_index] = image_src[read_index + 2];
            image_dest[write_index + 1] = image_src[read_index + 1];
            image_dest[write_index + 2] = image_src[read_index];
            if (color_mode == 4) {
                const byte alpha{image_src[read_index + 3]};
                for (size_t j{}; j < 3; j++)
                    image_dest[write_index + j] = static_cast<byte>(
                        (image_dest[write_index + j] * alpha) / 255);
                image_dest[write_index + 3] = alpha;
            } else {
                image_dest[write_index + 3] = 255;
            }

            write_index += 4;
            read_index += color_mode;
        }
    }

    texture_dimensions.x = header.width;
    texture_dimensions.y = header.height;

    return GenerateTexture({image_dest, image_size}, texture_dimensions);
}

Rml::TextureHandle
render_interface_gl2::GenerateTexture(Rml::Span<const Rml::byte> source,
                                      Rml::Vector2i source_dimensions) {
    RMLUI_ASSERT(source.data() &&
                 source.size() == static_cast<size_t>(source_dimensions.x *
                                                      source_dimensions.y * 4));

    GLuint texture_id{};
    glGenTextures(1, &texture_id);
    if (texture_id == 0) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "Failed to generate texture.");
        return {};
    }

    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, source_dimensions.x,
                 source_dimensions.y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 source.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return static_cast<Rml::TextureHandle>(texture_id);
}

void render_interface_gl2::ReleaseTexture(Rml::TextureHandle texture_handle) {
    glDeleteTextures(1, reinterpret_cast<GLuint *>(&texture_handle));
}

void render_interface_gl2::SetTransform(const Rml::Matrix4f *transform) {
    transform_enabled_ = (transform != nullptr);

    if (transform) {
        if constexpr (std::is_same_v<Rml::Matrix4f, Rml::ColumnMajorMatrix4f>)
            glLoadMatrixf(transform->data());
        else if constexpr (std::is_same_v<Rml::Matrix4f, Rml::RowMajorMatrix4f>)
            glLoadMatrixf(transform->Transpose().data());
    } else {
        glLoadIdentity();
    }
}

// ---------------------------------------------------------------------------
// Layer support
// ---------------------------------------------------------------------------

bool render_interface_gl2::ensure_layer_support() {
    if (layer_support_checked_) {
        return layer_support_;
    }
    layer_support_checked_ = true;
    layer_support_ = layer_functions().available;
    if (!layer_support_) {
        Rml::Log::Message(Rml::Log::LT_WARNING,
                          "Framebuffer objects unavailable; layer based "
                          "effects (box-shadow, filters) are disabled.");
    }
    return layer_support_;
}

void render_interface_gl2::destroy_layer(layer_entry &layer) {
    const layer_api &api{layer_functions()};
    if (layer.framebuffer != 0) {
        api.delete_framebuffers(1, &layer.framebuffer);
    }
    if (layer.depth_stencil != 0) {
        api.delete_renderbuffers(1, &layer.depth_stencil);
    }
    if (layer.texture != 0) {
        glDeleteTextures(1, &layer.texture);
    }
    layer = {};
}

bool render_interface_gl2::ensure_layer_storage(layer_entry &layer) {
    const int width{std::max(viewport_width_, 1)};
    const int height{std::max(viewport_height_, 1)};

    if (layer.framebuffer != 0 && layer.width == width &&
        layer.height == height) {
        return true;
    }
    destroy_layer(layer);

    const layer_api &api{layer_functions()};
    api.gen_framebuffers(1, &layer.framebuffer);
    glGenTextures(1, &layer.texture);
    glBindTexture(GL_TEXTURE_2D, layer.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    api.gen_renderbuffers(1, &layer.depth_stencil);
    api.bind_renderbuffer(GL_RENDERBUFFER, layer.depth_stencil);
    api.renderbuffer_storage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width,
                             height);

    api.bind_framebuffer(GL_FRAMEBUFFER, layer.framebuffer);
    api.framebuffer_texture2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, layer.texture, 0);
    api.framebuffer_renderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                 GL_RENDERBUFFER, layer.depth_stencil);
    const unsigned int status{api.check_framebuffer_status(GL_FRAMEBUFFER)};

    api.bind_renderbuffer(GL_RENDERBUFFER, 0);
    api.bind_framebuffer(GL_FRAMEBUFFER,
                         layer_stack_.empty()
                             ? main_framebuffer_
                             : layer_stack_.back()->framebuffer);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Rml::Log::Message(Rml::Log::LT_WARNING,
                          "Failed to create a render layer (FBO incomplete: "
                          "%u).",
                          status);
        destroy_layer(layer);
        return false;
    }

    layer.width = width;
    layer.height = height;
    return true;
}

render_interface_gl2::layer_entry *render_interface_gl2::acquire_layer() {
    if (!ensure_layer_support()) {
        return nullptr;
    }
    while (!free_layers_.empty()) {
        layer_entry *candidate{free_layers_.back()};
        free_layers_.pop_back();
        if (ensure_layer_storage(*candidate)) {
            return candidate;
        }
    }
    auto entry{std::make_unique<layer_entry>()};
    if (!ensure_layer_storage(*entry)) {
        return nullptr;
    }
    layer_entry *result{entry.get()};
    layers_.push_back(std::move(entry));
    return result;
}

void render_interface_gl2::sync_layer_pool() {
    free_layers_.clear();
    free_layers_.reserve(layers_.size());
    for (auto &layer : layers_) {
        const bool in_stack{
            std::find(layer_stack_.begin(), layer_stack_.end(), layer.get()) !=
            layer_stack_.end()};
        if (!in_stack) {
            free_layers_.push_back(layer.get());
        }
    }
}

void render_interface_gl2::bind_layer_target(const layer_entry *layer) {
    const layer_api &api{layer_functions()};
    api.bind_framebuffer(GL_FRAMEBUFFER,
                         layer != nullptr ? layer->framebuffer
                                          : main_framebuffer_);
    glViewport(0, 0, viewport_width_, viewport_height_);
}

void render_interface_gl2::draw_layer_quad(unsigned int texture, int x, int y,
                                           int width, int height,
                                           bool flip_vertical) {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const GLboolean vertex_array{glIsEnabled(GL_VERTEX_ARRAY)};
    const GLboolean color_array{glIsEnabled(GL_COLOR_ARRAY)};
    const GLboolean texcoord_array{glIsEnabled(GL_TEXTURE_COORD_ARRAY)};
    if (vertex_array) {
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    if (color_array) {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    if (texcoord_array) {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4f(1.0F, 1.0F, 1.0F, 1.0F);

    const float v0{flip_vertical ? 1.0F : 0.0F};
    const float v1{flip_vertical ? 0.0F : 1.0F};

    glBegin(GL_QUADS);
    glTexCoord2f(0.0F, v0);
    glVertex2f(static_cast<float>(x), static_cast<float>(y));
    glTexCoord2f(1.0F, v0);
    glVertex2f(static_cast<float>(x + width), static_cast<float>(y));
    glTexCoord2f(1.0F, v1);
    glVertex2f(static_cast<float>(x + width), static_cast<float>(y + height));
    glTexCoord2f(0.0F, v1);
    glVertex2f(static_cast<float>(x), static_cast<float>(y + height));
    glEnd();

    if (vertex_array) {
        glEnableClientState(GL_VERTEX_ARRAY);
    }
    if (color_array) {
        glEnableClientState(GL_COLOR_ARRAY);
    }
    if (texcoord_array) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glPopMatrix();
}

Rml::LayerHandle render_interface_gl2::PushLayer() {
    layer_entry *layer{acquire_layer()};
    if (layer == nullptr) {
        return 0;
    }

    layer_stack_.push_back(layer);
    bind_layer_target(layer);

    // Clear the whole layer, not only the active scissor. The box-shadow blur
    // chain down samples and samples the entire layer texture, so stale
    // pixels of a previously reused layer outside the element's region would
    // bleed into the shadow and show up as a transient black frame while the
    // element is dragged or resized.
    GLboolean color_mask[4]{};
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
    const GLboolean scissor_enabled{glIsEnabled(GL_SCISSOR_TEST)};
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (scissor_enabled) {
        glEnable(GL_SCISSOR_TEST);
    }
    glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);

    return reinterpret_cast<Rml::LayerHandle>(layer);
}

void render_interface_gl2::CompositeLayers(
    Rml::LayerHandle source, Rml::LayerHandle destination,
    Rml::BlendMode blend_mode,
    Rml::Span<const Rml::CompiledFilterHandle> filters) {
    const auto *source_layer{reinterpret_cast<const layer_entry *>(source)};
    const auto *destination_layer{
        reinterpret_cast<const layer_entry *>(destination)};
    if (source_layer == nullptr || source_layer == destination_layer) {
        return;
    }

    // IMPORTANT: capture the render state *before* the filter chain runs.
    // The filter passes rebind framebuffers/viewports/textures and toggle
    // blending; capturing afterwards (as this used to do) made the "restore"
    // put the post-process state back instead, so every draw after the
    // composite that frame lost blending and the correct target: text glyphs
    // turned into black blocks and the box-shadow band went black while an
    // element was dragged or a popup opened.
    GLint previous_framebuffer{};
    GLint previous_viewport[4]{};
    GLint previous_texture{};
    GLint previous_blend_src{};
    GLint previous_blend_dst{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previous_blend_src);
    glGetIntegerv(GL_BLEND_DST_RGB, &previous_blend_dst);
    const GLboolean blend_enabled{glIsEnabled(GL_BLEND)};
    const GLboolean scissor_enabled{glIsEnabled(GL_SCISSOR_TEST)};
    GLint previous_scissor[4]{};
    glGetIntegerv(GL_SCISSOR_BOX, previous_scissor);

    // Run the filter chain first (if any); the result is left in a post
    // process texture, `filtered_texture`/`filtered_u/v` describe it. When
    // the filters cannot run, dropping this frame's shadow beats falling back
    // to the raw (hard edged) shape, which would flash a dark ring around the
    // element.
    unsigned int filtered_texture{source_layer->texture};
    float filtered_u{1.0F};
    float filtered_v{1.0F};
    if (!filters.empty()) {
        if (!apply_filters(source_layer->texture, filters, filtered_texture,
                           filtered_u, filtered_v)) {
            return;
        }
    }

    glEnable(GL_SCISSOR_TEST);

    bind_layer_target(destination_layer);

    if (blend_mode == Rml::BlendMode::Blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    draw_quad_uv(filtered_texture, filtered_u, filtered_v);

    glScissor(previous_scissor[0], previous_scissor[1], previous_scissor[2],
              previous_scissor[3]);
    if (!scissor_enabled) {
        glDisable(GL_SCISSOR_TEST);
    }

    glBindTexture(GL_TEXTURE_2D,
                  static_cast<GLuint>(previous_texture));
    if (blend_enabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    glBlendFunc(static_cast<GLenum>(previous_blend_src),
                static_cast<GLenum>(previous_blend_dst));
    const layer_api &api{layer_functions()};
    api.bind_framebuffer(GL_FRAMEBUFFER,
                         static_cast<GLuint>(previous_framebuffer));
    glViewport(previous_viewport[0], previous_viewport[1],
               previous_viewport[2], previous_viewport[3]);
    if (!filters.empty() && shaders().available) {
        shaders().use_program(0);
    }
}

void render_interface_gl2::PopLayer() {
    if (layer_stack_.empty()) {
        return;
    }
    layer_entry *popped{layer_stack_.back()};
    layer_stack_.pop_back();
    bind_layer_target(layer_stack_.empty() ? nullptr : layer_stack_.back());
    free_layers_.push_back(popped);
}

Rml::TextureHandle render_interface_gl2::SaveLayerAsTexture() {
    const layer_entry *current{layer_stack_.empty() ? nullptr
                                                    : layer_stack_.back()};
    if (current == nullptr || !ensure_layer_support()) {
        return 0;
    }

    GLint scissor[4]{};
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    const int x{scissor[0]};
    const int y{scissor[1]};
    const int width{scissor[2]};
    const int height{scissor[3]};
    if (width <= 0 || height <= 0) {
        return 0;
    }

    const layer_api &api{layer_functions()};

    unsigned int texture{};
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    unsigned int framebuffer{};
    api.gen_framebuffers(1, &framebuffer);
    api.bind_framebuffer(GL_FRAMEBUFFER, framebuffer);
    api.framebuffer_texture2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, texture, 0);
    if (api.check_framebuffer_status(GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
        api.delete_framebuffers(1, &framebuffer);
        glDeleteTextures(1, &texture);
        return 0;
    }

    GLint previous_viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    const GLboolean blend_enabled{glIsEnabled(GL_BLEND)};
    const GLboolean scissor_enabled{glIsEnabled(GL_SCISSOR_TEST)};
    GLint previous_scissor[4]{};
    glGetIntegerv(GL_SCISSOR_BOX, previous_scissor);
    GLboolean color_mask[4]{};
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);

    // The active scissor is expressed in the parent layer's coordinates and
    // must not clip the extraction render target.
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    // Bottom-left origin within this target keeps image row 0 at the top of
    // the extracted texture, matching how RmlUi samples it back.
    glOrtho(0.0, static_cast<double>(width), 0.0, static_cast<double>(height),
            -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const GLboolean vertex_array{glIsEnabled(GL_VERTEX_ARRAY)};
    const GLboolean color_array{glIsEnabled(GL_COLOR_ARRAY)};
    const GLboolean texcoord_array{glIsEnabled(GL_TEXTURE_COORD_ARRAY)};
    if (vertex_array) {
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    if (color_array) {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    if (texcoord_array) {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, current->texture);
    glColor4f(1.0F, 1.0F, 1.0F, 1.0F);

    const float u0{static_cast<float>(x) / static_cast<float>(current->width)};
    const float u1{static_cast<float>(x + width) /
                   static_cast<float>(current->width)};
    const float v_top{static_cast<float>(y + height) /
                      static_cast<float>(current->height)};
    const float v_bottom{static_cast<float>(y) /
                         static_cast<float>(current->height)};

    glBegin(GL_QUADS);
    glTexCoord2f(u0, v_top);
    glVertex2f(0.0F, 0.0F);
    glTexCoord2f(u1, v_top);
    glVertex2f(static_cast<float>(width), 0.0F);
    glTexCoord2f(u1, v_bottom);
    glVertex2f(static_cast<float>(width), static_cast<float>(height));
    glTexCoord2f(u0, v_bottom);
    glVertex2f(0.0F, static_cast<float>(height));
    glEnd();

    if (vertex_array) {
        glEnableClientState(GL_VERTEX_ARRAY);
    }
    if (color_array) {
        glEnableClientState(GL_COLOR_ARRAY);
    }
    if (texcoord_array) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    if (blend_enabled) {
        glEnable(GL_BLEND);
    }

    glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    if (scissor_enabled) {
        glEnable(GL_SCISSOR_TEST);
    }
    glScissor(previous_scissor[0], previous_scissor[1], previous_scissor[2],
              previous_scissor[3]);

    api.bind_framebuffer(GL_FRAMEBUFFER, current->framebuffer);
    glViewport(previous_viewport[0], previous_viewport[1],
               previous_viewport[2], previous_viewport[3]);
    api.delete_framebuffers(1, &framebuffer);

    return static_cast<Rml::TextureHandle>(texture);
}

// ---------------------------------------------------------------------------
// Filters
//
// `box-shadow` blur and the CSS `filter` functions are evaluated by rendering
// the source layer through small GLSL programs into two post-process targets.
// Blurs are down sampled first: every halving keeps bilinear filtering exact,
// so a wide blur costs a fixed number of taps. The separable Gaussian runs at
// 1 / 2^level resolution and the final composite upscales it with bilinear
// filtering, which is the cheapest way to get a smooth large radius shadow
// out of a fixed function pipeline.
// ---------------------------------------------------------------------------

namespace {
/// Taps per side; a pass always stays at an effective sigma of at most 3.
constexpr int blur_max_taps{9};

const char *const filter_vertex_shader{R"(
varying vec2 filter_uv;
void main()
{
    filter_uv = gl_MultiTexCoord0.xy;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)"};

const char *const blur_fragment_shader{R"(
varying vec2 filter_uv;
uniform sampler2D filter_texture;
uniform vec2 filter_direction;
uniform vec2 filter_bounds_min;
uniform vec2 filter_bounds_max;
uniform float filter_weights[10];
uniform float filter_radius;
void main()
{
    // Taps outside the valid region are dropped instead of clamped: the rows
    // just outside the scissor may hold stale pixels of a reused layer, and
    // repeatedly sampling them would smear a dark frame around the element.
    vec2 uv = clamp(filter_uv, filter_bounds_min, filter_bounds_max);
    vec4 colour = texture2D(filter_texture, uv) * filter_weights[0];
    for (int i = 1; i <= 9; ++i) {
        if (float(i) > filter_radius) {
            break;
        }
        vec2 step = filter_direction * float(i);
        vec2 high = uv + step;
        vec2 low = uv - step;
        if (all(greaterThanEqual(high, filter_bounds_min)) &&
            all(lessThanEqual(high, filter_bounds_max))) {
            colour += texture2D(filter_texture, high) * filter_weights[i];
        }
        if (all(greaterThanEqual(low, filter_bounds_min)) &&
            all(lessThanEqual(low, filter_bounds_max))) {
            colour += texture2D(filter_texture, low) * filter_weights[i];
        }
    }
    gl_FragColor = colour;
}
)"};

const char *const color_matrix_fragment_shader{R"(
varying vec2 filter_uv;
uniform sampler2D filter_texture;
uniform mat4 filter_matrix;
uniform vec4 filter_offset;
void main()
{
    gl_FragColor =
        filter_matrix * texture2D(filter_texture, filter_uv) + filter_offset;
}
)"};
} // namespace

const render_interface_gl2::shader_api &render_interface_gl2::shaders() {
    static const shader_api api{[] {
        shader_api result{};
        load_proc(result.create_shader, "glCreateShader");
        load_proc(result.shader_source, "glShaderSource");
        load_proc(result.compile_shader, "glCompileShader");
        load_proc(result.get_shader_iv, "glGetShaderiv");
        load_proc(result.get_shader_info_log, "glGetShaderInfoLog");
        load_proc(result.delete_shader, "glDeleteShader");
        load_proc(result.create_program, "glCreateProgram");
        load_proc(result.attach_shader, "glAttachShader");
        load_proc(result.link_program, "glLinkProgram");
        load_proc(result.get_program_iv, "glGetProgramiv");
        load_proc(result.get_program_info_log, "glGetProgramInfoLog");
        load_proc(result.delete_program, "glDeleteProgram");
        load_proc(result.use_program, "glUseProgram");
        load_proc(result.get_uniform_location, "glGetUniformLocation");
        load_proc(result.uniform1f, "glUniform1f");
        load_proc(result.uniform2f, "glUniform2f");
        load_proc(result.uniform4f, "glUniform4f");
        load_proc(result.uniform1fv, "glUniform1fv");
        load_proc(result.uniform_matrix4fv, "glUniformMatrix4fv");
        result.available = result.create_shader != nullptr &&
                           result.shader_source != nullptr &&
                           result.compile_shader != nullptr &&
                           result.create_program != nullptr &&
                           result.attach_shader != nullptr &&
                           result.link_program != nullptr &&
                           result.use_program != nullptr &&
                           result.get_uniform_location != nullptr &&
                           result.uniform1fv != nullptr &&
                           result.uniform_matrix4fv != nullptr;
        return result;
    }()};
    return api;
}

bool render_interface_gl2::ensure_filter_programs() {
    if (programs_ready_) {
        return true;
    }
    if (programs_failed_) {
        return false;
    }
    const shader_api &api{shaders()};
    if (!api.available) {
        programs_failed_ = true;
        Rml::Log::Message(Rml::Log::LT_WARNING,
                          "GLSL is unavailable; CSS filters are ignored.");
        return false;
    }

    const auto compile{[](unsigned int type,
                          const char *source) -> unsigned int {
        const shader_api &api{shaders()};
        const unsigned int shader{api.create_shader(type)};
        if (shader == 0) {
            return 0;
        }
        api.shader_source(shader, 1, &source, nullptr);
        api.compile_shader(shader);
        int status{};
        api.get_shader_iv(shader, GL_COMPILE_STATUS, &status);
        if (status == 0) {
            char log[512]{};
            api.get_shader_info_log(shader, sizeof(log), nullptr, log);
            Rml::Log::Message(Rml::Log::LT_WARNING,
                              "filter shader compilation failed: %s", log);
            api.delete_shader(shader);
            return 0;
        }
        return shader;
    }};
    const auto link{[](unsigned int vertex,
                       unsigned int fragment) -> unsigned int {
        const shader_api &api{shaders()};
        const unsigned int program{api.create_program()};
        if (program == 0) {
            return 0;
        }
        api.attach_shader(program, vertex);
        api.attach_shader(program, fragment);
        api.link_program(program);
        int status{};
        api.get_program_iv(program, GL_LINK_STATUS, &status);
        if (status == 0) {
            char log[512]{};
            api.get_program_info_log(program, sizeof(log), nullptr, log);
            Rml::Log::Message(Rml::Log::LT_WARNING,
                              "filter program link failed: %s", log);
            api.delete_program(program);
            return 0;
        }
        return program;
    }};

    const unsigned int vertex{compile(GL_VERTEX_SHADER, filter_vertex_shader)};
    if (vertex == 0) {
        programs_failed_ = true;
        return false;
    }

    const unsigned int blur_fragment{
        compile(GL_FRAGMENT_SHADER, blur_fragment_shader)};
    const unsigned int color_fragment{
        compile(GL_FRAGMENT_SHADER, color_matrix_fragment_shader)};
    if (blur_fragment != 0) {
        programs_.blur = link(vertex, blur_fragment);
        api.delete_shader(blur_fragment);
    }
    if (color_fragment != 0) {
        programs_.color_matrix = link(vertex, color_fragment);
        api.delete_shader(color_fragment);
    }
    api.delete_shader(vertex);

    if (programs_.blur == 0 || programs_.color_matrix == 0) {
        destroy_filter_programs();
        programs_failed_ = true;
        return false;
    }

    programs_.blur_texture =
        api.get_uniform_location(programs_.blur, "filter_texture");
    programs_.blur_direction =
        api.get_uniform_location(programs_.blur, "filter_direction");
    programs_.blur_bounds_min =
        api.get_uniform_location(programs_.blur, "filter_bounds_min");
    programs_.blur_bounds_max =
        api.get_uniform_location(programs_.blur, "filter_bounds_max");
    programs_.blur_weights =
        api.get_uniform_location(programs_.blur, "filter_weights");
    programs_.blur_radius =
        api.get_uniform_location(programs_.blur, "filter_radius");
    programs_.color_matrix_texture =
        api.get_uniform_location(programs_.color_matrix, "filter_texture");
    programs_.color_matrix_value =
        api.get_uniform_location(programs_.color_matrix, "filter_matrix");
    programs_.color_matrix_offset =
        api.get_uniform_location(programs_.color_matrix, "filter_offset");

    programs_ready_ = true;
    return true;
}

void render_interface_gl2::destroy_filter_programs() {
    const shader_api &api{shaders()};
    if (api.available) {
        if (programs_.blur != 0) {
            api.delete_program(programs_.blur);
        }
        if (programs_.color_matrix != 0) {
            api.delete_program(programs_.color_matrix);
        }
    }
    programs_ = {};
    programs_ready_ = false;
}

bool render_interface_gl2::ensure_post_storage() {
    if (post_storage_ready_) {
        return true;
    }
    const auto create{[](layer_entry &entry, int width, int height) {
        const layer_api &api{layer_functions()};
        entry.width = std::max(width, 1);
        entry.height = std::max(height, 1);
        api.gen_framebuffers(1, &entry.framebuffer);
        glGenTextures(1, &entry.texture);
        glBindTexture(GL_TEXTURE_2D, entry.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, entry.width, entry.height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        api.bind_framebuffer(GL_FRAMEBUFFER, entry.framebuffer);
        api.framebuffer_texture2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, entry.texture, 0);
        const unsigned int status{
            api.check_framebuffer_status(GL_FRAMEBUFFER)};
        return status == GL_FRAMEBUFFER_COMPLETE;
    }};

    bool complete{true};
    for (int level = 0; level < post_levels && complete; ++level) {
        for (layer_entry &entry : post_[level]) {
            if (!create(entry, viewport_width_ >> level,
                        viewport_height_ >> level)) {
                complete = false;
                break;
            }
        }
    }
    if (!complete) {
        Rml::Log::Message(Rml::Log::LT_WARNING,
                          "Failed to create the filter post process targets.");
        destroy_post_storage();
        return false;
    }
    post_storage_ready_ = true;
    return true;
}

void render_interface_gl2::destroy_post_storage() {
    for (auto &level : post_) {
        for (layer_entry &entry : level) {
            destroy_layer(entry);
        }
    }
    post_storage_ready_ = false;
}

void render_interface_gl2::draw_quad_uv(unsigned int texture, float u_max,
                                        float v_max, float u_off,
                                        float v_off) noexcept {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const GLboolean vertex_array{glIsEnabled(GL_VERTEX_ARRAY)};
    const GLboolean color_array{glIsEnabled(GL_COLOR_ARRAY)};
    const GLboolean texcoord_array{glIsEnabled(GL_TEXTURE_COORD_ARRAY)};
    if (vertex_array) {
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    if (color_array) {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    if (texcoord_array) {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4f(1.0F, 1.0F, 1.0F, 1.0F);

    const float x1{static_cast<float>(viewport_width_)};
    const float y1{static_cast<float>(viewport_height_)};
    const float u1{u_max + u_off};
    const float v1{v_max + v_off};

    glBegin(GL_QUADS);
    glTexCoord2f(u_off, v1);
    glVertex2f(0.0F, 0.0F);
    glTexCoord2f(u1, v1);
    glVertex2f(x1, 0.0F);
    glTexCoord2f(u1, v_off);
    glVertex2f(x1, y1);
    glTexCoord2f(u_off, v_off);
    glVertex2f(0.0F, y1);
    glEnd();

    if (vertex_array) {
        glEnableClientState(GL_VERTEX_ARRAY);
    }
    if (color_array) {
        glEnableClientState(GL_COLOR_ARRAY);
    }
    if (texcoord_array) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    glPopMatrix();
}

void render_interface_gl2::read_clip_region(float &u_min, float &v_min,
                                            float &u_max,
                                            float &v_max) const noexcept {
    GLint scissor[4]{};
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    const float inv_w{1.0F /
                      static_cast<float>(std::max(viewport_width_, 1))};
    const float inv_h{1.0F /
                      static_cast<float>(std::max(viewport_height_, 1))};
    u_min = static_cast<float>(scissor[0]) * inv_w;
    u_max = static_cast<float>(scissor[0] + scissor[2]) * inv_w;
    v_min = static_cast<float>(scissor[1]) * inv_h;
    v_max = static_cast<float>(scissor[1] + scissor[3]) * inv_h;
}

Rml::CompiledFilterHandle render_interface_gl2::CompileFilter(
    const Rml::String &name, const Rml::Dictionary &parameters) {
    filter_entry filter{};

    if (name == "opacity") {
        filter.type = filter_entry::kind::passthrough;
        filter.amount = Rml::Get(parameters, "value", 1.0F);
    } else if (name == "blur") {
        filter.type = filter_entry::kind::blur;
        filter.amount = Rml::Get(parameters, "sigma", 1.0F);
    } else if (name == "drop-shadow") {
        filter.type = filter_entry::kind::drop_shadow;
        filter.amount = Rml::Get(parameters, "sigma", 0.0F);
        filter.color =
            Rml::Get(parameters, "color", Rml::Colourb()).ToPremultiplied();
        filter.drop_offset =
            Rml::Get(parameters, "offset", Rml::Vector2f(0.0F, 0.0F));
    } else if (name == "brightness") {
        filter.type = filter_entry::kind::color_matrix;
        const float value{Rml::Get(parameters, "value", 1.0F)};
        filter.matrix = Rml::Matrix4f::Diag(value, value, value, 1.0F);
    } else if (name == "contrast") {
        filter.type = filter_entry::kind::color_matrix;
        const float value{Rml::Get(parameters, "value", 1.0F)};
        const float grayness{0.5F - 0.5F * value};
        filter.matrix = Rml::Matrix4f::Diag(value, value, value, 1.0F);
        filter.matrix_offset = Rml::Vector4f(grayness, grayness, grayness, 0.0F);
    } else if (name == "invert") {
        filter.type = filter_entry::kind::color_matrix;
        const float value{
            Rml::Math::Clamp(Rml::Get(parameters, "value", 1.0F), 0.0F, 1.0F)};
        const float inverted{1.0F - 2.0F * value};
        filter.matrix = Rml::Matrix4f::Diag(inverted, inverted, inverted, 1.0F);
        filter.matrix_offset = Rml::Vector4f(value, value, value, 0.0F);
    } else if (name == "grayscale") {
        filter.type = filter_entry::kind::color_matrix;
        const float value{Rml::Get(parameters, "value", 1.0F)};
        const float rev{1.0F - value};
        const Rml::Vector3f gray{
            value * Rml::Vector3f(0.2126F, 0.7152F, 0.0722F)};
        filter.matrix = Rml::Matrix4f::FromRows(
            {gray.x + rev, gray.y, gray.z, 0.0F},
            {gray.x, gray.y + rev, gray.z, 0.0F},
            {gray.x, gray.y, gray.z + rev, 0.0F},
            {0.0F, 0.0F, 0.0F, 1.0F});
    } else if (name == "sepia") {
        filter.type = filter_entry::kind::color_matrix;
        const float value{Rml::Get(parameters, "value", 1.0F)};
        const float rev{1.0F - value};
        const Rml::Vector3f r_mix{value *
                                  Rml::Vector3f(0.393F, 0.769F, 0.189F)};
        const Rml::Vector3f g_mix{value *
                                  Rml::Vector3f(0.349F, 0.686F, 0.168F)};
        const Rml::Vector3f b_mix{value *
                                  Rml::Vector3f(0.272F, 0.534F, 0.131F)};
        filter.matrix = Rml::Matrix4f::FromRows(
            {r_mix.x + rev, r_mix.y, r_mix.z, 0.0F},
            {g_mix.x, g_mix.y + rev, g_mix.z, 0.0F},
            {b_mix.x, b_mix.y, b_mix.z + rev, 0.0F},
            {0.0F, 0.0F, 0.0F, 1.0F});
    } else if (name == "hue-rotate") {
        filter.type = filter_entry::kind::color_matrix;
        const float value{Rml::Get(parameters, "value", 1.0F)};
        const float s{Rml::Math::Sin(value)};
        const float c{Rml::Math::Cos(value)};
        filter.matrix = Rml::Matrix4f::FromRows(
            {0.213F + 0.787F * c - 0.213F * s,
             0.715F - 0.715F * c - 0.715F * s,
             0.072F - 0.072F * c + 0.928F * s, 0.0F},
            {0.213F - 0.213F * c + 0.143F * s,
             0.715F + 0.285F * c + 0.140F * s,
             0.072F - 0.072F * c - 0.283F * s, 0.0F},
            {0.213F - 0.213F * c - 0.787F * s,
             0.715F - 0.715F * c + 0.715F * s,
             0.072F + 0.928F * c + 0.072F * s, 0.0F},
            {0.0F, 0.0F, 0.0F, 1.0F});
    } else if (name == "saturate") {
        filter.type = filter_entry::kind::color_matrix;
        const float value{Rml::Get(parameters, "value", 1.0F)};
        filter.matrix = Rml::Matrix4f::FromRows(
            {0.213F + 0.787F * value, 0.715F - 0.715F * value,
             0.072F - 0.072F * value, 0.0F},
            {0.213F - 0.213F * value, 0.715F + 0.285F * value,
             0.072F - 0.072F * value, 0.0F},
            {0.213F - 0.213F * value, 0.715F - 0.715F * value,
             0.072F + 0.928F * value, 0.0F},
            {0.0F, 0.0F, 0.0F, 1.0F});
    } else {
        Rml::Log::Message(Rml::Log::LT_WARNING,
                          "Unsupported CSS filter '%s'; it is ignored.",
                          name.c_str());
        return {};
    }

    return reinterpret_cast<Rml::CompiledFilterHandle>(new filter_entry{filter});
}

void render_interface_gl2::ReleaseFilter(Rml::CompiledFilterHandle handle) {
    delete reinterpret_cast<filter_entry *>(handle);
}

bool render_interface_gl2::apply_filters(
    unsigned int source_texture,
    Rml::Span<const Rml::CompiledFilterHandle> filters,
    unsigned int &out_texture, float &out_u, float &out_v) {
    if (!ensure_post_storage() || !ensure_filter_programs()) {
        return false;
    }
    const layer_api &api{layer_functions()};
    const shader_api &gl{shaders()};

    // The scissor region belongs to the screen space of the source layer; the
    // post process targets are drawn in their own space, so both the scissor
    // and the clip mask (stencil) must be off while filtering. They are
    // restored at the end, the caller composites with RmlUi's state.
    const GLboolean scissor_was_enabled{glIsEnabled(GL_SCISSOR_TEST)};
    const GLboolean stencil_was_enabled{glIsEnabled(GL_STENCIL_TEST)};
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);

    // Scissor region in normalised texture coordinates; the same values apply
    // at every down sample level, the mapping is proportional.
    float bounds_u0{};
    float bounds_v0{};
    float bounds_u1{};
    float bounds_v1{};
    read_clip_region(bounds_u0, bounds_v0, bounds_u1, bounds_v1);

    unsigned int current_texture{source_texture};
    int current_level{-1};

    // Bind `post_[level][index]` as the render target and fill it completely
    // with `texture` (the whole source is mapped onto the whole target).
    const auto blit{[&](int level, int index, unsigned int texture) {
        layer_entry &destination{post_[level][index]};
        api.bind_framebuffer(GL_FRAMEBUFFER, destination.framebuffer);
        glViewport(0, 0, destination.width, destination.height);
        glDisable(GL_BLEND);
        glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
        draw_quad_uv(texture, 1.0F, 1.0F);
    }};

    // One separable Gaussian pass, `direction` is the per tap offset in
    // texture coordinates of the (down sampled) target.
    const auto blur_pass{[&](int level, int index, unsigned int texture,
                             float direction_x, float direction_y, int radius,
                             const float (&weights)[10]) {
        layer_entry &destination{post_[level][index]};
        api.bind_framebuffer(GL_FRAMEBUFFER, destination.framebuffer);
        glViewport(0, 0, destination.width, destination.height);
        glDisable(GL_BLEND);
        gl.use_program(programs_.blur);
        gl.uniform2f(programs_.blur_direction, direction_x, direction_y);
        gl.uniform2f(programs_.blur_bounds_min, bounds_u0, bounds_v0);
        gl.uniform2f(programs_.blur_bounds_max, bounds_u1, bounds_v1);
        gl.uniform1f(programs_.blur_radius, static_cast<float>(radius));
        gl.uniform1fv(programs_.blur_weights, 10, weights);
        glBindTexture(GL_TEXTURE_2D, texture);
        draw_quad_uv(texture, 1.0F, 1.0F);
        gl.use_program(0);
    }};

    // Blur `current_texture` and leave the result in the post target pair of
    // its level; returns the level that was used.
    const auto blur{[&](float sigma) -> int {
        if (sigma < 0.5F) {
            return 0;
        }
        int level{0};
        while (level < post_levels - 1 &&
               sigma / static_cast<float>(1 << level) > 3.0F) {
            ++level;
        }
        const float working_sigma{sigma / static_cast<float>(1 << level)};
        int radius{static_cast<int>(std::ceil(3.0F * working_sigma))};
        radius = std::clamp(radius, 1, blur_max_taps);
        float weights[10]{};
        float total{};
        for (int i = 0; i <= radius; ++i) {
            const float x{static_cast<float>(i) / working_sigma};
            weights[i] = std::exp(-0.5F * x * x);
            total += (i == 0) ? weights[i] : 2.0F * weights[i];
        }
        for (float &weight : weights) {
            weight /= total;
        }

        // Down sample step by step onto the target level, then run the
        // separable passes, ping-ponging within the level's target pair.
        int first{0};
        if (current_level == level &&
            current_texture == post_[level][0].texture) {
            first = 1;
        }
        unsigned int working{current_texture};
        for (int step = 1; step <= level; ++step) {
            const int destination{(step % 2 == 1) ? first : 1 - first};
            blit(step, destination, working);
            working = post_[step][destination].texture;
        }
        if (level == 0) {
            blit(0, first, working);
            working = post_[0][first].texture;
        }

        const float step_x{1.0F / static_cast<float>(post_[level][first].width)};
        const float step_y{
            1.0F / static_cast<float>(post_[level][first].height)};
        blur_pass(level, 1 - first, working, step_x, 0.0F, radius, weights);
        blur_pass(level, first, post_[level][1 - first].texture, 0.0F, step_y,
                  radius, weights);

        current_texture = post_[level][first].texture;
        current_level = level;
        return level;
    }};

    for (const Rml::CompiledFilterHandle handle : filters) {
        const auto *filter{reinterpret_cast<const filter_entry *>(handle)};
        if (filter == nullptr) {
            continue;
        }
        switch (filter->type) {
        case filter_entry::kind::passthrough: {
            const int target{current_texture == post_[0][0].texture ? 1 : 0};
            blit(0, target, current_texture);
            api.bind_framebuffer(GL_FRAMEBUFFER, post_[0][target].framebuffer);
            glViewport(0, 0, post_[0][target].width, post_[0][target].height);
            glDisable(GL_BLEND);
            glColor4f(filter->amount, filter->amount, filter->amount,
                      filter->amount);
            draw_quad_uv(current_texture, 1.0F, 1.0F);
            glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
            current_texture = post_[0][target].texture;
            current_level = 0;
        } break;
        case filter_entry::kind::color_matrix: {
            const int target{current_texture == post_[0][0].texture ? 1 : 0};
            api.bind_framebuffer(GL_FRAMEBUFFER, post_[0][target].framebuffer);
            glViewport(0, 0, post_[0][target].width, post_[0][target].height);
            glDisable(GL_BLEND);
            gl.use_program(programs_.color_matrix);
            gl.uniform_matrix4fv(programs_.color_matrix_value, 1, 0,
                                 filter->matrix.data());
            gl.uniform4f(programs_.color_matrix_offset,
                         filter->matrix_offset.x, filter->matrix_offset.y,
                         filter->matrix_offset.z, filter->matrix_offset.w);
            glBindTexture(GL_TEXTURE_2D, current_texture);
            draw_quad_uv(current_texture, 1.0F, 1.0F);
            gl.use_program(0);
            current_texture = post_[0][target].texture;
            current_level = 0;
        } break;
        case filter_entry::kind::blur: {
            blur(filter->amount);
        } break;
        case filter_entry::kind::drop_shadow: {
            // Tint and offset the source to produce the shadow. Level 0 for
            // the tint pass, the blur below re-samples it.
            {
                const int target{
                    current_texture == post_[0][0].texture ? 1 : 0};
                api.bind_framebuffer(GL_FRAMEBUFFER,
                                     post_[0][target].framebuffer);
                glViewport(0, 0, post_[0][target].width,
                           post_[0][target].height);
                glDisable(GL_BLEND);
                gl.use_program(programs_.color_matrix);
                gl.uniform_matrix4fv(
                    programs_.color_matrix_value, 1, 0,
                    Rml::Matrix4f::Diag(
                        filter->color.red / 255.0F,
                        filter->color.green / 255.0F,
                        filter->color.blue / 255.0F,
                        filter->color.alpha / 255.0F)
                        .data());
                gl.uniform4f(programs_.color_matrix_offset, 0.0F, 0.0F, 0.0F,
                             0.0F);
                glBindTexture(GL_TEXTURE_2D, current_texture);
                const float offset_u{
                    -filter->drop_offset.x /
                    static_cast<float>(std::max(viewport_width_, 1))};
                const float offset_v{
                    filter->drop_offset.y /
                    static_cast<float>(std::max(viewport_height_, 1))};
                draw_quad_uv(current_texture, 1.0F, 1.0F, offset_u, offset_v);
                gl.use_program(0);
                current_texture = post_[0][target].texture;
                current_level = 0;
            }

            // Blur the shadow, then draw the source over it again.
            const int level{blur(filter->amount)};
            const int target{current_texture == post_[level][0].texture ? 1
                                                                        : 0};
            layer_entry &result{post_[level][target]};
            api.bind_framebuffer(GL_FRAMEBUFFER, result.framebuffer);
            glViewport(0, 0, result.width, result.height);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            draw_quad_uv(source_texture, 1.0F, 1.0F);
            current_texture = result.texture;
            current_level = level;
        } break;
        }
    }

    glDisable(GL_BLEND);
    if (scissor_was_enabled) {
        glEnable(GL_SCISSOR_TEST);
    }
    if (stencil_was_enabled) {
        glEnable(GL_STENCIL_TEST);
    }

    out_texture = current_texture;
    out_u = 1.0F;
    out_v = 1.0F;
    return true;
}
void render_interface_gl2::release_gpu_resources() {
    for (auto &layer : layers_) {
        destroy_layer(*layer);
    }
    layers_.clear();
    layer_stack_.clear();
    free_layers_.clear();
    destroy_post_storage();
    destroy_filter_programs();
    programs_failed_ = false;
    layer_support_checked_ = false;
    layer_support_ = false;
}

} // namespace zephyrus::backend
