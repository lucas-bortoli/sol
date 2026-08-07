#include <raylib.h>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "fonts.h"
#include "palette.h"
#include "window_manager.h"

namespace wm {

struct Window {
    const WindowHandle handle;
    std::string title;
    Vector2 position;
    Vector2 size;
    bool resizable;
    std::optional<DrawingHandler_t> drawHandler;
    std::optional<CloseButtonHandler_t> closeHandler;
    std::optional<PointerHandler_t> pointerHandler;
    std::optional<KeyboardHandler_t> keyboardHandler;
};

static WindowHandle _handle_counter = 0;
static std::map<WindowHandle, Window> _window_map;
static std::mutex _window_mutex;

WindowHandle WindowCreate() {
    std::lock_guard<std::mutex> lock(_window_mutex);

    Window window = {
        .handle = _handle_counter++,
        .title = "New Window",
        .position = {0, 0},
        .size = {0, 0},
        .resizable = true,
        .drawHandler = std::nullopt,
        .closeHandler = std::nullopt,
        .pointerHandler = std::nullopt,
        .keyboardHandler = std::nullopt,
    };

    _window_map.emplace(window.handle, window);

    return window.handle;
}

void WindowSetTitle(WindowHandle handle, const std::string& newTitle) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.title = newTitle;
}

void WindowSetPosition(WindowHandle handle, const Vector2& newPosition) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.position = newPosition;
}

void WindowSetSize(WindowHandle handle, const Vector2& newSize) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.size = newSize;
}

void WindowSetResizable(WindowHandle handle, const bool resizable) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.resizable = resizable;
}

void WindowDestroy(WindowHandle handle) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    _window_map.erase(handle);
}

void WindowSetHandlerForDrawing(WindowHandle handle,
                                std::optional<DrawingHandler_t> handler) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.drawHandler = handler;
}

void WindowSetHandlerForCloseButton(
    WindowHandle handle,
    std::optional<CloseButtonHandler_t> handler) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.closeHandler = handler;
}

void WindowSetHandlerForPointer(WindowHandle handle,
                                std::optional<PointerHandler_t> handler) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.pointerHandler = handler;
}

void WindowSetHandlerForKeyboard(WindowHandle handle,
                                 std::optional<KeyboardHandler_t> handler) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.keyboardHandler = handler;
}

namespace internal {
void Initialize() {}

void Draw() {
    std::lock_guard<std::mutex> lock(_window_mutex);

    for (const auto& [handle, window] : _window_map) {
        if (window.size.x == 0 && window.size.y == 0) {
            continue;
        }

        // shadow
        for (int i = 1; i <= 2; i++) {
            DrawRectangleLines(window.position.x + i, window.position.y + i,
                               window.size.x, window.size.y, GREY_600);
        }

        // background
        DrawRectangle(window.position.x, window.position.y, window.size.x,
                      window.size.y, WHITE);

        // border
        DrawRectangleLines(window.position.x, window.position.y, window.size.x,
                           window.size.y, GREY_600);

        // titlebar
        DrawRectangle(window.position.x + 1, window.position.y + 1,
                      window.size.x - 2, 16, GREY_400);
        DrawTextEx(fonts::cozette, window.title.c_str(),
                   {window.position.x + 3, window.position.y + 3},
                   fonts::cozette.baseSize, 0, GREY_600);

        if (window.closeHandler.has_value()) {
            // titlebar X
            DrawRectangle(window.position.x + window.size.x - 18 - 1,
                          window.position.y + 1, 16, 14, RED);
            DrawTextEx(
                fonts::cozette, "x",
                {window.position.x + window.size.x - 14, window.position.y + 1},
                fonts::cozette.baseSize, 0, GREY_100);
        }
    }
}

void Cleanup() {}

}  // namespace internal

}  // namespace wm
