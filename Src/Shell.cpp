#include "Shell.h"

#include <raylib.h>

#include <ctime>
#include <string>

#include "Assets.h"
#include "Lib/UI/Utils.h"
#include "Palette.h"

namespace Shell {
void ProcessEvents() {}

void Draw() {
    DrawRectangle(0, 0, GetScreenWidth(), 24, ZINC_100);
    DrawLine(0, 24, GetScreenWidth(), 24, NEUTRAL_600);
    DrawLine(1, 24 + 1, GetScreenWidth() + 1, 24 + 1, NEUTRAL_200);
    DrawLine(2, 24 + 2, GetScreenWidth() + 2, 24 + 2, NEUTRAL_200);

    DrawRectangle(0, 0, 24, 24, SKY_200);
    DrawTexture(Assets::IconSearch, 4, 4, BLACK);

    // clock (dynamic)
    std::time_t t = std::time(nullptr);
    std::tm localTm{};
#ifdef _WIN32
    localtime_s(&localTm, &t);
#else
    localtime_r(&t, &localTm);
#endif

    char buf[6];  // "HH:MM" + '\0'
    std::snprintf(buf, sizeof(buf), "%02d:%02d", localTm.tm_hour, localTm.tm_min);
    const char* str = buf;

    auto size = MeasureTextEx(Assets::cozette, str, Assets::cozette.baseSize, 0).x;
    UI::DrawTextWithShadow(str, Vector2{GetScreenWidth() - size - 8.0f, 6.0f}, NEUTRAL_600);
}
}  // namespace Shell