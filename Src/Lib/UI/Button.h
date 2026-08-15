#pragma once

#include <raylib.h>

#include <string>

#include "Widget.h"

namespace UI {

/// A clickable rectangle with a text label and border/shadow chrome. Use
/// SetOnActivate() (inherited from Widget) for the button's primary
/// action so it also fires on keyboard Enter/Space; SetOnClick()/
/// SetOnHoverChange() are also available for mouse-only reactions. Becomes
/// the globally focused widget when pressed; Draw() renders a focus ring
/// while focused.
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

}  // namespace UI
