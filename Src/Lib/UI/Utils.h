#pragma once

#include <raylib.h>

#include "../../Palette.h"

namespace UI {
/// Draws text with no shadow, in the given color.
void DrawText(const char* text, int posX, int posY, Color color);
/// Draws text with no shadow, in the given color.
void DrawText(const char* text, Vector2 position, Color color);

/// Draws text with a drop shadow, in the given color.
void DrawTextWithShadow(
    const char* text,
    int posX,
    int posY,
    Color color,
    Color shadowColor = NEUTRAL_300
);
/// Draws text with a drop shadow, in the given color.
void DrawTextWithShadow(
    const char* text,
    Vector2 position,
    Color color,
    Color shadowColor = NEUTRAL_300
);

/// Draws a filled, bordered rectangle with a drop shadow.
void DrawRectWithBorderAndShadow(
    Rectangle rectangle,
    Color fillColor,
    Color borderColor,
    unsigned int shadowDistance = 2
);
}  // namespace UI