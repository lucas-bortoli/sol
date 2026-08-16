#pragma once

#include <raylib.h>

#include <memory>
#include <vector>

#include "Container.h"
#include "MenuBarItem.h"
#include "Widget.h"

namespace UI {

/// A horizontal bar of top-level MenuBarItems. Named MenuStrip rather than
/// MenuBar so it doesn't hide the MenuBar() tree-literal factory from
/// unqualified lookup (a function and a class sharing a name in the same
/// scope makes the class inaccessible via unqualified lookup afterward —
/// same reasoning as Spacer/Space(), see Tree.h) — built via MenuBar()/
/// Menu() in Tree.h, not constructed directly. Follows Win32 menu-bar
/// conventions: only one dropdown open at a time, hovering a different
/// top-level item while one is open switches to it, and — once keyboard
/// focus is somewhere inside the bar or its open dropdown — Left/Right
/// hop between top-level items, Up/Down move within an open dropdown, and
/// Escape closes it and returns focus to its MenuBarItem. Tab reaches
/// MenuBarItems like any other focusable widget; Enter/Space's
/// open/select behavior comes from each item's own onActivate, already
/// handled by the ordinary Widget::ProcessKeyboardFocus() machinery — this
/// class only adds the directional/Escape handling that doesn't have.
class MenuStrip : public Container {
   public:
    explicit MenuStrip(std::vector<std::unique_ptr<Widget>> menus);

    void ProcessEvents() override;

   private:
    /// The MenuBarItem whose dropdown is currently open, or nullptr.
    MenuBarItem* openItem = nullptr;
    /// Set by HandleKeyboard() when Enter/Space selects an item inside the
    /// open popup, and acted on at the very start of the *next*
    /// ProcessEvents() call rather than immediately. Closing (and
    /// restoring focus to the MenuBarItem) right away would happen before
    /// Widget::ProcessKeyboardFocus() — called after this whole
    /// ProcessEvents() — has fired the selected MenuItem's own onActivate,
    /// so by the time it ran, focus would already have moved off that item
    /// and onto the MenuBarItem instead, firing *its* onActivate (which
    /// just reopens the popup) rather than the item's actual command.
    /// Deferring by one frame (imperceptible — the popup was about to
    /// close anyway) lets the item's onActivate fire first, undisturbed.
    bool closeOnNextFrame = false;
    /// Which top-level MenuBarItem was directly focused as of the last
    /// ProcessEvents() call (nullptr if none was). Used to detect a fresh
    /// Tab-driven focus *transition* onto a different item — as opposed to
    /// a merely-persistent mismatch between focus and openItem, which
    /// happens legitimately whenever the mouse hover-switches openItem
    /// without moving focus (see switchedByHover in ProcessEvents()).
    /// Reacting to the mismatch itself instead of the transition would
    /// fight hover-switch forever: it changes openItem, next frame this
    /// would "correct" it back since focus never moved, which lets
    /// hover-switch fire again the frame after — an infinite flicker.
    MenuBarItem* lastFocusedTopLevel = nullptr;
    /// The mouse position as of the last ProcessEvents() call, used to
    /// gate hover-switch on the mouse actually having moved this frame
    /// (see mouseMoved in ProcessEvents()) rather than merely resting over
    /// a different item — a stationary mouse must never fight a
    /// Left/Right-driven change of `openItem` back to whatever it happens
    /// to be sitting on.
    Vector2 lastMousePosition{};
    /// False until the first ProcessEvents() call, so that first call
    /// always treats the mouse as having "moved" rather than comparing
    /// against a meaningless default-constructed lastMousePosition.
    bool hasLastMousePosition = false;

    void ToggleItem(MenuBarItem* item);
    void OpenItem(MenuBarItem* item);
    void CloseOpenItem();

    /// This bar's direct children, as MenuBarItem* (every child is one —
    /// enforced by construction via the Menu()/MenuBar() factories).
    std::vector<MenuBarItem*> Items() const;
    /// Whether the globally-focused widget is one of `items`, or (if
    /// `openItem` is set) one of its open dropdown's items.
    bool OwnsFocus(const std::vector<MenuBarItem*>& items) const;

    void HandleKeyboard(const std::vector<MenuBarItem*>& items);
};

}  // namespace UI
