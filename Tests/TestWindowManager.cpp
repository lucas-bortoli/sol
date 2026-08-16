// Drag-to-move / drag-to-resize tests driven through FakeInput, exercising
// WM::internal::ProcessEvents()'s hit-testing and grid-snap logic. WM state
// is a process-lifetime singleton (Src/WindowManager.cpp), so every test
// destroys the windows it creates when done.

#include <doctest.h>

#include "FakeInput.h"
#include "../Src/Lib/UI/UI.h"
#include "../Src/WindowManager.h"

namespace {
struct ScopedInput {
    explicit ScopedInput(FakeInput& fake) { UI::SetInput(&fake); }
    ~ScopedInput() { UI::SetInput(nullptr); }
};

struct ScopedWindow {
    WM::WindowHandle handle = WM::WindowCreate();
    ~ScopedWindow() { WM::WindowDestroy(handle); }
};
}  // namespace

TEST_CASE("WM: dragging the titlebar moves the window, snapped to 4px") {
    FakeInput fake;
    ScopedInput scoped(fake);
    ScopedWindow window;

    WM::WindowSetPosition(window.handle, {100, 100});
    WM::WindowSetSize(window.handle, {200, 150});

    fake.MoveMouseTo({150, 105});  // inside the titlebar
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({161, 110});  // +11, +5 -> snaps to +12, +4
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    Rectangle client = WM::WindowGetClientRect(window.handle);
    CHECK(client.x == 100 + 12 + 1);  // +1 border inset
    CHECK(client.y == 100 + 4 + 19);  // +19 titlebar+border inset
}

TEST_CASE("WM: dragging a corner resizes the window, clamped and snapped") {
    FakeInput fake;
    ScopedInput scoped(fake);
    ScopedWindow window;

    WM::WindowSetPosition(window.handle, {100, 100});
    WM::WindowSetSize(window.handle, {200, 150});

    // Just outside the bottom-right corner, in the invisible resize band.
    fake.MoveMouseTo({303, 253});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({312, 256});  // +9, +3 -> snaps to +12, +6
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    Rectangle client = WM::WindowGetClientRect(window.handle);
    CHECK(client.width == 200 + 12 - 2);   // minus the 1px+1px border inset
    CHECK(client.height == 150 + 6 - 20);  // minus the 19px+1px inset

    // Attempt to shrink the window far below the minimum size, from its new
    // (post-resize) outer bounds of {100, 100, 208, 152}.
    fake.NextFrame();
    fake.MoveMouseTo({97, 97});  // top-left corner band, dragging inward
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({400, 400});  // huge drag toward the opposite corner
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    // Clamped to the 40px minimum outer size in both dimensions.
    Rectangle shrunk = WM::WindowGetClientRect(window.handle);
    CHECK(shrunk.width == 40 - 2);
    CHECK(shrunk.height == 40 - 20);
}

TEST_CASE("WM: the resize band sits outside clientRect, never over content") {
    FakeInput fake;
    ScopedInput scoped(fake);
    ScopedWindow window;

    WM::WindowSetPosition(window.handle, {100, 100});
    WM::WindowSetSize(window.handle, {200, 150});

    // Well inside the client area: must not start a resize/move drag.
    fake.MoveMouseTo({200, 175});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({250, 225});
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    Rectangle client = WM::WindowGetClientRect(window.handle);
    CHECK(client.x == 101);
    CHECK(client.y == 119);
    CHECK(client.width == 198);
    CHECK(client.height == 130);
}

TEST_CASE("WM: a non-resizable window ignores border drags but still moves") {
    FakeInput fake;
    ScopedInput scoped(fake);
    ScopedWindow window;

    WM::WindowSetPosition(window.handle, {100, 100});
    WM::WindowSetSize(window.handle, {200, 150});
    WM::WindowSetResizable(window.handle, false);

    fake.MoveMouseTo({303, 253});  // bottom-right resize band
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({320, 270});
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    Rectangle client = WM::WindowGetClientRect(window.handle);
    CHECK(client.width == 198);   // unchanged: 200 - 2px inset
    CHECK(client.height == 130);  // unchanged: 150 - 20px inset

    // But moving via the titlebar still works.
    fake.NextFrame();
    fake.MoveMouseTo({150, 105});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({154, 109});
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    Rectangle moved = WM::WindowGetClientRect(window.handle);
    CHECK(moved.x == 105);
    CHECK(moved.y == 123);
}

TEST_CASE("WM: releasing the mouse ends the drag") {
    FakeInput fake;
    ScopedInput scoped(fake);
    ScopedWindow window;

    WM::WindowSetPosition(window.handle, {100, 100});
    WM::WindowSetSize(window.handle, {200, 150});

    fake.MoveMouseTo({150, 105});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({160, 115});
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    Rectangle afterRelease = WM::WindowGetClientRect(window.handle);

    // Further mouse movement with the button up must have no effect.
    fake.NextFrame();
    fake.MoveMouseTo({400, 400});
    WM::internal::ProcessEvents();

    Rectangle afterExtraMove = WM::WindowGetClientRect(window.handle);
    CHECK(afterRelease.x == afterExtraMove.x);
    CHECK(afterRelease.y == afterExtraMove.y);
    CHECK(afterRelease.width == afterExtraMove.width);
    CHECK(afterRelease.height == afterExtraMove.height);
}

