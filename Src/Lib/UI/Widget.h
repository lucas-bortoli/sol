#pragma once

#include <raylib.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "LayerStacker.h"

namespace UI {

class Container;

namespace internal {
/// Suppresses pointer input (hover/press/click/activate) for every widget
/// polled while suppressed — set by WM around an occluded window's whole
/// content tree for the duration of its ProcessEvents() call, so a click
/// in the overlap between two windows can't reach the one that isn't on
/// top. Does not affect keyboard focus/activation.
void SetPointerEventsSuppressed(bool suppressed);
/// The current value set by SetPointerEventsSuppressed() — for a caller
/// that needs to temporarily override it and then restore whatever it
/// was before (e.g. MenuBar unsuppressing its own open popup, which is
/// always topmost over its own window regardless of that window's own
/// occlusion state, without clobbering the enclosing suppression for
/// whatever runs after it).
bool IsPointerEventsSuppressed();

/// Marks the start of a new real engine frame for Widget::
/// ProcessKeyboardFocus()'s once-per-frame bookkeeping (Tab-repeat timing,
/// Enter/Space activation, raw key press/down/up draining) — call exactly
/// once per real frame, before the first ProcessKeyboardFocus() call
/// (WM::internal::ProcessEvents() does this). Needed because that
/// bookkeeping used to detect a "new frame" by comparing consecutive
/// CurrentInput().GetTime() reads, which only holds for a simulated clock
/// that advances in fixed FakeInput::AdvanceTime() steps — real raylib's
/// GetTime() is a live high-resolution clock, so two calls microseconds
/// apart (e.g. ProcessKeyboardFocus() for a second window in the same
/// real frame) reliably return different values, making every call look
/// like a new frame and double-firing onActivate for whichever widget is
/// focused.
void BeginKeyboardFocusFrame();
}  // namespace internal

/// Base of every UI element. Owns nothing about its children; layout and
/// painting are two separate passes so the flexbox math can be reasoned
/// about (and tested) without touching raylib.
class Widget : public LayerStacker::Drawable {
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
    /// onClick/onActivate/onHoverChange as needed and caching the result
    /// for Draw() to read back. Runs after Layout() and before Draw() each
    /// frame. Containers must recurse into their children.
    virtual void ProcessEvents();

    /// Paints using the last computed rect. Runs unconditionally every
    /// frame; only the layout pass is skipped when clean.
    virtual void Draw() const = 0;

    /// Marks this widget (and, transitively, ancestors) as needing a
    /// layout recompute. Call after any mutation that can change size
    /// (SetText, property setters, adding/removing children).
    void Invalidate();

    /// Detaches this widget from its parent (if any) and returns ownership
    /// of it to the caller — DOM Node.remove()-style, but since C++ needs
    /// an explicit owner, the returned unique_ptr IS this widget: keep it
    /// to reattach elsewhere (e.g. pass to another Container's
    /// AppendChild/InsertChild), or let it fall out of scope to destroy it
    /// outright. No-op (returns nullptr) if this widget has no parent.
    /// Safe to call — and discard the result of, destroying the widget —
    /// from any onClick/onActivate/onHoverChange/onKeyPress/onKeyDown/
    /// onKeyUp callback fired on it *or on any of its descendants*, e.g.
    /// a "remove" button calling Remove() on its own parent row rather
    /// than on itself. Every place that dispatches these callbacks
    /// (PollPointerEvents, ReleaseAllKeys, ProcessKeyboardFocus, and
    /// Container::ProcessEvents' child-iteration loop) checks a copy of
    /// the destroyed object's alive-token before touching it again, so a
    /// callback tearing down its own ancestor mid-dispatch can't leave
    /// anything iterating over freed memory.
    std::unique_ptr<Widget> Remove();

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

    /// Registers a callback fired once on the frame the widget is clicked
    /// with the mouse: the mouse button must have been pressed while the
    /// pointer was over this widget's computed rect, and is then released
    /// while the pointer is still (or again) over it. Dragging the press
    /// origin off the widget before releasing cancels the click. Not fired
    /// by keyboard activation — see SetOnActivate for a callback that fires
    /// on both. Pass a default-constructed std::function to clear.
    Widget& SetOnClick(std::function<void()> callback);

    /// Registers a callback fired when the widget is "activated" — either
    /// by the same mouse click that fires onClick, or by pressing
    /// Enter/Space while the widget is focused (see
    /// Widget::ProcessKeyboardFocus). Use this instead of SetOnClick for
    /// anything that should also work from the keyboard, e.g. a Button's
    /// primary action.
    Widget& SetOnActivate(std::function<void()> callback);

