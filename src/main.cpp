#include <raylib.h>

#include <cmath>
#include <string>

#include "assets.h"
#include "lib/ui/UI.h"
#include "palette.h"
#include "window_manager.h"

int main() {
    InitWindow(640, 360, "Geminata OS");
    SetTargetFPS(30);
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

        // lastly: the cursor
        if (IsCursorOnScreen()) {
            DrawTexture(assets::CursorDefault, mousePos.x, mousePos.y, WHITE);
            DrawPixel(mousePos.x, mousePos.y, RED_400);
        }

        EndDrawing();
    }

    wm::WindowDestroy(myWindow);
    wm::WindowDestroy(myWindow2);

    wm::internal::Cleanup();
    assets::Cleanup();

    CloseWindow();

    return 0;
}
