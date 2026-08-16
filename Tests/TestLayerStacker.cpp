// Pure registry-logic tests — no raylib window/font involved, only the
// CheckCollisionPointRec calls LayerStacker itself makes against plain
// Rectangle/Vector2 values.

#include <doctest.h>

#include "../Src/Lib/UI/LayerStacker.h"

namespace {
/// A minimal Drawable stub that just counts how many times Draw() ran and
/// records the call order into a shared log, so DrawAll() ordering can be
/// asserted on.
struct RecordingDrawable : UI::LayerStacker::Drawable {
    std::vector<int>* log;
    int id;

    RecordingDrawable(std::vector<int>& log, int id) : log(&log), id(id) {}

    void Draw() const override { log->push_back(id); }
};
}  // namespace

TEST_CASE("LayerStacker: TopmostAt finds the frontmost overlapping item "
          "within a layer") {
    UI::LayerStacker stacker;
    std::vector<int> log;
    RecordingDrawable a(log, 1);
    RecordingDrawable b(log, 2);

    auto idA = stacker.Register(UI::Layer::Windows, a);
    auto idB = stacker.Register(UI::Layer::Windows, b);
    stacker.SetBounds(idA, {0, 0, 100, 100});
    stacker.SetBounds(idB, {50, 50, 100, 100});

    // b was registered later, so it's frontmost by default.
    CHECK(stacker.TopmostAt({75, 75}) == idB);
    CHECK(stacker.TopmostAt({10, 10}) == idA);
    CHECK(stacker.TopmostAt({500, 500}) == std::nullopt);
}

TEST_CASE("LayerStacker: BringToFront/SendToBack/BringForward/SendBackward "
          "reorder within a layer") {
    UI::LayerStacker stacker;
    std::vector<int> log;
    RecordingDrawable a(log, 1);
    RecordingDrawable b(log, 2);
    RecordingDrawable c(log, 3);

    auto idA = stacker.Register(UI::Layer::Windows, a);
    auto idB = stacker.Register(UI::Layer::Windows, b);
    auto idC = stacker.Register(UI::Layer::Windows, c);
    // All three fully overlap so ordering alone decides TopmostAt.
    stacker.SetBounds(idA, {0, 0, 10, 10});
    stacker.SetBounds(idB, {0, 0, 10, 10});
    stacker.SetBounds(idC, {0, 0, 10, 10});

    CHECK(stacker.TopmostAt({5, 5}) == idC);  // registered last

    stacker.BringToFront(idA);
    CHECK(stacker.TopmostAt({5, 5}) == idA);

    stacker.SendToBack(idA);
    CHECK(stacker.TopmostAt({5, 5}) == idC);

    stacker.BringForward(idA);  // a: back -> middle, swaps with b
    stacker.DrawAll();
    CHECK(log == std::vector<int>{2, 1, 3});
}

TEST_CASE("LayerStacker: items in a later layer always win TopmostAt, "
          "regardless of stack position") {
    UI::LayerStacker stacker;
    std::vector<int> log;
    RecordingDrawable background(log, 1);
    RecordingDrawable window(log, 2);

    auto bgId = stacker.Register(UI::Layer::Background, background);
    auto winId = stacker.Register(UI::Layer::Windows, window);
    stacker.SetBounds(bgId, {0, 0, 100, 100});
    stacker.SetBounds(winId, {0, 0, 100, 100});

    CHECK(stacker.TopmostAt({50, 50}) == winId);

    // Even sending the Windows item to the back of its own layer can't let
    // Background win.
    stacker.SendToBack(winId);
    CHECK(stacker.TopmostAt({50, 50}) == winId);
}

TEST_CASE("LayerStacker: IsTopmostAt matches TopmostAt") {
    UI::LayerStacker stacker;
    std::vector<int> log;
    RecordingDrawable a(log, 1);
    RecordingDrawable b(log, 2);

    auto idA = stacker.Register(UI::Layer::Windows, a);
    auto idB = stacker.Register(UI::Layer::Windows, b);
    stacker.SetBounds(idA, {0, 0, 100, 100});
    stacker.SetBounds(idB, {0, 0, 100, 100});

    CHECK(stacker.IsTopmostAt(idB, {10, 10}));
    CHECK_FALSE(stacker.IsTopmostAt(idA, {10, 10}));
}

TEST_CASE("LayerStacker: Unregister removes an item from hit-testing and "
          "DrawAll") {
    UI::LayerStacker stacker;
    std::vector<int> log;
    RecordingDrawable a(log, 1);

    auto idA = stacker.Register(UI::Layer::Windows, a);
    stacker.SetBounds(idA, {0, 0, 100, 100});
    stacker.Unregister(idA);

    CHECK(stacker.TopmostAt({10, 10}) == std::nullopt);
    stacker.DrawAll();
    CHECK(log.empty());
}

TEST_CASE("LayerStacker: DrawAll runs every item back-to-front across "
          "layers") {
    UI::LayerStacker stacker;
    std::vector<int> log;
    RecordingDrawable background(log, 1);
    RecordingDrawable windowA(log, 2);
    RecordingDrawable windowB(log, 3);
    RecordingDrawable shell(log, 4);

    stacker.Register(UI::Layer::Shell, shell);
    stacker.Register(UI::Layer::Background, background);
    stacker.Register(UI::Layer::Windows, windowA);
    stacker.Register(UI::Layer::Windows, windowB);

    stacker.DrawAll();
    CHECK(log == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("LayerStacker: an empty registry reports no topmost item") {
    UI::LayerStacker stacker;
    CHECK(stacker.TopmostAt({0, 0}) == std::nullopt);
    CHECK(stacker.ItemsFrontToBack(UI::Layer::Windows).empty());
}
