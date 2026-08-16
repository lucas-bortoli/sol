#include "WindowManager.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "Assets.h"
#include "Lib/UI/Input.h"
#include "Lib/UI/LayerStacker.h"
#include "Lib/UI/Utils.h"
#include "Palette.h"

namespace WM {

struct Window : UI::LayerStacker::Drawable {
    const WindowHandle handle;
    std::string title = "New Window";
    Rectangle clientRect{0, 0, 0, 0};
    bool resizable = true;
    std::unique_ptr<UI::Widget> content;
    UI::LayerStacker::ItemId layerToken = 0;

    explicit Window(WindowHandle handle) : handle(handle) {}

    /// Paints this window's chrome (border/titlebar/close button) then, if
    /// content has been set, lays out and paints it — the single delegate
    /// GlobalLayerStacker().DrawAll() invokes for this window, keeping
    /// chrome and content glued together in paint order.
    void Draw() const override;
};

static WindowHandle _handle_counter = 0;
static std::map<WindowHandle, Window> _window_map;
static std::mutex _window_mutex;

enum class DragMode {
    None,
    Move,
    ResizeN,
    ResizeS,
    ResizeE,
    ResizeW,
    ResizeNE,
    ResizeNW,
    ResizeSE,
    ResizeSW,
};

static WindowHandle _dragHandle = 0;
static DragMode _dragMode = DragMode::None;
static Vector2 _dragStartMouse{};
static Rectangle _dragStartRect{};

WindowHandle WindowCreate() {
    std::lock_guard<std::mutex> lock(_window_mutex);

    WindowHandle handle = _handle_counter++;
    auto [it, inserted] = _window_map.try_emplace(handle, handle);
    Window& window = it->second;
    window.layerToken = UI::GlobalLayerStacker().Register(UI::Layer::Windows, window);

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
    auto it = _window_map.find(handle);
    if (it == _window_map.end()) return;
    UI::GlobalLayerStacker().Unregister(it->second.layerToken);
    _window_map.erase(it);
}

namespace {
/// The Windows-layer registration is per-Window, not per-WindowHandle
/// index, so a LayerStacker::ItemId (e.g. from TopmostAt()) is mapped back
/// to its Window with a linear scan — fine given how few windows exist at
/// once.
Window* FindByLayerToken(UI::LayerStacker::ItemId token) {
    for (auto& [handle, window] : _window_map) {
        if (window.layerToken == token) return &window;
    }
    return nullptr;
}
}  // namespace

namespace {
Rectangle ComputeClientRect(const Window& window) {
    // Content-inset geometry: breathing room around the 1px border + 16px
    // titlebar that Draw() paints , matched to what call sites used to
    // hand-type per window before WindowGetClientRect() existed.
    constexpr float kContentInsetLeft = 1.0f;    // 1px border
    constexpr float kContentInsetRight = 1.0f;   // 1px border
    constexpr float kContentInsetTop = 19.0f;    // 18px height + 1px border
    constexpr float kContentInsetBottom = 1.0f;  // 1px border

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

Rectangle ComputeTitlebarRect(const Window& window) {
    return {
        window.clientRect.x + 1,
        window.clientRect.y + 1,
        window.clientRect.width - 2,
        18,
    };
}

Rectangle ComputeCloseButtonRect(const Window& window) {
    return {
        window.clientRect.x + window.clientRect.width - 18 - 1,
        window.clientRect.y + 1,
        16,
        14,
    };
}

// Windows move and resize on an 8px grid, so their edges always line up
// cleanly with each other regardless of drag start position.
constexpr float kGridSize = 8.0f;
constexpr float kMinWindowSize = 40.0f;
// The resize hit region is a band entirely outside `clientRect`, so it
// never overlaps the visible window/border or its content.
constexpr float kResizeBorder = 8.0f;

float SnapToGrid(float value) {
    return std::round(value / kGridSize) * kGridSize;
}

bool IsInTitlebar(const Window& window, Vector2 mouse) {
    if (!CheckCollisionPointRec(mouse, ComputeTitlebarRect(window))) {
        return false;
    }
    return !CheckCollisionPointRec(mouse, ComputeCloseButtonRect(window));
}

DragMode HitTestResizeBorder(const Window& window, Vector2 mouse) {
    if (!window.resizable) return DragMode::None;

    Rectangle outer = window.clientRect;
    Rectangle expanded = {
        outer.x - kResizeBorder,
        outer.y - kResizeBorder,
        outer.width + 2 * kResizeBorder,
        outer.height + 2 * kResizeBorder,
    };
    if (!CheckCollisionPointRec(mouse, expanded)) return DragMode::None;
    if (CheckCollisionPointRec(mouse, outer)) return DragMode::None;

    bool west = mouse.x < outer.x;
    bool east = mouse.x > outer.x + outer.width;
    bool north = mouse.y < outer.y;
    bool south = mouse.y > outer.y + outer.height;

    if (north && west) return DragMode::ResizeNW;
    if (north && east) return DragMode::ResizeNE;
    if (south && west) return DragMode::ResizeSW;
    if (south && east) return DragMode::ResizeSE;
    if (north) return DragMode::ResizeN;
    if (south) return DragMode::ResizeS;
    if (west) return DragMode::ResizeW;
    if (east) return DragMode::ResizeE;
    return DragMode::None;
}

/// Applies `_dragMode`'s effect for a mouse-delta of `delta` from
/// `_dragStartRect`, snapping every resulting edge to the grid and
/// clamping so width/height never drop below kMinWindowSize.
Rectangle ApplyDrag(DragMode mode, const Rectangle& start, Vector2 delta) {
    Rectangle result = start;

    if (mode == DragMode::Move) {
        result.x = SnapToGrid(start.x + delta.x);
        result.y = SnapToGrid(start.y + delta.y);
        return result;
    }

    bool west = mode == DragMode::ResizeW || mode == DragMode::ResizeNW ||
                mode == DragMode::ResizeSW;
    bool east = mode == DragMode::ResizeE || mode == DragMode::ResizeNE ||
                mode == DragMode::ResizeSE;
    bool north = mode == DragMode::ResizeN || mode == DragMode::ResizeNW ||
                 mode == DragMode::ResizeNE;
    bool south = mode == DragMode::ResizeS || mode == DragMode::ResizeSW ||
                 mode == DragMode::ResizeSE;

    if (west) {
        float newLeft = SnapToGrid(start.x + delta.x);
        newLeft = std::min(newLeft, start.x + start.width - kMinWindowSize);
        result.width = (start.x + start.width) - newLeft;
        result.x = newLeft;
    } else if (east) {
        float newRight = SnapToGrid(start.x + start.width + delta.x);
        newRight = std::max(newRight, start.x + kMinWindowSize);
        result.width = newRight - start.x;
    }

    if (north) {
        float newTop = SnapToGrid(start.y + delta.y);
        newTop = std::min(newTop, start.y + start.height - kMinWindowSize);
        result.height = (start.y + start.height) - newTop;
        result.y = newTop;
    } else if (south) {
        float newBottom = SnapToGrid(start.y + start.height + delta.y);
        newBottom = std::max(newBottom, start.y + kMinWindowSize);
        result.height = newBottom - start.y;
    }

    return result;
}
}  // namespace

void Window::Draw() const {
    if (clientRect.width == 0 && clientRect.height == 0) return;

    UI::DrawRectWithBorderAndShadow(clientRect, WHITE, NEUTRAL_600, 2);

    Rectangle titlebar = ComputeTitlebarRect(*this);

    DrawRectangleGradientH(
        titlebar.x, titlebar.y, titlebar.width, titlebar.height, ORANGE_600,
        ORANGE_800
    );

    UI::DrawTextWithShadow(
        title.c_str(), clientRect.x + 4, clientRect.y + 4, NEUTRAL_200,
        NEUTRAL_500
    );

    Rectangle closeButtonRect = ComputeCloseButtonRect(*this);

    // titlebar X
    DrawRectangle(
        closeButtonRect.x, closeButtonRect.y, closeButtonRect.width,
        closeButtonRect.height, RED
    );
    DrawTexture(
        Assets::WindowCloseButtonX, closeButtonRect.x + 5,
        closeButtonRect.y + 4, WHITE
    );

    if (content) {
        Rectangle innerRect = ComputeClientRect(*this);
        content->Layout(innerRect);
        content->Draw();
    }
}

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

