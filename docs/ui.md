# The `ui::` widget toolkit

A small, retained-mode UI toolkit built on top of raylib, used to draw the
in-game window contents (see `src/window_manager.h` for the windows
themselves — this document only covers what's drawn *inside* one).

- **Retained-mode**: you build a tree of `Widget` objects once (usually via
  the `Tree.h` declarative literals) and keep it around across frames,
  mutating it imperatively (`SetText`, `SetGrow`, etc.) rather than
  rebuilding it every frame.
- **Three passes per frame, in order**: `Layout()` (measure + arrange),
  `ProcessEvents()` (poll input, fire callbacks), `Draw()` (paint). See
  `src/lib/ui/Widget.h:13-15`.
- **C++20**, one `.h`/`.cpp` pair per widget, everything in `namespace ui`.
  Include `lib/ui/UI.h` to pull in the whole toolkit.

---

## Guide: building a small window

This walks through the "Player Stats" window from `src/main.cpp`, piece by
piece.

### 1. Describe the tree

Widgets are built with the tree-literal functions in `Tree.h` — `Row()`,
`Column()`, `Text()`, `Btn()`, `Input()`, `Textarea()`, `Space()` — not by
calling widget constructors directly:

```cpp
ui::Label* scoreValue = nullptr;

std::unique_ptr<ui::Widget> ui_root = ui::Column(
    {.gap = 6, .padding = 8},
    ui::Text("Player Stats"),
    ui::Row(
        {.justify = ui::Justify::SpaceBetween},
        ui::Text("Score"),
        ui::Text("0").Ref(scoreValue)
    ),
    ui::Btn("Level Up").OnActivate([] { /* ... */ })
);
```

`Column`/`Row` take a props struct (`ContainerProps`) as their first
argument, then any number of children. Each child call
(`Text(...)`, `Btn(...)`, a nested `Row(...)`) returns a `Node<T>` that can
be chained (`.Ref()`, `.Width()`, `.Grow()`, `.OnActivate()`, ...) before
being consumed by the parent `Row`/`Column` call — see `Node<T>` in
`Tree.h:45-125`.

`.Ref(scoreValue)` stashes the constructed widget's address into an
out-parameter so you can mutate it later (React `ref={}` style) — the tree
owns the widget; the pointer is just a handle into it.

### 2. Drive it every frame

The tree doesn't do anything on its own — your app loop calls the three
passes explicitly, in order, once per frame:

```cpp
while (!WindowShouldClose()) {
    BeginDrawing();

    ui_root->ProcessEvents();
    ui::Widget::ProcessKeyboardFocus(*ui_root);  // once per tree, see below

    scoreValue->SetText(std::to_string(score));  // mutate via your Ref()s

    ui_root->Layout(windowContentRect);           // where does this tree live?
    ui_root->Draw();

    EndDrawing();
}
```

`Layout()` takes the `Rectangle` the tree should occupy this frame (e.g. a
window's content area) and is cheap to call every frame — it early-returns
without recomputing if nothing changed (same bounds, nothing invalidated).

### 3. React to changes

Widget mutations (`SetText`, `SetWidth`, `SetGrow`, adding a child, ...) call
`Invalidate()` internally, which marks the widget *and its ancestors* dirty
so the next `Layout()` call actually recomputes. You don't need to call it
yourself unless you're writing a new widget (see "Writing a new widget"
below).

### 4. Mutate the tree at runtime

Trees aren't static once built — a `Container` supports inserting,
removing, and querying children after construction, and any `Widget` can
detach itself from wherever it lives:

```cpp
ui::Button* dismissButton = nullptr;
parentColumn->AppendChild(
    ui::Btn("Dismiss").OnActivate([&dismissButton] {
        dismissButton->Remove();  // detaches and destroys itself
    }).Ref(dismissButton)
);
```

See "Dynamic children" under the `Container` reference below for the full
`InsertChild`/`RemoveChild`/query surface, and the note on `Widget::Remove()`
for why it's safe to call even from the widget's own callback.

---

## Core concepts

### Sizing: fixed vs. intrinsic vs. grow/shrink

Every `Widget` carries (`Widget.h:124-127`):

| Field | Set via | Meaning |
|---|---|---|
| `fixedWidth`/`fixedHeight` | `SetWidth()`/`SetHeight()` | Pin this axis; overrides intrinsic sizing. Equivalent to CSS `width`/`height` (not `flex-basis`). |
| `growFactor` | `SetGrow()` | Share of a parent `Container`'s leftover main-axis space this widget claims, relative to siblings. Default `0` (don't grow). Equivalent to CSS `flex-grow`. |
| `shrinkFactor` | `SetShrink()` | How much this widget shrinks, relative to siblings, when the parent is too small. Default `1`. Equivalent to CSS `flex-shrink`. |