    /// Registers a callback fired on the frame hover state changes, with
    /// the new hover state (true = pointer just entered, false = pointer
    /// just left). Not fired every frame while hovered — only on
    /// transitions.
    Widget& SetOnHoverChange(std::function<void(bool)> callback);

    /// Registers a callback fired once on the frame a key is first pressed
    /// while this widget is focused, with the raylib KeyboardKey code.
    /// Independent of onActivate/onClick — a focused TextBox, for example,
    /// sees every key it's sent (including Enter/Space) here regardless of
    /// whether onActivate is also set. Only ever fires for the single
    /// currently-focused widget.
    Widget& SetOnKeyPress(std::function<void(int)> callback);

    /// Registers a callback fired every frame a key is held down while this
    /// widget is focused (including the frame it was first pressed, after
    /// onKeyPress). Stops firing the frame the key is released or focus
    /// moves away.
    Widget& SetOnKeyDown(std::function<void(int)> callback);

    /// Registers a callback fired once on the frame a previously-held key is
    /// released while this widget is focused, or once for each key still
    /// held if focus moves away from this widget first (so a consumer never
    /// sees a "stuck" key with no matching release).
    Widget& SetOnKeyUp(std::function<void(int)> callback);

    /// The rectangle computed by the most recent Layout() call.
    const Rectangle& GetComputedRect() const { return computedRect; }

    /// This widget's natural size (see the protected IntrinsicWidth()/
    /// Height() virtuals below), ignoring any fixed width/height override
    /// — public so a caller driving Layout() externally (e.g. a floating
    /// panel that isn't itself part of a Container's flow layout, like
    /// MenuPopup) can size a bounds rect to fit before calling Layout().
    /// Ordinary tree-literal usage never needs this — Container computes
    /// it internally as part of its own flex arrangement.
    float GetIntrinsicWidth() const { return IntrinsicWidth(); }
    float GetIntrinsicHeight() const { return IntrinsicHeight(); }

    /// This widget's parent Container, or nullptr for a tree root. Only
    /// Container ever has children, so this is typed as Container* rather
    /// than the more generic Widget* — see the forward declaration above.
    Container* GetParent() const { return parent; }

    /// Whether this widget currently holds the (single, app-wide) input
    /// focus. Only widgets that opt in (see `focusable`) can ever become
    /// focused; for everything else this is always false.
    bool IsFocused() const { return focused; }

    /// Whether this widget type participates in focus at all (see the
    /// protected `focusable` field) — public so external code walking a
    /// list of widgets (e.g. MenuBar skipping disabled items/separators
    /// during arrow-key navigation) can tell which ones are eligible
    /// without needing derived-class-specific knowledge like "is this a
    /// disabled MenuItem."
    bool IsFocusable() const { return focusable; }

    /// Explicitly moves the single app-wide keyboard focus pointer to this
    /// widget, releasing the previously-focused widget's held keys first
    /// (same as a mouse press or Tab landing on it would). No-op if this
    /// widget isn't focusable or is already focused. For widgets that need
    /// to move focus programmatically — e.g. a menu opening and focusing
    /// its first item — rather than waiting for a mouse press or Tab.
    void Focus();

    /// Handles Tab / Shift+Tab focus cycling and Enter/Space activation
    /// (fires onActivate, not onClick — see SetOnActivate) for the tree
    /// rooted at `root`. Must be called once per tree per frame (not once
    /// per widget) — reads raylib's frame-scoped IsKeyPressed state, which
    /// would double-fire if invoked from ProcessEvents()'s per-widget
    /// recursion. Call after root->ProcessEvents() and before root->Draw().
    /// Safe to call once for each of several window trees within the same
    /// real frame (main.cpp does, via WM) — internally gates the
    /// once-per-real-frame part (Enter/Space, raw key events) behind
    /// internal::BeginKeyboardFocusFrame(), which the caller driving
    /// multiple trees must call exactly once per real frame, before the
    /// first of these calls.
    static void ProcessKeyboardFocus(Widget& root);

    /// Appends this widget to `out` if it is focusable. Containers override
    /// to recurse into children first, so the resulting list is in
    /// depth-first tree order — the order Tab/Shift+Tab cycle through.
    /// Called on demand only when Tab is actually pressed, not every frame.
    /// Public (rather than protected, like the rest of this traversal
    /// trio) so a Widget that owns another Widget it didn't itself
    /// construct — e.g. WM::GetWindowClientWidget's wrapper — can still
    /// recurse into it without needing Container's friendship.
    virtual void CollectFocusable(std::vector<Widget*>& out);

