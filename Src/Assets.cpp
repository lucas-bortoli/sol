#include <raylib.h>

#include <string>

#include "Lib/FontParser/Bdf.h"

namespace Assets {
Font cozette;
Texture2D CursorDefault;
Texture2D WindowCloseButtonX;

void Initialize() {
    const std::string assetsDir =
        std::string(GetApplicationDirectory()) + "Assets/";

    cozette = LoadFontBDF((assetsDir + "cozette.bdf").c_str());
    CursorDefault = LoadTexture((assetsDir + "CursorDefault.png").c_str());
    WindowCloseButtonX =
        LoadTexture((assetsDir + "WindowCloseButtonX.png").c_str());
}

void Cleanup() {
    UnloadTexture(WindowCloseButtonX);
    UnloadTexture(CursorDefault);
    UnloadFont(cozette);
}
}  // namespace Assets