When no fixed size is set, a widget falls back to `IntrinsicWidth()`/
`IntrinsicHeight()` — its "natural" size (e.g. a `Label`'s measured text
extent). Every widget subclass overrides these for its own content.

### The Layout/ProcessEvents/Draw split

- **`Layout(bounds)`**: recomputes `computedRect` (and, for containers,
  every child's rect). Skips work when `!layoutDirty && bounds unchanged` —
  see the guard at the top of `Container::Layout` (`Container.cpp:79-80`).
- **`ProcessEvents()`**: polls raylib input against the *last* computed
  rect, fires `onClick`/`onActivate`/`onHoverChange`/key callbacks, and
  caches results (`pointerDown`, `focused`, ...) for `Draw()` to read.
  Containers must recurse into children — see `Widget::PollPointerEvents`
  (`Widget.cpp:111-138`).
- **`Draw() const`**: paints using the last computed rect. Runs
  unconditionally every frame — only the layout pass is skippable. `const`
  because it must not mutate model state; widgets that cache derived
  per-frame state (scroll offsets, wrap geometry) mark those fields
  `mutable` with a comment explaining why (e.g. `TextArea.h:223-226`).

### Focus and keyboard input

Only widgets that opt in (`focusable = true` in their constructor — `Button`,
`TextBox`, and `TextArea` all do this) can hold the single, app-wide focus
pointer. Call
`ui::Widget::ProcessKeyboardFocus(*root)` **once per tree per frame**,
after `root->ProcessEvents()` — it handles Tab/Shift+Tab cycling and
Enter/Space activation. If your app draws multiple independent trees (one
per window, as `main.cpp` does), call it once for each tree; it's safe to
call more than once in the same real frame because it internally gates
frame-scoped work (raw key events, repeat timing) behind a `GetTime()`
check (`Widget.cpp:144-159`).

---

## Layout system: `Container` (flexbox)

`Container` (`src/lib/ui/Container.h`/`.cpp`) is the only layout-bearing
widget — everything else just reports an intrinsic size and paints itself.
It lays children out along a **main axis** and distributes/aligns them,
closely mirroring CSS flexbox. Built via `Row()`/`Column()`, never
constructed directly.

### `ContainerProps`

```cpp
struct ContainerProps {
    Justify justify = Justify::Start;
    Align align = Align::Stretch;
    float gap = 0.0f;
    float padding = 0.0f;          // uniform shorthand
    std::optional<float> paddingTop;
    std::optional<float> paddingRight;
    std::optional<float> paddingBottom;
    std::optional<float> paddingLeft;
    bool reverse = false;
    Overflow overflow = Overflow::Visible;
};
```

| Field | Mirrors | Notes |
|---|---|---|
| `justify` | `justify-content` | `Start`, `End`, `Center`, `SpaceBetween`, `SpaceAround`, `SpaceEvenly`. Distributes leftover main-axis space. |
| `align` | `align-items` | `Start`, `End`, `Center`, `Stretch` (default — fills the cross axis unless the child has a fixed cross-axis size). |
| `gap` | `gap` | Space between siblings, main axis. |
| `padding` | `padding: Npx` shorthand | Applied to any side not overridden by the four `padding*` fields below. |
| `paddingTop`/`Right`/`Bottom`/`Left` | `padding-*` | Per-side overrides; each falls back to `padding` when unset. |
| `reverse` | `-reverse` direction variant | `Row` → `RowReverse`, `Column` → `ColumnReverse`. Reverses child placement order without changing DOM/tree order. |
| `overflow` | `overflow` | See below. |

Example mixing shorthand and overrides:

```cpp
ui::Column({.padding = 8, .paddingTop = 30, .paddingLeft = 0}, ...);
```

### The flex algorithm (`Container::Layout`, `Container.cpp:79-238`)

1. **Base size** per child: fixed size if set, else intrinsic size on the
   main axis.
2. **Grow or shrink** to fill/fit `delta = mainSize - totalBase - totalGap`:
   extra space distributes by `growFactor`; a shortfall shrinks by
   `shrinkFactor * baseSize`, clamped to `0` — *unless* `overflow ==
   Overflow::Scroll`, in which case a shortfall is left alone instead (see
   below).
3. Whatever the grow/shrink pass didn't consume is leftover space,
   distributed per `justify`.
4. Children are placed by walking the (possibly reversed) child list and
   advancing a cursor by `childMain + justifyGap`.

Cross-axis sizing/positioning is independent: `align == Stretch` fills the
cross axis (unless the child has a fixed cross size); otherwise the child
takes its fixed-or-intrinsic cross size and is start/end/center-aligned.

### Overflow and scrolling

`Overflow::Visible` (default) is the behavior described above — content
that doesn't fit gets shrunk down to fit, clamped at `0`.

`Overflow::Scroll` instead lets main-axis content overflow the box and
become scrollable:

```cpp
ui::Column(
    {.gap = 4, .overflow = ui::Overflow::Scroll},
    ui::Text("Item 1"), ui::Text("Item 2"), /* ... */
).Height(120)
```

- The shrink step is skipped; children keep their natural size and the
  container tracks `contentMainSize` (total natural extent) vs.
  `viewportMainSize` (the box's own size).
- `Draw()` clips children to the container's bounds via raylib's
  `BeginScissorMode`/`EndScissorMode` (same pattern as `TextBox`/
  `TextArea`'s own clipping).
- A thin **overlay scrollbar thumb** — semi-transparent, drawn on top of
  content, reserving no layout space — appears on the trailing edge
  (right for `Column`, bottom for `Row`) whenever content actually
  overflows.
- Scroll via mouse wheel (while hovered) or by dragging the thumb.
  `ProcessEvents()` handles both and calls `Invalidate()` on change so the
  next `Layout()` re-arranges children at the new scroll offset
  (`Container.cpp:289-337`).
- Scroll is **main-axis only** — a `Row`'s overflow scrolls horizontally,
  a `Column`'s vertically. There's no 2D scroll.
- Nested `Overflow::Scroll` containers aren't specially handled: since
  `ProcessEvents()` polls the outer widget before recursing into children,
  an outer scrollable currently claims wheel events over an inner one.

### Dynamic children

Beyond the tree-literal children passed at construction, `Container`
supports runtime mutation, DOM-`Node`-style:

| Method | DOM equivalent | Notes |
|---|---|---|
| `AppendChild(child)` | `appendChild` | Adds at the end. |
| `InsertChild(index, child)` | `insertBefore` (roughly) | Inserts at `index`, clamped to the current child count (so an out-of-range index behaves like `AppendChild`). |
| `RemoveChild(child)` | `removeChild` | Detaches `child` (a raw `Widget*`) and returns ownership as a `std::unique_ptr<Widget>`, or `nullptr` if it isn't actually a child of this container. Doesn't throw on a not-found child — safe to call speculatively. |
| `ChildCount()` | `childElementCount` | |
| `ChildAt(index)` | `children[index]` | Throws `std::out_of_range` if `index` is out of bounds (`std::vector::at` convention). |
| `IndexOf(child)` | — | Returns `std::optional<size_t>`, `nullopt` if `child` isn't a direct child. |
| `Children()` | `children` | Returns a fresh `std::vector<Widget*>` snapshot each call — fine on demand, not meant to be polled every frame. |

Every `Widget` (not just `Container`) also has:

- **`GetParent()`** → `Container*` (or `nullptr` for a tree root) — DOM
  `Node.parentNode`. Typed as `Container*` rather than the more generic
  `Widget*` because `Container` is the only widget that ever has children.
- **`Remove()`** → `std::unique_ptr<Widget>` — DOM `Node.remove()`, adapted
  for C++'s explicit ownership: it detaches the widget from its parent
  and hands you back the same object as a `unique_ptr`. Keep it to
  reattach elsewhere (`someOtherContainer->AppendChild(widget->Remove())`),
  or discard the return value to destroy it on the spot
  (`widget->Remove();`). **Safe to call from the widget's own
  `onClick`/`onActivate`/`onHoverChange`/`onKeyUp` callback** — a
  self-removing button is expected usage, and `PollPointerEvents`/
  `ReleaseAllKeys` (`Widget.cpp`) are deliberately written to copy out
  whatever they need before firing any callback, so nothing touches the
  widget again if that callback destroys it.

---

## Widget reference

| Widget | Factory | Purpose |
|---|---|---|
| `Container` | `Row(props, ...)` / `Column(props, ...)` | Flexbox layout — see above. Never constructed directly. Supports runtime child mutation (`AppendChild`/`InsertChild`/`RemoveChild`/queries) — see "Dynamic children" above. |
| `Label` | `Text(str)` | Static single-line text, sized to content. `SetText()` to change it. |
| `Button` | `Btn(str)` | Clickable rect with text + border/shadow chrome. Focusable; shows a focus ring. Use `OnActivate()` for its primary action (fires on click *and* keyboard Enter/Space). |
| `TextBox` | `Input(initial = "")` | Single-line UTF-8 text input: click/Tab focus, click-to-place-caret, click-drag/double/triple-click select, Ctrl+A/C/X/V, horizontal auto-scroll. No wrapping. |
| `TextArea` | `Textarea(initial = "")` | Multi-line UTF-8 text input — everything `TextBox` does, plus Up/Down with sticky-column, word/character/no-wrap modes (`SetWrapMode`), configurable visible rows (`SetVisibleRows`), and `SetOnSubmit()` for Shift+Enter. |
| `Spacer` | `Space()` | Renders nothing; reserves layout space via `SetWidth`/`SetHeight`/`SetGrow`. E.g. `Space().Grow(1)` pushes later siblings to the far end of a `Row`/`Column`. |

All of the above derive from `Widget` (`Widget.h`) and inherit its common
surface: `SetWidth`/`SetHeight`/`SetGrow`/`SetShrink`,
`SetOnClick`/`SetOnActivate`/`SetOnHoverChange`/`SetOnKeyPress`/
`SetOnKeyDown`/`SetOnKeyUp`, `IsFocused()`, `GetComputedRect()`,
`GetParent()`, `Invalidate()`, `Remove()` (see "Dynamic children" above).

### Tree-literal DSL (`Tree.h`)

- `Node<T>` wraps a freshly-constructed widget while it's configured
  inline; every method returns `*this` for chaining and it implicitly
  converts to `std::unique_ptr<Widget>` when handed to a `Row`/`Column`
  call. Each `Node` is meant to be written and consumed exactly once.
- Chain methods available on every `Node<T>`: `.Ref(outPtr)`, `.Width(px)`,
  `.Height(px)`, `.Grow(factor)`, `.Shrink(factor)`, `.OnClick(fn)`,
  `.OnActivate(fn)`, `.OnHoverChange(fn)`, `.OnKeyPress(fn)`,
  `.OnKeyDown(fn)`, `.OnKeyUp(fn)`.
- `TextArea`-specific chain methods: `.WrapMode(mode)`, `.VisibleRows(n)`,
  `.OnSubmit(fn)`.
- Factories: `Text(str)`, `Btn(str)`, `Input(initial)`, `Textarea(initial)`,
  `Space()`, `Row(props, children...)`, `Column(props, children...)`.

  `Space()` is deliberately not named `Spacer()` — a free function sharing
  a class's name hides the class from unqualified lookup afterward (the
  classic `struct stat`/`stat()` pitfall), which would make `ui::Spacer*`
  stop compiling anywhere below the factory's declaration.

---

## Writing a new widget

Follow the existing widgets as templates (`Label` is the simplest). The
pattern:

1. `.h`/`.cpp` pair in `src/lib/ui/`, add both to `CMakeLists.txt`'s
   `add_executable(rocketship ...)` source list, and include the header
   from `UI.h`.
2. Derive from `Widget`; override `Draw() const` (pure virtual — every
   widget must paint itself). Override `Layout()`/`ProcessEvents()` only
   if you need custom behavior beyond the `Widget` base (most leaf widgets
   only need the base `Layout()`, which just records `computedRect`).
3. Override `IntrinsicWidth()`/`IntrinsicHeight()` if the widget has a
   natural size (e.g. from measured text).
4. Call `Invalidate()` from any setter that can change size — see
   `Label::SetText` for the minimal example.
5. If the widget needs draw-time-only derived state (scroll offsets,
   cached wrap geometry, ...), mark those fields `mutable` with a comment
   explaining why, matching `TextArea.h:223-226` — this keeps `Draw()`
   `const` while still allowing frame-to-frame caching.
6. Add a tree-literal factory function to `Tree.h` (`inline Node<YourType>
   YourFactory(...) { return MakeNode<YourType>(...); }`) so it fits the
   declarative call shape everything else uses. Pick a factory name
   distinct from the class name (see the `Spacer`/`Space()` note above).
7. If it needs mouse/keyboard interaction, reuse
   `Widget::PollPointerEvents(rect)` from `ProcessEvents()` rather than
   reimplementing hover/press/click detection.

There is currently no test suite in this repo (no `*test*` targets in
`CMakeLists.txt`) — verify new widgets by wiring them into `src/main.cpp`
and running the app.
