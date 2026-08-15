// Pure layout/mutation tests — deliberately never touch raylib (no window,
// no font load): Container/Widget construction, Layout(), and the dynamic
// mutation API never call raylib except for MeasureTextEx/friends inside
// IntrinsicWidth()/Height() (Label/Button), which is why these tests stick
// to UI::Spacer (IntrinsicWidth()/Height() are the Widget-base 0.0f
// default, no raylib involved at all) for anything that goes through
// Container's flex arrangement.

#include <doctest.h>

#include "../Src/Lib/UI/UI.h"

using UI::Align;
using UI::Column;
using UI::Justify;
using UI::Overflow;
using UI::Row;
using UI::Space;

TEST_CASE("Container: grow distributes leftover main-axis space by ratio") {
    UI::Spacer* a = nullptr;
    UI::Spacer* b = nullptr;
    std::unique_ptr<UI::Widget> root =
        Row({.gap = 0}, Space().Grow(1).Ref(a), Space().Grow(2).Ref(b));
    root->Layout({0, 0, 300, 50});

    CHECK(a->GetComputedRect().width == doctest::Approx(100.0f));
    CHECK(b->GetComputedRect().width == doctest::Approx(200.0f));
}

TEST_CASE("Container: shrink clamps at zero when Overflow::Visible") {
    UI::Spacer* a = nullptr;
    UI::Spacer* b = nullptr;
    std::unique_ptr<UI::Widget> root = Row(
        {.gap = 0},
        Space().Width(80).Shrink(1).Ref(a),
        Space().Width(80).Shrink(1).Ref(b)
    );
    // Base widths sum to 160 in a 100-wide box: 60px deficit, split evenly
    // since both have equal shrink weight (shrink * base is equal for both).
    root->Layout({0, 0, 100, 50});

    CHECK(a->GetComputedRect().width == doctest::Approx(50.0f));
    CHECK(b->GetComputedRect().width == doctest::Approx(50.0f));
}

TEST_CASE("Container: justify SpaceBetween pins children to the ends") {
    UI::Spacer* a = nullptr;
    UI::Spacer* b = nullptr;
    std::unique_ptr<UI::Widget> root = Row(
        {.justify = Justify::SpaceBetween, .gap = 0},
        Space().Width(10).Shrink(0).Ref(a),
        Space().Width(10).Shrink(0).Ref(b)
    );
    root->Layout({0, 0, 100, 50});

    CHECK(a->GetComputedRect().x == doctest::Approx(0.0f));
    CHECK(b->GetComputedRect().x == doctest::Approx(90.0f));
}

TEST_CASE("Container: align Center centers a child on the cross axis") {
    UI::Spacer* child = nullptr;
    std::unique_ptr<UI::Widget> root = Row(
        {.align = Align::Center, .gap = 0},
        Space().Width(10).Height(10).Ref(child)
    );
    root->Layout({0, 0, 100, 100});

    CHECK(child->GetComputedRect().y == doctest::Approx(45.0f));
}

TEST_CASE("Container: per-side padding insets content independently") {
    UI::Spacer* child = nullptr;
    std::unique_ptr<UI::Widget> root = Column(
        {.padding = 8, .paddingTop = 20, .paddingLeft = 0},
        Space().Width(10).Height(10).Ref(child)
    );
    root->Layout({0, 0, 100, 100});

    CHECK(child->GetComputedRect().x == doctest::Approx(0.0f));   // paddingLeft
    CHECK(child->GetComputedRect().y == doctest::Approx(20.0f));  // paddingTop
}

TEST_CASE(
    "Container: Overflow::Scroll keeps children at their natural size "
    "instead of shrinking them to fit"
) {
    UI::Spacer* a = nullptr;
    UI::Spacer* b = nullptr;
    std::unique_ptr<UI::Widget> root = Column(
        {.gap = 0, .overflow = Overflow::Scroll},
        Space().Height(50).Ref(a),
        Space().Height(50).Ref(b)
    );
    // 100px of natural content in a 60px viewport: Overflow::Visible would
    // shrink both to 30px each; Overflow::Scroll must not.
    root->Layout({0, 0, 100, 60});

    CHECK(a->GetComputedRect().height == doctest::Approx(50.0f));
    CHECK(b->GetComputedRect().height == doctest::Approx(50.0f));
}

TEST_CASE("Container: AppendChild/InsertChild/queries") {
    UI::Container* list = nullptr;
    std::unique_ptr<UI::Widget> root = Column({}).Ref(list);

    UI::Spacer* first = nullptr;
    UI::Spacer* second = nullptr;
    UI::Spacer* middle = nullptr;
    list->AppendChild(Space().Ref(first));
    list->AppendChild(Space().Ref(second));

    REQUIRE(list->ChildCount() == 2);
    CHECK(list->ChildAt(0) == first);
    CHECK(list->ChildAt(1) == second);
    CHECK(list->IndexOf(first) == 0);
    CHECK(list->IndexOf(second) == 1);
    CHECK(list->IndexOf(nullptr) == std::nullopt);

    list->InsertChild(1, Space().Ref(middle));
    REQUIRE(list->ChildCount() == 3);
    CHECK(list->ChildAt(0) == first);
    CHECK(list->ChildAt(1) == middle);
    CHECK(list->ChildAt(2) == second);

    std::vector<UI::Widget*> snapshot = list->Children();
    REQUIRE(snapshot.size() == 3);
    CHECK(snapshot[1] == middle);

    // Out-of-range InsertChild clamps to the end, same as AppendChild.
    UI::Spacer* last = nullptr;
    list->InsertChild(999, Space().Ref(last));
    CHECK(list->ChildAt(3) == last);
}

TEST_CASE("Container: RemoveChild returns ownership, or nullptr if not a child") {
    UI::Container* list = nullptr;
    std::unique_ptr<UI::Widget> root = Column({}).Ref(list);

    UI::Spacer* childPtr = nullptr;
    list->AppendChild(Space().Ref(childPtr));
    REQUIRE(list->ChildCount() == 1);

    UI::Spacer notAChild;
    CHECK(list->RemoveChild(&notAChild) == nullptr);
    CHECK(list->ChildCount() == 1);

    std::unique_ptr<UI::Widget> removed = list->RemoveChild(childPtr);
    REQUIRE(removed != nullptr);
    CHECK(removed.get() == childPtr);
    CHECK(list->ChildCount() == 0);
    CHECK(childPtr->GetParent() == nullptr);
}

TEST_CASE("Widget: GetParent()/Remove()") {
    UI::Container* list = nullptr;
    UI::Spacer* childPtr = nullptr;
    std::unique_ptr<UI::Widget> root = Column(
        {}, Space().Ref(childPtr)
    ).Ref(list);

    CHECK(childPtr->GetParent() == list);
    CHECK(list->GetParent() == nullptr);  // tree root

    std::unique_ptr<UI::Widget> detached = childPtr->Remove();
    REQUIRE(detached != nullptr);
    CHECK(detached.get() == childPtr);
    CHECK(list->ChildCount() == 0);
    CHECK(childPtr->GetParent() == nullptr);

    // Reattach elsewhere — the same object, not a new one.
    UI::Container* other = nullptr;
    std::unique_ptr<UI::Widget> otherRoot = Column({}).Ref(other);
    other->AppendChild(std::move(detached));
    CHECK(childPtr->GetParent() == other);
    CHECK(other->ChildAt(0) == childPtr);
}
