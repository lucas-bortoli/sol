// MenuBar/Menu interaction tests driven through FakeInput, mirroring the
// style of TestInputDriven.cpp: build a tree directly (no WM window)
// except for the one test that specifically needs two overlapping WM
// windows to prove Layer::Menu participates correctly in WM's existing
// window-vs-window occlusion check.

#include <doctest.h>

#include "FakeInput.h"
#include "../Src/Lib/UI/UI.h"
#include "../Src/WindowManager.h"

namespace {
struct ScopedInput {
    explicit ScopedInput(FakeInput& fake) { UI::SetInput(&fake); }
    ~ScopedInput() { UI::SetInput(nullptr); }
};

Vector2 CenterOf(const Rectangle& rect) {
    return {rect.x + rect.width / 2, rect.y + rect.height / 2};
}

/// Simulates a full click (press this frame, release next frame),
/// polling `root` between and after, per FakeInput.h's documented
/// press/poll/NextFrame/release/poll sequence.
void Click(FakeInput& fake, UI::Widget& root, Vector2 at) {
    fake.MoveMouseTo(at);
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    root.ProcessEvents();

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    root.ProcessEvents();
}

/// Simulates one simulated frame's key press: sets `key` down, polls
/// `root`, then calls NextFrame() and releases it — matching FakeInput.h's
/// documented press/poll/NextFrame/release/poll pattern. Without the
/// NextFrame() call, `key`'s IsKeyPressed edge would still read true on
/// whatever's polled next, silently re-triggering edge-triggered logic
/// (e.g. Widget::ProcessKeyboardFocus's Tab-cycle) a second time.
void PressKey(FakeInput& fake, UI::Widget& root, int key) {
    fake.SetKeyDown(key, true);
    root.ProcessEvents();
    UI::internal::BeginKeyboardFocusFrame();
    UI::Widget::ProcessKeyboardFocus(root);

    fake.NextFrame();
    fake.SetKeyDown(key, false);
}
}  // namespace

TEST_CASE("MenuBar: clicking a title opens its dropdown; clicking again "
          "closes it") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    std::unique_ptr<UI::Widget> root =
        UI::MenuBar(UI::Menu("File", UI::Item("New")).Ref(fileMenu));
    root->Layout({0, 0, 300, 200});

    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    CHECK(fileMenu->IsPopupOpen());

    fake.NextFrame();
    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    CHECK_FALSE(fileMenu->IsPopupOpen());
}

TEST_CASE("MenuBar: clicking outside the open dropdown closes it, and "
          "selecting an item both fires onActivate and closes it") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuItem* newItem = nullptr;
    UI::Button* sibling = nullptr;
    std::unique_ptr<UI::Widget> root = UI::Column(
        {.gap = 4},
        UI::MenuBar(UI::Menu(
                        "File",
                        UI::Item("New").Ref(newItem).OnActivate([] {})
        )
                        .Ref(fileMenu)),
        UI::Btn("Elsewhere").Ref(sibling)
    );
    root->Layout({0, 0, 300, 200});

    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    REQUIRE(fileMenu->IsPopupOpen());

    fake.NextFrame();
    Click(fake, *root, CenterOf(sibling->GetComputedRect()));
    CHECK_FALSE(fileMenu->IsPopupOpen());

    // Reopen, then select the item itself.
    fake.NextFrame();
    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    REQUIRE(fileMenu->IsPopupOpen());

    bool activated = false;
    newItem->SetOnActivate([&activated] { activated = true; });

    fake.NextFrame();
    Click(fake, *root, CenterOf(newItem->GetComputedRect()));
    CHECK(activated);
    CHECK_FALSE(fileMenu->IsPopupOpen());
}

TEST_CASE("MenuBar: selecting an item with the mouse leaves keyboard focus "
          "on its MenuBarItem, same as Enter/Space would") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuItem* newItem = nullptr;
    std::unique_ptr<UI::Widget> root = UI::MenuBar(
        UI::Menu("File", UI::Item("New").Ref(newItem)).Ref(fileMenu)
    );
    root->Layout({0, 0, 300, 200});

    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    REQUIRE(fileMenu->IsPopupOpen());

    fake.NextFrame();
    Click(fake, *root, CenterOf(newItem->GetComputedRect()));
    CHECK_FALSE(fileMenu->IsPopupOpen());
    CHECK(fileMenu->IsFocused());
    CHECK_FALSE(newItem->IsFocused());
}

