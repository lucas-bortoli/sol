#pragma once

#include "Widget.h"

namespace UI {

/// Reserves layout space without rendering anything. Give it a size via
/// SetWidth/SetHeight for a fixed gap, or SetGrow to have it eat leftover
/// main-axis space in a Container (e.g. to push later siblings to the far
/// end).
class Spacer : public Widget {
   public:
    Spacer() = default;

    void Draw() const override;
};

}  // namespace UI
