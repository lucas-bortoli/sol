# `UI::LayerStacker` — z-order and click-ownership

A small, `UI::`-level registry that answers two questions for anything that
can visually float independent of normal parent-child containment — today
that's each `wm` window, eventually a popup menu or toast: **"who's on
top here?"** and **"in what order does everything paint?"**. One global
instance (`UI::GlobalLayerStacker()`) backs both. See
[`Documentation/WindowManager.md`](WindowManager.md) for how `wm` is
actually wired up to it; this document covers the registry itself.

- **Why it exists.** Before this, `wm` drew windows in `std::map` order
  and hit-tested drag/resize by walking that same order in reverse —
  implicit, never-updated, and disconnected from the widget layer
  entirely. `UI::Widget::PollPointerEvents` (`Widget.cpp`) polls the
  *global* mouse position against its own `computedRect` with no
  awareness of any other widget or window, so two overlapping windows'
  content trees would each independently see a click in their shared
  region as "hovered" and both fire. `LayerStacker` is the single source
  of truth both problems now go through.
- **Bucketed by `Layer`, not one flat stack.** Items only ever reorder
  *within* their own `Layer` (`Background`, `Windows`, `Shell`, `Menu`,
  `Notification`, declared in that back-to-front order) — bringing an
  item to front can never let it paint over, or steal a click from, a
  later-listed layer. A raised window can never end up above the taskbar,
  for instance, no matter how many times it's clicked.
- **Draw-by-delegate, not draw-by-callback.** Registration takes a
  `LayerStacker::Drawable&` (one pure-virtual `Draw() const`), not a
  `std::function`. `Widget` already has a matching `virtual void Draw()
  const`, so `Widget` derives from `Drawable` and gets registration for
  free; `wm`'s `Window` struct (chrome + content, not itself a `Widget`)
  implements it directly. No per-registration heap allocation, no
  dangling-capture risk.

---

## API reference

All declarations live in `Src/Lib/UI/LayerStacker.h`; see the doc-comment
on each for the authoritative contract.

| Member | Purpose |
|---|---|
| `Register(Layer, Drawable&)` | Adds an item at the front (top) of `layer`'s own stack, returns its `ItemId` ("layerToken"). Non-owning — the registrant must `Unregister()` before it's destroyed. |
| `Unregister(id)` | Removes an item. No-op if already unregistered — safe to call unconditionally from a destructor. |
| `SetBounds(id, rect)` | Refreshes the screen-space rect an item occupies. Call once per frame, before any `TopmostAt`/`IsTopmostAt` call, for every item still on screen. |
| `BringToFront(id)` / `SendToBack(id)` | Moves an item to the front/back of its own layer's stack. |
| `BringForward(id)` / `SendBackward(id)` | Swaps an item with its immediate neighbor above/below in its own layer's stack. |
| `TopmostAt(point)` | The frontmost registered item whose last-set bounds contain `point`, scanning layers back-to-front and, within a layer, front-to-back. `std::nullopt` if nothing matches. |
| `IsTopmostAt(id, point)` | `TopmostAt(point) == id` — what an item asks to find out whether a click is actually directed at it, not just within its hitbox. |
| `DrawAll()` | Calls every registered item's `Draw()` once, back-to-front — the single place paint order is decided app-wide. |
| `ItemsFrontToBack(layer)` | A layer's own items, topmost first — for callers doing custom hit-testing beyond a simple bounds check (e.g. `wm`'s resize-border band, which extends outside a window's registered bounds) that still needs current z-order. |

`GlobalLayerStacker()` returns the app-wide instance. It's a plain
accessor, not a `FakeInput`-style swappable seam like `CurrentInput()` —
`LayerStacker` holds no OS/raylib state, so tests just construct their own
local instance instead of swapping this one (see `Tests/TestLayerStacker.cpp`).

---

## Widget integration

`Widget` (`Widget.h`/`.cpp`) carries an `std::optional<LayerStacker::ItemId>
layerToken`, unset by default, plus a protected `RegisterLayer(Layer)`
that opts a widget in:

- `~Widget()` unregisters if a token was ever assigned.
- `Layout(bounds)` keeps a registered widget's bounds current via
  `SetBounds`.
- `PollPointerEvents(rect)`: a registered widget additionally requires
  `GlobalLayerStacker().IsTopmostAt(*layerToken, mouse)` before treating
  itself as hovered — everything downstream (press-origin tracking,
  click/activate firing, `pointerDown`) already derives from `hovered`, so
  an occluded widget cleanly sees "mouse is nowhere," the same way
  dragging a press off a widget already works.

**Nothing calls `RegisterLayer` today.** `wm` registers one `LayerStacker`
item per `Window` (the struct, not a `Widget`) — see
[`Documentation/WindowManager.md`](WindowManager.md) — so ordinary widgets
inside a window's content tree stay off this path entirely, hit-testing
locally against their own `computedRect` exactly as before `LayerStacker`
existed. The occlusion between window content trees is instead handled by
a separate, coarser mechanism: `UI::internal::SetPointerEventsSuppressed(bool)`
(`Widget.h`/`.cpp`), which `wm` sets around an occluded window's whole
`ProcessEvents()` call so nothing in that subtree sees the pointer for the
frame, without every widget in it needing its own registration. `Widget`'s
`layerToken`/`RegisterLayer` machinery exists for the day something *does*
need per-widget registration — a popup menu or toast spawned from inside a
window's tree, arbitrated against every window and every other such popup
through the same registry — not because anything uses it yet.

---

## Design notes / known gaps

- **`ItemId` is registry-assigned, not caller-supplied.** `Register()`
  hands back its own incrementing id; a caller that needs to map an
  `ItemId` back to its own object (e.g. `wm`'s `FindByLayerToken` in
  `WindowManager.cpp`) currently does a linear scan. Fine at
  window-count scale; would be worth adding a
  `Register(Layer, ItemId, Drawable&)` overload (letting `wm` pass its
  own `WindowHandle` directly) if that scan ever needed to go away.
- **No cross-layer reordering, ever.** There's deliberately no API to move
  an item from one `Layer` to another — an item's layer is fixed at
  `Register()` time. If something needs to change bands at runtime,
  `Unregister()`/`Register()` again.
- **Not thread-safe by itself.** `LayerStacker` has no internal locking;
  `wm` already serializes all window state behind `_window_mutex`, and
  this codebase is single-threaded end to end today, so it hasn't
  mattered in practice — same caveat as the rest of `wm`'s state (see
  WindowManager.md's design notes).
