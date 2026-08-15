#include "Label.h"

#include "../../assets.h"
#include "../../palette.h"
#include "Utils.h"

namespace ui {

Label::Label(std::string initialText) : text(std::move(initialText)) {}

void Label::SetText(std::string newText) {
    text = std::move(newText);
    Invalidate();
}

float Label::IntrinsicWidth() const {
    return MeasureTextEx(
               assets::cozette, text.c_str(), assets::cozette.baseSize, 0
    )
        .x;
}

float Label::IntrinsicHeight() const {
    return assets::cozette.baseSize;
}

void Label::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
}

void Label::Draw() const {
    ui::DrawText(text.c_str(), computedRect.x, computedRect.y, NEUTRAL_200);
}

}  // namespace ui
