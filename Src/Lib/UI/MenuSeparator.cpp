#include "MenuSeparator.h"

#include "../../Palette.h"

namespace UI {

float MenuSeparator::IntrinsicHeight() const { return 1.0f + 2 * kPaddingY; }

void MenuSeparator::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
}

// Never hovered/pressed/clicked — no pointer state to poll.
void MenuSeparator::ProcessEvents() {}

void MenuSeparator::Draw() const {
    float y = computedRect.y + kPaddingY;
    DrawLine(
        static_cast<int>(computedRect.x), static_cast<int>(y),
        static_cast<int>(computedRect.x + computedRect.width),
        static_cast<int>(y), NEUTRAL_300
    );
}

}  // namespace UI
