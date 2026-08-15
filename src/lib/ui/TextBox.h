#pragma once

#include <raylib.h>

#include <string>

#include "Widget.h"

namespace ui {

/// A single-line, UTF-8 text input. Click (or Tab) to focus; clicking also
/// places the caret at the nearest character boundary to the click. Type to
/// insert; Backspace/Delete remove one codepoint at a time, Left/Right move
/// the caret one codepoint, Home/End jump to the start/end. Holding any of
/// these repeats it after a short delay, like a standard OS text field. No
/// selection, no clipboard, no multi-line/wrap, no IME composition.
/// Content that overflows the box's width scrolls horizontally to keep the
/// caret visible.
class TextBox : public Widget {
   public:
    /// `initialText` seeds the box's contents; the caret starts at the end
    /// of it.
    explicit TextBox(std::string initialText = "");

    /// Replaces the box's contents and moves the caret to the end.
    void SetText(std::string newText);

    /// Current contents, as raw UTF-8 bytes.
    const std::string& GetText() const { return text; }

    void ProcessEvents() override;
    void Draw() const override;

   protected:
    float IntrinsicWidth() const override;
    float IntrinsicHeight() const override;

   private:
    /// Inserts `byteCount` UTF-8 bytes at the caret and advances the caret
    /// past them. Resets the blink phase.
    void InsertCodepoint(const char* utf8Bytes, int byteCount);
    /// Deletes the codepoint immediately before the caret (no-op at
    /// position 0). Resets the blink phase.
    void DeleteBackward();
    /// Deletes the codepoint immediately after the caret (no-op at end of
    /// text). Resets the blink phase.
    void DeleteForward();
    /// Moves the caret one codepoint left (direction < 0) or right
    /// (direction > 0), clamped to [0, text.size()]. Resets blink phase.
    void MoveCaret(int direction);
    /// Recomputes scrollOffsetPx so the caret's on-screen x position stays
    /// within the box's inner content rect.
    void ScrollToKeepCaretVisible() const;
    /// Moves the caret to the character boundary closest to `mousePosition`
    /// (screen space), accounting for the current scroll offset. Resets the
    /// blink phase.
    void PlaceCaretAtMouse(Vector2 mousePosition);

    std::string text;
    size_t caretByteIndex = 0;
    float blinkTimer = 0.0f;

    // Per-key held-duration state for IsKeyRepeated (see Widget.h) — one
    // slot per repeatable editing key, since each must track its own hold
    // time independently.
    float backspaceHeldSeconds = 0.0f;
    float deleteHeldSeconds = 0.0f;
    float leftHeldSeconds = 0.0f;
    float rightHeldSeconds = 0.0f;
    float homeHeldSeconds = 0.0f;
    float endHeldSeconds = 0.0f;

    /// Horizontal scroll offset in pixels, recomputed every Draw() call
    /// from computedRect/caret position — a draw-time cache, not model
    /// state, hence mutable (same rationale as other widgets' cached
    /// per-frame bookkeeping).
    mutable float scrollOffsetPx = 0.0f;

    static constexpr float kPaddingX = 4.0f;
    static constexpr float kPaddingY = 4.0f;
    static constexpr float kBlinkPeriod = 1.0f;  // seconds, full on/off cycle
    static constexpr float kMinWidthChars = 12.0f;
};

}  // namespace ui
