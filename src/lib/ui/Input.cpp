#include "Input.h"

namespace ui {

namespace {

/// Forwards every method 1:1 to the real raylib function — this is the
/// default CurrentInput() target, so swapping every call site over to
/// CurrentInput() is behavior-preserving for the real app.
///
/// Every method here must call the global-namespace-qualified raylib
/// function (`::GetMousePosition()`, not `GetMousePosition()`) — the
/// member function's own name otherwise shadows raylib's free function of
/// the same name, turning the call into infinite self-recursion instead
/// of reaching raylib.
class RaylibInputSource : public InputSource {
   public:
    Vector2 GetMousePosition() override { return ::GetMousePosition(); }
    bool IsMouseButtonPressed(int button) override {
        return ::IsMouseButtonPressed(button);
    }
    bool IsMouseButtonReleased(int button) override {
        return ::IsMouseButtonReleased(button);
    }
    bool IsMouseButtonDown(int button) override {
        return ::IsMouseButtonDown(button);
    }
    float GetMouseWheelMove() override { return ::GetMouseWheelMove(); }

    bool IsKeyPressed(int key) override { return ::IsKeyPressed(key); }
    bool IsKeyDown(int key) override { return ::IsKeyDown(key); }
    bool IsKeyUp(int key) override { return ::IsKeyUp(key); }
    int GetKeyPressed() override { return ::GetKeyPressed(); }
    int GetCharPressed() override { return ::GetCharPressed(); }

    double GetTime() override { return ::GetTime(); }
    float GetFrameTime() override { return ::GetFrameTime(); }

    const char* GetClipboardText() override { return ::GetClipboardText(); }
    void SetClipboardText(const char* text) override {
        ::SetClipboardText(text);
    }
};

RaylibInputSource g_defaultInput;
InputSource* g_activeInput = &g_defaultInput;

}  // namespace

InputSource& CurrentInput() { return *g_activeInput; }

void SetInput(InputSource* source) {
    g_activeInput = source ? source : &g_defaultInput;
}

}  // namespace ui
