#include <raylib.h>
#include <raymath.h>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "assets.h"
#include "lib/ui/Utils.h"
#include "palette.h"
#include "window_manager.h"

namespace wm {

struct Window {
    const WindowHandle handle;
    std::string title;
    Rectangle clientRect;
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
        .clientRect = {0, 0, 0, 0},
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
    window.clientRect.x = newPosition.x;
    window.clientRect.y = newPosition.y;
}

void WindowSetSize(WindowHandle handle, const Vector2& newSize) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.clientRect.width = newSize.x;
    window.clientRect.height = newSize.y;
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

    auto mousePos = GetMousePosition();

    for (const auto& [handle, window] : _window_map) {
        if (window.clientRect.width == 0 && window.clientRect.height == 0) {
            continue;
        }

        ui::DrawRectWithBorderAndShadow(window.clientRect, WHITE, NEUTRAL_600,
                                        2);

        // titlebar
        Rectangle titlebar = {window.clientRect.x + 1, window.clientRect.y + 1,
                              window.clientRect.width - 2, 16};

        if (CheckCollisionPointRec(mousePos, titlebar)) {
            DrawRectangle(titlebar.x, titlebar.y, titlebar.width,
                          titlebar.height, NEUTRAL_400);
        } else {
            DrawRectangle(titlebar.x, titlebar.y, titlebar.width,
                          titlebar.height, RED_400);
        }

        ui::DrawText(window.title.c_str(), window.clientRect.x + 3,
                     window.clientRect.y + 3, NEUTRAL_200);

        if (true || window.closeHandler.has_value()) {
            Rectangle closeButtonRect = {
                window.clientRect.x + window.clientRect.width - 18 - 1,
                window.clientRect.y + 1, 16, 14};
            // titlebar X
            DrawRectangle(closeButtonRect.x, closeButtonRect.y,
                          closeButtonRect.width, closeButtonRect.height, RED);
            DrawTexture(assets::WindowCloseButtonX, closeButtonRect.x + 5,
                        closeButtonRect.y + 4, WHITE);
        }
    }
}

void Cleanup() {}

}  // namespace internal

}  // namespace wm
