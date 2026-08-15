#pragma once

#include <raylib.h>

#include <string>

namespace wm {

typedef unsigned int WindowHandle;

WindowHandle WindowCreate();
void WindowSetTitle(WindowHandle handle, const std::string& newTitle);
void WindowSetPosition(WindowHandle handle, const Vector2& newPosition);
void WindowSetSize(WindowHandle handle, const Vector2& newSize);
void WindowSetResizable(WindowHandle handle, const bool resizable);
void WindowDestroy(WindowHandle handle);

namespace internal {
void Initialize();
void Draw();
void Cleanup();
}  // namespace internal

}  // namespace wm
