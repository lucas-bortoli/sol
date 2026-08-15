#include "Utf8.h"

#include <raylib.h>

namespace UI {

size_t NextCodepointBoundary(const std::string& text, size_t pos) {
    if (pos >= text.size()) return text.size();
    int codepointSize = 0;
    GetCodepointNext(text.c_str() + pos, &codepointSize);
    return pos + static_cast<size_t>(codepointSize);
}

size_t PrevCodepointBoundary(const std::string& text, size_t pos) {
    if (pos == 0) return 0;
    int codepointSize = 0;
    GetCodepointPrevious(text.c_str() + pos, &codepointSize);
    return pos - static_cast<size_t>(codepointSize);
}

}  // namespace UI
