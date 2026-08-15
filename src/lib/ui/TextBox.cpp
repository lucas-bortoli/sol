#include "TextBox.h"

#include <cmath>

#include "../../assets.h"
#include "../../palette.h"
#include "Utils.h"

namespace ui {

namespace {

/// Byte index one codepoint right of `pos`, clamped to text.size().
size_t NextCodepointBoundary(const std::string& text, size_t pos) {
    if (pos >= text.size()) return text.size();
    int codepointSize = 0;
    GetCodepointNext(text.c_str() + pos, &codepointSize);
    return pos + static_cast<size_t>(codepointSize);
}

/// Byte index one codepoint left of `pos`, clamped to 0. Assumes `pos` is
/// already codepoint-aligned (always true — see caretByteIndex's invariant
/// in TextBox.h).
size_t PrevCodepointBoundary(const std::string& text, size_t pos) {
    if (pos == 0) return 0;
    int codepointSize = 0;
    GetCodepointPrevious(text.c_str() + pos, &codepointSize);
    return pos - static_cast<size_t>(codepointSize);
}

}  // namespace

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
    text.insert(caretByteIndex, utf8Bytes, byteCount);
    caretByteIndex += byteCount;
    blinkTimer = 0.0f;
    Invalidate();
}

void TextBox::DeleteBackward() {
    if (caretByteIndex == 0) return;
    size_t start = PrevCodepointBoundary(text, caretByteIndex);
    text.erase(start, caretByteIndex - start);
    caretByteIndex = start;
    blinkTimer = 0.0f;
    Invalidate();
}

void TextBox::DeleteForward() {
    if (caretByteIndex >= text.size()) return;
    size_t end = NextCodepointBoundary(text, caretByteIndex);
    text.erase(caretByteIndex, end - caretByteIndex);
    blinkTimer = 0.0f;
    Invalidate();
}

void TextBox::MoveCaret(int direction) {
    if (direction < 0) {
        caretByteIndex = PrevCodepointBoundary(text, caretByteIndex);
    } else if (direction > 0) {
        caretByteIndex = NextCodepointBoundary(text, caretByteIndex);
    }
    blinkTimer = 0.0f;
}

void TextBox::ProcessEvents() {
    Widget::ProcessEvents();  // click-to-focus, for free
    if (!focused) return;

    blinkTimer += GetFrameTime();

    int codepoint;
    while ((codepoint = GetCharPressed()) != 0) {
        int byteCount = 0;
        const char* utf8 = CodepointToUTF8(codepoint, &byteCount);
        InsertCodepoint(utf8, byteCount);
    }

    if (IsKeyPressed(KEY_BACKSPACE)) DeleteBackward();
    if (IsKeyPressed(KEY_DELETE)) DeleteForward();
    if (IsKeyPressed(KEY_LEFT)) MoveCaret(-1);
    if (IsKeyPressed(KEY_RIGHT)) MoveCaret(1);
    if (IsKeyPressed(KEY_HOME)) {
        caretByteIndex = 0;
        blinkTimer = 0.0f;
    }
    if (IsKeyPressed(KEY_END)) {
        caretByteIndex = text.size();
        blinkTimer = 0.0f;
    }
}

void TextBox::ScrollToKeepCaretVisible() const {
    Rectangle rect = GetComputedRect();
    float innerWidth = rect.width - 2 * kPaddingX;

    std::string beforeCaret = text.substr(0, caretByteIndex);
    float caretX =
        MeasureTextEx(
            assets::cozette, beforeCaret.c_str(), assets::cozette.baseSize, 0
        )
            .x;

    if (caretX - scrollOffsetPx > innerWidth) {
        scrollOffsetPx = caretX - innerWidth;
    }
    if (caretX - scrollOffsetPx < 0) {
        scrollOffsetPx = caretX;
    }
    if (scrollOffsetPx < 0) scrollOffsetPx = 0;
}

void TextBox::Draw() const {
    Rectangle rect = GetComputedRect();

    ui::DrawRectWithBorderAndShadow(rect, WHITE, NEUTRAL_600, 1);

    ScrollToKeepCaretVisible();

    Rectangle innerRect{
        rect.x + kPaddingX,
        rect.y + kPaddingY,
        rect.width - 2 * kPaddingX,
        rect.height - 2 * kPaddingY
    };

    BeginScissorMode(
        static_cast<int>(innerRect.x),
        static_cast<int>(innerRect.y),
        static_cast<int>(innerRect.width),
        static_cast<int>(innerRect.height)
    );

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
        NEUTRAL_200
    );

    if (focused && fmodf(blinkTimer, kBlinkPeriod) < kBlinkPeriod / 2.0f) {
        std::string beforeCaret = text.substr(0, caretByteIndex);
        float caretX = textX + MeasureTextEx(
                                   assets::cozette,
                                   beforeCaret.c_str(),
                                   assets::cozette.baseSize,
                                   0
                               )
                                   .x;
        DrawLine(
            static_cast<int>(caretX),
            static_cast<int>(innerRect.y),
            static_cast<int>(caretX),
            static_cast<int>(innerRect.y + innerRect.height),
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
