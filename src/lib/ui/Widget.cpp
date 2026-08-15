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
}

}  // namespace ui
