#pragma once

#include <raylib.h>

namespace UI {

/// The seam between widget code and "live input" — every raylib call that
/// reads hidden global state (mouse/keyboard/wheel/clipboard/clock) goes
/// through this instead of calling raylib directly, so tests can swap in
/// a scriptable fake and drive the toolkit without a real window.
/// Deliberately excludes CheckCollisionPointRec: it's pure geometry on
/// already-passed-in values, not a read of global state, so every call
/// site keeps calling raylib's version directly.
class InputSource {
   public:
    virtual ~InputSource() = default;

    virtual Vector2 GetMousePosition() = 0;
    virtual bool IsMouseButtonPressed(int button) = 0;
    virtual bool IsMouseButtonReleased(int button) = 0;
    virtual bool IsMouseButtonDown(int button) = 0;
    virtual float GetMouseWheelMove() = 0;

    virtual bool IsKeyPressed(int key) = 0;
    virtual bool IsKeyDown(int key) = 0;
    virtual bool IsKeyUp(int key) = 0;
    /// Drains one event from a frame-scoped queue per call, returning 0
    /// once empty — matches raylib's own GetKeyPressed() semantics
    /// exactly, since callers rely on that draining behavior.
    virtual int GetKeyPressed() = 0;
    /// Same queue-draining contract as GetKeyPressed(), for typed
    /// Unicode codepoints instead of raw key codes.
    virtual int GetCharPressed() = 0;

    virtual double GetTime() = 0;
    virtual float GetFrameTime() = 0;

    virtual const char* GetClipboardText() = 0;
    virtual void SetClipboardText(const char* text) = 0;
};

/// The InputSource all widget code currently reads through. Defaults to a
/// raylib-backed implementation; see SetInput() to override it.
InputSource& CurrentInput();

/// Overrides CurrentInput() with `source` (e.g. a test's FakeInput).
/// Pass nullptr to reset to the default raylib-backed source. Does not
/// take ownership — the caller keeps `source` alive for as long as it's
/// installed.
void SetInput(InputSource* source);

}  // namespace UI
