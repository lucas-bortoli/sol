# `WM::` — the window manager

Draws the OS-chrome windows (border, titlebar, close button) that host UI
content, and — since `WM::WindowSetContent()` — owns and drives that
content's `UI::` widget tree too. See [`Documentation/UI.md`](UI.md) for the widget
toolkit itself; this document only covers the window shell around it and
how the two are wired together.

- **Opaque handles, not pointers.** A `WM::WindowHandle` is a plain
  `unsigned int` (`WindowManager.h:12`) — an id you pass around, not
  something you dereference. Internally it keys a
  `std::map<WindowHandle, Window>` guarded by a mutex
  (`WindowManager.cpp:16-28`).
- **A de facto global singleton.** All window state lives in file-scope
  `static`s in `WindowManager.cpp`; `WM::internal::Initialize()`/
  `Cleanup()` are currently no-ops (`WindowManager.cpp:117,200`) — nothing
  about window state is tied to their lifetime.
- **No z-order, but windows are draggable and resizable.** Windows draw
  in ascending-handle (creation) order — there's no click-to-front or
  raise-on-focus. Dragging the titlebar moves a window; dragging the
  invisible border region just outside a window's edges/corners resizes it. Both move
  and resize snap position/size to a grid.

---

## Guide: creating a window with content

```cpp
auto myWindow = WM::WindowCreate();
WM::WindowSetSize(myWindow, {260, 190});
WM::WindowSetPosition(myWindow, {16, 16});
WM::WindowSetTitle(myWindow, "Player Stats");

WM::WindowSetContent(
    myWindow,
    UI::Column(
        {.gap = 6, .padding = 8},
        UI::Text("Player Stats"),
        UI::Btn("Level Up").OnActivate([] { /* ... */ })
    )
);
```

`WindowSetContent` takes ownership of a `UI::Widget` tree — build it
inline with the `Tree.h` literals exactly as you would for a standalone
tree (see the [UI toolkit guide](UI.md)); there's no special wrapper type
to reach for. Grab any `.Ref()`'d pointers you'll need for later mutation
before or as part of that call, same as usual — ownership moving into `wm`
doesn't invalidate them (moving a `unique_ptr<Widget>` doesn't move the
underlying heap object).

Then, once per frame:

```cpp
while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    WM::internal::ProcessEvents();       // input + keyboard focus, every window

    scoreValue->SetText(std::to_string(score));   // mutate via your Ref()s

    WM::internal::Draw();                // chrome, then each window's content

    EndDrawing();
}
```

That's the whole loop — `wm` handles laying out and drawing every
window's content itself; the caller never touches a content `Rectangle`
or calls a widget's `Layout`/`Draw`/`ProcessEvents` directly. Call
`WM::internal::ProcessEvents()` before mutating any widget state (so input
from _this_ frame is reflected before you read/write it) and
`WM::internal::Draw()` after.

Destroy with `WM::WindowDestroy(handle)` when done (typically at shutdown,
alongside `WM::internal::Cleanup()`) — this also drops the window's
`content` tree, freeing every widget in it.

---

## API reference

### Window lifecycle

| Function                            | Purpose                                                                                                                                                                                                                                                                                                    |
| ----------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `WindowCreate()`                    | Creates a window (title `"New Window"`, zero-sized, resizable) and returns its handle.                                                                                                                                                                                                                     |
| `WindowSetTitle(handle, title)`     | Sets the titlebar text.                                                                                                                                                                                                                                                                                    |
| `WindowSetPosition(handle, {x, y})` | Sets the window's outer top-left corner.                                                                                                                                                                                                                                                                   |
| `WindowSetSize(handle, {w, h})`     | Sets the window's outer width/height. A window with zero size in both dimensions is skipped entirely by `Draw()` (`WindowManager.cpp:134-136`) — nothing is drawn until you size it.                                                                                                                       |
| `WindowSetResizable(handle, bool)`  | Whether the border-drag resize interaction responds for this window (default `true`). `false` still allows moving via the titlebar.                                                                                                                                                                        |
| `WindowDestroy(handle)`             | Removes the window (and its content tree, if any) from the registry.                                                                                                                                                                                                                                       |
| `WindowSetContent(handle, content)` | Takes ownership of a `UI::Widget` tree as the window's content. Replaces any previously-set content. See the guide above.                                                                                                                                                                                  |
| `WindowGetClientRect(handle)`       | Returns the window's _content_ rectangle — inside the border/titlebar chrome. `{0,0,0,0}` if never sized. You only need this directly if you're doing something `WM::internal::Draw()` doesn't already handle for you (e.g. custom drawing relative to a window without going through `WindowSetContent`). |

