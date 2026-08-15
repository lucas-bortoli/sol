#include "TextArea.h"

#include <cmath>

#include "../../assets.h"
#include "../../palette.h"
#include "Input.h"
#include "Utf8.h"
#include "Utils.h"

namespace ui {

TextArea::TextArea(std::string initialText, TextAreaWrapMode initialWrapMode)
    : text(std::move(initialText)), wrapMode(initialWrapMode) {
    focusable = true;
    caretByteIndex = text.size();
    selectionAnchor = caretByteIndex;
}

void TextArea::SetText(std::string newText) {
    text = std::move(newText);
    caretByteIndex = text.size();
    selectionAnchor = caretByteIndex;
    visualLinesDirty = true;
    desiredColumnDirty = true;
    Invalidate();
}

TextArea& TextArea::SetWrapMode(TextAreaWrapMode mode) {
    wrapMode = mode;
    visualLinesDirty = true;
    Invalidate();
    return *this;
}

TextArea& TextArea::SetVisibleRows(int rows) {
    visibleRows = rows;
    Invalidate();
    return *this;
}

TextArea& TextArea::SetOnSubmit(std::function<void()> callback) {
    onSubmit = std::move(callback);
    return *this;
}

float TextArea::IntrinsicWidth() const {
    // Same "guess a reasonable width in characters" fallback TextBox uses
    // — content doesn't drive width the way it drives a Label's size, so
    // there's no "natural" value to measure; callers that want a specific
    // width still just call .Width(...).
    return kMinWidthChars *
               MeasureTextEx(assets::cozette, "M", assets::cozette.baseSize, 0)
                   .x +
           2 * kPaddingX;
}

float TextArea::IntrinsicHeight() const {
    // Always a fixed number of rows (visibleRows, default 4) — content
    // beyond that scrolls vertically (ScrollToKeepCaretVisible) rather than
    // growing the box. This is a deliberately simple, single-formula
    // height with no dependency on the box's width or current text, unlike
    // the wrap-aware geometry everything else in this file computes —
    // there used to be an auto-growing sizing mode here, but it ran into a
    // genuine chicken-and-egg problem (Container queries IntrinsicHeight()
    // before a child's width is resolved for the frame, so "grow to fit
    // wrapped content" couldn't know its own wrap width without lagging a
    // frame behind) and was removed in favor of just this.
    return static_cast<float>(visibleRows) * assets::cozette.baseSize +
           2 * kPaddingY;
}

// All five mutator/mover methods below share the same three-part
// bookkeeping ritual, for the same three independent reasons:
//   - blinkTimer = 0.0f     — restart the caret's blink cycle so it's
//                             always solid right after you interact.
//   - visualLinesDirty/     — the wrapped-line table and the byte offset
//     Invalidate()            it was measured against are now stale (text
//                             changed) or the box's own size might change
//                             (Invalidate bubbles to the parent layout);
//                             RewrapIfNeeded() will lazily recompute the
//                             table next time anything asks for it.
//   - desiredColumnDirty    — any horizontal caret movement invalidates
//     = true                  the Up/Down "sticky column" (see
//                             MoveCaretVertical) until it's resynced.
// Pure caret-movement methods with no text change (MoveCaret,
// MoveCaretVertical) skip the visualLinesDirty/Invalidate() step, since
// moving the caret never changes what's on screen at the layout level.

void TextArea::InsertCodepoint(const char* utf8Bytes, int byteCount) {
    // Byte-buffer insert is safe here for the same reason as TextBox's
    // version: utf8Bytes is always one whole already-encoded codepoint, and
    // caretByteIndex is always codepoint-aligned.
    text.insert(caretByteIndex, utf8Bytes, byteCount);
    caretByteIndex += byteCount;
    // Resync the anchor to the new caret position — InsertCodepoint is
    // never used to extend a selection (unlike MoveCaret/MoveCaretVertical,
    // which are also called from ApplySelectableMovement's Shift-extend
    // path and must NOT do this), so typing always collapses any leftover
    // selection state instead of leaving a stale anchor behind that would
    // make HasSelection() spuriously true afterward.
    selectionAnchor = caretByteIndex;
    blinkTimer = 0.0f;
    visualLinesDirty = true;
    desiredColumnDirty = true;
    Invalidate();
}

void TextArea::InsertNewline() {
    // Inserted as a raw '\n' byte rather than routed through
    // CodepointToUTF8 — codepoint 10 is one ASCII byte either way, but
    // going straight to the literal byte avoids depending on
    // CodepointToUTF8's (undocumented) behavior for control characters.
    // This '\n' is what RewrapIfNeeded's hard-break scan looks for.
    text.insert(caretByteIndex, "\n", 1);
    caretByteIndex += 1;
    selectionAnchor = caretByteIndex;  // see InsertCodepoint's comment
    blinkTimer = 0.0f;
    visualLinesDirty = true;
    desiredColumnDirty = true;
    Invalidate();
}

void TextArea::DeleteBackward() {
    if (caretByteIndex == 0) return;
    // Deletes one whole codepoint — which, if it happens to be the '\n'
    // right after the caret's line, effectively joins that line with the
    // one before it (RewrapIfNeeded will naturally treat the merged text
    // as one hard-broken segment again next time it runs).
    size_t start = PrevCodepointBoundary(text, caretByteIndex);
    text.erase(start, caretByteIndex - start);
    caretByteIndex = start;
    selectionAnchor = caretByteIndex;  // see InsertCodepoint's comment
    blinkTimer = 0.0f;
    visualLinesDirty = true;
    desiredColumnDirty = true;
    Invalidate();
}

void TextArea::DeleteForward() {
    if (caretByteIndex >= text.size()) return;
    // Symmetric to DeleteBackward, deleting the codepoint ahead of the
    // caret instead (same line-joining behavior if it's a '\n').
    size_t end = NextCodepointBoundary(text, caretByteIndex);
    text.erase(caretByteIndex, end - caretByteIndex);
    selectionAnchor = caretByteIndex;  // see InsertCodepoint's comment
    blinkTimer = 0.0f;
    visualLinesDirty = true;
    desiredColumnDirty = true;
    Invalidate();
}

void TextArea::MoveCaret(int direction) {
    // Horizontal, codepoint-wise movement — deliberately unaware of visual
    // lines. Moving past a hard '\n' or a soft wrap point just continues
    // into the adjacent line's bytes, since the underlying `text` is one
    // flat, contiguous string; VisualLineIndexForByte figures out which
    // visual line the resulting caretByteIndex now belongs to whenever
    // something needs that (rendering, Up/Down, Home/End).
    if (direction < 0) {
        caretByteIndex = PrevCodepointBoundary(text, caretByteIndex);
    } else if (direction > 0) {
        caretByteIndex = NextCodepointBoundary(text, caretByteIndex);
    }
    blinkTimer = 0.0f;
    desiredColumnDirty = true;
}

void TextArea::DeleteSelection() {
    if (!HasSelection()) return;
    ByteRange range = SelectionRange();
    text.erase(range.start, range.end - range.start);
    caretByteIndex = range.start;
    selectionAnchor = range.start;
    blinkTimer = 0.0f;
    visualLinesDirty = true;
    desiredColumnDirty = true;
    Invalidate();
}

void TextArea::CopySelectionToClipboard() const {
    if (!HasSelection()) return;
    ByteRange range = SelectionRange();
    std::string selected = text.substr(range.start, range.end - range.start);
    CurrentInput().SetClipboardText(selected.c_str());
}

void TextArea::PasteFromClipboard() {
    const char* clipboard = CurrentInput().GetClipboardText();
    if (!clipboard) return;
    // TextArea is multi-line, so pasted newlines stay as hard breaks —
    // just normalized to plain '\n' regardless of the source's line-ending
    // convention.
    std::string sanitized =
        SanitizePastedText(clipboard, /*allowNewlines=*/true);
    if (sanitized.empty()) return;

    DeleteSelection();
    text.insert(caretByteIndex, sanitized);
    caretByteIndex += sanitized.size();
    selectionAnchor = caretByteIndex;
    blinkTimer = 0.0f;
    visualLinesDirty = true;
    desiredColumnDirty = true;
    Invalidate();
}

// "Sticky column" is the standard text-editor behavior where pressing
// Down repeatedly (through lines of varying length) keeps the caret at
// roughly the same horizontal pixel position, rather than snapping to
// whatever the previous line's length happened to be. It works by
// remembering a target x position (desiredColumnPx) that's set once, the
// first time you move vertically after any horizontal action, and then
// deliberately left alone across however many consecutive Up/Down presses
// follow — each one just re-aims for that same remembered x, landing on
// whichever character in the new line is closest to it.
void TextArea::MoveCaretVertical(int direction) {
    if (visualLines.empty()) return;

    auto [lineIdx, byteInLine] = VisualLineIndexForByte(caretByteIndex);

    // Resync desiredColumnPx from the caret's actual current position, but
    // only if something horizontal happened since the last vertical move
    // (typing, Left/Right, Home/End, a click — anything that sets
    // desiredColumnDirty). This is why desiredColumnPx can't just be
    // computed unconditionally every call: doing so would make it track
    // the *landed* column after every Up/Down instead of the *original*
    // one, defeating the whole point of "sticky".
    if (desiredColumnDirty) {
        const VisualLine& currentLine = visualLines[lineIdx];
        std::string beforeCaret =
            text.substr(currentLine.startByte, byteInLine);
        desiredColumnPx = MeasureTextEx(
                              assets::cozette,
                              beforeCaret.c_str(),
                              assets::cozette.baseSize,
                              0
        )
                              .x;
        desiredColumnDirty = false;
    }

    // Clamp at the very first/last visual line rather than doing nothing —
    // Up at the top of the box lands you at its start, Down at the bottom
    // lands you at its end, matching Home/End-style clamping elsewhere in
    // this widget rather than leaving the caret motionless (which would
    // feel like the key silently didn't work).
    size_t targetLine;
    if (direction < 0) {
        if (lineIdx == 0) {
            caretByteIndex = visualLines[0].startByte;
            blinkTimer = 0.0f;
            return;
        }
        targetLine = lineIdx - 1;
    } else {
        if (lineIdx + 1 >= visualLines.size()) {
            caretByteIndex = visualLines.back().endByte;
            blinkTimer = 0.0f;
            return;
        }
        targetLine = lineIdx + 1;
    }

    // Same "walk codepoints, track the closest cumulative-width match"
    // search PlaceCaretAtMouse uses for a click, just aimed at
    // desiredColumnPx instead of a mouse x, and scoped to the target
    // line's byte range instead of the whole line under the cursor.
    const VisualLine& line = visualLines[targetLine];
    size_t bestIndex = line.startByte;
    float bestDistance = fabsf(desiredColumnPx);
    float cumulativeWidth = 0.0f;
    size_t pos = line.startByte;
    while (pos < line.endByte) {
        size_t next = NextCodepointBoundary(text, pos);
        std::string glyph = text.substr(pos, next - pos);
        cumulativeWidth +=
            MeasureTextEx(
                assets::cozette, glyph.c_str(), assets::cozette.baseSize, 0
            )
                .x;
        float distance = fabsf(desiredColumnPx - cumulativeWidth);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = next;
        }
        pos = next;
    }

