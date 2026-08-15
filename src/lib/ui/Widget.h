#pragma once

#include <raylib.h>

#include <optional>

namespace ui {

class Panel;

/// Base of every UI element. Owns nothing about its children; layout and
/// painting are two separate passes so the flexbox math can be reasoned
/// about (and tested) without touching raylib.
class Widget {
   public:
    virtual ~Widget() = default;

    /// Recomputes computedRect (and, for containers, every child's rect)
    /// for the space `bounds` given by the parent. Implementations should
    /// return early without recomputing when !layoutDirty and bounds is
    /// unchanged.
    virtual void Layout(const Rectangle& bounds);

    /// Paints using the last computed rect. Runs unconditionally every
    /// frame; only the layout pass is skipped when clean.
    virtual void Draw() const = 0;

    /// Marks this widget (and, transitively, ancestors) as needing a
    /// layout recompute. Call after any mutation that can change size
    /// (SetText, property setters, adding/removing children).
    void Invalidate();

    /// Pin this widget's width instead of letting the parent size it from
    /// IntrinsicWidth(). Equivalent to CSS width (not flex-basis).
    Widget& SetWidth(float width);
    /// Pin this widget's height instead of letting the parent size it from
    /// IntrinsicHeight(). Equivalent to CSS height.
    Widget& SetHeight(float height);
    /// Share of the parent Panel's leftover main-axis space this widget
    /// should claim, relative to its siblings' grow factors. 0 = don't
    /// grow (the default). Equivalent to CSS flex-grow.
    Widget& SetGrow(float grow);
    /// How much this widget shrinks, relative to its siblings, when the
    /// parent Panel is too small to fit everyone at their base size.
    /// Equivalent to CSS flex-shrink; defaults to 1.
    Widget& SetShrink(float shrink);

    /// The rectangle computed by the most recent Layout() call.
    const Rectangle& GetComputedRect() const { return computedRect; }

    friend class Panel;

   protected:
    Widget* parent = nullptr;
    bool layoutDirty = true;
    Rectangle computedRect{};

    std::optional<float> fixedWidth;
    std::optional<float> fixedHeight;
    float growFactor = 0.0f;
    float shrinkFactor = 1.0f;

    /// Natural width when nothing constrains this widget, e.g. a label's
    /// measured text extent. Used as the flex-basis whenever fixedWidth
    /// isn't set. Ignored on the axis SetWidth/SetHeight applies to.
    virtual float IntrinsicWidth() const { return 0.0f; }
    /// Natural height when nothing constrains this widget. See
    /// IntrinsicWidth().
    virtual float IntrinsicHeight() const { return 0.0f; }
};

}  // namespace ui
