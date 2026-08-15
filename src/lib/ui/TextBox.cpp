#include "TextBox.h"

#include <cmath>

#include "../../assets.h"
#include "../../palette.h"
#include "Utf8.h"
#include "Utils.h"

namespace ui {

TextBox::TextBox(std::string initialText) : text(std::move(initialText)) {
    focusable = true;
    caretByteIndex = text.size();
}

void TextBox::SetText(std::string newText) {
    text = std::move(newText);
    caretByteIndex = text.size();
    Invalidate();
}

float TextBox::IntrinsicWidth() const {
    return kMinWidthChars *
               MeasureTextEx(assets::cozette, "M", assets::cozette.baseSize, 0)
                   .x +
           2 * kPaddingX;
}

float TextBox::IntrinsicHeight() const {
    return assets::cozette.baseSize + 2 * kPaddingY;
}

void TextBox::InsertCodepoint(const char* utf8Bytes, int byteCount) {
    // `text.insert` is a plain byte-buffer insert — safe here because
    // `utf8Bytes` is always exactly one already-encoded codepoint (1-4
    // bytes), and caretByteIndex is always codepoint-aligned, so we can't
    // split a multi-byte character in half.
    text.insert(caretByteIndex, utf8Bytes, byteCount);
    caretByteIndex += byteCount;
    blinkTimer = 0.0f;  // typing should always show a solid, visible caret
    Invalidate();       // width may have changed, so parent layout must redo
}

void TextBox::DeleteBackward() {
    if (caretByteIndex == 0) return;
    // Erase the whole codepoint behind the caret, not just one byte — a
    // naive caretByteIndex-1 would land mid-character for anything outside
    // ASCII (e.g. chop a 2-byte 'é' into a dangling continuation byte).
    size_t start = PrevCodepointBoundary(text, caretByteIndex);
    text.erase(start, caretByteIndex - start);
    caretByteIndex = start;
    blinkTimer = 0.0f;
    Invalidate();
}

void TextBox::DeleteForward() {
    if (caretByteIndex >= text.size()) return;
    // Same codepoint-at-a-time reasoning as DeleteBackward, just erasing
    // the codepoint ahead of the caret instead of behind it.
    size_t end = NextCodepointBoundary(text, caretByteIndex);
    text.erase(caretByteIndex, end - caretByteIndex);
    blinkTimer = 0.0f;
    Invalidate();
}

void TextBox::MoveCaret(int direction) {
    // direction is a sign, not a magnitude: Left always moves exactly one
    // codepoint left, Right one codepoint right, regardless of how many
    // UTF-8 bytes that codepoint takes.
    if (direction < 0) {
        caretByteIndex = PrevCodepointBoundary(text, caretByteIndex);
    } else if (direction > 0) {
        caretByteIndex = NextCodepointBoundary(text, caretByteIndex);
    }
    blinkTimer = 0.0f;
    // No Invalidate() here — moving the caret never changes the box's
    // measured size, so there's nothing for the parent layout to redo.
}

void TextBox::ProcessEvents() {
    Widget::ProcessEvents();  // click-to-focus, for free (see Widget.cpp's
                              // PollPointerEvents — click-to-focus and the
                              // click-to-place-caret logic just below are
                              // deliberately independent: a click both
                              // focuses AND repositions the caret in the
                              // same frame, even the very click that first
                              // focuses an unfocused box).

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(GetMousePosition(), GetComputedRect())) {
        PlaceCaretAtMouse(GetMousePosition());
    }

    // Everything below only makes sense while this box holds keyboard
    // focus — an unfocused box can still be clicked (handled above) but
    // shouldn't react to typing/editing keys meant for whichever widget
    // actually has focus.
    if (!focused) return;

    blinkTimer += GetFrameTime();

    // GetCharPressed() is a frame-scoped queue of already-decoded Unicode
    // codepoints (raylib/GLFW does the keyboard-layout-aware decoding for
    // us) — drain it completely so multiple characters typed in one frame
    // (e.g. fast typing at a low frame rate) all get inserted, not just the
    // first. CodepointToUTF8 re-encodes each one back into raw UTF-8 bytes
    // so it can be spliced into the byte-indexed `text` buffer.
    int codepoint;
    while ((codepoint = GetCharPressed()) != 0) {
        int byteCount = 0;
        const char* utf8 = CodepointToUTF8(codepoint, &byteCount);
        InsertCodepoint(utf8, byteCount);
    }

    // Each of these uses IsKeyRepeated rather than IsKeyPressed, so holding
    // the key down repeats the action after a short delay (see Widget.h)
    // instead of requiring a fresh physical press every time — standard OS
    // text-field behavior.
    if (IsKeyRepeated(KEY_BACKSPACE, backspaceHeldSeconds)) DeleteBackward();
    if (IsKeyRepeated(KEY_DELETE, deleteHeldSeconds)) DeleteForward();
    if (IsKeyRepeated(KEY_LEFT, leftHeldSeconds)) MoveCaret(-1);
    if (IsKeyRepeated(KEY_RIGHT, rightHeldSeconds)) MoveCaret(1);
    if (IsKeyRepeated(KEY_HOME, homeHeldSeconds)) {
        caretByteIndex = 0;
        blinkTimer = 0.0f;
    }
    if (IsKeyRepeated(KEY_END, endHeldSeconds)) {
        caretByteIndex = text.size();
        blinkTimer = 0.0f;
    }
}

