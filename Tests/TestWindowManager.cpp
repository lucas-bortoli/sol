// Drag-to-move / drag-to-resize tests driven through FakeInput, exercising
// WM::internal::ProcessEvents()'s hit-testing and grid-snap logic. WM state
// is a process-lifetime singleton (Src/WindowManager.cpp), so every test
// destroys the windows it creates when done.

#include <doctest.h>

#include "FakeInput.h"
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