TEST_CASE("MenuBar: only one dropdown is open at a time, and hovering a "
          "different title while engaged switches to it") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuBarItem* editMenu = nullptr;
    std::unique_ptr<UI::Widget> root = UI::MenuBar(
        UI::Menu("File", UI::Item("New")).Ref(fileMenu),
        UI::Menu("Edit", UI::Item("Cut")).Ref(editMenu)
    );
    root->Layout({0, 0, 300, 200});

    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    REQUIRE(fileMenu->IsPopupOpen());
    REQUIRE_FALSE(editMenu->IsPopupOpen());

    // No click at all — just hovering Edit while File is open switches.
    fake.NextFrame();
    fake.MoveMouseTo(CenterOf(editMenu->GetComputedRect()));
    root->ProcessEvents();

    CHECK_FALSE(fileMenu->IsPopupOpen());
    CHECK(editMenu->IsPopupOpen());

    // Regression: keyboard focus never moved off fileMenu (a mouse hover
    // doesn't touch it) — polling several more frames with the mouse held
    // steady over Edit must stay on Edit, not flicker back to File. That
    // would happen if stale focus (still on fileMenu) kept "correcting"
    // openItem back on every frame the mouse wasn't the one to move it.
    for (int i = 0; i < 3; i++) {
        fake.NextFrame();
        root->ProcessEvents();
        CHECK_FALSE(fileMenu->IsPopupOpen());
        CHECK(editMenu->IsPopupOpen());
    }
}

TEST_CASE("MenuBar: Right-selecting a different item via keyboard while the "
          "mouse rests stationary over yet another title doesn't flicker "
          "back to the hovered one") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuBarItem* editMenu = nullptr;
    UI::MenuBarItem* viewMenu = nullptr;
    std::unique_ptr<UI::Widget> root = UI::MenuBar(
        UI::Menu("File", UI::Item("New")).Ref(fileMenu),
        UI::Menu("Edit", UI::Item("Cut")).Ref(editMenu),
        UI::Menu("View", UI::Item("Zoom")).Ref(viewMenu)
    );
    root->Layout({0, 0, 300, 200});

    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    REQUIRE(fileMenu->IsPopupOpen());

    // Hover View (without touching keyboard focus, still on File) — the
    // mouse move itself legitimately hover-switches the open dropdown to
    // View.
    fake.NextFrame();
    fake.MoveMouseTo(CenterOf(viewMenu->GetComputedRect()));
    root->ProcessEvents();
    REQUIRE(viewMenu->IsPopupOpen());

    // Right-arrow from the still-focused File moves keyboard selection (and
    // the open dropdown) to Edit, with the mouse staying put over View the
    // whole time. Without gating hover-switch on the mouse actually moving,
    // the very next frame would immediately fight this back to View, and
    // every subsequent stationary frame would flicker between the two.
    fake.NextFrame();
    PressKey(fake, *root, KEY_RIGHT);
    REQUIRE(editMenu->IsPopupOpen());

    for (int i = 0; i < 3; i++) {
        fake.NextFrame();
        root->ProcessEvents();
        CHECK(editMenu->IsPopupOpen());
        CHECK_FALSE(viewMenu->IsPopupOpen());
    }
}

TEST_CASE("MenuBar: a disabled item never fires onActivate on click") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuItem* saveItem = nullptr;
    bool activated = false;
    std::unique_ptr<UI::Widget> root = UI::MenuBar(UI::Menu(
                                                        "File",
                                                        UI::Item("Save")
                                                            .Ref(saveItem)
                                                            .Disabled()
                                                            .OnActivate(
                                                                [&activated] {
                                                                    activated =
                                                                        true;
                                                                }
                                                            )
    )
                                                        .Ref(fileMenu));
    root->Layout({0, 0, 300, 200});

    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    REQUIRE(fileMenu->IsPopupOpen());

    fake.NextFrame();
    Click(fake, *root, CenterOf(saveItem->GetComputedRect()));

    CHECK_FALSE(activated);
    CHECK_FALSE(saveItem->IsFocusable());
}

