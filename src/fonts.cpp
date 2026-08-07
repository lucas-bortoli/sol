
#include <raylib.h>

#include "bdf.h"

namespace fonts {
Font cozette;

void Initialize() {
    cozette = LoadFontBDF("./assets/cozette.bdf");
}

void Cleanup() {
    UnloadFont(cozette);
}
}  // namespace fonts
