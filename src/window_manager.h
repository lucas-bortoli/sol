#pragma once

#include <raylib.h>

#include <memory>
#include <string>

#include "lib/ui/Widget.h"

namespace wm {

typedef unsigned int WindowHandle;

WindowHandle WindowCreate();
void WindowSetTitle(WindowHandle handle, const std::string& newTitle);
void WindowSetPosition(WindowHandle handle, const Vector2& newPosition);
void WindowSetSize(WindowHandle handle, const Vector2& newSize);
void WindowSetResizable(WindowHandle handle, const bool resizable);
void WindowDestroy(WindowHandle handle);

/// The window's content Rectangle — inside the border and titlebar chrome
/// painted by wm::internal::Draw(). {0,0,0,0} if the window hasn't been
/// sized yet (WindowSetSize never called), matching Draw()'s own
/// skip-until-sized behavior.
Rectangle WindowGetClientRect(WindowHandle handle);

/// Takes ownership of `content` as this window's UI tree. From then on,
/// wm::internal::ProcessEvents()/Draw() drive its ProcessEvents/
/// ProcessKeyboardFocus/Layout/Draw every frame, sized to
/// WindowGetClientRect(handle) — the caller no longer needs to touch it
/// directly. Replaces any previously-set content for this window.
void WindowSetContent(WindowHandle handle, std::unique_ptr<ui::Widget> content);

namespace internal {
void Initialize();
/// Polls input and keyboard focus for every window's content, once per
/// window. Call once per frame, before mutating any widget state (e.g.
/// via Ref()'d pointers) and before Draw().
void ProcessEvents();
/// Paints every window's chrome, then — immediately after, so overlapping
/// windows keep correct z-order — lays out and paints its content (if
/// any was set via WindowSetContent).
void Draw();
void Cleanup();
}  // namespace internal

}  // namespace wm
