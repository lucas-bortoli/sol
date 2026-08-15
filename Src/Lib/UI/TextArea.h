#pragma once

#include <raylib.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Selection.h"
#include "Widget.h"

namespace UI {

/// How TextArea breaks a hard-broken line segment into multiple visual
/// (on-screen) rows when it's too wide for the box's content width. Hard
/// line breaks (an embedded '\n' byte) always start a new visual line
/// regardless of this setting; WrapMode only controls further splitting of
/// each such segment.
enum class TextAreaWrapMode {
    None,       ///< Never soft-wraps; overflow scrolls horizontally per line.
    Word,       ///< Breaks at the nearest preceding word boundary that fits.
    Character,  ///< Breaks at any codepoint boundary once width overflows.
};

/// A multi-line, UTF-8, word/character/no-wrap text input. Behaves like
/// TextBox (click/Tab to focus, click-to-place-caret and click-drag to
/// select — double-click a word, triple-click a line, and dragging after
/// either extends the selection a whole word/line at a time — Shift+
/// arrows/Home/End to extend a selection, Ctrl+A/C/X/V) extended to two
/// dimensions: Up/Down move the caret between visual lines
/// with "sticky column" behavior (and, like Home/End, collapse a selection
/// rather than moving relative to the caret when not held with Shift),
/// Home/End target the current visual line (not the whole text), and
/// Enter inserts a newline rather than doing nothing — Shift+Enter instead
/// fires SetOnSubmit(), bypassing the normal Widget Enter-activates-
/// onActivate path entirely (TextArea never sets onActivate). No IME
/// composition.
class TextArea : public Widget {
   public:
    /// `initialText` seeds the box's contents (embedded '\n' bytes are
    /// treated as hard line breaks); the caret starts at the end of it.
    /// `wrapMode` sets the corresponding property up front — equivalent to
    /// calling SetWrapMode immediately after.
    explicit TextArea(
        std::string initialText = "",
        TextAreaWrapMode wrapMode = TextAreaWrapMode::Word
    );

    /// Replaces the box's contents and moves the caret to the end.
    void SetText(std::string newText);
    /// Current contents, as raw UTF-8 bytes (embedded '\n' bytes mark hard
    /// line breaks).
    const std::string& GetText() const { return text; }

    /// Changes how overflowing lines are soft-wrapped. Triggers a rewrap on
    /// next use (Draw/ProcessEvents/IntrinsicHeight).
    TextArea& SetWrapMode(TextAreaWrapMode mode);
    /// Sets the number of visible rows used by IntrinsicHeight(). Content
    /// beyond this scrolls vertically to keep the caret visible. Default 4.
    TextArea& SetVisibleRows(int rows);

    /// Registers a callback fired when Shift+Enter is pressed while this
    /// box is focused — the "submit"/"send" escape hatch for chat-style
    /// UIs, since plain Enter always inserts a newline instead. Deliberately
    /// separate from SetOnActivate: TextArea never populates the inherited
    /// onActivate, so Widget::ProcessKeyboardFocus's global plain-Enter-
    /// activates behavior never fires for a TextArea (see class doc
    /// comment).
    TextArea& SetOnSubmit(std::function<void()> callback);

    void ProcessEvents() override;
    void Draw() const override;

   protected:
    float IntrinsicWidth() const override;
    float IntrinsicHeight() const override;

   private:
    /// One on-screen row: a byte range into `text`, exclusive of its own
    /// trailing '\n' if hard-broken.
    struct VisualLine {
        size_t startByte;
        size_t endByte;
    };

