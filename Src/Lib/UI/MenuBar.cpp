#include "MenuBar.h"

#include <algorithm>

#include "Input.h"

namespace UI {

namespace {
/// Closes `item`'s popup, restoring keyboard focus to `item` itself if
/// focus was currently on it or on one of its popup's items — otherwise
/// closing would leave the global focus pointer on a MenuItem that lives
/// in MenuPopup's own private Container (not part of the window's real
/// widget tree), which Widget::ProcessKeyboardFocus's Tab-cycle can never
/// reach again, silently breaking Tab from then on. Shared by every path
/// that can close a popup — CloseOpenItem() (Escape, Enter/Space
/// selection, click-outside) and OpenItem()'s close-the-previous-item
/// step (hover-switch) — so none of them has to remember this on its own.
void ClosePreservingFocus(MenuBarItem& item) {
    bool focusWasInside = item.IsFocused();
    if (!focusWasInside) {
        for (Widget* child : item.Popup().Panel().Children()) {
            if (child->IsFocused()) {
                focusWasInside = true;
                break;
            }
        }
    }
    item.ClosePopup();
    if (focusWasInside) item.Focus();
}
}  // namespace

MenuStrip::MenuStrip(std::vector<std::unique_ptr<Widget>> menus)
    : Container(
          Direction::Row,
          Justify::Start,
          Align::Stretch,
          0.0f,
          Padding{},
          Overflow::Visible,
          std::nullopt,
          std::move(menus)
      ) {
    for (MenuBarItem* item : Items()) {
        item->SetOnActivate([this, item] { ToggleItem(item); });
        // Mouse-selecting one of this item's popup items closes it through
        // the same CloseOpenItem()/ClosePreservingFocus() path as keyboard
        // selection, so focus lands back on the MenuBarItem instead of
        // being left stranded on a MenuItem inside a now-closed popup (see
        // MenuPopup::SetOnItemSelected's doc-comment).
        item->Popup().SetOnItemSelected([this, item] {
            if (openItem == item) CloseOpenItem();
        });
    }
    this->SetShrink(0);
}

std::vector<MenuBarItem*> MenuStrip::Items() const {
    std::vector<MenuBarItem*> items;
    for (Widget* child : Children()) {
        items.push_back(static_cast<MenuBarItem*>(child));
    }
    return items;
}

void MenuStrip::ToggleItem(MenuBarItem* item) {
    if (openItem == item) {
        CloseOpenItem();
    } else {
        OpenItem(item);
    }
}

void MenuStrip::OpenItem(MenuBarItem* item) {
    if (openItem && openItem != item) ClosePreservingFocus(*openItem);
    item->OpenPopup();
    openItem = item;
    // Deliberately doesn't move focus into the popup's items — this path
    // is shared by mouse clicks and keyboard Enter/Space alike (both fire
    // through the same onActivate), and a mouse-opened menu shouldn't
    // show a keyboard-selection highlight nobody asked for. Focus only
    // moves into an item once an actual navigation key (Up/Down) is
    // pressed — see HandleKeyboard()'s explicit focus-on-Down.
}

void MenuStrip::CloseOpenItem() {
    if (!openItem) return;
    ClosePreservingFocus(*openItem);
    openItem = nullptr;
}

bool MenuStrip::OwnsFocus(const std::vector<MenuBarItem*>& items) const {
    for (MenuBarItem* item : items) {
        if (item->IsFocused()) return true;
    }
    if (openItem) {
        for (Widget* child : openItem->Popup().Panel().Children()) {
            if (child->IsFocused()) return true;
        }
    }
    return false;
}

void MenuStrip::ProcessEvents() {
    // See closeOnNextFrame's doc-comment: this is last frame's
    // Enter/Space selection catching up, run before anything else this
    // frame so it's already closed by the time this frame's own input is
    // processed below.
    if (closeOnNextFrame) {
        closeOnNextFrame = false;
        CloseOpenItem();
    }

    Container::ProcessEvents();

    std::vector<MenuBarItem*> items = Items();
    InputSource& input = CurrentInput();
    Vector2 mouse = input.GetMousePosition();

    // Hover-switch while engaged: hovering a different top-level item
    // takes over from the currently-open one (classic Win32 "engaged menu
    // bar" behavior). Deliberately doesn't move keyboard focus — tracked
    // separately so the Tab-follow check below doesn't then "correct" this
    // back by seeing keyboard focus still on the old item.
    bool switchedByHover = false;
    if (openItem) {
        for (MenuBarItem* item : items) {
            if (item != openItem && CheckCollisionPointRec(mouse, item->GetComputedRect())) {
                OpenItem(item);
                switchedByHover = true;
                break;
            }
        }
    }

    if (openItem) {
        // An open popup is always topmost over its own window wherever it
        // visually sits (Layer::Menu unconditionally beats Layer::Windows
        // in LayerStacker::TopmostAt) — but WM's per-window occlusion
        // check has no idea this popup exists; it only sees "the mouse is
        // somewhere my window isn't topmost" and suppresses this whole
        // content->ProcessEvents() call, which is where this nested call
        // happens to live. Lift that suppression just for the popup's own
        // processing, then restore it for whatever runs after.
        bool wasSuppressed = UI::internal::IsPointerEventsSuppressed();
        UI::internal::SetPointerEventsSuppressed(false);
        openItem->Popup().ProcessEvents();
        UI::internal::SetPointerEventsSuppressed(wasSuppressed);
    }

    // Click-outside-to-close: a fresh press landing on neither the open
    // item itself nor its popup dismisses it. A click on some other,
    // unrelated widget still reaches that widget normally — the popup
    // just doesn't cover it there, so no extra suppression is needed.
    if (openItem && input.IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool insideItem = CheckCollisionPointRec(mouse, openItem->GetComputedRect());
        bool insidePopup = CheckCollisionPointRec(mouse, openItem->Popup().Bounds());
        if (!insideItem && !insidePopup) CloseOpenItem();
    }

    auto focusedTopLevelIt = std::find_if(items.begin(), items.end(), [](MenuBarItem* i) { return i->IsFocused(); });
    MenuBarItem* currentFocusedTopLevel = focusedTopLevelIt != items.end() ? *focusedTopLevelIt : nullptr;
    bool focusJustMovedToNewTopLevel =
        currentFocusedTopLevel != nullptr && currentFocusedTopLevel != lastFocusedTopLevel;
    lastFocusedTopLevel = currentFocusedTopLevel;
    bool ownsFocus = OwnsFocus(items);

    if (!switchedByHover && openItem && focusJustMovedToNewTopLevel && currentFocusedTopLevel != openItem) {
        // Focus just transitioned directly onto a different top-level item
        // than the one whose popup is open — most commonly Tab having
        // cycled onto it — so follow it open too, same as hovering a
        // different title with the mouse already does. Gated on the
        // transition itself, not just "focus != openItem" (see
        // lastFocusedTopLevel's doc-comment) — that mismatch can persist
        // legitimately after a mouse hover-switch, which must not keep
        // re-triggering this every frame.
        OpenItem(currentFocusedTopLevel);
    } else if (openItem && !ownsFocus) {
        // Focus moved off the MenuBarItem and its popup entirely since
        // last frame — most commonly Tab cycling past it to some other
        // widget in the window — so the popup shouldn't linger open with
        // nothing pointing at it. CloseOpenItem() (via ClosePreservingFocus)
        // only reclaims focus for itself if focus was still inside; here
        // it's already elsewhere, so this just closes without disturbing
        // wherever focus actually went.
        CloseOpenItem();
    } else if (ownsFocus) {
        HandleKeyboard(items);
    }
}

void MenuStrip::HandleKeyboard(const std::vector<MenuBarItem*>& items) {
    if (items.empty()) return;
    InputSource& input = CurrentInput();

    if (input.IsKeyPressed(KEY_ESCAPE) && openItem) {
        // CloseOpenItem() restores focus to the MenuBarItem itself (see
        // ClosePreservingFocus) — reachable here because HandleKeyboard()
        // only ever runs while OwnsFocus() is true.
        CloseOpenItem();
        return;
    }

    if (input.IsKeyPressed(KEY_LEFT) || input.IsKeyPressed(KEY_RIGHT)) {
        auto focusedIt = std::find_if(items.begin(), items.end(), [](MenuBarItem* i) { return i->IsFocused(); });
        size_t current;
        if (focusedIt != items.end()) {
            current = static_cast<size_t>(focusedIt - items.begin());
        } else {
            // Focus is inside the open popup, not on a top-level item —
            // treat the currently-open item as "current" so Left/Right
            // moves relative to it.
            auto openIt = std::find(items.begin(), items.end(), openItem);
            current = openIt != items.end() ? static_cast<size_t>(openIt - items.begin()) : 0;
        }
        size_t n = items.size();
        bool forward = input.IsKeyPressed(KEY_RIGHT);
        size_t next = forward ? (current + 1) % n : (current + n - 1) % n;

        bool wasOpen = openItem != nullptr;
        items[next]->Focus();
        if (wasOpen) OpenItem(items[next]);
        return;
    }

    // Down opens the focused top-level item and moves focus to its first
    // enabled item — an explicit navigation gesture, unlike Enter/Space
    // (see OpenItem()), so it's the one case where diving straight into
    // the popup's items is actually what was asked for.
    if (input.IsKeyPressed(KEY_DOWN) && !openItem) {
        auto focusedIt = std::find_if(items.begin(), items.end(), [](MenuBarItem* i) { return i->IsFocused(); });
        if (focusedIt != items.end()) {
            OpenItem(*focusedIt);
            for (Widget* child : (*focusedIt)->Popup().Panel().Children()) {
                if (child->IsFocusable()) {
                    child->Focus();
                    break;
                }
            }
        }
        return;
    }

    // Up/Down move within an open popup's items, skipping separators and
    // disabled items.
    if (openItem && (input.IsKeyPressed(KEY_UP) || input.IsKeyPressed(KEY_DOWN))) {
        std::vector<Widget*> focusable;
        for (Widget* child : openItem->Popup().Panel().Children()) {
            if (child->IsFocusable()) focusable.push_back(child);
        }
        if (!focusable.empty()) {
            auto focusedIt = std::find_if(focusable.begin(), focusable.end(), [](Widget* w) { return w->IsFocused(); });
            bool forward = input.IsKeyPressed(KEY_DOWN);
            size_t n = focusable.size();
            size_t next;
            if (focusedIt == focusable.end()) {
                next = forward ? 0 : n - 1;
            } else {
                size_t current = static_cast<size_t>(focusedIt - focusable.begin());
                next = forward ? (current + 1) % n : (current + n - 1) % n;
            }
            focusable[next]->Focus();
        }
        return;
    }

    // Enter/Space selecting an item inside the open popup queues the
    // popup to close next frame (see closeOnNextFrame) rather than
    // closing it immediately — Widget::ProcessKeyboardFocus(), called
    // after this whole ProcessEvents(), is what actually fires the
    // selected MenuItem's own onActivate (the user's action) this same
    // frame, and it must still be focused when that happens. Mouse
    // selection is closed via the onItemSelected wiring set up in
    // MenuStrip's constructor instead (immediately, no deferral needed —
    // onClick fires on release, after which nothing else touches focus
    // that frame), since onClick never fires from the keyboard path.
    if (openItem && (input.IsKeyPressed(KEY_ENTER) || input.IsKeyPressed(KEY_SPACE))) {
        bool focusInPopup = false;
        for (Widget* child : openItem->Popup().Panel().Children()) {
            if (child->IsFocused()) {
                focusInPopup = true;
                break;
            }
        }
        if (focusInPopup) closeOnNextFrame = true;
    }
}

}  // namespace UI
