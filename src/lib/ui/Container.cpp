#include "Container.h"

#include "../../palette.h"
#include "Input.h"

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
    Padding initialPadding,
    Overflow initialOverflow,
    std::vector<std::unique_ptr<Widget>> initialChildren
)
    : direction(initialDirection),
      justify(initialJustify),
      align(initialAlign),
      gap(initialGap),
      padding(initialPadding),
      overflow(initialOverflow),
      children(std::move(initialChildren)) {
    for (auto& child : children) child->parent = this;
}

void Container::AppendChild(std::unique_ptr<Widget> child) {
    InsertChild(children.size(), std::move(child));
}

void Container::InsertChild(size_t index, std::unique_ptr<Widget> child) {
    index = std::min(index, children.size());
    child->parent = this;
    children.insert(
        children.begin() + static_cast<std::ptrdiff_t>(index), std::move(child)
    );
    Invalidate();
}

std::unique_ptr<Widget> Container::RemoveChild(Widget* child) {
    auto it = std::find_if(
        children.begin(), children.end(),
        [child](const std::unique_ptr<Widget>& c) { return c.get() == child; }
    );
    if (it == children.end()) return nullptr;

    std::unique_ptr<Widget> removed = std::move(*it);
    children.erase(it);
    removed->parent = nullptr;
    Invalidate();
    return removed;
}

size_t Container::ChildCount() const { return children.size(); }

Widget* Container::ChildAt(size_t index) const {
    return children.at(index).get();
}

std::optional<size_t> Container::IndexOf(const Widget* child) const {
    for (size_t i = 0; i < children.size(); i++) {
        if (children[i].get() == child) return i;
    }
    return std::nullopt;
}