    caretByteIndex = bestIndex;
    blinkTimer = 0.0f;
    // desiredColumnPx/desiredColumnDirty intentionally untouched — that's
    // what makes repeated Up/Down "sticky".
}

// Rebuilds `visualLines`, the table every other method in this file reads
// to translate between "byte offset in the flat `text` string" and "which
// on-screen row, and where in it". This is the one place multi-line
// geometry actually gets computed; everywhere else (Draw, caret movement,
// scrolling, click hit-testing) just looks the table up.
//
// Two-level splitting, matching how every real text editor treats
// newlines vs. word-wrap:
//   1. Hard breaks first: `text` is always split at every literal '\n'
//      byte, regardless of wrapMode — this is what keeps typed Enter
//      presses meaningful even when word-wrap is off, and is why an empty
//      line (two '\n' in a row, or a trailing '\n') still gets its own
//      zero-length VisualLine rather than disappearing.
//   2. Soft breaks second: each hard-broken segment is then independently
//      re-split by appendSegment() according to wrapMode, so wrapping
//      decisions never cross a '\n' boundary.
void TextArea::RewrapIfNeeded(float innerWidthPx) const {
    // Cheap early-out: skip the whole O(text length) glyph-measuring walk
    // below unless something that could actually change the wrapping
    // happened since the last call — new text, a resize, or a wrap-mode
    // switch. Every text-mutating method and SetWrapMode() sets
    // visualLinesDirty; everything else just calls RewrapIfNeeded()
    // speculatively every frame and relies on this check making repeated
    // calls in the same frame (Draw + ScrollToKeepCaretVisible +
    // ProcessEvents can each call it) effectively free.
    if (!visualLinesDirty && visualLinesComputedForWidth == innerWidthPx &&
        visualLinesComputedForWrapMode == wrapMode) {
        return;
    }

    visualLines.clear();

    // Soft-wraps one hard-broken segment [segStart, segEnd) — i.e. a run
    // of text with no '\n' in it — into one or more VisualLines, per the
    // current wrapMode. Captured by reference so it can push directly into
    // the outer `visualLines` as it goes, rather than building and
    // returning a separate list per segment.
    auto appendSegment = [&](size_t segStart, size_t segEnd) {
        // WrapMode::None (or a segment too short to need splitting at
        // all): the whole segment is exactly one visual line, however wide
        // it renders — horizontal overflow is handled later by
        // ScrollToKeepCaretVisible's per-line horizontal scroll instead of
        // by breaking the line here.
        if (wrapMode == TextAreaWrapMode::None || segStart == segEnd) {
            visualLines.push_back({segStart, segEnd});
            return;
        }

        size_t lineStart = segStart;
        // Word mode only: byte offset of the most recent "safe to break
        // here" point seen so far on the current line — specifically, the
        // position right after the last run of spaces. Character mode
        // ignores this (it always breaks exactly where width overflows).
        size_t lastBreakOpportunity = segStart;
        float widthPx = 0.0f;
        size_t pos = segStart;
        while (pos < segEnd) {
            size_t next = NextCodepointBoundary(text, pos);
            std::string glyph = text.substr(pos, next - pos);
            float glyphW =
                MeasureTextEx(
                    assets::cozette, glyph.c_str(), assets::cozette.baseSize, 0
                )
                    .x;

            // `pos > lineStart` guards against ever breaking before a
            // single character has been placed — otherwise a glyph wider
            // than the whole box on its own would trigger this every
            // iteration without ever advancing, looping forever on a
            // zero-length line.
            if (widthPx + glyphW > innerWidthPx && pos > lineStart) {
                if (wrapMode == TextAreaWrapMode::Word &&
                    lastBreakOpportunity > lineStart) {
                    // A word boundary exists somewhere on this line before
                    // the overflow point: break there instead of
                    // mid-word. Rewinding `pos` back to that boundary and
                    // `continue`-ing means the characters between the
                    // boundary and the overflow point get re-walked (and
                    // re-measured) as part of the *next* line — simple,
                    // if not maximally efficient, and fine for
                    // human-typed-length text.
                    visualLines.push_back({lineStart, lastBreakOpportunity});
                    lineStart = lastBreakOpportunity;
                    pos = lineStart;
                    widthPx = 0.0f;
                    continue;
                }
                // Either in Character mode, or in Word mode with no space
                // seen yet on this line (a single word/run wider than the
                // box) — fall back to breaking exactly at the overflow
                // point, mid-character-run if necessary. This is the
                // standard "long unbreakable word" behavior every wrapping
                // text editor needs to avoid an infinitely wide line.
                visualLines.push_back({lineStart, pos});
                lineStart = pos;
                lastBreakOpportunity = lineStart;
                widthPx = 0.0f;
            }

            widthPx += glyphW;
            // A space is itself part of the current line (not trimmed),
            // but marks the point *after* it as a valid break — so the
            // space that caused the wrap stays at the end of the upper
            // line rather than getting silently dropped or bumped down.
            if (wrapMode == TextAreaWrapMode::Word && glyph == " ") {
                lastBreakOpportunity = next;
            }
            pos = next;
        }
        // Whatever's left after the loop (from lineStart to the end of
        // the segment) becomes the segment's final visual line — every
        // segment always ends with a push here, even a segment that never
        // needed to wrap (widthPx never exceeded innerWidthPx).
        visualLines.push_back({lineStart, segEnd});
    };

    // Scan `text` once, treating every '\n' (and the implicit end of the
    // string) as a hard-break boundary; each [segStart, i) run between
    // boundaries is one paragraph-like segment handed to appendSegment.
    // The `i == text.size()` branch is what makes the final run of text
    // (which has no trailing '\n') still get wrapped — and, symmetrically,
    // a `text` that itself ends in '\n' produces one extra empty trailing
    // VisualLine, matching how a cursor lands on a new empty line after
    // pressing Enter at the very end of a document.
    size_t segStart = 0;
    for (size_t i = 0; i <= text.size(); i++) {
        if (i == text.size() || text[i] == '\n') {
            appendSegment(segStart, i);
            segStart = i + 1;
        }
    }
    // Dead in practice — the loop above always executes at least once
    // (i == 0 satisfies i == text.size() when text is empty) and always
    // pushes a segment, so visualLines is never actually empty here.
    // Left in as a defensive belt-and-suspenders check so every other
    // method can safely assume visualLines is non-empty without having to
    // re-prove it.
    if (visualLines.empty()) visualLines.push_back({0, 0});

    visualLinesDirty = false;
    visualLinesComputedForWidth = innerWidthPx;
    visualLinesComputedForWrapMode = wrapMode;
}

