#include "Label.h"

#include "../../Assets.h"
#include "../../Palette.h"
#include "Utils.h"

namespace UI {

Label::Label(std::string initialText) : text(std::move(initialText)) {}

void Label::SetText(std::string newText) {
    text = std::move(newText);
    Invalidate();
}

float Label::IntrinsicWidth() const {
    return MeasureTextEx(
               Assets::cozette, text.c_str(), Assets::cozette.baseSize, 0
    )
        .x;
}

float Label::IntrinsicHeight() const { return Assets::cozette.baseSize; }

void Label::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
}

void Label::Draw() const {
    UI::DrawTextWithShadow(
        text.c_str(), computedRect.x, computedRect.y, NEUTRAL_600
    );
}

}  // namespace UI
