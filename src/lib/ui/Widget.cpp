#include "Widget.h"

#include <algorithm>

namespace ui {

namespace {
Widget* g_focusedWidget = nullptr;
}  // namespace

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
    if (IsKeyPressed(KEY_TAB)) {
        std::vector<Widget*> focusableWidgets;
        root.CollectFocusable(focusableWidgets);
        if (!focusableWidgets.empty()) {
            bool backward =
                IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            auto it = std::find(
                focusableWidgets.begin(),
                focusableWidgets.end(),
                g_focusedWidget
            );
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

    if (g_focusedWidget) {
        g_focusedWidget->keyDown = IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_SPACE);
    }

    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) &&
        g_focusedWidget) {
        if (g_focusedWidget->onActivate) g_focusedWidget->onActivate();
    }

    // Raw key press/down/up bookkeeping for the focused widget. Drains
    // GetKeyPressed()'s frame-scoped queue exactly once (same reasoning as
    // KEY_TAB above — must happen from this single per-frame call, not from
    // per-widget ProcessEvents()), then walks the widget's own heldKeys to
    // fire onKeyDown every frame a key stays held and onKeyUp the frame it's
    // released.
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