TEST_CASE("MenuBar: Tab reaches items in order; Enter opens the focused "
          "menu without selecting an item (no keyboard-selection "
          "highlight until an actual navigation key is pressed); Escape "
          "closes it and returns focus") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuBarItem* editMenu = nullptr;
    UI::MenuItem* saveItem = nullptr;
    UI::MenuItem* newItem = nullptr;
    std::unique_ptr<UI::Widget> root = UI::MenuBar(
        UI::Menu(
            "File", UI::Item("Save").Ref(saveItem).Disabled(),
            UI::Item("New").Ref(newItem)
        )
            .Ref(fileMenu),
        UI::Menu("Edit", UI::Item("Cut")).Ref(editMenu)
    );
    root->Layout({0, 0, 300, 200});

    PressKey(fake, *root, KEY_TAB);
    REQUIRE(fileMenu->IsFocused());

    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_ENTER);

    REQUIRE(fileMenu->IsPopupOpen());
    // Enter just opens it — same onActivate path a mouse click uses, so
    // it mustn't auto-select an item (that would show a keyboard-selection
    // highlight the user never asked for by pressing a navigation key).
    CHECK_FALSE(newItem->IsFocused());
    CHECK(fileMenu->IsFocused());

    // The first Down press is what actually selects an item — landing on
    // New, since Save is disabled.
    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_DOWN);
    CHECK(newItem->IsFocused());

    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_ESCAPE);

    CHECK_FALSE(fileMenu->IsPopupOpen());
    CHECK(fileMenu->IsFocused());
}

TEST_CASE("MenuBar: Enter on a Down-selected item fires its own "
          "onActivate — not the owning MenuBarItem's (regression: closing "
          "the popup immediately, before Widget::ProcessKeyboardFocus() "
          "later that same frame actually fires the selected item's "
          "onActivate, moved focus off the item and onto the MenuBarItem "
          "first, so the MenuBarItem's onActivate — which just reopens "
          "the popup — fired instead of the item's real command)") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuItem* newItem = nullptr;
    int newActivations = 0;
    std::unique_ptr<UI::Widget> root = UI::MenuBar(UI::Menu(
                                                        "File",
                                                        UI::Item("New")
                                                            .Ref(newItem)
                                                            .OnActivate(
                                                                [&newActivations] {
                                                                    newActivations++;
                                                                }
                                                            )
    )
                                                        .Ref(fileMenu));
    root->Layout({0, 0, 300, 200});

    PressKey(fake, *root, KEY_TAB);
    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_ENTER);
    REQUIRE(fileMenu->IsPopupOpen());

    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_DOWN);
    REQUIRE(newItem->IsFocused());

    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_ENTER);

    CHECK(newActivations == 1);
    // The popup closing is deferred to the following frame (see
    // MenuStrip::closeOnNextFrame) — nudge one more frame to let it catch
    // up before checking.
    fake.AdvanceTime(1.0f);
    root->ProcessEvents();
    CHECK_FALSE(fileMenu->IsPopupOpen());
    CHECK(fileMenu->IsFocused());
}

TEST_CASE("MenuBar: Up/Down move within an open dropdown, skipping "
          "disabled items; Left/Right hop between top-level menus while "
          "one is open") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuBarItem* editMenu = nullptr;
    UI::MenuItem* newItem = nullptr;
    UI::MenuItem* saveItem = nullptr;
    UI::MenuItem* openItem = nullptr;
    std::unique_ptr<UI::Widget> root = UI::MenuBar(
        UI::Menu(
            "File", UI::Item("New").Ref(newItem),
            UI::Item("Open").Ref(openItem),
            UI::Item("Save").Ref(saveItem).Disabled()
        )
            .Ref(fileMenu),
        UI::Menu("Edit", UI::Item("Cut")).Ref(editMenu)
    );
    root->Layout({0, 0, 300, 200});

    // Open through the real interaction path (not MenuBarItem::OpenPopup()
    // directly) so MenuStrip's own openItem bookkeeping — which every
    // HandleKeyboard() branch below depends on — is actually in sync. A
    // mouse-opened menu doesn't select an item on its own (no keyboard-
    // selection highlight until an actual navigation key is pressed).
    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    fake.NextFrame();
    REQUIRE_FALSE(newItem->IsFocused());

    // The first Down selects the first enabled item.
    PressKey(fake, *root, KEY_DOWN);
    CHECK(newItem->IsFocused());

    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_DOWN);
    CHECK(openItem->IsFocused());

    // Save is disabled — Down again must skip it and wrap to New.
    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_DOWN);
    CHECK(newItem->IsFocused());

    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_RIGHT);

    CHECK_FALSE(fileMenu->IsPopupOpen());
    CHECK(editMenu->IsPopupOpen());
    CHECK(editMenu->IsFocused());
}