    friend class Container;

   protected:
    /// Liveness sentinel: true for as long as this Widget exists, flipped
    /// to false in ~Widget(). A shared_ptr, not a plain bool, so it can be
    /// observed (via a copy of the pointer) from code that keeps running
    /// after `this` might have been destroyed — e.g. Container::
    /// ProcessEvents() checking whether firing a child's callback (which
    /// may call Widget::Remove() on an ancestor, not just itself)
    /// destroyed `this` container, or destroyed a not-yet-visited sibling
    /// — before touching `this` or that sibling again. The bool itself
    /// outlives the Widget as long as at least one shared_ptr to it is
    /// still held (it's a separate heap allocation from the Widget).
    std::shared_ptr<bool> aliveToken = std::make_shared<bool>(true);

    Container* parent = nullptr;
    bool layoutDirty = true;
    Rectangle computedRect{};

    std::optional<float> fixedWidth;
    std::optional<float> fixedHeight;
    float growFactor = 0.0f;
    float shrinkFactor = 1.0f;

    std::function<void()> onClick;
    std::function<void()> onActivate;
    std::function<void(bool)> onHoverChange;
    std::function<void(int)> onKeyPress;
    std::function<void(int)> onKeyDown;
    std::function<void(int)> onKeyUp;
    bool wasHovered = false;
    bool pressOrigin = false;

    /// Keys currently tracked as held for this widget (only ever populated
    /// on the focused widget) — drives onKeyDown/onKeyUp bookkeeping in
    /// ProcessKeyboardFocus(). Not the same as raylib's own key state; this
    /// is just "which keys has this widget seen a press for that it hasn't
    /// yet seen a matching release for."
    std::vector<int> heldKeys;

    /// Whether the pointer is currently down and inside computedRect, as of
    /// the last ProcessEvents() call. Draw() reads this instead of polling
    /// input itself, since Draw() is const.
    bool pointerDown = false;

    /// Whether this widget is focused and Enter/Space is currently held, as
    /// of the last ProcessKeyboardFocus() call. Draw() reads this alongside
    /// pointerDown so a keyboard-activated widget shows the same pressed
    /// visual as a mouse-pressed one.
    bool keyDown = false;

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

    /// Reads current mouse state against `rect`, fires onClick/onActivate/
    /// onHoverChange as needed, and updates pointerDown. Called from
    /// ProcessEvents() by widgets that support input.
    void PollPointerEvents(const Rectangle& rect);

    /// Opts this widget into GlobalLayerStacker() as `layer`'s newest
    /// item — for widgets that can visually float independent of normal
    /// parent-child containment (a window's content root, a future popup
    /// menu), not for ordinary flow children. Once registered, hovering
    /// additionally requires this widget to be the topmost registered item
    /// under the pointer, so overlapping unrelated widget trees can't both
    /// react to the same click. No-op if already registered.
    void RegisterLayer(Layer layer);

    /// This widget's GlobalLayerStacker() token, if RegisterLayer() has
    /// been called.
    std::optional<LayerStacker::ItemId> layerToken;

   private:
    /// Fires onKeyUp for every still-held key and clears heldKeys. Called
    /// whenever focus moves away from this widget, so a consumer never sees
    /// a key reported as pressed with no matching release.
    void ReleaseAllKeys();

    /// Shared by Focus(), PollPointerEvents()'s mouse-press branch, and
    /// ProcessKeyboardFocus()'s Tab-cycle branch: releases the previous
    /// focus holder's held keys and moves the single global focus pointer
    /// to `next`. No-op if `next` is null, isn't focusable, or is already
    /// focused.
    static void MoveFocusTo(Widget* next);
};

/// Standard OS-style press-then-repeat timing: true on the initial press of
/// `key` (the IsKeyPressed edge), then true again every `interval` seconds
/// once `key` has been held for at least `delay` seconds. `heldSeconds` is
/// caller-owned state — the same variable must be passed in every frame for
/// a given key/widget pairing — and self-resets to 0 once the key is no
/// longer down, so releasing and re-pressing restarts the delay. Used for
/// things like Tab-cycling and TextBox editing keys, where a single
/// IsKeyPressed() only fires once per physical press.
bool IsKeyRepeated(
    int key, float& heldSeconds, float delay = 0.4f, float interval = 0.05f
);

}  // namespace UI
