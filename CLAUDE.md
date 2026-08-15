# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

Wrapped in a `justfile` — run `just --list` for the full set.

```sh
just submodules # first time only — pulls in raylib and doctest
just build
just run
just test # whole suite, via ctest
just test-case "Button: a full click fires onClick and onActivate" # a single test case
just sanitize # whole suite under AddressSanitizer + LeakSanitizer, in a separate build dir
just clean
```

## Architecture

Two subsystems, connected but independently understandable:

- **`ui::`** (`src/lib/ui/`) is a retained-mode widget toolkit: build a
  `Widget` tree once — via `Tree.h`'s declarative `Row()`/`Column()`/
  `Text()`/`Btn()`/... literals, not by constructing widget classes
  directly — then drive it every frame through three passes, in order:
  `Layout()` (measure + arrange), `ProcessEvents()` (poll input, fire
  callbacks), `Draw()` (paint). `Container` is the only layout-bearing
  widget, a CSS-flexbox-like engine (direction/justify/align/gap/padding/
  grow/shrink/`Overflow::Scroll`); every other widget just reports an
  intrinsic size and paints itself. Full guide: `docs/ui.md`.

- **`wm::`** (`src/window_manager.h`/`.cpp`) is the window manager. It
  owns and drives each window's `ui::` content: `WindowSetContent(handle,
  tree)` hands over a widget tree, and `wm::internal::ProcessEvents()`/
  `Draw()` call that tree's `ProcessEvents`/`ProcessKeyboardFocus`/
  `Layout`/`Draw` every frame — callers never touch a window's widget
  tree directly after handing it over. Full guide: `docs/WindowManager.md`.

- All raylib reads of live input state (mouse, keyboard, clipboard,
  clock) go through `ui::CurrentInput()` (`src/lib/ui/Input.h`/`.cpp`)
  rather than calling raylib directly. This is the seam the test suite
  (`tests/`) uses to drive the toolkit with a scripted `FakeInput`
  instead of a real window. Full guide: `docs/testing.md`.

- `assets.h`/`.cpp` and `palette.h` hold global font/texture/color state,
  loaded once at startup (`assets::Initialize()`) and freed at shutdown
  (`assets::Cleanup()`) — not owned by any individual widget.
