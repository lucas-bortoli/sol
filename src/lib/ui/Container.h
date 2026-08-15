#pragma once

#include <raylib.h>

#include <memory>
#include <optional>
#include <vector>

#include "Widget.h"

namespace ui {

/// Axis a Container's children are laid out along, and which end is "first".
/// Mirrors CSS flex-direction.
enum class Direction { Row, RowReverse, Column, ColumnReverse };

/// How leftover main-axis space (after grow/shrink) is distributed between
/// a Container's children. Mirrors CSS justify-content.
enum class Justify {
    Start,
    End,
    Center,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

/// How a child is positioned/sized on the cross axis. Mirrors CSS
/// align-items; Stretch fills the cross axis unless the child has a fixed
/// cross-axis size.
enum class Align { Start, End, Center, Stretch };

/// Whether a Container clips and shrinks its children to fit (Visible, the
/// default — matches today's behavior) or lets main-axis content overflow
/// and become scrollable behind a thin overlay scrollbar (Scroll). Mirrors
/// CSS overflow, restricted to the container's main axis.
enum class Overflow { Visible, Scroll };

/// Per-side padding for a Container's content, in px. Mirrors CSS
/// padding-top/-right/-bottom/-left.
struct Padding {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;
};

/// A flexbox-style container: lays children out along a main axis
/// (Direction) and distributes/aligns them per Justify/Align, honoring each
/// child's grow/shrink factors and fixed sizes.
///
/// Not built directly — use the Row()/Column() tree-literal functions in
/// Tree.h, which own child-list construction and keep the declarative call
/// shape.
class Container : public Widget {
   public:
    /// Takes ownership of initialChildren and sets each one's parent to
    /// this Container. Prefer building via Row()/Column() rather than calling
    /// this directly.
    Container(
        Direction initialDirection,
        Justify initialJustify,
        Align initialAlign,
        float initialGap,
        Padding initialPadding,
        Overflow initialOverflow,
        std::vector<std::unique_ptr<Widget>> initialChildren
    );

    /// Appends a child at runtime, e.g. for a dynamically-growing list.
    /// Tree literals built via Row()/Column() don't need this.
    void AppendChild(std::unique_ptr<Widget> child);

    /// Inserts a child at `index` (clamped to [0, current child count], so
    /// an out-of-range index behaves like AppendChild). Tree literals
    /// built via Row()/Column() don't need this — it's for runtime
    /// mutation.
    void InsertChild(size_t index, std::unique_ptr<Widget> child);

    /// Detaches `child` from this container and returns ownership of it,
    /// or nullptr if `child` isn't actually a child of this container.
    /// Widget::Remove() is usually more convenient than calling this
    /// directly from outside.
    std::unique_ptr<Widget> RemoveChild(Widget* child);

    /// Number of direct children.
    size_t ChildCount() const;
    /// The child at `index`. Throws std::out_of_range if index is out of
    /// bounds (same convention as std::vector::at).
    Widget* ChildAt(size_t index) const;
    /// `child`'s position among this container's direct children, or
    /// nullopt if it isn't one.
    std::optional<size_t> IndexOf(const Widget* child) const;
    /// A snapshot of every direct child, in order. Builds a fresh vector
    /// of non-owning pointers each call — fine for on-demand use (mirrors
    /// CollectFocusable's own on-demand traversal), not meant to be
    /// polled every frame.
    std::vector<Widget*> Children() const;

    void Layout(const Rectangle& bounds) override;
    void ProcessEvents() override;
    void Draw() const override;

   protected:
    float IntrinsicWidth() const override;
    float IntrinsicHeight() const override;
    void CollectFocusable(std::vector<Widget*>& out) override;

   private:
    Direction direction;
    Justify justify;
    Align align;
    float gap;
    Padding padding;
    Overflow overflow;

    // Overflow::Scroll state. Mutable: derived draw/scroll state recomputed
    // from Layout(), not model state — same rationale as TextArea's
    // scrollOffsetRows/scrollOffsetXPx.
    mutable float scrollOffsetPx = 0.0f;
    mutable float contentMainSize = 0.0f;
    mutable float viewportMainSize = 0.0f;
    mutable bool draggingThumb = false;
    mutable float dragStartMouseMain = 0.0f;
    mutable float dragStartScrollOffsetPx = 0.0f;

    std::vector<std::unique_ptr<Widget>> children;

    /// Whether content currently overflows the viewport on the main axis —
    /// the gate for scissor clipping, wheel/drag handling, and drawing the
    /// scrollbar thumb.
    bool IsOverflowing() const;

    /// Track/thumb length in px and max scroll range, shared by ThumbRect()
    /// and the drag-to-scroll math in ProcessEvents().
    struct ThumbMetrics {
        float trackLen;
        float thumbLen;
        float maxScroll;
    };
    ThumbMetrics ComputeThumbMetrics() const;
    /// Computed thumb rectangle in screen space, valid only when
    /// IsOverflowing() — used by both ProcessEvents() (hit-testing drag)
    /// and Draw() (painting).
    Rectangle ThumbRect() const;
};

}  // namespace ui
