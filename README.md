# Sol

Sol is a sunny little fantasy operating system, built from scratch in C++20 on
top of [raylib](https://www.raylib.com/).

## Features

- **A flexbox-style UI toolkit** — containers with justify/align/gap/
  per-side padding, grow/shrink sizing, overlay scrollbars with no
  reserved gutter, and a small set of widgets (labels, buttons, text
  boxes, text areas, spacers), all built through a declarative
  tree-literal DSL rather than constructed directly — see the
  [UI toolkit guide](docs/ui.md) for the full walkthrough.
- **A DOM-like mutation API** for adding, inserting, removing, and
  querying children at runtime, safe to call even from a widget's own
  callback (e.g. a row's own "remove" button tearing itself down).
- **A window manager** that owns and drives each window's UI content —
  border/shadow chrome, a titlebar, per-window widget trees laid out and
  drawn automatically every frame.
- **An input abstraction** — every read of live mouse/keyboard/clipboard/
  clock state goes through a seam that can be swapped for a scriptable
  fake, so the whole toolkit can be driven programmatically without a
  real window.
- **An automated test suite** (doctest) exercising both pure layout math
  and simulated click/drag/keyboard interaction, plus a sanitizer build
  option to run it under AddressSanitizer + LeakSanitizer.
- **A hand-rolled BDF font parser** — bitmap fonts, loaded and rendered
  with no external font library.

## Building

Commands are wrapped in a [`justfile`](justfile) — run `just --list` to
see all of them.

```sh
just submodules   # first time only — pulls in raylib and doctest
just build
```

### Running

```sh
just run
```

### Testing

```sh
just test # whole suite, via ctest
just test-case "name" # a single test case, via doctest's own filtering
just sanitize # whole suite under ASan + LeakSanitizer
```

## Documentation

- [UI toolkit guide](docs/ui.md) — building UIs, the layout system,
  dynamic mutation, writing a new widget.
- [Window manager guide](docs/WindowManager.md) — window chrome and its
  integration with the UI toolkit.
- [Testing guide](docs/testing.md) — running the test suite, how input
  simulation works, and how to add coverage.
