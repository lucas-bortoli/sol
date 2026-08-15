# Testing the `ui::` toolkit

An automated test suite (`rocketship_tests`, using [doctest](https://github.com/doctest/doctest),
vendored as a submodule at `external/doctest`) that drives the widget
toolkit programmatically, without a real window — including simulated
clicks, drags, and keyboard focus — plus a way to run the whole suite
under AddressSanitizer + LeakSanitizer.

---

## Running the tests

```sh
cmake --build build --target rocketship_tests
ctest --test-dir build            # or: ./build/rocketship_tests directly
```

The `rocketship` app target is untouched by any of this — see "Why this
doesn't change runtime behavior" below.

### Leak checking

```sh
cmake -B build-sanitize -DROCKETSHIP_SANITIZE=ON
cmake --build build-sanitize --target rocketship_tests
./build-sanitize/rocketship_tests
```

Same test suite, compiled with `-fsanitize=address,leak`. LeakSanitizer
runs automatically at process exit and makes the process exit non-zero if
anything leaked — so this is a real pass/fail signal, not something that
needs manual output inspection. It only catches leaks along paths the
test suite actually exercises, so it's as good as the suite's coverage,
not a substitute for it.

---

## How input simulation works

Every raylib call that reads live input state — mouse position/buttons,
keyboard, wheel, clipboard, the clock — is routed through `ui::InputSource`
(`src/lib/ui/Input.h`/`.cpp`) instead of being called directly. `ui::
CurrentInput()` is what all widget code actually calls; it defaults to a
`RaylibInputSource` that forwards 1:1 to real raylib, so this is a
behavior-preserving seam for the real app — swapping every call site over
to `CurrentInput()` didn't change what `rocketship` does at runtime.
(`CheckCollisionPointRec` is the one exception: it's pure geometry on
already-passed-in values, not a read of hidden global state, so every
call site still calls raylib's version directly.)

Tests install a `FakeInput` (`tests/FakeInput.h`/`.cpp`) via
`ui::SetInput(&fake)` to drive the toolkit without any of that:

```cpp
FakeInput fake;
ui::SetInput(&fake);

fake.MoveMouseTo({10, 10});
fake.PressMouseButton(MOUSE_BUTTON_LEFT);
button->ProcessEvents();       // press alone doesn't fire a click

fake.NextFrame();               // clears this-frame edge state, like a real frame boundary
fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
button->ProcessEvents();        // now onClick/onActivate fire

ui::SetInput(nullptr);          // reset to the default raylib-backed source
```

Always reset with `ui::SetInput(nullptr)` after a test — otherwise a
later test (or the real app, if it ever ran in the same process) would
keep reading a dangling `FakeInput*`. `tests/test_input_driven.cpp` wraps
this in a small `ScopedInput` RAII helper so a failed `CHECK` (which
doctest continues past, rather than unwinding the stack) can't skip the
reset.

### Frame semantics

`FakeInput` mirrors raylib's actual frame boundaries, since the widget
code's correctness depends on them:

- `IsMouseButtonPressed`/`Released` and `IsKeyPressed` are edge-triggered
  — true only until the next `NextFrame()` call, not for as long as the
  button/key is held (that's what `IsMouseButtonDown`/`IsKeyDown` are
  for, driven by `PressMouseButton`/`ReleaseMouseButton`/`SetKeyDown`
  instead).
- `GetKeyPressed()`/`GetCharPressed()` drain a FIFO queue
  (`QueueKeyPress`/`QueueChar`), exactly like raylib's — call once per
  loop iteration until it returns `0`, don't just check it once.
- A real mouse click can't have its press and release land in the same
  frame — simulate one the same way: press, poll, `NextFrame()`,
  release, poll again. `Widget::PollPointerEvents` (`Widget.cpp`) relies
  on this: `IsMouseButtonReleased` firing a click checks `pressOrigin`,
  which was only set true by an *earlier* poll's `IsMouseButtonPressed`.

### A gotcha if you extend `InputSource`

Inside `RaylibInputSource` (`Input.cpp`), every method must call the
global-namespace-qualified raylib function — `::GetMousePosition()`, not
`GetMousePosition()` — because the member function's own name otherwise
shadows raylib's free function of the same name, turning the call into
infinite self-recursion instead of reaching raylib. Caught before it ever
shipped, but easy to reintroduce if a new method is added carelessly.

---

## What's covered today

- `tests/test_container.cpp` — pure layout/mutation, no input needed:
  flex grow/shrink/justify/align math, per-side padding, `Overflow::
  Scroll` content sizing, `AppendChild`/`InsertChild`/`RemoveChild`/
  `ChildAt`/`IndexOf`/`Children`, `Widget::Remove()`/`GetParent()`. Uses
  `ui::Spacer` exclusively for anything laid out through a `Container` —
  Spacer's `IntrinsicWidth()`/`Height()` are the `Widget` base's `0.0f`
  default, so these tests never touch raylib at all (no window, no font
  load needed). `Label`/`Button` are safe to use too if a test needs
  them — `MeasureTextEx` guards against a zero-initialized `Font`
  (`texture.id == 0`) and returns `{0,0}` rather than crashing — but
  their *size* won't reflect real text metrics without a loaded font, so
  give them explicit `.Width()`/`.Height()` if a test's assertions
  depend on their size.
- `tests/test_input_driven.cpp` — via `FakeInput`: button click/drag-off-
  cancel, Tab/Shift+Tab focus cycling, Enter activation, Container mouse-
  wheel scrolling, and a regression test that reproduces the exact "a
  button's click destroys its own parent row while `Container::
  ProcessEvents` is mid-iteration over its children" crash found and
  fixed earlier — verified to actually catch it by temporarily reverting
  the fix and confirming this exact test crashes (`SIGSEGV`) without it.

This is a starter suite, not exhaustive — `TextBox`/`TextArea`'s editing,
selection, and clipboard behavior aren't covered yet. The infrastructure
(`InputSource`, `FakeInput`, the CMake target) is the reusable part; add
`TEST_CASE`s to the existing files or new ones as coverage grows.

## A cross-test hazard worth knowing about

`Widget::ProcessKeyboardFocus` gates its once-per-frame logic (Tab-cycle,
Enter activation, key dispatch) behind a function-local `static`
comparison against the previous `GetTime()` value — state that persists
for the whole test binary's run, not just within one `TEST_CASE`. If two
different tests' `FakeInput`s both started their clock at `0.0`, a later
test's first `ProcessKeyboardFocus` call could look like "the same frame"
as an earlier test's last call and get spuriously skipped.
`FakeInput`'s constructor sidesteps this by giving every instance a
distinct, ever-increasing starting clock value (`NextStartingClock()` in
`FakeInput.h`) — no production code change needed, but worth knowing if
`ProcessKeyboardFocus` tests start behaving strangely in an unrelated way.
