#pragma once

#include <raylib.h>

#include <optional>
#include <string>

namespace wm {

typedef unsigned int WindowHandle;

typedef void (*DrawingHandler_t)();
typedef void (*CloseButtonHandler_t)();
typedef void (*PointerHandler_t)();
typedef void (*KeyboardHandler_t)();

WindowHandle WindowCreate();
void WindowSetTitle(WindowHandle handle, const std::string& newTitle);
void WindowSetPosition(WindowHandle handle, const Vector2& newPosition);
void WindowSetSize(WindowHandle handle, const Vector2& newSize);
void WindowSetResizable(WindowHandle handle, const bool resizable);
void WindowDestroy(WindowHandle handle);
void WindowSetHandlerForDrawing(
    WindowHandle handle, std::optional<DrawingHandler_t> handler
);
void WindowSetHandlerForCloseButton(
    WindowHandle handle, std::optional<CloseButtonHandler_t> handler
);
void WindowSetHandlerForPointer(
    WindowHandle handle, std::optional<PointerHandler_t> handler
);
void WindowSetHandlerForKeyboard(
    WindowHandle handle, std::optional<KeyboardHandler_t> handler
);

namespace internal {
void Initialize();
void Draw();
void Cleanup();
}  // namespace internal

}  // namespace wm