void TextBox::ScrollToKeepCaretVisible() const {
    Rectangle rect = GetComputedRect();
    float innerWidth = rect.width - 2 * kPaddingX;

    // caretX is the caret's x position measured from the *start of the
    // text*, in an unscrolled/infinite-width coordinate space — i.e.
    // "how many pixels of text are to the left of the caret". Comparing
    // this against the current scrollOffsetPx (how many of those pixels
    // are currently scrolled out of view to the left) tells us whether the
    // caret is still inside the visible window [scrollOffsetPx,
    // scrollOffsetPx + innerWidth].
    std::string beforeCaret = text.substr(0, caretByteIndex);
    float caretX =
        MeasureTextEx(
            assets::cozette, beforeCaret.c_str(), assets::cozette.baseSize, 0
        )
            .x;

    // Caret walked off the right edge of the visible window: scroll right
    // just enough to bring it flush with the right edge.
    if (caretX - scrollOffsetPx > innerWidth) {
        scrollOffsetPx = caretX - innerWidth;
    }
    // Caret walked off the left edge (or behind it, e.g. after Home/
    // Backspace-ing near the start): scroll left just enough to bring it
    // flush with the left edge. Note this is a second, independent `if`
    // rather than `else if` — after the block above runs, this can still
    // trigger (e.g. right after typing pushed scrollOffsetPx forward, then
    // Home yanks the caret back to position 0), so both directions must be
    // checked freshly against the same caretX.
    if (caretX - scrollOffsetPx < 0) {
        scrollOffsetPx = caretX;
    }
    // Guard against ever scrolling past the very start of the text (can
    // only happen from floating-point slop, but cheap to clamp).
    if (scrollOffsetPx < 0) scrollOffsetPx = 0;
}

void TextBox::PlaceCaretAtMouse(Vector2 mousePosition) {
    Rectangle rect = GetComputedRect();
    // Convert the click from screen space into the same "pixels from start
    // of text" coordinate space ScrollToKeepCaretVisible/Draw use: subtract
    // the box's left edge and padding, then add back the current scroll
    // offset (since the visible text has been shifted left by that much).
    float localX = mousePosition.x - (rect.x + kPaddingX) + scrollOffsetPx;

    // Walk every codepoint boundary left-to-right, tracking the cumulative
    // rendered width up to each one, and remember whichever boundary's
    // position is closest to the click. This correctly picks "before" vs.
    // "after" a character based on which half of its glyph was clicked
    // (clicking the left half of a wide glyph lands the caret before it,
    // the right half lands after), without needing per-glyph metrics
    // beyond what MeasureTextEx already gives us. O(n) per click, which is
    // fine for a single-line, human-typed-length field.
    size_t bestIndex = 0;
    float bestDistance = fabsf(localX);  // distance if caret goes to index 0
    float cumulativeWidth = 0.0f;
    size_t pos = 0;
    while (pos < text.size()) {
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
}

void TextBox::Draw() const {
    Rectangle rect = GetComputedRect();

    // Same chrome (fill/border) as Button, for visual consistency across
    // the toolkit's interactive widgets.
    ui::DrawRectWithBorderAndShadow(rect, WHITE, NEUTRAL_600, 1);

    // Recompute scrollOffsetPx *before* drawing, since Draw() is where the
    // box's current computedRect and caret position are both known — this
    // is the only place both are guaranteed fresh in the same frame.
    ScrollToKeepCaretVisible();

    Rectangle innerRect{
        rect.x + kPaddingX,
        rect.y + kPaddingY,
        rect.width - 2 * kPaddingX,
        rect.height - 2 * kPaddingY
    };

    // Clip text/caret drawing to the padded interior so a scrolled-off (or
    // simply too-long) string can never paint outside the box's border,
    // regardless of how far scrollOffsetPx has shifted it.
    BeginScissorMode(
        static_cast<int>(innerRect.x),
        static_cast<int>(innerRect.y),
        static_cast<int>(innerRect.width),
        static_cast<int>(innerRect.height)
    );

    // Shifting the draw position left by scrollOffsetPx (rather than, say,
    // shifting the text itself) is what makes scrolling "work" — the full
    // string is always drawn in full, just starting further off the left
    // edge of the scissor region as scrollOffsetPx grows.
    float textX = innerRect.x - scrollOffsetPx;
    // Drawn with raw DrawTextEx rather than ui::DrawText: the latter's
    // color param is ignored and it always bakes in a drop-shadow offset,
    // which reads poorly next to a thin caret line.
    DrawTextEx(
        assets::cozette,
        text.c_str(),
        {textX, innerRect.y},
        assets::cozette.baseSize,
        0,
        NEUTRAL_600
    );

    // Blink: solid for the first half of each kBlinkPeriod-second cycle,
    // hidden for the second half. blinkTimer is reset to 0 on every edit/
    // caret-move (see the mutator methods above), so the caret always
    // starts a fresh, fully-visible cycle right after you interact with the
    // box rather than potentially blinking off immediately.
    if (focused && fmodf(blinkTimer, kBlinkPeriod) < kBlinkPeriod / 2.0f) {
        std::string beforeCaret = text.substr(0, caretByteIndex);
        float caretX = textX + MeasureTextEx(
                                   assets::cozette,
                                   beforeCaret.c_str(),
                                   assets::cozette.baseSize,
                                   0
                               )
                                   .x;

        // otherwise the caret clips out
        if (caretX == textX) {
            caretX += 1;
        }

        DrawLine(
            static_cast<int>(caretX),
            static_cast<int>(innerRect.y),
            static_cast<int>(caretX),
            static_cast<int>(innerRect.y + innerRect.height),
            BLUE_500
        );
    }

    EndScissorMode();

    // Focus ring drawn last, outside the scissor region, so it's never
    // itself clipped — it's meant to outline the whole box, not just its
    // padded interior. Same BLUE_500 2px-outset ring Button uses, for
    // visual consistency between focusable widgets.
    if (focused) {
        DrawRectangleLines(
            rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4, BLUE_500
        );
    }
}

}  // namespace ui
