#pragma once

#include <raylib.h>

#include <deque>
#include <set>
#include <string>

#include "../Src/Lib/UI/Input.h"

/// A scriptable UI::InputSource for driving the widget toolkit without a
/// real window. Install with UI::SetInput(&fake) before a test and
/// UI::SetInput(nullptr) after (see TearDown pattern in the test files).
///
/// Mirrors raylib's own frame semantics: IsMouseButtonPressed/Released and
/// GetKeyPressed/GetCharPressed are edge-triggered/queue-draining and only
/// reflect events recorded since the last NextFrame() call. To simulate a
/// full click, call PressMouseButton(), poll the widget tree once,
/// NextFrame(), then ReleaseMouseButton() and poll again — a click
/// spanning a single poll can't happen for real (a physical press and
/// release can't land in the same frame), so tests shouldn't simulate one
/// that way either.
class FakeInput : public UI::InputSource {
   public:
    // Widget::ProcessKeyboardFocus gates its once-per-frame logic behind a
    // function-local `static` comparison against the previous GetTime()
    // value — process-lifetime state, not reset between tests. Starting
    // each FakeInput's clock at a distinct, ever-increasing offset (rather
    // than every instance starting at 0.0) guarantees no test's GetTime()
    // values can ever collide with a previous test's, so that gating can't
    // spuriously think two different tests' first frames are "the same
    // frame" and skip one of them.
    FakeInput() : clock(NextStartingClock()) {}

    // --- test-facing scripting API ---

    void MoveMouseTo(Vector2 pos) { mousePosition = pos; }
    void PressMouseButton(int button);
    void ReleaseMouseButton(int button);
    void Scroll(float delta) { wheelDelta += delta; }

    void SetKeyDown(int key, bool down);
    /// Queues an event for GetKeyPressed() to drain on the next call(s) —
    /// independent of SetKeyDown, matching raylib's own GetKeyPressed()
    /// being a separate event queue from the IsKeyDown/IsKeyPressed
    /// per-key edge state.
    void QueueKeyPress(int key) { keyPressedQueue.push_back(key); }
    /// Queues a decoded Unicode codepoint for GetCharPressed() to drain.
    void QueueChar(int codepoint) { charPressedQueue.push_back(codepoint); }

    /// Advances the fake clock by `seconds`; GetFrameTime() reports this
    /// value until the next call.
    void AdvanceTime(float seconds);

    /// Clears this-poll-only edge state (pressed/released mouse buttons).
    /// Call between simulated frames, mirroring raylib's own per-frame
    /// edge-state reset. Does NOT drain the key/char queues or reset
    /// IsKeyDown state — those are consumed/cleared by their own APIs,
    /// same as real raylib.
    void NextFrame();

    const std::string& ClipboardText() const { return clipboardText; }

    // --- UI::InputSource ---

    Vector2 GetMousePosition() override { return mousePosition; }
    bool IsMouseButtonPressed(int button) override {
        return mousePressedThisFrame.count(button) > 0;
    }
    bool IsMouseButtonReleased(int button) override {
        return mouseReleasedThisFrame.count(button) > 0;
    }
    bool IsMouseButtonDown(int button) override {
        return mouseButtonsDown.count(button) > 0;
    }
    float GetMouseWheelMove() override { return wheelDelta; }

    bool IsKeyPressed(int key) override {
        return keysPressedThisFrame.count(key) > 0;
    }
    bool IsKeyDown(int key) override { return keysDown.count(key) > 0; }
    bool IsKeyUp(int key) override { return keysDown.count(key) == 0; }
    int GetKeyPressed() override;
    int GetCharPressed() override;

    double GetTime() override { return clock; }
    float GetFrameTime() override { return frameTime; }

    const char* GetClipboardText() override { return clipboardText.c_str(); }
    void SetClipboardText(const char* text) override {
        clipboardText = text ? text : "";
    }

   private:
    static double NextStartingClock() {
        static double next = 0.0;
        double value = next;
        next += 1000.0;  // generous headroom past any test's AdvanceTime() calls
        return value;
    }

    Vector2 mousePosition{};
    std::set<int> mouseButtonsDown;
    std::set<int> mousePressedThisFrame;
    std::set<int> mouseReleasedThisFrame;
    float wheelDelta = 0.0f;

    std::set<int> keysDown;
    std::set<int> keysPressedThisFrame;
    std::deque<int> keyPressedQueue;
    std::deque<int> charPressedQueue;

    double clock = 0.0;
    float frameTime = 1.0f / 60.0f;

    std::string clipboardText;
};
