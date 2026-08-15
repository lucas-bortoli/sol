#pragma once

#include <raylib.h>

#include <memory>
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
        float initialPadding,
        std::vector<std::unique_ptr<Widget>> initialChildren
    );

    /// Appends a child at runtime, e.g. for a dynamically-growing list.
    /// Tree literals built via Row()/Column() don't need this.
    void AppendChild(std::unique_ptr<Widget> child);

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
    float padding;

    std::vector<std::unique_ptr<Widget>> children;
};

}  // namespace ui