### Per-frame driving (`WM::internal`)

| Function          | Purpose                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `Initialize()`    | No-op today; call once at startup for forward compatibility.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| `ProcessEvents()` | For every window with content set: polls input against it (`Widget::ProcessEvents()`) and runs keyboard-focus cycling (`Widget::ProcessKeyboardFocus`) — once per window, matching the "once per tree per frame" contract each of those has. Call once per frame, before reading/mutating widget state.                                                                                                                                                                                                                                                                              |
| `Draw()`          | For every window (skipping unsized ones): paints the border+shadow, titlebar (hover-highlighted), title text, and close button (not yet wired to `WindowDestroy`) — then, immediately after that window's own chrome, lays out (`Layout(WindowGetClientRect(handle))`) and draws its content, if any. Content is laid out fresh every frame (see below), so it always reflects the window's current position/size. Draw order is ascending handle (creation) order for both chrome and content, so overlapping windows' content stays correctly on top of / behind the right chrome. |
| `Cleanup()`       | No-op today; call once at shutdown for forward compatibility.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |

---

## The content rectangle

`Draw()` paints, per window (`WindowManager.cpp:138-190`): a bordered,
drop-shadowed rectangle at the window's full outer bounds; a titlebar
strip 1px inset from the top/sides, 16px tall; the title text near its
top-left; and a close-button box at its top-right. The **content
rectangle** — where `Layout()` places a window's `content` tree — insets
the window's outer bounds by a fixed amount on each side, computed by an
internal `ComputeClientRect()` helper (`WindowManager.cpp:79-100`) shared
by both `Draw()`'s own content-layout call and the public
`WindowGetClientRect()`. The exact inset values are named constants right
next to that function — check there for the current numbers rather than
relying on this doc, since they're a purely cosmetic tuning knob that may
still move as the chrome design settles.

There is currently **one shared inset for every window** — no per-window
override. If a particular window needs different breathing room than the
rest, that's not supported today; it would mean either parameterizing
`ComputeClientRect()` per window or giving `WindowSetContent`/a sibling
call an explicit inset override.

---

## Design notes / known gaps

- **Thread-unsafe callers, thread-safe storage.** Every `WM::` function
  locks the same mutex around `_window_map`, so concurrent calls from
  different threads won't corrupt window state — but there's no
  synchronization _between_ `ProcessEvents()`/`Draw()` and the `UI::`
  widget tree's own state (callbacks, `Ref()`'d pointers); this codebase
  is single-threaded end to end today, so this hasn't mattered in
  practice.
- **Move/resize via drag.** `ProcessEvents()` hit-tests the titlebar (move)
  and a 6px band just outside each window's edges/corners (resize) against
  the topmost window under the cursor, and mutates `clientRect` directly
  while the mouse button is held — both snapped to a grid, with resize
  additionally clamped to a `kMinWindowSize`. Because `content->Layout()`
  is called fresh every frame from the window's _current_ `clientRect`,
  content tracks position/size changes automatically — no invalidation
  callback needed from `wm`.
- **No z-order / click-to-front.** Overlapping windows always draw in
  creation order; clicking a background window doesn't raise it.
- **The close button doesn't close anything yet** — it's painted but has
  no click handler wired to `WindowDestroy`.

These are gaps in `wm`, not `ui` — the widget toolkit side of the
integration (`WindowSetContent`, the per-frame `ProcessEvents`/`Draw`
contract) doesn't need to change for any of them to be filled in later.
