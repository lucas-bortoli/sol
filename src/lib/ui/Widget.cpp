#include "Widget.h"

namespace ui {

void Widget::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
}

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

}  // namespace ui
