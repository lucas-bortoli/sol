// Interaction tests driven through FakeInput (see FakeInput.h) instead of
// a real window — this is what ui::InputSource exists for. Every test
// installs its own FakeInput via ui::SetInput() and resets it to the
// default (raylib-backed) source afterward, so tests never leak a
// dangling FakeInput* into a later test via CurrentInput().

#include <doctest.h>

#include "FakeInput.h"
#include "../src/lib/ui/UI.h"

namespace {
/// RAII helper: installs `fake` as CurrentInput() for the scope, restores
/// the default source on destruction (including on a failed CHECK, since
/// doctest continues running the rest of the TEST_CASE rather than
/// unwinding the stack).
struct ScopedInput {
    explicit ScopedInput(FakeInput& fake) { ui::SetInput(&fake); }
    ~ScopedInput() { ui::SetInput(nullptr); }
};
}  // namespace

TEST_CASE("Button: a full click fires onClick and onActivate") {
    FakeInput fake;
    ScopedInput scoped(fake);

    bool clicked = false;
    bool activated = false;
    ui::Button* button = nullptr;
    std::unique_ptr<ui::Widget> root = ui::Btn("Remove")
                    .OnClick([&clicked] { clicked = true; })
                    .OnActivate([&activated] { activated = true; })
                    .Ref(button);
    button->Layout({0, 0, 60, 20});

    fake.MoveMouseTo({10, 10});  // inside the button's rect
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    button->ProcessEvents();
    CHECK_FALSE(clicked);  // press alone doesn't fire a click

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    button->ProcessEvents();

    CHECK(clicked);
    CHECK(activated);
}

TEST_CASE("Button: dragging the press origin off the widget cancels the click") {
    FakeInput fake;
    ScopedInput scoped(fake);

    bool clicked = false;
    ui::Button* button = nullptr;
    std::unique_ptr<ui::Widget> root =
        ui::Btn("Remove").OnClick([&clicked] { clicked = true; }).Ref(button);
    button->Layout({0, 0, 60, 20});

    fake.MoveMouseTo({10, 10});
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    button->ProcessEvents();

    fake.NextFrame();
    fake.MoveMouseTo({500, 500});  // drag well outside the button
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    button->ProcessEvents();

    CHECK_FALSE(clicked);
}

TEST_CASE("Widget::ProcessKeyboardFocus: Tab cycles focus in tree order") {
    FakeInput fake;
    ScopedInput scoped(fake);

    ui::Button* first = nullptr;
    ui::Button* second = nullptr;
    std::unique_ptr<ui::Widget> root = ui::Row(
        {.gap = 0},
        ui::Btn("A").Width(20).Height(20).Ref(first),
        ui::Btn("B").Width(20).Height(20).Ref(second)
    );
    root->Layout({0, 0, 100, 20});

    fake.QueueKeyPress(KEY_TAB);
    fake.SetKeyDown(KEY_TAB, true);
    root->ProcessEvents();
    ui::Widget::ProcessKeyboardFocus(*root);
    CHECK(first->IsFocused());
    CHECK_FALSE(second->IsFocused());

    fake.SetKeyDown(KEY_TAB, false);
    fake.AdvanceTime(1.0f);  // clear IsKeyRepeated's held-timer state
    fake.SetKeyDown(KEY_TAB, true);
    root->ProcessEvents();
    ui::Widget::ProcessKeyboardFocus(*root);
    CHECK_FALSE(first->IsFocused());
    CHECK(second->IsFocused());
}

