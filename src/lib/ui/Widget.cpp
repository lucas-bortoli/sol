#include "Widget.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {
Widget* g_focusedWidget = nullptr;
}  // namespace

bool IsKeyRepeated(int key, float& heldSeconds, float delay, float interval) {
    if (IsKeyPressed(key)) {
        heldSeconds = 0.0f;
        return true;
    }
    if (!IsKeyDown(key)) {
        heldSeconds = 0.0f;
        return false;
    }

    float previous = heldSeconds;
    heldSeconds += GetFrameTime();
    if (previous < delay) {
        return heldSeconds >= delay;
    }
    float previousTicks = floorf((previous - delay) / interval);
    float currentTicks = floorf((heldSeconds - delay) / interval);
    return currentTicks > previousTicks;
}

Widget::~Widget() {
    if (g_focusedWidget == this) g_focusedWidget = nullptr;
}

void Widget::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
}

void Widget::ProcessEvents() { PollPointerEvents(computedRect); }

void Widget::Invalidate() {
    if (!layoutDirty) {
        layoutDirty = true;
        if (parent) parent->Invalidate();
    }
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
    for (int key : heldKeys) {
        if (onKeyUp) onKeyUp(key);
    }
    heldKeys.clear();
}

void Widget::PollPointerEvents(const Rectangle& rect) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, rect);
    if (hovered != wasHovered) {
        wasHovered = hovered;
        if (onHoverChange) onHoverChange(hovered);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered) {
        pressOrigin = true;
        if (focusable && g_focusedWidget != this) {
            if (g_focusedWidget) {
                g_focusedWidget->focused = false;
                g_focusedWidget->keyDown = false;
                g_focusedWidget->ReleaseAllKeys();
            }
            g_focusedWidget = this;
            focused = true;
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (hovered && pressOrigin) {
            if (onClick) onClick();
            if (onActivate) onActivate();
        }
        pressOrigin = false;
    }
    pointerDown = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

void Widget::CollectFocusable(std::vector<Widget*>& out) {
    if (focusable) out.push_back(this);
}

void Widget::ProcessKeyboardFocus(Widget& root) {
    // main.cpp calls this once per window tree, so it can run more than
    // once within the same real frame. Everything below that isn't scoped
    // to a specific root (repeat timing, onActivate, raw key events) must
    // still only happen once per real frame — cache a "new frame?" decision
    // by GetTime() (changes once per real frame) and gate that work behind
    // it so extra calls in the same frame are no-ops for it.
    static float tabHeldSeconds = 0.0f;
    static double lastFrameTime = -1.0;
    static bool tabRepeatedThisFrame = false;
    double now = GetTime();
    bool newFrame = now != lastFrameTime;
    if (newFrame) {
        lastFrameTime = now;
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
            bool backward =
                IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
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
            if (g_focusedWidget) {
                g_focusedWidget->focused = false;
                g_focusedWidget->keyDown = false;
                g_focusedWidget->ReleaseAllKeys();
            }
            g_focusedWidget = focusableWidgets[nextIndex];
            g_focusedWidget->focused = true;
        }
    }

    // Everything below reads/mutates only g_focusedWidget's own state, not
    // anything root-specific, so it must run exactly once per real frame —
    // not once per root call — or onActivate/onKeyDown would double-fire.
    if (!newFrame) return;

    if (g_focusedWidget) {
        g_focusedWidget->keyDown = IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_SPACE);
    }

    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) &&
        g_focusedWidget) {
        if (g_focusedWidget->onActivate) g_focusedWidget->onActivate();
    }

    // Raw key press/down/up bookkeeping for the focused widget. Drains
    // GetKeyPressed()'s frame-scoped queue exactly once, then walks the
    // widget's own heldKeys to fire onKeyDown every frame a key stays held
    // and onKeyUp the frame it's released.
    if (g_focusedWidget) {
        Widget& focusedWidget = *g_focusedWidget;
        int key;
        while ((key = GetKeyPressed()) != 0) {
            if (std::find(
                    focusedWidget.heldKeys.begin(),
                    focusedWidget.heldKeys.end(),
                    key
                ) == focusedWidget.heldKeys.end()) {
                focusedWidget.heldKeys.push_back(key);
            }
            if (focusedWidget.onKeyPress) focusedWidget.onKeyPress(key);
        }

        for (size_t i = 0; i < focusedWidget.heldKeys.size();) {
            int heldKey = focusedWidget.heldKeys[i];
            if (IsKeyUp(heldKey)) {
                if (focusedWidget.onKeyUp) focusedWidget.onKeyUp(heldKey);
                focusedWidget.heldKeys.erase(
                    focusedWidget.heldKeys.begin() +
                    static_cast<std::ptrdiff_t>(i)
                );
            } else {
                if (focusedWidget.onKeyDown) focusedWidget.onKeyDown(heldKey);
                i++;
            }
        }
    }
}

}  // namespace ui
