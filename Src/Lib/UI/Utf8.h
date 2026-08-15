#pragma once

#include <string>

namespace UI {

/// Byte index one codepoint right of `pos` in `text`, clamped to
/// text.size().
size_t NextCodepointBoundary(const std::string& text, size_t pos);

/// Byte index one codepoint left of `pos` in `text`, clamped to 0. Assumes
/// `pos` is already codepoint-aligned.
size_t PrevCodepointBoundary(const std::string& text, size_t pos);

}  // namespace UI