    UI::InputSource& input = UI::CurrentInput();
    Vector2 mouse = input.GetMousePosition();

    for (auto& [handle, window] : _window_map) {
        UI::GlobalLayerStacker().SetBounds(window.layerToken, window.clientRect);
    }

    if (_dragMode == DragMode::None &&
        input.IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // Single front-to-back walk decides both "who does this click
        // raise to front" and "does it start a drag" — using the same
        // per-window test (border/titlebar first, falling back to plain
        // clientRect occlusion) so the two questions can never disagree.
        // A pure LayerStacker::TopmostAt(mouse) lookup isn't enough here:
        // it only knows about each window's clientRect, not the resize
        // border that pokes out past it, so a click on the topmost
        // window's own corner could otherwise resolve to a different,
        // merely-underneath window instead.
        Window* hitWindow = nullptr;
        DragMode hitMode = DragMode::None;
        for (UI::LayerStacker::ItemId token :
             UI::GlobalLayerStacker().ItemsFrontToBack(UI::Layer::Windows)) {
            Window* window = FindByLayerToken(token);
            if (!window) continue;
            DragMode mode = HitTestResizeBorder(*window, mouse);
            if (mode == DragMode::None && IsInTitlebar(*window, mouse)) {
                mode = DragMode::Move;
            }
            if (mode != DragMode::None) {
                hitWindow = window;
                hitMode = mode;
                break;
            }
            // No border/titlebar match, but the pointer is over this
            // window's visible body — it occludes every window behind it
            // here, so the click belongs to it (with no drag) rather than
            // falling through to a window further back.
            if (CheckCollisionPointRec(mouse, window->clientRect)) {
                hitWindow = window;
                break;
            }
        }

        if (hitWindow) {
            UI::GlobalLayerStacker().BringToFront(hitWindow->layerToken);
            if (hitMode != DragMode::None) {
                _dragHandle = hitWindow->handle;
                _dragMode = hitMode;
                _dragStartMouse = mouse;
                _dragStartRect = hitWindow->clientRect;
            }
        }
    } else if (_dragMode != DragMode::None) {
        if (input.IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = {
                mouse.x - _dragStartMouse.x, mouse.y - _dragStartMouse.y
            };
            _window_map.at(_dragHandle).clientRect =
                ApplyDrag(_dragMode, _dragStartRect, delta);
        }
        if (input.IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            _dragMode = DragMode::None;
        }
    }

    for (const auto& [handle, window] : _window_map) {
        if (!window.content) continue;
        bool topmost =
            UI::GlobalLayerStacker().IsTopmostAt(window.layerToken, mouse);
        UI::internal::SetPointerEventsSuppressed(!topmost);
        window.content->ProcessEvents();
        UI::Widget::ProcessKeyboardFocus(*window.content);
    }
    UI::internal::SetPointerEventsSuppressed(false);
}

void Draw() {
    std::lock_guard<std::mutex> lock(_window_mutex);
    UI::GlobalLayerStacker().DrawAll();
}

void Cleanup() {}

}  // namespace internal

}  // namespace WM
