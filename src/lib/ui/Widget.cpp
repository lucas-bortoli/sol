#include "Widget.h"

namespace ui {

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
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (hovered && pressOrigin && onClick) {
            onClick();
        }
        pressOrigin = false;
    }
    pointerDown = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

}  // namespace ui
