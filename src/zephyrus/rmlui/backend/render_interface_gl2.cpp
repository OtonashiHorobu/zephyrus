// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/rmlui/backend/render_interface_gl2.hpp>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Platform.h>
#include <cstring>

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

namespace zephyrus::backend {

void render_interface_gl2::set_viewport(int in_viewport_width,
                                        int in_viewport_height) {
    viewport_width_ = in_viewport_width;
    viewport_height_ = in_viewport_height;
}

void render_interface_gl2::begin_frame() {
    RMLUI_ASSERT(viewport_width_ >= 0 && viewport_height_ >= 0);
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
}

void render_interface_gl2::end_frame() {}

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

} // namespace zephyrus::backend