TEST_CASE("WM: an open Menu dropdown in one window can't be clicked "
          "through to a button in another window underneath it, and the "
          "click still closes the dropdown") {
    FakeInput fake;
    ScopedInput scoped(fake);

    // behindWindow first, so menuWindow (created second) starts frontmost
    // without needing an extra raise-to-front click.
    auto behindWindow = WM::WindowCreate();
    WM::WindowSetPosition(behindWindow, {0, 0});
    WM::WindowSetSize(behindWindow, {400, 400});

    auto menuWindow = WM::WindowCreate();
    WM::WindowSetPosition(menuWindow, {0, 0});
    WM::WindowSetSize(menuWindow, {150, 100});

    UI::MenuStrip* menuBar = nullptr;
    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuItem* newItem = nullptr;
    int newClicks = 0;
    WM::WindowSetContent(
        menuWindow, UI::MenuBar(UI::Menu(
                                     "File", UI::Item("New")
                                                 .Ref(newItem)
                                                 .OnActivate([&newClicks] {
                                                     newClicks++;
                                                 })
    )
                                     .Ref(fileMenu))
                        .Ref(menuBar)
    );

    int behindClicks = 0;
    UI::Button* behindButton = nullptr;
    WM::WindowSetContent(
        behindWindow, UI::Btn("Behind")
                          .Ref(behindButton)
                          .OnActivate([&behindClicks] { behindClicks++; })
    );

    // Lay both windows' content out directly (Layout() normally runs
    // inside WM::internal::Draw(), which also issues real raylib draw
    // calls this headless test can't make — see the WindowManager tests
    // for the same technique). Layout the actual content root of each
    // window (the MenuStrip, not fileMenu directly) so it flex-arranges
    // fileMenu's rect the same way Draw() would.
    menuBar->Layout(WM::WindowGetClientRect(menuWindow));
    behindButton->Layout(WM::WindowGetClientRect(behindWindow));

    // Open File's dropdown. Both windows sit at (0,0) and behindWindow's
    // button fills its whole (larger) window, so the dropdown — anchored
    // just below menuWindow's MenuBar — necessarily overlaps it.
    Vector2 fileMenuCenter = {
        fileMenu->GetComputedRect().x + fileMenu->GetComputedRect().width / 2,
        fileMenu->GetComputedRect().y + fileMenu->GetComputedRect().height / 2
    };
    fake.MoveMouseTo(fileMenuCenter);
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();
    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();
    REQUIRE(fileMenu->IsPopupOpen());

    Rectangle popupBounds = fileMenu->Popup().Bounds();
    Vector2 insidePopup = CenterOf(popupBounds);

    fake.NextFrame();
    fake.MoveMouseTo(insidePopup);
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();
    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    WM::internal::ProcessEvents();

    // The click landed inside the popup's own bounds — on "New" itself —
    // and never reached behindButton underneath it.
    CHECK(newClicks == 1);
    CHECK(behindClicks == 0);
    CHECK_FALSE(fileMenu->IsPopupOpen());

    WM::WindowDestroy(menuWindow);
    WM::WindowDestroy(behindWindow);
}