// Translates a flat byte offset (as caretByteIndex always is) into "which
// visual line, and how far into it" — the inverse of a VisualLine's byte
// range. Every caller that needs to know where the caret visually sits
// (Draw, MoveCaretVertical, Home/End, ScrollToKeepCaretVisible) goes
// through this rather than tracking line/column as separate state, so
// there's only ever one source of truth (caretByteIndex) to keep correct.
std::pair<size_t, size_t> TextArea::VisualLineIndexForByte(
    size_t byteIndex
) const {
    // Lines are contiguous and ascending, so the first line whose end
    // reaches at least byteIndex is the owning one. At an exact boundary
    // between two adjacent lines (byteIndex == that line's endByte), this
    // deliberately resolves to the *earlier* line — i.e. the caret renders
    // at the end of the line above rather than the start of the line
    // below — which matches the "clicking right at a wrap point lands you
    // at the end of the upper line" feel most editors have.
    for (size_t i = 0; i < visualLines.size(); i++) {
        if (byteIndex <= visualLines[i].endByte) {
            return {i, byteIndex - visualLines[i].startByte};
        }
    }
    // Only reached if byteIndex is past every line's endByte — shouldn't
    // normally happen (caretByteIndex is always clamped to [0,
    // text.size()], and the last line's endByte is always text.size()),
    // but falls back to "end of the last line" rather than an out-of-range
    // read if it ever does.
    size_t lastIndex = visualLines.empty() ? 0 : visualLines.size() - 1;
    size_t startByte =
        visualLines.empty() ? 0 : visualLines[lastIndex].startByte;
    return {lastIndex, byteIndex - startByte};
}