TEST_CASE("Widget::ProcessKeyboardFocus: Enter activates the focused widget") {
    FakeInput fake;
    ScopedInput scoped(fake);

    bool activated = false;
    ui::Button* button = nullptr;
    std::unique_ptr<ui::Widget> root = ui::Btn("Go")
                    .Width(20)
                    .Height(20)
                    .OnActivate([&activated] { activated = true; })
                    .Ref(button);
    root->Layout({0, 0, 20, 20});

    fake.QueueKeyPress(KEY_TAB);
    fake.SetKeyDown(KEY_TAB, true);
    root->ProcessEvents();
    ui::Widget::ProcessKeyboardFocus(*root);
    REQUIRE(button->IsFocused());

    fake.AdvanceTime(1.0f);
    fake.SetKeyDown(KEY_TAB, false);
    fake.SetKeyDown(KEY_ENTER, true);
    root->ProcessEvents();
    ui::Widget::ProcessKeyboardFocus(*root);

    CHECK(activated);
}

TEST_CASE("Container: mouse wheel scrolls an overflowing Column") {
    FakeInput fake;
    ScopedInput scoped(fake);

    ui::Container* list = nullptr;
    std::unique_ptr<ui::Widget> root = ui::Column({.gap = 0, .overflow = ui::Overflow::Scroll},
                            ui::Space().Height(50),
                            ui::Space().Height(50),
                            ui::Space().Height(50))
                    .Ref(list);
    list->Layout({0, 0, 100, 60});  // 150px of content, 60px viewport

    ui::Spacer* firstChild =
        static_cast<ui::Spacer*>(list->ChildAt(0));
    float startY = firstChild->GetComputedRect().y;

    fake.MoveMouseTo({50, 30});  // inside the list
    fake.Scroll(-1.0f);          // scroll down
    list->ProcessEvents();
    list->Layout({0, 0, 100, 60});  // re-Layout to apply the new offset

    CHECK(firstChild->GetComputedRect().y < startY);
}

TEST_CASE(
    "Regression: a button removing its own parent row from within "
    "Container::ProcessEvents' child-iteration loop doesn't crash "
    "(the bug fixed earlier this session)"
) {
    FakeInput fake;
    ScopedInput scoped(fake);

    // Mirrors main.cpp's AddTodoItem exactly: a row containing a label and
    // a "remove" button whose onActivate calls Remove() on its own parent
    // row — while itemList->ProcessEvents() is mid-iteration over its
    // children (one of which is that very row).
    ui::Container* itemList = nullptr;
    std::unique_ptr<ui::Widget> root = ui::Column({.gap = 0, .overflow = ui::Overflow::Scroll})
                    .Ref(itemList);

    auto addItem = [itemList] {
        ui::Container* rowRef = nullptr;
        ui::Button* removeButtonRef = nullptr;
        itemList->AppendChild(
            ui::Row(
                {.justify = ui::Justify::SpaceBetween, .gap = 0},
                ui::Space().Grow(1),
                ui::Btn("x").Width(20).Height(20).Ref(removeButtonRef)
            )
                .Ref(rowRef)
        );
        removeButtonRef->SetOnActivate([rowRef] { rowRef->Remove(); });
    };

    addItem();
    addItem();
    addItem();
    REQUIRE(itemList->ChildCount() == 3);

    itemList->Layout({0, 0, 200, 200});
    auto* firstRow = static_cast<ui::Container*>(itemList->ChildAt(0));
    auto* removeButton = static_cast<ui::Button*>(firstRow->ChildAt(1));
    Rectangle removeButtonRect = removeButton->GetComputedRect();
    Vector2 clickPos = {
        removeButtonRect.x + removeButtonRect.width / 2,
        removeButtonRect.y + removeButtonRect.height / 2
    };

    fake.MoveMouseTo(clickPos);
    fake.PressMouseButton(MOUSE_BUTTON_LEFT);
    itemList->ProcessEvents();  // the crash site: iterating children while
                                // one of their descendants' callbacks is
                                // about to destroy an ancestor

    fake.NextFrame();
    fake.ReleaseMouseButton(MOUSE_BUTTON_LEFT);
    itemList->ProcessEvents();  // fires onClick/onActivate -> Remove()

    CHECK(itemList->ChildCount() == 2);
}
