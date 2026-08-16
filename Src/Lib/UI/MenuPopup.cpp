#include "MenuPopup.h"

#include "../../Palette.h"
#include "Utils.h"

namespace UI {

MenuPopup::MenuPopup(std::vector<std::unique_ptr<Widget>> items)
    : panel(std::make_unique<Container>(
          Direction::Column, Justify::Start, Align::Stretch, 0.0f,
          Padding{2, 2, 2, 2}, Overflow::Visible, std::nullopt,
          std::move(items)
      )) {
    // Mouse-selecting any item reports it via onItemSelected rather than
    // closing the popup directly — only the owning MenuBarItem/MenuStrip
    // can close it while also restoring keyboard focus correctly (see
    // SetOnItemSelected's doc-comment). SetOnClick() is inert for a
    // MenuSeparator (never processes pointer input) and for a disabled
    // MenuItem (ProcessEvents() skips PollPointerEvents entirely), so
    // wiring it unconditionally on every child is safe. Keyboard
    // (Enter/Space) selection is closed by MenuBar instead, since onClick
    // never fires from the keyboard path.
    for (Widget* child : panel->Children()) {
        child->SetOnClick([this] {
            if (onItemSelected) onItemSelected();
        });
    }
}

void MenuPopup::Relayout() const {
    bounds = {
        anchor.x, anchor.y, panel->GetIntrinsicWidth(),
        panel->GetIntrinsicHeight()
    };
    panel->Layout(bounds);
    if (layerToken) GlobalLayerStacker().SetBounds(*layerToken, bounds);
}

void MenuPopup::Open(Vector2 topLeft) {
    anchor = topLeft;
    if (!layerToken) {
        layerToken = GlobalLayerStacker().Register(Layer::Menu, *this);
    }
    Relayout();
}

void MenuPopup::Close() {
    if (!layerToken) return;
    GlobalLayerStacker().Unregister(*layerToken);
    layerToken = std::nullopt;
}

void MenuPopup::ProcessEvents() {
    if (!layerToken) return;
    Relayout();
    panel->ProcessEvents();
}

void MenuPopup::Draw() const {
    if (!layerToken) return;
    Relayout();
    UI::DrawRectWithBorderAndShadow(bounds, ZINC_100, NEUTRAL_600, 2);
    panel->Draw();
}

}  // namespace UI
