#pragma once

#include <raylib.h>

#include <memory>
#include <string>
#include <vector>

#include "MenuPopup.h"
#include "Widget.h"

namespace UI {

/// One top-level title in a MenuBar (e.g. "File"), owning the MenuPopup
/// dropdown it opens. Drawn flat/borderless like a Button, highlighted on
/// hover or while its dropdown is open. Built via the Menu() tree-literal
/// factory (Tree.h), not constructed directly — MenuBar is what
/// orchestrates opening/closing/keyboard navigation across its
/// MenuBarItems; a lone MenuBarItem only knows how to toggle its own
/// popup.
class MenuBarItem : public Widget {
   public:
    MenuBarItem(std::string label, std::vector<std::unique_ptr<Widget>> items);

    const std::string& GetLabel() const { return label; }

    /// Opens this item's dropdown directly below its own computed rect.
    /// No-op if already open.
    void OpenPopup();
    void ClosePopup();
    bool IsPopupOpen() const { return popup->IsOpen(); }
    MenuPopup& Popup() { return *popup; }

    void Layout(const Rectangle& bounds) override;
    void Draw() const override;

   protected:
    float IntrinsicWidth() const override;
    float IntrinsicHeight() const override;

   private:
    std::string label;
    std::unique_ptr<MenuPopup> popup;

    static constexpr float kPaddingX = 8.0f;
    static constexpr float kPaddingY = 4.0f;
};

}  // namespace UI
