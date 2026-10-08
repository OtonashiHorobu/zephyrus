// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
}

/// Callback invoked for key-down events before the context consumes them.
/// Mirrors the helper interface shipped with the RmlUi sample backends; we
/// vendor it so zephyrus does not depend on RmlUi's source tree layout.
using KeyDownCallback =
    bool (*)(Rml::Context *context, Rml::Input::KeyIdentifier key,
             int key_modifier, float native_dp_ratio, bool priority);

namespace Backend {

/// Standalone only: opens a window and initializes the SDL/GL backends.
bool Initialize(const char *window_name, int width, int height,
                bool allow_resize);

/// Injected mode: attaches the system/render interfaces to the host's
/// existing SDL state and OpenGL context.
bool InitializeInjected();

/// Keep the render interface's viewport in sync with the host window.
void SetViewport(int width, int height);

/// Release render resources and destroy the backend interfaces. Must only be
/// called while the host's OpenGL context and SDL video subsystem are both
/// still alive (i.e. before `SDL_Quit`), and *after* `Rml::Shutdown()`.
void Shutdown();

/// Emergency exit path used when the GL context is already gone: cursors are
/// dropped without touching SDL and the interfaces are leaked on purpose so
/// RmlUi's static teardown cannot call into freed objects or a dead GL
/// context.
void Abandon();

Rml::SystemInterface *GetSystemInterface();
Rml::RenderInterface *GetRenderInterface();

bool ProcessEvents(Rml::Context *context,
                   KeyDownCallback key_down_callback = nullptr,
                   bool power_save = false);
void RequestExit();

void BeginFrame();
void PresentFrame();

} // namespace Backend
