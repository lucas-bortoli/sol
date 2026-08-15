#include "Container.h"

#include <algorithm>

namespace ui {

namespace {

bool IsHorizontal(Direction direction) {
    return direction == Direction::Row || direction == Direction::RowReverse;
}

bool IsReversed(Direction direction) {
    return direction == Direction::RowReverse ||
           direction == Direction::ColumnReverse;
}

bool RectEquals(const Rectangle& a, const Rectangle& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width &&
           a.height == b.height;
}

}  // namespace

Container::Container(
    Direction initialDirection,
    Justify initialJustify,
    Align initialAlign,
    float initialGap,
    float initialPadding,
    std::vector<std::unique_ptr<Widget>> initialChildren
)
    : direction(initialDirection),
      justify(initialJustify),
      align(initialAlign),
      gap(initialGap),
      padding(initialPadding),
      children(std::move(initialChildren)) {
    for (auto& child : children) child->parent = this;
}

void Container::AppendChild(std::unique_ptr<Widget> child) {
    child->parent = this;
    children.push_back(std::move(child));
    Invalidate();
}

float Container::IntrinsicWidth() const {
    const bool horizontal = IsHorizontal(direction);
    float size = 0.0f;
    for (const auto& child : children) {
        float childWidth = child->fixedWidth.value_or(child->IntrinsicWidth());
        size = horizontal ? size + childWidth : std::max(size, childWidth);
    }
    if (horizontal && children.size() > 1) {
        size += gap * (children.size() - 1);
    }
    return size + 2 * padding;
}

float Container::IntrinsicHeight() const {
    const bool horizontal = IsHorizontal(direction);
    float size = 0.0f;
    for (const auto& child : children) {
        float childHeight =
            child->fixedHeight.value_or(child->IntrinsicHeight());
        size = horizontal ? std::max(size, childHeight) : size + childHeight;
    }
    if (!horizontal && children.size() > 1) {
        size += gap * (children.size() - 1);
    }
    return size + 2 * padding;
}

void Container::Layout(const Rectangle& bounds) {
    if (!layoutDirty && RectEquals(bounds, computedRect)) return;

    computedRect = bounds;

    const size_t n = children.size();
    if (n == 0) {
        layoutDirty = false;
        return;
    }

    const bool horizontal = IsHorizontal(direction);
    const bool reversed = IsReversed(direction);

    const Rectangle content = {
        bounds.x + padding,
        bounds.y + padding,
        std::max(0.0f, bounds.width - 2 * padding),
        std::max(0.0f, bounds.height - 2 * padding),
    };

    const float mainSize = horizontal ? content.width : content.height;
    const float crossSize = horizontal ? content.height : content.width;

    // 1. Base main-axis size per child (fixed size wins, else intrinsic).
    std::vector<float> baseMain(n);
    float totalBase = 0.0f;
    float totalGrow = 0.0f;
    float totalShrink = 0.0f;

    for (size_t i = 0; i < n; i++) {
        Widget* child = children[i].get();
        std::optional<float> fixed =
            horizontal ? child->fixedWidth : child->fixedHeight;
        float base = fixed.value_or(
            horizontal ? child->IntrinsicWidth() : child->IntrinsicHeight()
        );
        baseMain[i] = base;
        totalBase += base;
        totalGrow += child->growFactor;
        totalShrink += child->shrinkFactor * base;
    }

    const float totalGap = n > 1 ? gap * (n - 1) : 0.0f;
    const float delta = mainSize - totalBase - totalGap;

    // 2. Grow to fill extra space, or shrink to fit an overflow.
    std::vector<float> finalMain = baseMain;
    if (delta > 0 && totalGrow > 0) {
        for (size_t i = 0; i < n; i++) {
            finalMain[i] += delta * (children[i]->growFactor / totalGrow);
        }
    } else if (delta < 0 && totalShrink > 0) {
        for (size_t i = 0; i < n; i++) {
            float weight = children[i]->shrinkFactor * baseMain[i];
            float shrinkAmount = -delta * (weight / totalShrink);
            finalMain[i] = std::max(0.0f, baseMain[i] - shrinkAmount);
        }
    }

    // 3. Whatever space grow/shrink didn't consume is left for `justify`.
    float usedMain = totalGap;
    for (float f : finalMain) usedMain += f;
    const float leftover = std::max(0.0f, mainSize - usedMain);

    float cursor = 0.0f;
    float justifyGap = gap;
    switch (justify) {
        case Justify::Start:
            break;
        case Justify::End:
            cursor = leftover;
            break;
        case Justify::Center:
            cursor = leftover / 2.0f;
            break;
        case Justify::SpaceBetween:
            if (n > 1) justifyGap = gap + leftover / (n - 1);
            break;
        case Justify::SpaceAround:
            if (n > 0) {
                float pad = leftover / n;
                cursor = pad / 2.0f;
                justifyGap = gap + pad;
            }
            break;
        case Justify::SpaceEvenly:
            if (n > 0) {
                float pad = leftover / (n + 1);
                cursor = pad;
                justifyGap = gap + pad;
            }
            break;
    }

    // 4. Place children, walking in reverse order for *-Reverse directions.
    for (size_t k = 0; k < n; k++) {
        size_t i = reversed ? (n - 1 - k) : k;
        Widget* child = children[i].get();
        float childMain = finalMain[i];

        std::optional<float> fixedCross =
            horizontal ? child->fixedHeight : child->fixedWidth;
        float intrinsicCross =
            horizontal ? child->IntrinsicHeight() : child->IntrinsicWidth();

        float childCross;
        float crossOffset;
        if (align == Align::Stretch && !fixedCross.has_value()) {
            childCross = crossSize;
            crossOffset = 0.0f;
        } else {
            childCross =
                std::min(fixedCross.value_or(intrinsicCross), crossSize);
            switch (align) {
                case Align::Start:
                case Align::Stretch:
                    crossOffset = 0.0f;
                    break;
                case Align::End:
                    crossOffset = crossSize - childCross;
                    break;
                case Align::Center:
                    crossOffset = (crossSize - childCross) / 2.0f;
                    break;
            }
        }

        Rectangle childRect =
            horizontal
                ? Rectangle{
                      content.x + cursor, content.y + crossOffset, childMain,
                      childCross
                  }
                : Rectangle{
                      content.x + crossOffset, content.y + cursor, childCross,
                      childMain
                  };

        child->Layout(childRect);
        cursor += childMain + justifyGap;
    }

    layoutDirty = false;
}

void Container::ProcessEvents() {
    Widget::ProcessEvents();
    for (auto& child : children) child->ProcessEvents();
}

void Container::Draw() const {
    for (const auto& child : children) child->Draw();
}

}  // namespace ui