std::vector<Widget*> Container::Children() const {
    std::vector<Widget*> out;
    out.reserve(children.size());
    for (const auto& c : children) out.push_back(c.get());
    return out;
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
    return size + padding.left + padding.right;
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
    return size + padding.top + padding.bottom;
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
        bounds.x + padding.left,
        bounds.y + padding.top,
        std::max(0.0f, bounds.width - padding.left - padding.right),
        std::max(0.0f, bounds.height - padding.top - padding.bottom),
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

    // 2. Grow to fill extra space, or shrink to fit an overflow — unless
    // Overflow::Scroll is letting content overflow instead of shrinking.
    const bool letOverflow = overflow == Overflow::Scroll && delta < 0;
    std::vector<float> finalMain = baseMain;
    if (delta > 0 && totalGrow > 0) {
        for (size_t i = 0; i < n; i++) {
            finalMain[i] += delta * (children[i]->growFactor / totalGrow);
        }
    } else if (delta < 0 && totalShrink > 0 && !letOverflow) {
        for (size_t i = 0; i < n; i++) {
            float weight = children[i]->shrinkFactor * baseMain[i];
            float shrinkAmount = -delta * (weight / totalShrink);
            finalMain[i] = std::max(0.0f, baseMain[i] - shrinkAmount);
        }
    }

    contentMainSize = totalGap;
    for (float f : finalMain) contentMainSize += f;
    viewportMainSize = mainSize;
    if (overflow == Overflow::Scroll) {
        float maxScroll = std::max(0.0f, contentMainSize - mainSize);
        scrollOffsetPx = std::clamp(scrollOffsetPx, 0.0f, maxScroll);
    } else {
        scrollOffsetPx = 0.0f;
    }

    // 3. Whatever space grow/shrink didn't consume is left for `justify`.
    float usedMain = totalGap;
    for (float f : finalMain) usedMain += f;
    const float leftover = std::max(0.0f, mainSize - usedMain);

    float cursor = letOverflow ? -scrollOffsetPx : 0.0f;
    float justifyGap = gap;
    // Overflowing content ignores justify entirely — there's no leftover
    // space to distribute, and cursor must stay anchored to the scroll
    // offset rather than being recentered/reversed by the switch below.
    switch (letOverflow ? Justify::Start : justify) {
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

namespace {
constexpr float kThumbThickness = 6.0f;
constexpr float kThumbMargin = 2.0f;
constexpr float kThumbMinLength = 20.0f;
constexpr float kWheelPxPerNotch = 40.0f;
}  // namespace

bool Container::IsOverflowing() const {
    return overflow == Overflow::Scroll &&
           contentMainSize > viewportMainSize + 0.5f;
}

Container::ThumbMetrics Container::ComputeThumbMetrics() const {
    const bool horizontal = IsHorizontal(direction);
    float trackLen =
        (horizontal ? computedRect.width : computedRect.height) -
        2 * kThumbMargin;
    float thumbLen = trackLen;
    if (contentMainSize > 0.0f) {
        thumbLen = std::max(
            kThumbMinLength, trackLen * (viewportMainSize / contentMainSize)
        );
    }
    thumbLen = std::min(thumbLen, trackLen);
    float maxScroll = std::max(0.0f, contentMainSize - viewportMainSize);
    return {trackLen, thumbLen, maxScroll};
}

Rectangle Container::ThumbRect() const {
    const bool horizontal = IsHorizontal(direction);
    ThumbMetrics m = ComputeThumbMetrics();
    float freeTrack = m.trackLen - m.thumbLen;
    float trackPos =
        m.maxScroll > 0.0f ? freeTrack * (scrollOffsetPx / m.maxScroll) : 0.0f;

    if (horizontal) {
        return Rectangle{
            computedRect.x + kThumbMargin + trackPos,
            computedRect.y + computedRect.height - kThumbThickness -
                kThumbMargin,
            m.thumbLen, kThumbThickness
        };
    }
    return Rectangle{
        computedRect.x + computedRect.width - kThumbThickness - kThumbMargin,
        computedRect.y + kThumbMargin + trackPos, kThumbThickness, m.thumbLen
    };
}

void Container::ProcessEvents() {
    Widget::ProcessEvents();

    if (IsOverflowing()) {
        const bool horizontal = IsHorizontal(direction);
        Vector2 mouse = CurrentInput().GetMousePosition();
        float mouseMain = horizontal ? mouse.x : mouse.y;

        if (draggingThumb) {
            if (CurrentInput().IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                ThumbMetrics m = ComputeThumbMetrics();
                float freeTrack = m.trackLen - m.thumbLen;
                float scale =
                    freeTrack > 0.0f ? m.maxScroll / freeTrack : 0.0f;
                float newOffset = std::clamp(
                    dragStartScrollOffsetPx +
                        (mouseMain - dragStartMouseMain) * scale,
                    0.0f, m.maxScroll
                );
                if (newOffset != scrollOffsetPx) {
                    scrollOffsetPx = newOffset;
                    Invalidate();
                }
            } else {
                draggingThumb = false;
            }
        } else if (CurrentInput().IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                   CheckCollisionPointRec(mouse, ThumbRect())) {
            draggingThumb = true;
            dragStartMouseMain = mouseMain;
            dragStartScrollOffsetPx = scrollOffsetPx;
        } else if (CheckCollisionPointRec(mouse, computedRect)) {
            float wheel = CurrentInput().GetMouseWheelMove();
            if (wheel != 0.0f) {
                ThumbMetrics m = ComputeThumbMetrics();
                float newOffset = std::clamp(
                    scrollOffsetPx - wheel * kWheelPxPerNotch, 0.0f,
                    m.maxScroll
                );
                if (newOffset != scrollOffsetPx) {
                    scrollOffsetPx = newOffset;
                    Invalidate();
                }
            }
        }
    }

    // Snapshot children (pointer + alive-token) before iterating: a
    // child's callback (fired from child->ProcessEvents() below) may call
    // Widget::Remove() on any ancestor, including `this` container — not
    // just on itself — which would destroy `this->children` (the very
    // vector a plain range-for would still be iterating) or a
    // not-yet-visited sibling out from under us. Checking each token
    // before touching the corresponding object is what makes that safe:
    // the token is a separate heap allocation that outlives the Widget it
    // observes.
    std::shared_ptr<bool> selfAlive = aliveToken;
    std::vector<std::pair<Widget*, std::shared_ptr<bool>>> snapshot;
    snapshot.reserve(children.size());
    for (auto& child : children) snapshot.emplace_back(child.get(), child->aliveToken);

    for (auto& [child, childAlive] : snapshot) {
        if (!*childAlive) continue;  // already removed/destroyed by an earlier sibling's callback
        child->ProcessEvents();
        if (!*selfAlive) return;  // `this` was destroyed — `this->children` no longer exists
    }
}

void Container::CollectFocusable(std::vector<Widget*>& out) {
    Widget::CollectFocusable(out);
    for (auto& child : children) child->CollectFocusable(out);
}

void Container::Draw() const {
    bool overflowing = IsOverflowing();
    if (overflowing) {
        BeginScissorMode(
            static_cast<int>(computedRect.x), static_cast<int>(computedRect.y),
            static_cast<int>(computedRect.width),
            static_cast<int>(computedRect.height)
        );
    }

    for (const auto& child : children) child->Draw();

    if (overflowing) {
        EndScissorMode();
        Rectangle thumb = ThumbRect();
        DrawRectangleRounded(thumb, 0.5f, 4, Fade(NEUTRAL_600, 0.5f));
    }
}

}  // namespace ui
