#pragma once

#include <raylib.h>

#include <string>

#include "Selection.h"
#include "Widget.h"

namespace UI {

/// A single-line, UTF-8 text input. Click (or Tab) to focus; clicking also
/// places the caret at the nearest character boundary to the click, and
/// dragging while held selects. Double-click selects the word (or
/// whitespace/punctuation run) under the pointer; triple-click selects
/// everything (there's only one line). Continuing to drag after a double-
/// click extends the selection a whole word at a time. Shift+Left/Right/
/// Home/End extend a selection from the keyboard; Ctrl+A/C/X/V select-
/// all/copy/cut/paste. Type to insert (replacing any active selection);
/// Backspace/Delete
/// remove one codepoint at a time, or the selection if one exists;
/// Left/Right move the caret one codepoint, Home/End jump to the
/// start/end. Holding any of these repeats it after a short delay, like a
/// standard OS text field. No multi-line/wrap, no IME composition.
/// Content that overflows the box's width scrolls horizontally to keep
/// the caret visible.
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

    /// Whether the selection anchor and caret currently differ, i.e. there
    /// is a non-empty selection.
    bool HasSelection() const { return selectionAnchor != caretByteIndex; }
    /// The current selection as a normalized byte range. Only meaningful
    /// when HasSelection() is true.
    ByteRange SelectionRange() const {
        return NormalizeSelection(selectionAnchor, caretByteIndex);
    }
    /// Erases the current selection from `text`, collapsing the caret and
    /// anchor to where it started. No-op if there's no selection.
    void DeleteSelection();
    /// Copies the current selection to the system clipboard. No-op if
    /// there's no selection.
    void CopySelectionToClipboard() const;
    /// Replaces the current selection (if any) with the system clipboard's
    /// contents, sanitized to a single line (embedded newlines become
    /// spaces).
    void PasteFromClipboard();

    std::string text;
    size_t caretByteIndex = 0;
    /// The other end of the selection; equal to caretByteIndex when
    /// there's no selection. See HasSelection()/SelectionRange().
    size_t selectionAnchor = 0;
    float blinkTimer = 0.0f;

    /// True from the frame a press-drag begins inside this box until the
    /// mouse button is released, regardless of whether the pointer is
    /// still over the box — lets a drag past the box's edges keep
    /// extending the selection. Deliberately separate from Widget's own
    /// pressOrigin/pointerDown, which drive focus-claiming and the pressed
    /// visual, not selection.
    bool isDraggingSelection = false;

    /// GetTime() timestamp of the last mouse press inside this box, used
    /// (together with lastClickByteIndex) to detect a double/triple-click
    /// sequence: a press within kMultiClickIntervalSeconds of the previous
    /// one, at the same character, increments clickCount instead of
    /// resetting it.
    double lastClickTime = 0.0;
    /// caretByteIndex at the time of the last press, for the same-position
    /// check described above.
    size_t lastClickByteIndex = 0;
    /// How many consecutive same-position clicks have landed so far this
    /// sequence (1 = single click). Reset to 1 by any click that doesn't
    /// qualify as a continuation of the previous one.
    int clickCount = 0;
    /// clickCount for the *current* press-drag, capped at 3 (1 = char,
    /// 2 = word, 3 = select-all) — captured once when the press happens
    /// so a drag keeps using the same unit even if, hypothetically,
    /// clickCount changed mid-drag.
    int activeSelectUnit = 1;
    /// The word (activeSelectUnit == 2) or, for a triple-click, the whole
    /// text range, captured at press time — dragging extends the
    /// selection outward from whichever edge of this range is on the far
    /// side of the pointer, so a word/line stays a whole unit as you drag
    /// past further ones instead of being clipped mid-word.
    ByteRange dragAnchorRange{0, 0};

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

}  // namespace UI
