#include <raylib.h>
#include <yoga/Yoga.h>
#include <cmath>

#include "bdf.h"
#include "fonts.h"
#include "palette.h"
#include "window_manager.h"

int main() {
    InitWindow(640, 360, "Geminata OS");
    SetTargetFPS(30);

    fonts::Initialize();
    wm::internal::Initialize();

    auto myWindow = wm::WindowCreate();

    wm::WindowSetSize(myWindow, {200, 150});
    wm::WindowSetPosition(myWindow, {16, 16});
    wm::WindowSetTitle(myWindow, "Hello World!");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        wm::internal::Draw();

        EndDrawing();
    }

    wm::WindowDestroy(myWindow);

    wm::internal::Cleanup();
    fonts::Cleanup();

    CloseWindow();

    return 0;
}