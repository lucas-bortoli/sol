#pragma once

#include <raylib.h>

#include <string>

#include "Widget.h"

namespace ui {

/// A clickable rectangle with a text label and border/shadow chrome. Use
/// SetOnClick()/SetOnHoverChange() (inherited from Widget) to react to
/// input.
class Button : public Widget {
   public:
    explicit Button(std::string initialText);

    void SetText(std::string newText);

    void Layout(const Rectangle& bounds) override;
    void Draw() const override;

   protected:
    float IntrinsicWidth() const override;
    float IntrinsicHeight() const override;

   private:
    std::string text;

    static constexpr float kPaddingX = 8.0f;
    static constexpr float kPaddingY = 4.0f;
};

}  // namespace ui
