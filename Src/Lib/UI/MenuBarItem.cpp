#include "MenuBarItem.h"

#include "../../Assets.h"
#include "../../Palette.h"
#include "Utils.h"

namespace UI {

MenuBarItem::MenuBarItem(
    std::string label, std::vector<std::unique_ptr<Widget>> items
)
    : label(std::move(label)), popup(std::make_unique<MenuPopup>(std::move(items))) {
    focusable = true;
}

void MenuBarItem::OpenPopup() {
    if (popup->IsOpen()) return;
    popup->Open({computedRect.x, computedRect.y + computedRect.height});
}

void MenuBarItem::ClosePopup() { popup->Close(); }

float MenuBarItem::IntrinsicWidth() const {
    return MeasureTextEx(
               Assets::cozette, label.c_str(), Assets::cozette.baseSize, 0
           )
               .x +
           2 * kPaddingX;
}

float MenuBarItem::IntrinsicHeight() const {
    return Assets::cozette.baseSize + 2 * kPaddingY;
}

void MenuBarItem::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
    // Keep an already-open popup glued directly under this item as it
    // moves/resizes (e.g. its owning window being dragged).
    if (popup->IsOpen()) {
        popup->Open({computedRect.x, computedRect.y + computedRect.height});
    }
}

void MenuBarItem::Draw() const {
    bool open = popup->IsOpen();

    // Open gets the solid "active" fill; a plain mouse-hover (not open)
    // gets a subtler tint instead of the same fill — otherwise hovering
    // near an item with the mouse looks identical to it being open, and
    // (since the focus ring below was also always blue) can be mistaken
    // for keyboard focus too.
    if (open) {
        DrawRectangleRec(computedRect, BLUE_500);
    } else if (wasHovered) {
        DrawRectangleRec(computedRect, ZINC_200);
    }

    Color textColor = open ? ZINC_100 : NEUTRAL_600;
    UI::DrawTextWithShadow(
        label.c_str(), computedRect.x + kPaddingX, computedRect.y + kPaddingY,
        textColor
    );

    if (focused) {
        // Against the solid blue "open" fill a blue ring all but
        // disappears — swap to a light ring there so keyboard focus stays
        // visible regardless of open/hover state.
        Color ringColor = open ? ZINC_100 : BLUE_500;
        DrawRectangleLines(
            computedRect.x - 2, computedRect.y - 2, computedRect.width + 4,
            computedRect.height + 4, ringColor
        );
    }
}

}  // namespace UI