    /// Inserts `byteCount` UTF-8 bytes at the caret and advances the caret
    /// past them. Resets the blink phase and the sticky column.
    void InsertCodepoint(const char* utf8Bytes, int byteCount);
    /// Inserts a single '\n' byte at the caret. Resets the blink phase and
    /// the sticky column.
    void InsertNewline();
    /// Deletes the codepoint immediately before the caret (no-op at
    /// position 0). Resets the blink phase and the sticky column.
    void DeleteBackward();
    /// Deletes the codepoint immediately after the caret (no-op at end of
    /// text). Resets the blink phase and the sticky column.
    void DeleteForward();
    /// Moves the caret one codepoint left (direction < 0) or right
    /// (direction > 0), clamped to [0, text.size()]. Resets blink phase and
    /// the sticky column.
    void MoveCaret(int direction);
    /// Moves the caret to the equivalent on-screen column one visual line
    /// up (direction < 0) or down (direction > 0), clamped at the first/
    /// last line. Preserves desiredColumnPx (the "sticky column") rather
    /// than resetting it, so repeated Up/Down through lines of different
    /// lengths tracks the original horizontal position.
    void MoveCaretVertical(int direction);
    /// Recomputes `visualLines` for `innerWidthPx` and the current
    /// `wrapMode`, if the cached table is stale (text/width/wrapMode
    /// changed since it was last built). Cheap no-op otherwise.
    void RewrapIfNeeded(float innerWidthPx) const;
    /// Returns {visual line index, byte offset within that line} for
    /// `byteIndex`. `visualLines` must already be up to date.
    std::pair<size_t, size_t> VisualLineIndexForByte(size_t byteIndex) const;
    /// Recomputes scrollOffsetRows/scrollOffsetXPx so the caret's on-screen
    /// position stays within the box's inner content rect.
    void ScrollToKeepCaretVisible() const;
    /// Moves the caret to the character boundary closest to
    /// `mousePosition` (screen space), accounting for the current scroll
    /// offsets. Resets the blink phase and resyncs the sticky column.
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
    /// contents, sanitized to keep '\r\n'/'\r' as plain '\n' hard breaks.
    void PasteFromClipboard();

    std::string text;
    size_t caretByteIndex = 0;
    /// The other end of the selection; equal to caretByteIndex when
    /// there's no selection. See HasSelection()/SelectionRange().
    size_t selectionAnchor = 0;
    float blinkTimer = 0.0f;

    /// The caret's on-screen x position (px, relative to the line start)
    /// that Up/Down tries to preserve across lines of different lengths.
    /// Lazily resynced to the caret's current position the next time
    /// MoveCaretVertical runs after a horizontal-affecting action
    /// (Left/Right, insertion, deletion, Home/End, click) — see
    /// desiredColumnDirty — rather than eagerly recomputed at mutation
    /// time, since that would need up-to-date wrap geometry the mutation
    /// site doesn't necessarily have. Left untouched across consecutive
    /// Up/Down calls, which is the entire point of "sticky".
    mutable float desiredColumnPx = 0.0f;
    /// True after any horizontal-affecting action; tells the next
    /// MoveCaretVertical call to resync desiredColumnPx from the caret's
    /// current on-screen position before moving, instead of reusing a
    /// possibly-stale value.
    mutable bool desiredColumnDirty = true;

    TextAreaWrapMode wrapMode;
    int visibleRows = 4;

    std::function<void()> onSubmit;

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
    /// 2 = word, 3 = line) — captured once when the press happens so a
    /// drag keeps using the same unit for its whole duration.
    int activeSelectUnit = 1;
    /// The word or line (activeSelectUnit == 2 or 3) captured at press
    /// time — dragging extends the selection outward from whichever edge
    /// of this range is on the far side of the pointer, so a word/line
    /// stays a whole unit as you drag past further ones instead of being
    /// clipped mid-word/mid-line.
    ByteRange dragAnchorRange{0, 0};

    // Per-key held-duration state for IsKeyRepeated (see Widget.h) — one
    // slot per repeatable key, since each must track its own hold time
    // independently.
    float backspaceHeldSeconds = 0.0f;
    float deleteHeldSeconds = 0.0f;
    float leftHeldSeconds = 0.0f;
    float rightHeldSeconds = 0.0f;
    float upHeldSeconds = 0.0f;
    float downHeldSeconds = 0.0f;
    float homeHeldSeconds = 0.0f;
    float endHeldSeconds = 0.0f;
    float enterHeldSeconds = 0.0f;

    // Cached wrap geometry — draw/layout-time derived state, not model
    // state, hence mutable (same rationale as TextBox's scrollOffsetPx).
    mutable std::vector<VisualLine> visualLines;
    mutable bool visualLinesDirty = true;
    mutable float visualLinesComputedForWidth = -1.0f;
    mutable TextAreaWrapMode visualLinesComputedForWrapMode =
        TextAreaWrapMode::Word;

    mutable float scrollOffsetXPx = 0.0f;   // None-wrap-mode horizontal
                                            // scroll, driven by the
                                            // caret's own line.
    mutable float scrollOffsetRows = 0.0f;  // Fixed-sizing vertical scroll.

    static constexpr float kPaddingX = 4.0f;
    static constexpr float kPaddingY = 4.0f;
    static constexpr float kBlinkPeriod = 1.0f;  // seconds, full on/off cycle
    static constexpr float kMinWidthChars =
        12.0f;  // fallback width guess pre-first-layout
};

}  // namespace UI
