#pragma once

#include <raylib.h>

#include <optional>
#include <string>

#include "Widget.h"

namespace UI {

/// A single row inside a UI::MenuPopup dropdown: a text label with an
/// optional leading 16x16 icon, clickable/focusable unless disabled. Use
/// SetOnActivate() (inherited from Widget) for its action — fires on
/// click and on keyboard Enter/Space, same as any other focusable widget.
/// SetOnClick() is reserved: MenuPopup wires it internally to close the
/// popup after a mouse-selected item (Enter/Space selection is closed by
/// MenuBar instead, watching for activation while focus is inside an open
/// popup) — prefer OnActivate() for an item's actual action.
class MenuItem : public Widget {
   public:
    explicit MenuItem(std::string text);

    void SetText(std::string newText);
    /// Sets or clears (via std::nullopt) the leading icon. Every icon is
    /// drawn at a fixed 16x16, matching this toolkit's other 16px icon
    /// usage (see Assets.h) — pass a differently-sized texture and it'll
    /// still be drawn at 16x16, just stretched.
    void SetIcon(std::optional<Texture2D> newIcon);
    /// A disabled item ignores hover/press/click entirely (ProcessEvents()
    /// becomes a no-op — it never fires onClick/onActivate), isn't
    /// focusable (so Tab and menu arrow-key navigation skip it), and
    /// draws dimmed.
    void SetDisabled(bool newDisabled);
    bool IsDisabled() const { return disabled; }

    void Layout(const Rectangle& bounds) override;
    void ProcessEvents() override;
    void Draw() const override;

   protected:
    float IntrinsicWidth() const override;
    float IntrinsicHeight() const override;

   private:
    std::string text;
    std::optional<Texture2D> icon;
    bool disabled = false;

    static constexpr float kPaddingX = 8.0f;
    static constexpr float kPaddingY = 4.0f;
    static constexpr float kIconSize = 16.0f;
    static constexpr float kIconGap = 6.0f;
};

}  // namespace UI
