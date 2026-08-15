#pragma once

#include <raylib.h>

#include <functional>
#include <optional>

namespace ui {

class Container;

/// Base of every UI element. Owns nothing about its children; layout and
/// painting are two separate passes so the flexbox math can be reasoned
/// about (and tested) without touching raylib.
class Widget {
   public:
    /// Clears the global focus pointer if this widget currently holds it,
    /// so a destroyed widget can never be read back as focused.
    virtual ~Widget();

    /// Recomputes computedRect (and, for containers, every child's rect)
    /// for the space `bounds` given by the parent. Implementations should
    /// return early without recomputing when !layoutDirty and bounds is
    /// unchanged.
    virtual void Layout(const Rectangle& bounds);

    /// Polls current input state against the last computed rect, firing
    /// onClick/onHoverChange as needed and caching the result for Draw() to
    /// read back. Runs after Layout() and before Draw() each frame.
    /// Containers must recurse into their children.
    virtual void ProcessEvents();

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
    /// Share of the parent Container's leftover main-axis space this widget
    /// should claim, relative to its siblings' grow factors. 0 = don't
    /// grow (the default). Equivalent to CSS flex-grow.
    Widget& SetGrow(float grow);
    /// How much this widget shrinks, relative to its siblings, when the
    /// parent Container is too small to fit everyone at their base size.
    /// Equivalent to CSS flex-shrink; defaults to 1.
    Widget& SetShrink(float shrink);

    /// Registers a callback fired once on the frame the widget is clicked:
    /// the mouse button must have been pressed while the pointer was over
    /// this widget's computed rect, and is then released while the pointer
    /// is still (or again) over it. Dragging the press origin off the
    /// widget before releasing cancels the click. Pass a default-constructed
    /// std::function to clear.
    Widget& SetOnClick(std::function<void()> callback);

    /// Registers a callback fired on the frame hover state changes, with
    /// the new hover state (true = pointer just entered, false = pointer
    /// just left). Not fired every frame while hovered — only on
    /// transitions.
    Widget& SetOnHoverChange(std::function<void(bool)> callback);

    /// The rectangle computed by the most recent Layout() call.
    const Rectangle& GetComputedRect() const { return computedRect; }

    /// Whether this widget currently holds the (single, app-wide) input
    /// focus. Only widgets that opt in (see `focusable`) can ever become
    /// focused; for everything else this is always false.
    bool IsFocused() const { return focused; }

    friend class Container;

   protected:
    Widget* parent = nullptr;
    bool layoutDirty = true;
    Rectangle computedRect{};

    std::optional<float> fixedWidth;
    std::optional<float> fixedHeight;
    float growFactor = 0.0f;
    float shrinkFactor = 1.0f;

    std::function<void()> onClick;
    std::function<void(bool)> onHoverChange;
    bool wasHovered = false;
    bool pressOrigin = false;

    /// Whether the pointer is currently down and inside computedRect, as of
    /// the last ProcessEvents() call. Draw() reads this instead of polling
    /// input itself, since Draw() is const.
    bool pointerDown = false;

    /// Whether this widget type participates in focus at all. False for
    /// every Widget by default; set true by a subclass's constructor (e.g.
    /// Button) to opt in. Purely cosmetic for now — there is no keyboard
    /// navigation, so this only gates whether a press can claim the global
    /// focus pointer and whether IsFocused() can ever return true.
    bool focusable = false;

    /// Whether this widget currently holds the global focus pointer, kept
    /// in sync by PollPointerEvents() so Draw() (const) can read it without
    /// touching global state itself.
    bool focused = false;

    /// Natural width when nothing constrains this widget, e.g. a label's
    /// measured text extent. Used as the flex-basis whenever fixedWidth
    /// isn't set. Ignored on the axis SetWidth/SetHeight applies to.
    virtual float IntrinsicWidth() const { return 0.0f; }
    /// Natural height when nothing constrains this widget. See
    /// IntrinsicWidth().
    virtual float IntrinsicHeight() const { return 0.0f; }

    /// Reads current mouse state against `rect`, fires onClick/
    /// onHoverChange as needed, and updates pointerDown. Called from
    /// ProcessEvents() by widgets that support input.
    void PollPointerEvents(const Rectangle& rect);
};

}  // namespace ui
