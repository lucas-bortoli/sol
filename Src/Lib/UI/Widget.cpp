#include "Widget.h"

#include "Container.h"
#include "Input.h"

#include <algorithm>
#include <cmath>

namespace UI {

namespace {
Widget* g_focusedWidget = nullptr;
bool g_pointerEventsSuppressed = false;
bool g_newKeyboardFocusFrame = true;
}  // namespace

namespace internal {
void SetPointerEventsSuppressed(bool suppressed) {
    g_pointerEventsSuppressed = suppressed;
}
bool IsPointerEventsSuppressed() { return g_pointerEventsSuppressed; }
void BeginKeyboardFocusFrame() { g_newKeyboardFocusFrame = true; }
}  // namespace internal

void Widget::MoveFocusTo(Widget* next) {
    if (!next || !next->focusable || g_focusedWidget == next) return;
    if (g_focusedWidget) {
        g_focusedWidget->focused = false;
        g_focusedWidget->keyDown = false;
        g_focusedWidget->ReleaseAllKeys();
    }
    g_focusedWidget = next;
    next->focused = true;
}

void Widget::Focus() { MoveFocusTo(this); }

bool IsKeyRepeated(int key, float& heldSeconds, float delay, float interval) {
    if (CurrentInput().IsKeyPressed(key)) {
        heldSeconds = 0.0f;
        return true;
    }
    if (!CurrentInput().IsKeyDown(key)) {
        heldSeconds = 0.0f;
        return false;
    }

    float previous = heldSeconds;
    heldSeconds += CurrentInput().GetFrameTime();
    if (previous < delay) {
        return heldSeconds >= delay;
    }
    float previousTicks = floorf((previous - delay) / interval);
    float currentTicks = floorf((heldSeconds - delay) / interval);
    return currentTicks > previousTicks;
}

Widget::~Widget() {
    *aliveToken = false;
    if (g_focusedWidget == this) g_focusedWidget = nullptr;
    if (layerToken) GlobalLayerStacker().Unregister(*layerToken);
}

void Widget::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
    if (layerToken) GlobalLayerStacker().SetBounds(*layerToken, computedRect);
}

void Widget::RegisterLayer(Layer layer) {
    if (layerToken) return;
    layerToken = GlobalLayerStacker().Register(layer, *this);
}

void Widget::ProcessEvents() { PollPointerEvents(computedRect); }

void Widget::Invalidate() {
    if (!layoutDirty) {
        layoutDirty = true;
        if (parent) parent->Invalidate();
    }
}

std::unique_ptr<Widget> Widget::Remove() {
    return parent ? parent->RemoveChild(this) : nullptr;
}

Widget& Widget::SetWidth(float width) {
    fixedWidth = width;
    Invalidate();
    return *this;
}

Widget& Widget::SetHeight(float height) {
    fixedHeight = height;
    Invalidate();
    return *this;
}

Widget& Widget::SetGrow(float grow) {
    growFactor = grow;
    Invalidate();
    return *this;
}

Widget& Widget::SetShrink(float shrink) {
    shrinkFactor = shrink;
    Invalidate();
    return *this;
}

Widget& Widget::SetOnClick(std::function<void()> callback) {
    onClick = std::move(callback);
    return *this;
}

Widget& Widget::SetOnActivate(std::function<void()> callback) {
    onActivate = std::move(callback);
    return *this;
}

Widget& Widget::SetOnHoverChange(std::function<void(bool)> callback) {
    onHoverChange = std::move(callback);
    return *this;
}

Widget& Widget::SetOnKeyPress(std::function<void(int)> callback) {
    onKeyPress = std::move(callback);
    return *this;
}

Widget& Widget::SetOnKeyDown(std::function<void(int)> callback) {
    onKeyDown = std::move(callback);
    return *this;
}

Widget& Widget::SetOnKeyUp(std::function<void(int)> callback) {
    onKeyUp = std::move(callback);
    return *this;
}

void Widget::ReleaseAllKeys() {
    // Move heldKeys out and copy onKeyUp before firing anything —
    // onKeyUp may call Widget::Remove() on this widget (destroying it),
    // so nothing below the first callback call may touch `this` again.
    std::vector<int> keys = std::move(heldKeys);
    auto callback = onKeyUp;
    for (int key : keys) {
        if (callback) callback(key);
    }
}

void Widget::PollPointerEvents(const Rectangle& rect) {
    Vector2 mouse = CurrentInput().GetMousePosition();
    bool hovered =
        !g_pointerEventsSuppressed && CheckCollisionPointRec(mouse, rect);
    if (hovered && layerToken) {
        hovered = GlobalLayerStacker().IsTopmostAt(*layerToken, mouse);
    }
    bool hoverChanged = hovered != wasHovered;
    wasHovered = hovered;

    if (CurrentInput().IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered) {
        pressOrigin = true;
        MoveFocusTo(this);
    }

    bool firesClick = false;
    if (CurrentInput().IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        firesClick = hovered && pressOrigin;
        pressOrigin = false;
    }

    pointerDown = hovered && CurrentInput().IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    // Everything above this point is done touching `this`. Copy out
    // whichever callbacks are about to fire before calling any of them:
    // a callback (e.g. one that calls Widget::Remove() on this widget)
    // may destroy `this`, which would make any further access to it —
    // including reading another callback off it — undefined behavior.
    auto hoverChangeCallback = hoverChanged ? onHoverChange : nullptr;
    auto clickCallback = firesClick ? onClick : nullptr;
    auto activateCallback = firesClick ? onActivate : nullptr;

    if (hoverChangeCallback) hoverChangeCallback(hovered);
    if (clickCallback) clickCallback();
    if (activateCallback) activateCallback();
}