void TextArea::ProcessEvents() {
    Widget::ProcessEvents();  // click-to-focus, for free — see the matching
                              // comment in TextBox::ProcessEvents; the same
                              // "click both focuses and repositions the
                              // caret" reasoning applies here.

    Rectangle rect = GetComputedRect();
    // WrapMode::None uses an effectively unbounded width (1e6px) so
    // RewrapIfNeeded never soft-breaks a line — width overflow is instead
    // handled by ScrollToKeepCaretVisible's horizontal scroll, same as
    // TextBox. Every other call site in this file that needs innerWidth
    // computes this same ternary independently rather than caching it,
    // since RewrapIfNeeded's own dirty-check makes repeated calls cheap.
    float innerWidth =
        wrapMode == TextAreaWrapMode::None ? 1e6f : rect.width - 2 * kPaddingX;
    RewrapIfNeeded(innerWidth);

    // Press-and-hold starts a fresh selection at the click point; holding
    // the button — even once the mouse is outside this box's rect —
    // keeps extending it, since PlaceCaretAtMouse's closest-boundary
    // search degrades gracefully for a point outside the box (including
    // above/below all visible lines). isDraggingSelection is deliberately
    // independent of Widget's own pressOrigin/pointerDown (those drive
    // focus-claiming and the pressed visual, not selection).
    if (CurrentInput().IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(CurrentInput().GetMousePosition(), rect)) {
        PlaceCaretAtMouse(CurrentInput().GetMousePosition());

        // Double/triple-click detection: a press at the same character as
        // the previous one, within kMultiClickIntervalSeconds, continues
        // the sequence instead of starting a fresh single click.
        double now = CurrentInput().GetTime();
        if (clickCount > 0 &&
            now - lastClickTime <= kMultiClickIntervalSeconds &&
            caretByteIndex == lastClickByteIndex) {
            clickCount++;
        } else {
            clickCount = 1;
        }
        lastClickTime = now;
        lastClickByteIndex = caretByteIndex;
        activeSelectUnit = clickCount > 3 ? 3 : clickCount;

        if (activeSelectUnit == 2) {
            dragAnchorRange = WordRangeAt(text, caretByteIndex);
            selectionAnchor = dragAnchorRange.start;
            caretByteIndex = dragAnchorRange.end;
        } else if (activeSelectUnit == 3) {
            auto [lineIdx, byteInLine] = VisualLineIndexForByte(caretByteIndex);
            dragAnchorRange = {
                visualLines[lineIdx].startByte, visualLines[lineIdx].endByte
            };
            selectionAnchor = dragAnchorRange.start;
            caretByteIndex = dragAnchorRange.end;
        } else {
            dragAnchorRange = {caretByteIndex, caretByteIndex};
            selectionAnchor = caretByteIndex;
        }
        desiredColumnDirty = true;
        isDraggingSelection = true;
    }
    if (isDraggingSelection) {
        if (CurrentInput().IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            PlaceCaretAtMouse(CurrentInput().GetMousePosition());
            if (activeSelectUnit == 2) {
                // Word-wise drag: extend outward from whichever edge of
                // the originally-clicked word is on the far side of the
                // pointer, so dragging past further words selects them
                // whole rather than clipping mid-word.
                ByteRange hoverWord = WordRangeAt(text, caretByteIndex);
                if (caretByteIndex >= dragAnchorRange.start) {
                    selectionAnchor = dragAnchorRange.start;
                    caretByteIndex = hoverWord.end;
                } else {
                    selectionAnchor = dragAnchorRange.end;
                    caretByteIndex = hoverWord.start;
                }
            } else if (activeSelectUnit == 3) {
                // Line-wise drag: same idea, one whole visual line at a
                // time instead of one word.
                auto [lineIdx, byteInLine] =
                    VisualLineIndexForByte(caretByteIndex);
                ByteRange hoverLine = {
                    visualLines[lineIdx].startByte, visualLines[lineIdx].endByte
                };
                if (caretByteIndex >= dragAnchorRange.start) {
                    selectionAnchor = dragAnchorRange.start;
                    caretByteIndex = hoverLine.end;
                } else {
                    selectionAnchor = dragAnchorRange.end;
                    caretByteIndex = hoverLine.start;
                }
            }
        } else {
            isDraggingSelection = false;
        }
    }

    // As with TextBox: clicking (handled above) works on an unfocused box,
    // but keyboard editing only applies to whichever widget currently
    // holds focus.
    if (!focused) return;

    blinkTimer += CurrentInput().GetFrameTime();

    // Enter is claimed entirely by TextArea itself rather than routed
    // through Widget::onActivate/ProcessKeyboardFocus's global Enter-
    // activates behavior (see TextArea.h's class doc comment) — this is
    // what lets plain Enter always mean "insert a newline" with no risk of
    // also firing some unrelated activate callback the same frame.
    // Shift+Enter is the deliberate escape hatch for a "submit"/"send"
    // action instead, matching chat-app convention (Slack/Discord/ChatGPT).
    bool shiftHeld = CurrentInput().IsKeyDown(KEY_LEFT_SHIFT) ||
                      CurrentInput().IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyRepeated(KEY_ENTER, enterHeldSeconds)) {
        if (shiftHeld) {
            if (onSubmit) onSubmit();
        } else {
            if (HasSelection()) DeleteSelection();
            InsertNewline();
        }
    }

    // Same decoded-codepoint-queue drain as TextBox — see the matching
    // comment there for why this loop (rather than a single check) is
    // needed. Note GetCharPressed() never yields '\n' for an Enter press
    // (Enter isn't a "character" in raylib's sense), so this can't
    // double-insert a newline on top of the KEY_ENTER handling above. Each
    // character replaces the current selection, if any — but only the
    // first iteration actually needs to delete it, since DeleteSelection()
    // clears HasSelection() for the rest of the loop.
    int codepoint;
    while ((codepoint = CurrentInput().GetCharPressed()) != 0) {
        if (HasSelection()) DeleteSelection();
        int byteCount = 0;
        const char* utf8 = CodepointToUTF8(codepoint, &byteCount);
        InsertCodepoint(utf8, byteCount);
    }

    bool ctrlHeld = CurrentInput().IsKeyDown(KEY_LEFT_CONTROL) ||
                    CurrentInput().IsKeyDown(KEY_RIGHT_CONTROL);

    // Backspace/Delete remove the selection instead of a single codepoint
    // when one is active.
    if (IsKeyRepeated(KEY_BACKSPACE, backspaceHeldSeconds)) {
        if (HasSelection())
            DeleteSelection();
        else
            DeleteBackward();
    }
    if (IsKeyRepeated(KEY_DELETE, deleteHeldSeconds)) {
        if (HasSelection())
            DeleteSelection();
        else
            DeleteForward();
    }

    // Text may have changed above (Enter/typing/Backspace/Delete all
    // mutate `text` and set visualLinesDirty); explicitly refresh the wrap
    // table now so the caret-movement logic below always reads
    // *this* frame's geometry, not whatever was cached before those edits.
    RewrapIfNeeded(innerWidth);

    // Left/Right/Up/Down/Home/End all go through ApplySelectableMovement
    // (Selection.h) so Shift extends the selection and a plain press
    // instead collapses to whichever edge is appropriate for the
    // direction, matching every mainstream text editor. For Left/Right/
    // Home/End, desiredColumnDirty is force-set true after the call
    // regardless of which branch ApplySelectableMovement took — MoveCaret/
    // the Home/End lambdas already set it when they run, but the
    // selection-collapse branch (which skips them entirely) wouldn't
    // otherwise, and a horizontal action always invalidates the sticky
    // column either way.
    if (IsKeyRepeated(KEY_LEFT, leftHeldSeconds)) {
        ApplySelectableMovement(
            caretByteIndex, selectionAnchor, shiftHeld, true, [this] {
                MoveCaret(-1);
            }
        );
        desiredColumnDirty = true;
    }
    if (IsKeyRepeated(KEY_RIGHT, rightHeldSeconds)) {
        ApplySelectableMovement(
            caretByteIndex, selectionAnchor, shiftHeld, false, [this] {
                MoveCaret(1);
            }
        );
        desiredColumnDirty = true;
    }
    if (IsKeyRepeated(KEY_UP, upHeldSeconds)) {
        ApplySelectableMovement(
            caretByteIndex, selectionAnchor, shiftHeld, true, [this] {
                MoveCaretVertical(-1);
            }
        );
    }
    if (IsKeyRepeated(KEY_DOWN, downHeldSeconds)) {
        ApplySelectableMovement(
            caretByteIndex, selectionAnchor, shiftHeld, false, [this] {
                MoveCaretVertical(1);
            }
        );
    }
    // Home/End are scoped to the caret's *current visual line*, not the
    // whole text (unlike TextBox, where "the whole text" and "the current
    // line" are the same thing) — this is the multi-line-appropriate
    // behavior: End on a long wrapped paragraph goes to the end of the
    // visible row you're on, not the end of the entire paragraph three
    // rows down.
    if (IsKeyRepeated(KEY_HOME, homeHeldSeconds)) {
        ApplySelectableMovement(
            caretByteIndex, selectionAnchor, shiftHeld, true, [this] {
                auto [lineIdx, byteInLine] =
                    VisualLineIndexForByte(caretByteIndex);
                caretByteIndex = visualLines[lineIdx].startByte;
                blinkTimer = 0.0f;
            }
        );
        desiredColumnDirty = true;
    }
    if (IsKeyRepeated(KEY_END, endHeldSeconds)) {
        ApplySelectableMovement(
            caretByteIndex, selectionAnchor, shiftHeld, false, [this] {
                auto [lineIdx, byteInLine] =
                    VisualLineIndexForByte(caretByteIndex);
                caretByteIndex = visualLines[lineIdx].endByte;
                blinkTimer = 0.0f;
            }
        );
        desiredColumnDirty = true;
    }

    // Clipboard: one-shot (IsKeyPressed, not IsKeyRepeated) since holding
    // Ctrl+C/X/V/A shouldn't repeat the action every frame.
    if (ctrlHeld && CurrentInput().IsKeyPressed(KEY_A)) {
        selectionAnchor = 0;
        caretByteIndex = text.size();
        blinkTimer = 0.0f;
        desiredColumnDirty = true;
    }
    if (ctrlHeld && CurrentInput().IsKeyPressed(KEY_C)) CopySelectionToClipboard();
    if (ctrlHeld && CurrentInput().IsKeyPressed(KEY_X)) {
        CopySelectionToClipboard();
        DeleteSelection();
    }
    if (ctrlHeld && CurrentInput().IsKeyPressed(KEY_V)) PasteFromClipboard();
}

