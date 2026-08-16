#pragma once

#include <raylib.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "Container.h"
#include "LayerStacker.h"
#include "Widget.h"

namespace UI {

/// The floating dropdown panel a MenuBarItem opens. Not a Widget itself —
/// architecturally a sibling of WM::Window (WindowManager.cpp): owns a
/// Container (its items, laid out as a Column) and drives that Container's
/// Layout/ProcessEvents/Draw manually rather than through a parent's flow
/// layout, exactly like a Window drives its content. Registers/
/// unregisters itself with GlobalLayerStacker() under Layer::Menu on
/// Open()/Close() (rather than through Widget::RegisterLayer, which is a
/// register-once helper unsuited to a panel that repeatedly opens and
/// closes) so it always draws above, and exclusively owns clicks over,
/// whatever it happens to overlap.
class MenuPopup : public LayerStacker::Drawable {
   public:
    /// Takes ownership of `items` (MenuItem/MenuSeparator widgets) as this
    /// popup's content, laid out top-to-bottom.
    explicit MenuPopup(std::vector<std::unique_ptr<Widget>> items);

    /// Opens (or repositions, if already open) the panel with its
    /// top-left corner at `topLeft`, sized to fit its items. Registers
    /// with GlobalLayerStacker() if not already registered.
    void Open(Vector2 topLeft);
    /// Closes the panel, unregistering it from GlobalLayerStacker(). No-op
    /// if already closed.
    void Close();
    bool IsOpen() const { return layerToken.has_value(); }

    /// Polls input against the panel's items. No-op while closed. Must be
    /// called once per frame by whatever owns this popup (MenuBarItem/
    /// MenuBar) — there's no generic per-frame dispatch for LayerStacker
    /// items beyond DrawAll()'s painting, same as a Window's content isn't
    /// driven through LayerStacker for events either.
    void ProcessEvents();

    /// The panel's current screen-space rect, valid once open (updated by
    /// Open()/ProcessEvents()/Draw(), all of which keep it in sync with
    /// its anchor point and item list).
    Rectangle Bounds() const { return bounds; }
    /// The item list, e.g. for MenuBar's Up/Down navigation.
    Container& Panel() { return *panel; }

    /// Fired when a mouse click selects one of this popup's items (see the
    /// constructor's onClick wiring). Deliberately doesn't close the popup
    /// itself — only the owning MenuBarItem/MenuStrip knows how to do that
    /// while also restoring keyboard focus to the MenuBarItem (see
    /// ClosePreservingFocus in MenuBar.cpp), which a self-inflicted Close()
    /// here would bypass entirely.
    void SetOnItemSelected(std::function<void()> callback) {
        onItemSelected = std::move(callback);
    }

    void Draw() const override;

   private:
    std::unique_ptr<Container> panel;
    std::optional<LayerStacker::ItemId> layerToken;
    Vector2 anchor{};
    std::function<void()> onItemSelected;
    /// Derived from anchor + the panel's current intrinsic size — mutable
    /// so Draw() const can keep it (and the panel's own Layout()) in sync
    /// on every call, same rationale as Container's own mutable scroll
    /// state.
    mutable Rectangle bounds{};

    /// Recomputes `bounds` from the current anchor and the panel's
    /// intrinsic size, and lays the panel out to match. Called from
    /// Open()/ProcessEvents()/Draw() so bounds/hit-testing/painting can
    /// never disagree even if item content changes while open.
    void Relayout() const;
};

}  // namespace UI
