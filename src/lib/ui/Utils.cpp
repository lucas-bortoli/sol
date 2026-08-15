#include "../../assets.h"
#include "../../palette.h"
#include "raylib.h"

namespace ui {
void DrawText(const char* text, int posX, int posY, Color color) {
    // shadow
    DrawTextEx(
        assets::cozette,
        text,
        {(float)posX + 0, (float)posY + 1},
        assets::cozette.baseSize,
        0,
        NEUTRAL_800
    );
    DrawTextEx(
        assets::cozette,
        text,
        {(float)posX + 1, (float)posY + 0},
        assets::cozette.baseSize,
        0,
        NEUTRAL_800
    );
    DrawTextEx(
        assets::cozette,
        text,
        {(float)posX + 1, (float)posY + 1},
        assets::cozette.baseSize,
        0,
        NEUTRAL_800
    );

    // actual text
    DrawTextEx(
        assets::cozette,
        text,
        {(float)posX, (float)posY},
        assets::cozette.baseSize,
        0,
        NEUTRAL_200
    );
}
void DrawText(const char* text, Vector2 position, Color color) {
    DrawText(text, position.x, position.y, color);
}

void DrawRectWithBorderAndShadow(
    Rectangle rectangle,
    Color fillColor,
    Color borderColor,
    unsigned int shadowDistance = 2
) {
    // shadow
    for (int i = 1; i <= shadowDistance; i++) {
        DrawRectangleLines(
            rectangle.x + i,
            rectangle.y + i,
            rectangle.width,
            rectangle.height,
            NEUTRAL_600
        );
    }

    // background
    DrawRectangle(
        rectangle.x, rectangle.y, rectangle.width, rectangle.height, WHITE
    );

    // border
    DrawRectangleLines(
        rectangle.x, rectangle.y, rectangle.width, rectangle.height, NEUTRAL_600
    );
}
}  // namespace ui