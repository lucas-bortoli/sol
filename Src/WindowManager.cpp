#include "WindowManager.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "Assets.h"
#include "Lib/UI/Utils.h"
#include "Palette.h"

namespace WM {

struct Window {
    const WindowHandle handle;
    std::string title;
    Rectangle clientRect;
    bool resizable;
    std::unique_ptr<UI::Widget> content;
};

static WindowHandle _handle_counter = 0;
static std::map<WindowHandle, Window> _window_map;
static std::mutex _window_mutex;

WindowHandle WindowCreate() {
    std::lock_guard<std::mutex> lock(_window_mutex);

    WindowHandle handle = _handle_counter++;
    Window window = {
        .handle = handle,
        .title = "New Window",
        .clientRect = {0, 0, 0, 0},
        .resizable = true,
        .content = nullptr,
    };

    _window_map.emplace(handle, std::move(window));

    return handle;
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

namespace {
Rectangle ComputeClientRect(const Window& window) {
    // Content-inset geometry: breathing room around the 1px border + 16px
    // titlebar that Draw() paints , matched to what call sites used to
    // hand-type per window before WindowGetClientRect() existed.
    constexpr float kContentInsetLeft = 0.0f;
    constexpr float kContentInsetRight = 0.0f;
    constexpr float kContentInsetTop = 18.0f;
    constexpr float kContentInsetBottom = 0.0f;

    return {
        window.clientRect.x + kContentInsetLeft,
        window.clientRect.y + kContentInsetTop,
        std::max(
            0.0f,
            window.clientRect.width - kContentInsetLeft - kContentInsetRight
        ),
        std::max(
            0.0f,
            window.clientRect.height - kContentInsetTop - kContentInsetBottom
        ),
    };
}
}  // namespace

Rectangle WindowGetClientRect(WindowHandle handle) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    return ComputeClientRect(_window_map.at(handle));
}

void WindowSetContent(
    WindowHandle handle, std::unique_ptr<UI::Widget> content
) {
    std::lock_guard<std::mutex> lock(_window_mutex);
    Window& window = _window_map.at(handle);
    window.content = std::move(content);
}

namespace internal {
void Initialize() {}

void ProcessEvents() {
    std::lock_guard<std::mutex> lock(_window_mutex);
    for (const auto& [handle, window] : _window_map) {
        if (!window.content) continue;
        window.content->ProcessEvents();
        UI::Widget::ProcessKeyboardFocus(*window.content);
    }
}

void Draw() {
    std::lock_guard<std::mutex> lock(_window_mutex);

    auto mousePos = GetMousePosition();

    for (const auto& [handle, window] : _window_map) {
        if (window.clientRect.width == 0 && window.clientRect.height == 0) {
            continue;
        }

        UI::DrawRectWithBorderAndShadow(
            window.clientRect, WHITE, NEUTRAL_600, 2
        );

        // titlebar
        Rectangle titlebar = {
            window.clientRect.x + 1,
            window.clientRect.y + 1,
            window.clientRect.width - 2,
            16
        };

        if (CheckCollisionPointRec(mousePos, titlebar)) {
            DrawRectangle(
                titlebar.x,
                titlebar.y,
                titlebar.width,
                titlebar.height,
                NEUTRAL_400
            );
        } else {
            DrawRectangle(
                titlebar.x, titlebar.y, titlebar.width, titlebar.height, RED_400
            );
        }

        UI::DrawText(
            window.title.c_str(),
            window.clientRect.x + 3,
            window.clientRect.y + 3,
            NEUTRAL_200
        );

        Rectangle closeButtonRect = {
            window.clientRect.x + window.clientRect.width - 18 - 1,
            window.clientRect.y + 1,
            16,
            14
        };
        // titlebar X
        DrawRectangle(
            closeButtonRect.x,
            closeButtonRect.y,
            closeButtonRect.width,
            closeButtonRect.height,
            RED
        );
        DrawTexture(
            Assets::WindowCloseButtonX,
            closeButtonRect.x + 5,
            closeButtonRect.y + 4,
            WHITE
        );

        if (window.content) {
            Rectangle clientRect = ComputeClientRect(window);
            window.content->Layout(clientRect);
            window.content->Draw();
        }
    }
}

void Cleanup() {}

}  // namespace internal

}  // namespace WM