void Widget::CollectFocusable(std::vector<Widget*>& out) {
    if (focusable) out.push_back(this);
}

void Widget::ProcessKeyboardFocus(Widget& root) {
    // main.cpp (via WM) calls this once per window tree, so it can run
    // more than once within the same real frame. Everything below that
    // isn't scoped to a specific root (repeat timing, onActivate, raw key
    // events) must still only happen once per real frame — gate that work
    // behind internal::BeginKeyboardFocusFrame()'s flag rather than
    // comparing CurrentInput().GetTime() reads: that only reliably detects
    // "still the same frame" for a simulated clock that advances in fixed
    // steps (FakeInput::AdvanceTime()) — real raylib's GetTime() is a live
    // high-resolution clock, so two calls microseconds apart (this
    // function running for a second window in the same real frame) return
    // different values, making every call look like a new frame and
    // double-firing onActivate for whatever's focused.
    static float tabHeldSeconds = 0.0f;
    static bool tabRepeatedThisFrame = false;
    bool newFrame = g_newKeyboardFocusFrame;
    if (newFrame) {
        g_newKeyboardFocusFrame = false;
        tabRepeatedThisFrame = IsKeyRepeated(KEY_TAB, tabHeldSeconds);
    }

    if (tabRepeatedThisFrame) {
        std::vector<Widget*> focusableWidgets;
        root.CollectFocusable(focusableWidgets);
        auto it = std::find(
            focusableWidgets.begin(), focusableWidgets.end(), g_focusedWidget
        );
        // Only cycle within `root` if focus already lives in this tree, or
        // nothing is focused anywhere yet — otherwise this call belongs to
        // a different window than the one that currently owns focus, and
        // must leave it alone (each window's tree gets its own call this
        // frame; without this check, whichever call runs second would
        // always steal focus into its own tree).
        bool ownsFocus = it != focusableWidgets.end();
        bool nothingFocusedAnywhere = g_focusedWidget == nullptr;
        if (!focusableWidgets.empty() &&
            (ownsFocus || nothingFocusedAnywhere)) {
            bool backward = CurrentInput().IsKeyDown(KEY_LEFT_SHIFT) ||
                             CurrentInput().IsKeyDown(KEY_RIGHT_SHIFT);
            size_t n = focusableWidgets.size();
            size_t nextIndex;
            if (it == focusableWidgets.end()) {
                nextIndex = backward ? n - 1 : 0;
            } else {
                size_t current =
                    static_cast<size_t>(it - focusableWidgets.begin());
                nextIndex =
                    backward ? (current + n - 1) % n : (current + 1) % n;
            }
            MoveFocusTo(focusableWidgets[nextIndex]);
        }
    }

    // Everything below reads/mutates only g_focusedWidget's own state, not
    // anything root-specific, so it must run exactly once per real frame —
    // not once per root call — or onActivate/onKeyDown would double-fire.
    if (!newFrame) return;

    if (g_focusedWidget) {
        g_focusedWidget->keyDown = CurrentInput().IsKeyDown(KEY_ENTER) ||
                                    CurrentInput().IsKeyDown(KEY_SPACE);
    }

    if ((CurrentInput().IsKeyPressed(KEY_ENTER) ||
         CurrentInput().IsKeyPressed(KEY_SPACE)) &&
        g_focusedWidget) {
        // Copy the callback out before calling it — onActivate may call
        // Widget::Remove() on the focused widget (or an ancestor of it),
        // destroying it.
        auto activateCallback = g_focusedWidget->onActivate;
        if (activateCallback) activateCallback();
    }

    // Raw key press/down/up bookkeeping for the focused widget. Drains
    // GetKeyPressed()'s frame-scoped queue exactly once, then walks the
    // widget's own heldKeys to fire onKeyDown every frame a key stays held
    // and onKeyUp the frame it's released. onKeyPress/onKeyDown/onKeyUp may
    // call Widget::Remove() on the focused widget (or an ancestor),
    // destroying it mid-loop — `focusedAlive` (a copy of its alive-token,
    // which outlives the widget) is checked before every further touch of
    // `focusedWidget`, and GetKeyPressed() is still drained to completion
    // even after that happens, so no key events leak into next frame.
    if (g_focusedWidget) {
        Widget* focusedWidget = g_focusedWidget;
        std::shared_ptr<bool> focusedAlive = focusedWidget->aliveToken;

        int key;
        while ((key = CurrentInput().GetKeyPressed()) != 0) {
            if (!*focusedAlive) continue;
            if (std::find(
                    focusedWidget->heldKeys.begin(),
                    focusedWidget->heldKeys.end(), key
                ) == focusedWidget->heldKeys.end()) {
                focusedWidget->heldKeys.push_back(key);
            }
            auto onKeyPress = focusedWidget->onKeyPress;
            if (onKeyPress) onKeyPress(key);
        }

        for (size_t i = 0; *focusedAlive && i < focusedWidget->heldKeys.size();) {
            int heldKey = focusedWidget->heldKeys[i];
            if (CurrentInput().IsKeyUp(heldKey)) {
                auto onKeyUp = focusedWidget->onKeyUp;
                if (onKeyUp) onKeyUp(heldKey);
                if (!*focusedAlive) break;
                focusedWidget->heldKeys.erase(
                    focusedWidget->heldKeys.begin() +
                    static_cast<std::ptrdiff_t>(i)
                );
            } else {
                auto onKeyDown = focusedWidget->onKeyDown;
                if (onKeyDown) onKeyDown(heldKey);
                if (!*focusedAlive) break;
                i++;
            }
        }
    }
}

}  // namespace UI
