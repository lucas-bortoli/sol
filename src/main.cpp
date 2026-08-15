#include <raylib.h>

#include <cmath>
#include <string>

#include "assets.h"
#include "lib/ui/UI.h"
#include "palette.h"
#include "window_manager.h"

int main() {
    InitWindow(640, 400, "Geminata OS");
    SetTargetFPS(60);
    HideCursor();

    assets::Initialize();
    wm::internal::Initialize();

    auto myWindow = wm::WindowCreate();
    wm::WindowSetSize(myWindow, {260, 190});
    wm::WindowSetPosition(myWindow, {16, 16});
    wm::WindowSetTitle(myWindow, "Player Stats");

    auto myWindow2 = wm::WindowCreate();
    wm::WindowSetSize(myWindow2, {200, 140});
    wm::WindowSetPosition(myWindow2, {300, 16});
    wm::WindowSetTitle(myWindow2, "Greeter");

    auto myWindow3 = wm::WindowCreate();
    wm::WindowSetSize(myWindow3, {260, 150});
    wm::WindowSetPosition(myWindow3, {16, 220});
    wm::WindowSetTitle(myWindow3, "Notes");

    auto myWindow4 = wm::WindowCreate();
    wm::WindowSetSize(myWindow4, {200, 180});
    wm::WindowSetPosition(myWindow4, {300, 180});
    wm::WindowSetTitle(myWindow4, "Scroll Test");

    int score = 0;
    int level = 1;

    ui::Label* scoreValue = nullptr;
    ui::Label* levelValue = nullptr;

    std::unique_ptr<ui::Widget> ui_root = ui::Column(
        {.gap = 6, .padding = 8},
        ui::Text("Player Stats"),
        ui::Row(
            {.justify = ui::Justify::SpaceBetween},
            ui::Text("Score"),
            ui::Text(std::to_string(score)).Ref(scoreValue)
        ),
        ui::Row(
            {.justify = ui::Justify::SpaceBetween},
            ui::Text("Level"),
            ui::Text(std::to_string(level)).Ref(levelValue)
        ),
        ui::Row(
            {.gap = 4},
            ui::Btn("-1").OnActivate([&score] { score--; }).Grow(1),
            ui::Btn("Reset").OnActivate([&score] { score = 0; }).Grow(1),
            ui::Btn("+1").OnActivate([&score] { score++; }).Grow(1)
        ),
        ui::Btn("Level Up").OnActivate([&level] { level++; })
    );

    ui::Label* greeting = nullptr;
    ui::TextBox* nameInput = nullptr;

    std::unique_ptr<ui::Widget> ui_root2 = ui::Column(
        {.gap = 6, .padding = 8},
        ui::Text("Hello there!").Ref(greeting),
        ui::Input("").Ref(nameInput),
        ui::Btn("Say Hi").OnActivate([&greeting, &nameInput] {
            std::string name = nameInput->GetText();
            greeting->SetText(
                name.empty() ? "Hi yourself!" : "Hi, " + name + "!"
            );
        })
    );

    std::unique_ptr<ui::Widget> ui_root3 = ui::Column(
        {.gap = 6, .padding = 8},
        ui::Text("Notes (Shift+Enter to log)"),
        ui::Textarea("Type some notes here.\nTry a long line to see it wrap.")
            .WrapMode(ui::TextAreaWrapMode::Character)
            .VisibleRows(3)
            .Grow(0)
    );

    std::unique_ptr<ui::Widget> ui_root4 = ui::Column(
        {.gap = 4, .padding = 0},
        ui::Text("Scroll with mouse wheel or drag the thumb").Shrink(0),
        ui::Column(
            {.gap = 4, .overflow = ui::Overflow::Scroll},
            ui::Text("Item 1"),
            ui::Text("Item 2"),
            ui::Text("Item 3"),
            ui::Text("Item 4"),
            ui::Text("Item 5"),
            ui::Text("Item 6"),
            ui::Text("Item 7"),
            ui::Text("Item 8"),
            ui::Text("Item 9"),
            ui::Text("Item 10"),
            ui::Text("Item 11"),
            ui::Text("Item 12"),
            ui::Text("Item 13"),
            ui::Text("Item 14"),
            ui::Text("Item 15"),
            ui::Text("Item 16"),
            ui::Text("Item 17"),
            ui::Text("Item 18"),
            ui::Text("Item 19"),
            ui::Text("Item 20")
        )
            .Grow(1),
        ui::Row(
            {.gap = 4,
             .paddingBottom = 12.0f,
             .overflow = ui::Overflow::Scroll},
            ui::Btn("One"),
            ui::Btn("Two"),
            ui::Btn("Three"),
            ui::Btn("Four"),
            ui::Btn("Five"),
            ui::Btn("Six"),
            ui::Btn("Seven"),
            ui::Btn("Eight")
        )
            .Shrink(0)
    );

    while (!WindowShouldClose()) {
        auto mousePos = GetMousePosition();

        BeginDrawing();
        ClearBackground(RAYWHITE);

        wm::internal::Draw();

        Rectangle windowContent = {16 + 4, 16 + 18, 260 - 8, 190 - 22};
        ui_root->ProcessEvents();
        ui::Widget::ProcessKeyboardFocus(*ui_root);

        scoreValue->SetText(std::to_string(score));
        levelValue->SetText(std::to_string(level));

        ui_root->Layout(windowContent);
        ui_root->Draw();

        Rectangle windowContent2 = {300 + 4, 16 + 18, 200 - 8, 140 - 22};
        ui_root2->ProcessEvents();
        ui::Widget::ProcessKeyboardFocus(*ui_root2);

        ui_root2->Layout(windowContent2);
        ui_root2->Draw();

        Rectangle windowContent3 = {16 + 4, 220 + 18, 260 - 8, 150 - 22};
        ui_root3->ProcessEvents();
        ui::Widget::ProcessKeyboardFocus(*ui_root3);

        ui_root3->Layout(windowContent3);
        ui_root3->Draw();

        Rectangle windowContent4 = {300 + 4, 180 + 18, 200 - 8, 180 - 22};
        ui_root4->ProcessEvents();
        ui::Widget::ProcessKeyboardFocus(*ui_root4);

        ui_root4->Layout(windowContent4);
        ui_root4->Draw();

        // lastly: the cursor
        if (IsCursorOnScreen()) {
            DrawTexture(assets::CursorDefault, mousePos.x, mousePos.y, WHITE);
            DrawPixel(mousePos.x, mousePos.y, RED_400);
        }

        EndDrawing();
    }

    wm::WindowDestroy(myWindow);
    wm::WindowDestroy(myWindow2);
    wm::WindowDestroy(myWindow3);
    wm::WindowDestroy(myWindow4);

    wm::internal::Cleanup();
    assets::Cleanup();

    CloseWindow();

    return 0;
}
