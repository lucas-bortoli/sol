#include "Button.h"

#include "../../assets.h"
#include "../../palette.h"
#include "Utils.h"

namespace ui {

Button::Button(std::string initialText) : text(std::move(initialText)) {
    focusable = true;
}

void Button::SetText(std::string newText) {
    text = std::move(newText);
    Invalidate();
}

float Button::IntrinsicWidth() const {
    return MeasureTextEx(
               assets::cozette, text.c_str(), assets::cozette.baseSize, 0
    )
               .x +
           2 * kPaddingX;
}

float Button::IntrinsicHeight() const {
    return assets::cozette.baseSize + 2 * kPaddingY;
}

void Button::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
}

void Button::Draw() const {
    auto computedRect = this->GetComputedRect();

    bool isBeingClicked = pointerDown || keyDown;

    Rectangle drawnRect = computedRect;

    if (isBeingClicked) {
        drawnRect.x += 1;
        drawnRect.y += 1;

        ui::DrawRectWithBorderAndShadow(drawnRect, WHITE, NEUTRAL_600, 0);
        ui::DrawText(
            text.c_str(), drawnRect.x + kPaddingX, drawnRect.y + kPaddingY,
            NEUTRAL_200
        );
    } else {
        ui::DrawRectWithBorderAndShadow(drawnRect, WHITE, NEUTRAL_600, 1);
        ui::DrawText(
            text.c_str(), drawnRect.x + kPaddingX, drawnRect.y + kPaddingY,
            NEUTRAL_200
        );
    }

    if (focused) {
        DrawRectangleLines(
            drawnRect.x - 2, drawnRect.y - 2, drawnRect.width + 4,
            drawnRect.height + 4, BLUE_500
        );
    }
}

}  // namespace ui
