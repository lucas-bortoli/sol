#include "FakeInput.h"

void FakeInput::PressMouseButton(int button) {
    mouseButtonsDown.insert(button);
    mousePressedThisFrame.insert(button);
}

void FakeInput::ReleaseMouseButton(int button) {
    mouseButtonsDown.erase(button);
    mouseReleasedThisFrame.insert(button);
}

void FakeInput::SetKeyDown(int key, bool down) {
    if (down) {
        if (keysDown.insert(key).second) keysPressedThisFrame.insert(key);
    } else {
        keysDown.erase(key);
    }
}

void FakeInput::AdvanceTime(float seconds) {
    clock += seconds;
    frameTime = seconds;
}

void FakeInput::NextFrame() {
    mousePressedThisFrame.clear();
    mouseReleasedThisFrame.clear();
    keysPressedThisFrame.clear();
    wheelDelta = 0.0f;
}

int FakeInput::GetKeyPressed() {
    if (keyPressedQueue.empty()) return 0;
    int key = keyPressedQueue.front();
    keyPressedQueue.pop_front();
    return key;
}

int FakeInput::GetCharPressed() {
    if (charPressedQueue.empty()) return 0;
    int codepoint = charPressedQueue.front();
    charPressedQueue.pop_front();
    return codepoint;
}
