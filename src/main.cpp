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

    int score = 0;
    int level = 1;

    ui::Label* scoreValue = nullptr;
    ui::Label* levelValue = nullptr;
    ui::Button* decrementButton = nullptr;
    ui::Button* resetButton = nullptr;
    ui::Button* incrementButton = nullptr;
    ui::Button* levelUpButton = nullptr;

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
            ui::Btn("-1").Ref(decrementButton).Grow(1),
            ui::Btn("Reset").Ref(resetButton).Grow(1),
            ui::Btn("+1").Ref(incrementButton).Grow(1)
        ),
        ui::Btn("Level Up").Ref(levelUpButton)
    );

    while (!WindowShouldClose()) {
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        auto mousePos = GetMousePosition();
        auto Hit = [&](ui::Button* button) {
            return clicked && button != nullptr &&
                   CheckCollisionPointRec(mousePos, button->GetComputedRect());
        };

        if (Hit(decrementButton)) score--;
        if (Hit(resetButton)) score = 0;
        if (Hit(incrementButton)) score++;
        if (Hit(levelUpButton)) level++;

        scoreValue->SetText(std::to_string(score));
        levelValue->SetText(std::to_string(level));

        BeginDrawing();
        ClearBackground(RAYWHITE);

        wm::internal::Draw();

        Rectangle windowContent = {16 + 4, 16 + 18, 260 - 8, 190 - 22};
        ui_root->Layout(windowContent);
        ui_root->Draw();

        // lastly: the cursor
        if (IsCursorOnScreen()) {
            DrawTexture(assets::CursorDefault, mousePos.x, mousePos.y, WHITE);
            DrawPixel(mousePos.x, mousePos.y, RED_400);
        }

        EndDrawing();
    }

    wm::WindowDestroy(myWindow);

    wm::internal::Cleanup();
    assets::Cleanup();

    CloseWindow();

    return 0;
}
