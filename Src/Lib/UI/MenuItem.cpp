#include "MenuItem.h"

#include "../../Assets.h"
#include "../../Palette.h"
#include "Utils.h"

namespace UI {

MenuItem::MenuItem(std::string text) : text(std::move(text)) {
    focusable = true;
}

void MenuItem::SetText(std::string newText) {
    text = std::move(newText);
    Invalidate();
}

void MenuItem::SetIcon(std::optional<Texture2D> newIcon) {
    icon = newIcon;
    Invalidate();
}

void MenuItem::SetDisabled(bool newDisabled) {
    if (disabled == newDisabled) return;
    disabled = newDisabled;
    focusable = !disabled;
    Invalidate();
}

float MenuItem::IntrinsicWidth() const {
    return MeasureTextEx(
               Assets::cozette, text.c_str(), Assets::cozette.baseSize, 0
           )
               .x +
           2 * kPaddingX + kIconSize + kIconGap;
}

float MenuItem::IntrinsicHeight() const {
    float contentHeight =
        std::max(static_cast<float>(Assets::cozette.baseSize), kIconSize);
    return contentHeight + 2 * kPaddingY;
}

void MenuItem::Layout(const Rectangle& bounds) {
    computedRect = bounds;
    layoutDirty = false;
}

void MenuItem::ProcessEvents() {
    // Disabled items never hover/press/click — skip PollPointerEvents
    // entirely rather than special-casing it internally.
    if (!disabled) Widget::ProcessEvents();
}

void MenuItem::Draw() const {
    bool highlighted = !disabled && (wasHovered || focused);

    if (highlighted) {
        DrawRectangleRec(computedRect, BLUE_500);
    }

    Color textColor = disabled ? NEUTRAL_400 : (highlighted ? ZINC_100 : NEUTRAL_600);

    // The icon slot is always reserved (see IntrinsicWidth/Height) even
    // when this item has no icon, so text stays aligned across every item
    // in a menu regardless of which ones actually have one.
    if (icon) {
        float iconY = computedRect.y + (computedRect.height - kIconSize) / 2.0f;
        Color tint = disabled ? Fade(WHITE, 0.4f) : WHITE;
        DrawTextureEx(
            *icon, {computedRect.x + kPaddingX, iconY},
            0.0f, kIconSize / static_cast<float>(icon->width), tint
        );
    }
    float textX = computedRect.x + kPaddingX + kIconSize + kIconGap;

    float textY = computedRect.y +
                   (computedRect.height - Assets::cozette.baseSize) / 2.0f;
    UI::DrawTextWithShadow(text.c_str(), textX, textY, textColor);
}

}  // namespace UI
