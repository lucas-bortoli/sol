#include <raylib.h>

#include "lib/font-parser/bdf.h"

namespace assets {
Font cozette;
Texture2D CursorDefault;
Texture2D WindowCloseButtonX;

void Initialize() {
    cozette = LoadFontBDF("./assets/cozette.bdf");
    CursorDefault = LoadTexture("./assets/CursorDefault.png");
    WindowCloseButtonX = LoadTexture("./assets/WindowCloseButtonX.png");
}

void Cleanup() {
    UnloadTexture(WindowCloseButtonX);
    UnloadTexture(CursorDefault);
    UnloadFont(cozette);
}
}  // namespace assets
