#pragma once

#include <raylib.h>

#include "Widget.h"

namespace UI {

/// A thin horizontal divider between MenuItems inside a MenuPopup. Never
/// focusable, never fires any callback — purely visual.
class MenuSeparator : public Widget {
   public:
    MenuSeparator() = default;

    void Layout(const Rectangle& bounds) override;
    void ProcessEvents() override;
    void Draw() const override;

   protected:
    float IntrinsicHeight() const override;

   private:
    static constexpr float kPaddingY = 4.0f;
};

}  // namespace UI