// Vertical counterpart to TextBox's ScrollToKeepCaretVisible, in row units
// instead of pixels (scrollOffsetRows: how many whole/fractional rows of
// content are currently scrolled above the visible top edge), plus a
// WrapMode::None-only horizontal component reusing TextBox's exact pixel
// approach for the caret's own line.
void TextArea::ScrollToKeepCaretVisible() const {
    Rectangle rect = GetComputedRect();
    float innerWidth = rect.width - 2 * kPaddingX;
    float innerHeight = rect.height - 2 * kPaddingY;
    float lineHeight = assets::cozette.baseSize;

    RewrapIfNeeded(wrapMode == TextAreaWrapMode::None ? 1e6f : innerWidth);
    if (visualLines.empty()) return;

    auto [caretLine, byteInLine] = VisualLineIndexForByte(caretByteIndex);

    // Same "push scroll toward the caret" logic runs unconditionally
    // (not gated on how the box got its height) — see IntrinsicHeight()'s
    // fixed-row-count comment. When there's more content than fits, this
    // is what makes the box scroll to follow the caret instead of just
    // silently clipping whatever's past the bottom.
    float visibleRowCount = innerHeight / lineHeight;
    // Caret scrolled above the visible window: pull the window up to it.
    if (static_cast<float>(caretLine) < scrollOffsetRows) {
        scrollOffsetRows = static_cast<float>(caretLine);
    }
    // Caret scrolled below the visible window (note "+1": caretLine is a
    // row *index*, so the caret's row occupies [caretLine, caretLine+1) in
    // row-count terms — this compares against where the bottom edge of
    // that row would need scrollOffsetRows to be to stay in view).
    if (static_cast<float>(caretLine) + 1.0f >
        scrollOffsetRows + visibleRowCount) {
        scrollOffsetRows =
            static_cast<float>(caretLine) + 1.0f - visibleRowCount;
    }
    if (scrollOffsetRows < 0.0f) scrollOffsetRows = 0.0f;

    // Horizontal scroll is only meaningful in None mode — Word/Character
    // wrap guarantee every visual line already fits within innerWidth by
    // construction, so there's nothing to scroll sideways. In None mode
    // this is exactly TextBox's ScrollToKeepCaretVisible algorithm, just
    // scoped to the caret's own visual line's text instead of the whole
    // (here, single, unwrapped) string.
    if (wrapMode == TextAreaWrapMode::None) {
        const VisualLine& line = visualLines[caretLine];
        std::string beforeCaret = text.substr(line.startByte, byteInLine);
        float caretX = MeasureTextEx(
                           assets::cozette,
                           beforeCaret.c_str(),
                           assets::cozette.baseSize,
                           0
        )
                           .x;
        if (caretX - scrollOffsetXPx > innerWidth) {
            scrollOffsetXPx = caretX - innerWidth;
        }
        if (caretX - scrollOffsetXPx < 0.0f) {
            scrollOffsetXPx = caretX;
        }
        if (scrollOffsetXPx < 0.0f) scrollOffsetXPx = 0.0f;
    } else {
        // Word/Character mode: always flush left, since there's never
        // horizontal overflow to scroll past.
        scrollOffsetXPx = 0.0f;
    }
}

