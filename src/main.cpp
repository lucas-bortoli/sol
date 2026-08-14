#include <raylib.h>
#include <yoga/Yoga.h>
#include <cmath>

#include "assets.h"
#include "palette.h"
#include "window_manager.h"

int main() {
    InitWindow(640, 360, "Geminata OS");
    SetTargetFPS(30);
    HideCursor();

    assets::Initialize();
    wm::internal::Initialize();

    auto myWindow = wm::WindowCreate();

    wm::WindowSetSize(myWindow, {200, 150});
    wm::WindowSetPosition(myWindow, {16, 16});
    wm::WindowSetTitle(myWindow, "Hello World!");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        wm::internal::Draw();

        // lastly: the cursor
        if (IsCursorOnScreen()) {
            auto mousePos = GetMousePosition();
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