TEST_CASE("Widget::ProcessKeyboardFocus: Enter opens a focused "
          "MenuBarItem even when a second, unrelated tree's "
          "ProcessKeyboardFocus() call happens in the same real frame "
          "(regression: the once-per-real-frame gate used to compare "
          "CurrentInput().GetTime() reads across calls, which only holds "
          "for a clock that advances in fixed steps — real raylib's "
          "GetTime() is live and drifts by microseconds between two "
          "windows' calls in the same frame, making the second call look "
          "like a new frame and double-firing onActivate: opening the "
          "menu, then immediately toggling it closed again)") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    std::unique_ptr<UI::Widget> menuTree =
        UI::MenuBar(UI::Menu("File", UI::Item("New")).Ref(fileMenu));
    menuTree->Layout({0, 0, 300, 200});

    // An unrelated second tree — stands in for a second WM window, whose
    // own ProcessKeyboardFocus() call WM drives within the same real
    // frame as menuTree's.
    std::unique_ptr<UI::Widget> otherTree = UI::Btn("=");
    otherTree->Layout({0, 0, 50, 20});

    fake.SetKeyDown(KEY_TAB, true);
    menuTree->ProcessEvents();
    UI::internal::BeginKeyboardFocusFrame();
    UI::Widget::ProcessKeyboardFocus(*menuTree);
    fake.NextFrame();
    fake.SetKeyDown(KEY_TAB, false);
    REQUIRE(fileMenu->IsFocused());

    // One real frame processing both trees — WM::internal::ProcessEvents()
    // does exactly this (BeginKeyboardFocusFrame() once, then one
    // ProcessKeyboardFocus() call per window). A microsecond of clock
    // drift between the two calls (never a whole NextFrame()) is what a
    // live high-resolution clock would produce and FakeInput's own clock
    // otherwise wouldn't, which is what let this bug hide from any test
    // that only drove FakeInput's clock in whole AdvanceTime() steps.
    fake.AdvanceTime(1.0f);
    fake.SetKeyDown(KEY_ENTER, true);
    UI::internal::BeginKeyboardFocusFrame();
    menuTree->ProcessEvents();
    UI::Widget::ProcessKeyboardFocus(*menuTree);
    fake.AdvanceTime(0.0001f);
    otherTree->ProcessEvents();
    UI::Widget::ProcessKeyboardFocus(*otherTree);
    fake.NextFrame();
    fake.SetKeyDown(KEY_ENTER, false);

    CHECK(fileMenu->IsPopupOpen());
}

TEST_CASE("MenuBar: Tab-ing focus away from the MenuBarItem closes its "
          "open popup") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::Button* sibling = nullptr;
    std::unique_ptr<UI::Widget> root = UI::Column(
        {.gap = 4},
        UI::MenuBar(UI::Menu("File", UI::Item("New")).Ref(fileMenu)),
        UI::Btn("Elsewhere").Ref(sibling)
    );
    root->Layout({0, 0, 300, 200});

    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    REQUIRE(fileMenu->IsPopupOpen());
    REQUIRE(fileMenu->IsFocused());

    fake.NextFrame();
    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_TAB);
    REQUIRE(sibling->IsFocused());

    // Tab already moved focus off fileMenu — one more poll is what lets
    // MenuStrip::ProcessEvents() actually notice and close the popup.
    root->ProcessEvents();
    CHECK_FALSE(fileMenu->IsPopupOpen());
}

TEST_CASE("MenuBar: Tab-ing to a different MenuBarItem while one's popup "
          "is open follows it — closes the old one, opens the new one") {
    FakeInput fake;
    ScopedInput scoped(fake);

    UI::MenuBarItem* fileMenu = nullptr;
    UI::MenuBarItem* editMenu = nullptr;
    std::unique_ptr<UI::Widget> root = UI::MenuBar(
        UI::Menu("File", UI::Item("New")).Ref(fileMenu),
        UI::Menu("Edit", UI::Item("Cut")).Ref(editMenu)
    );
    root->Layout({0, 0, 300, 200});

    Click(fake, *root, CenterOf(fileMenu->GetComputedRect()));
    REQUIRE(fileMenu->IsPopupOpen());

    fake.NextFrame();
    fake.AdvanceTime(1.0f);
    PressKey(fake, *root, KEY_TAB);
    REQUIRE(editMenu->IsFocused());

    // Tab already moved focus onto editMenu — one more poll is what lets
    // MenuStrip::ProcessEvents() notice and follow it open.
    root->ProcessEvents();
    CHECK_FALSE(fileMenu->IsPopupOpen());
    CHECK(editMenu->IsPopupOpen());
}