// Two-step click-to-caret: first find *which visual line* was clicked (by
// Y), then run TextBox's exact "walk codepoints, track the closest
// cumulative width" search (by X) within just that line's byte range.
void TextArea::PlaceCaretAtMouse(Vector2 mousePosition) {
    if (visualLines.empty()) return;

    Rectangle rect = GetComputedRect();
    float lineHeight = assets::cozette.baseSize;

    // Convert the click's Y into "rows from the top of the full (unscrolled)
    // content" — same idea as PlaceCaretAtMouse's X conversion in TextBox,
    // just on the vertical axis: subtract the box's top edge/padding, then
    // add back however many rows are currently scrolled out of view above.
    float localY =
        mousePosition.y - (rect.y + kPaddingY) + scrollOffsetRows * lineHeight;
    size_t lineIndex = 0;
    // Guard the float->size_t cast against negative localY (clicking in
    // the top padding) — casting a negative float to an unsigned type is
    // undefined/huge, not just wrong, so this must be checked before the
    // cast rather than clamped after it.
    if (localY > 0) lineIndex = static_cast<size_t>(localY / lineHeight);
    // Clicks below the last line (in the bottom padding, or on a
    // shorter-than-the-box content area) land on the last line rather than
    // being ignored — same "clamp to nearest valid target" philosophy as
    // the horizontal glyph search below.
    if (lineIndex >= visualLines.size()) lineIndex = visualLines.size() - 1;

    const VisualLine& line = visualLines[lineIndex];

    // Horizontal scroll only exists in None mode (see
    // ScrollToKeepCaretVisible) — Word/Character-wrapped lines are never
    // horizontally scrolled, so their local-space and screen-space X
    // coincide and no offset correction is needed.
    float localX =
        mousePosition.x - (rect.x + kPaddingX) +
        (wrapMode == TextAreaWrapMode::None ? scrollOffsetXPx : 0.0f);

    // Identical algorithm to TextBox::PlaceCaretAtMouse — see its comments
    // for the full walkthrough — just bounded to [line.startByte,
    // line.endByte) instead of the whole text.
    size_t bestIndex = line.startByte;
    float bestDistance = fabsf(localX);
    float cumulativeWidth = 0.0f;
    size_t pos = line.startByte;
    while (pos < line.endByte) {
        size_t next = NextCodepointBoundary(text, pos);
        std::string glyph = text.substr(pos, next - pos);
        cumulativeWidth +=
            MeasureTextEx(
                assets::cozette, glyph.c_str(), assets::cozette.baseSize, 0
            )
                .x;
        float distance = fabsf(localX - cumulativeWidth);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = next;
        }
        pos = next;
    }

    caretByteIndex = bestIndex;
    blinkTimer = 0.0f;
    desiredColumnDirty = true;
}

