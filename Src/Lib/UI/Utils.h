#pragma once

#include <raylib.h>

namespace UI {
void DrawText(const char* text, int posX, int posY, Color color);
void DrawText(const char* text, Vector2 position, Color color);

void DrawRectWithBorderAndShadow(
    Rectangle rectangle,
    Color fillColor,
    Color borderColor,
    unsigned int shadowDistance = 2
);
}  // namespace UI