TEST_CASE("WM: a click in the overlap between two windows only fires the "
          "topmost window's button") {
    FakeInput fake;
    ScopedInput scoped(fake);
    ScopedWindow windowA;
    ScopedWindow windowB;

    WM::WindowSetPosition(windowA.handle, {0, 0});
    WM::WindowSetSize(windowA.handle, {200, 200});
    WM::WindowSetPosition(windowB.handle, {100, 100});
    WM::WindowSetSize(windowB.handle, {200, 200});

    int clicksA = 0;
    int clicksB = 0;
    UI::Button* buttonA = nullptr;
    UI::Button* buttonB = nullptr;
    WM::WindowSetContent(
        windowA.handle,
        UI::Btn("A").Ref(buttonA).OnActivate([&clicksA] { clicksA++; })
    );
    WM::WindowSetContent(
        windowB.handle,
        UI::Btn("B").Ref(buttonB).OnActivate([&clicksB] { clicksB++; })
    );

    // ProcessEvents() hit-tests against the *previous* frame's Layout() —
    // normally run from Draw(), which also issues real raylib draw calls
    // this headless test can't make. Lay out both windows' content
    // directly instead (Widget::Layout() itself touches no raylib state).
    buttonA->Layout(WM::WindowGetClientRect(windowA.handle));
    buttonB->Layout(WM::WindowGetClientRect(windowB.handle));

    // windowB was created (and therefore registered) after windowA, so it
    // starts frontmost in the overlapping region — {150, 150} is inside
    // both windows' client rects.
    fake.MoveMouseTo({150, 150});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    CHECK(clicksA == 0);
    CHECK(clicksB == 1);

    // A click inside windowA's own titlebar brings it to front...
    fake.NextFrame();
    fake.MoveMouseTo({10, 5});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    // ...so a second click in the overlap now reaches windowA instead.
    fake.NextFrame();
    fake.MoveMouseTo({150, 150});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    CHECK(clicksA == 1);
    CHECK(clicksB == 1);
}

TEST_CASE("WM: a window's resize border can't be grabbed through another "
          "window drawn on top of it") {
    FakeInput fake;
    ScopedInput scoped(fake);
    ScopedWindow windowA;
    ScopedWindow windowB;

    WM::WindowSetPosition(windowA.handle, {100, 100});
    WM::WindowSetSize(windowA.handle, {200, 150});

    // windowB is registered after windowA, so it starts frontmost, and
    // sits directly on top of windowA's bottom-right resize band (just
    // outside windowA's own clientRect, at {292..308, 242..258}).
    WM::WindowSetPosition(windowB.handle, {250, 200});
    WM::WindowSetSize(windowB.handle, {100, 100});

    Rectangle before = WM::WindowGetClientRect(windowA.handle);

    // Same point the "dragging a corner resizes" test above uses to grab
    // windowA's resize band — but now it's covered by windowB.
    fake.MoveMouseTo({303, 253});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({312, 256});
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    Rectangle after = WM::WindowGetClientRect(windowA.handle);
    CHECK(after.x == before.x);
    CHECK(after.y == before.y);
    CHECK(after.width == before.width);
    CHECK(after.height == before.height);
}

TEST_CASE("WM: the topmost window's own resize border still works even "
          "where it overlaps a window behind it") {
    FakeInput fake;
    ScopedInput scoped(fake);
    // windowB first, so windowA (created second) starts frontmost —
    // mirrors the previous test's geometry with the stacking flipped:
    // now it's windowA on top, and windowA's own bottom-right resize band
    // is the thing overlapping windowB's clientRect.
    ScopedWindow windowB;
    ScopedWindow windowA;

    WM::WindowSetPosition(windowA.handle, {100, 100});
    WM::WindowSetSize(windowA.handle, {200, 150});
    WM::WindowSetPosition(windowB.handle, {250, 200});
    WM::WindowSetSize(windowB.handle, {100, 100});

    Rectangle beforeA = WM::WindowGetClientRect(windowA.handle);
    Rectangle beforeB = WM::WindowGetClientRect(windowB.handle);

    // windowA's own corner band, same point as the two tests above.
    fake.MoveMouseTo({303, 253});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({312, 256});
    WM::internal::ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    Rectangle afterA = WM::WindowGetClientRect(windowA.handle);
    CHECK(afterA.width == doctest::Approx(beforeA.width + 12));
    CHECK(afterA.height == doctest::Approx(beforeA.height + 6));

    // windowB, sitting behind, is untouched.
    Rectangle afterB = WM::WindowGetClientRect(windowB.handle);
    CHECK(afterB.x == beforeB.x);
    CHECK(afterB.y == beforeB.y);
    CHECK(afterB.width == beforeB.width);
    CHECK(afterB.height == beforeB.height);
}