void TextArea::Draw() const {
    Rectangle rect = GetComputedRect();

    // Same chrome/focus-ring visual language as Button/TextBox throughout
    // this function, for consistency across the toolkit's focusable
    // widgets — see TextBox::Draw's comments for the rationale behind each
    // individual piece (chrome color, why DrawTextEx instead of
    // ui::DrawText, why the focus ring is drawn outside the scissor
    // region). Only what's specific to being multi-line is called out
    // below.
    ui::DrawRectWithBorderAndShadow(rect, WHITE, NEUTRAL_600, 1);

    Rectangle innerRect{
        rect.x + kPaddingX,
        rect.y + kPaddingY,
        rect.width - 2 * kPaddingX,
        rect.height - 2 * kPaddingY
    };

    // Both must run before any drawing: RewrapIfNeeded ensures
    // `visualLines` matches this frame's actual width, and
    // ScrollToKeepCaretVisible (which itself also calls RewrapIfNeeded,
    // redundantly-but-cheaply) derives scrollOffsetRows/scrollOffsetXPx
    // from wherever the caret ended up this frame.
    RewrapIfNeeded(wrapMode == TextAreaWrapMode::None ? 1e6f : innerRect.width);
    ScrollToKeepCaretVisible();

    // One scissor region for the whole box, not one per line — clipping is
    // a GPU state, not a per-draw-call cost, so there's no benefit to
    // narrowing it per line, and a single region is simpler to reason
    // about.
    BeginScissorMode(
        static_cast<int>(innerRect.x),
        static_cast<int>(innerRect.y),
        static_cast<int>(innerRect.width),
        static_cast<int>(innerRect.height)
    );

    float lineHeight = assets::cozette.baseSize;
    // The Y coordinate row 0 would be drawn at, if it weren't scrolled off
    // screen — every row's actual Y is an offset from this, so scrolling
    // is "shift where drawing starts" rather than "redraw a different
    // subset of lines", mirroring how TextBox handles horizontal scroll.
    float firstVisibleY = innerRect.y - scrollOffsetRows * lineHeight;

    for (size_t i = 0; i < visualLines.size(); i++) {
        float y = firstVisibleY + static_cast<float>(i) * lineHeight;
        // Skip lines that fall entirely outside the visible rect — with
        // AutoGrow gone this rarely matters (a Fixed box only shows
        // visibleRows lines at a time anyway), but stays cheap insurance
        // against wastefully measuring/drawing rows that the scissor
        // region would just clip away regardless.
        if (y + lineHeight < innerRect.y ||
            y > innerRect.y + innerRect.height) {
            continue;
        }
        const VisualLine& line = visualLines[i];

        // Selection highlight for this line, drawn before its text so the
        // text renders on top. Only the portion of the selection that
        // falls within this line's own byte range is drawn — most lines
        // won't overlap the selection at all and skip this entirely.
        if (HasSelection()) {
            ByteRange selection = SelectionRange();
            size_t highlightStart = selection.start > line.startByte
                                        ? selection.start
                                        : line.startByte;
            size_t highlightEnd =
                selection.end < line.endByte ? selection.end : line.endByte;
            if (highlightStart < highlightEnd) {
                float highlightStartX =
                    innerRect.x - scrollOffsetXPx +
                    MeasureTextEx(
                        assets::cozette,
                        text.substr(
                                line.startByte, highlightStart - line.startByte
                        )
                            .c_str(),
                        assets::cozette.baseSize,
                        0
                    )
                        .x;
                float highlightEndX =
                    innerRect.x - scrollOffsetXPx +
                    MeasureTextEx(
                        assets::cozette,
                        text.substr(
                                line.startByte, highlightEnd - line.startByte
                        )
                            .c_str(),
                        assets::cozette.baseSize,
                        0
                    )
                        .x;
                DrawRectangle(
                    static_cast<int>(highlightStartX),
                    static_cast<int>(y),
                    static_cast<int>(highlightEndX - highlightStartX),
                    static_cast<int>(lineHeight),
                    BLUE_200
                );
            }
        }

        std::string lineText =
            text.substr(line.startByte, line.endByte - line.startByte);
        DrawTextEx(
            assets::cozette,
            lineText.c_str(),
            {innerRect.x - scrollOffsetXPx, y},
            lineHeight,
            0,
            NEUTRAL_600
        );
    }

    if (focused && fmodf(blinkTimer, kBlinkPeriod) < kBlinkPeriod / 2.0f &&
        !visualLines.empty()) {
        // Caret position is two independent lookups: which row
        // (VisualLineIndexForByte) and how far across that row's text the
        // caret's x position measures to (the MeasureTextEx call below) —
        // together they place the caret at exactly the same spot the text
        // itself was drawn at, since both use the same line's byte range
        // and the same scrollOffsetXPx-adjusted starting X.
        auto [lineIdx, byteInLine] = VisualLineIndexForByte(caretByteIndex);
        const VisualLine& line = visualLines[lineIdx];
        std::string beforeCaret = text.substr(line.startByte, byteInLine);
        float lineStartX = innerRect.x - scrollOffsetXPx;
        float caretX = lineStartX + MeasureTextEx(
                                        assets::cozette,
                                        beforeCaret.c_str(),
                                        assets::cozette.baseSize,
                                        0
                                    )
                                        .x;
        float caretY = firstVisibleY + static_cast<float>(lineIdx) * lineHeight;

        // otherwise the caret clips out
        if (caretX == lineStartX) {
            caretX += 1;
        }

        DrawLine(
            static_cast<int>(caretX),
            static_cast<int>(caretY),
            static_cast<int>(caretX),
            static_cast<int>(caretY + lineHeight),
            BLUE_500
        );
    }

    EndScissorMode();

    if (focused) {
        DrawRectangleLines(
            rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4, BLUE_500
        );
    }
}

}  // namespace ui
