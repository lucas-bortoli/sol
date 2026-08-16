#include "../../Assets.h"
#include "../../Palette.h"
#include "raylib.h"

namespace UI {
void DrawText(const char* text, int posX, int posY, Color color) {
    DrawTextEx(
        Assets::cozette,
        text,
        {(float)posX, (float)posY},
        Assets::cozette.baseSize,
        0,
        color
    );
}
void DrawText(const char* text, Vector2 position, Color color) {
    DrawText(text, position.x, position.y, color);
}

void DrawTextWithShadow(
    const char* text,
    int posX,
    int posY,
    Color color,
    Color shadowColor = NEUTRAL_300
) {
    // shadow
    DrawTextEx(
        Assets::cozette,
        text,
        {(float)posX + 0, (float)posY + 1},
        Assets::cozette.baseSize,
        0,
        shadowColor
    );
    DrawTextEx(
        Assets::cozette,
        text,
        {(float)posX + 1, (float)posY + 0},
        Assets::cozette.baseSize,
        0,
        shadowColor
    );
    DrawTextEx(
        Assets::cozette,
        text,
        {(float)posX + 1, (float)posY + 1},
        Assets::cozette.baseSize,
        0,
        shadowColor
    );

    // text
    DrawTextEx(
        Assets::cozette,
        text,
        {(float)posX, (float)posY},
        Assets::cozette.baseSize,
        0,
        color
    );
}
void DrawTextWithShadow(
    const char* text,
    Vector2 position,
    Color color,
    Color shadowColor = NEUTRAL_300
) {
    DrawTextWithShadow(text, position.x, position.y, color, shadowColor);
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
            NEUTRAL_300
        );
    }

    // background
    DrawRectangle(
        rectangle.x, rectangle.y, rectangle.width, rectangle.height, fillColor
    );

    // border
    DrawRectangleLines(
        rectangle.x, rectangle.y, rectangle.width, rectangle.height, borderColor
    );
}
}  // namespace UI