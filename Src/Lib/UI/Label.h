#pragma once

#include <raylib.h>

#include <string>

#include "Widget.h"

namespace UI {

/// Renders a single line of static text, sized to its content unless
/// SetWidth/SetHeight override it.
class Label : public Widget {
   public:
    explicit Label(std::string initialText);

    /// Replaces the displayed text and invalidates layout (size may have
    /// changed).
    void SetText(std::string newText);
    const std::string& GetText() const { return text; }

    void Layout(const Rectangle& bounds) override;
    void Draw() const override;

   protected:
    float IntrinsicWidth() const override;
    float IntrinsicHeight() const override;

   private:
    std::string text;
};

}  // namespace UI
