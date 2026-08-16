#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Button.h"
#include "Container.h"
#include "Label.h"
#include "MenuBar.h"
#include "MenuBarItem.h"
#include "MenuItem.h"
#include "MenuSeparator.h"
#include "Spacer.h"
#include "TextArea.h"
#include "TextBox.h"
#include "Widget.h"

namespace UI {

// ---------------------------------------------------------- tree literals --
//
// Widget/Container/Label/Button are the runtime widget classes; everything in
// this file is the declarative surface used to actually describe a UI,
// e.g.:
//
//   UI::Label* scoreValue = nullptr;
//   std::unique_ptr<UI::Widget> root = UI::Column(
//       {.gap = 6, .padding = 8},
//       UI::Text("Player Stats"),
//       UI::Row(
//           {.justify = UI::Justify::SpaceBetween},
//           UI::Text("Score"),
//           UI::Text("0").Ref(scoreValue)),
//       UI::Btn("Level Up"));
//
// Every child is a Node<T> returned by a factory (Text/Btn/Row/Column) and
// is consumed exactly once by whatever Row/Column call it's passed to —
// that's what lets .Ref()/.Grow()/etc chain directly off the constructor
// call instead of off a separate Add() statement that returns the wrong
// object.

/// Wraps a freshly-constructed widget while it's being configured inline in
/// a tree literal. Implicitly converts to std::unique_ptr<Widget> so it can
/// be handed to a Row()/Column() parameter list; every Node is meant to be
/// written and consumed once, inline, so the conversion isn't restricted to
/// rvalues the way a "real" ownership-transfer operator would be.
template <typename T>
class Node {
   public:
    explicit Node(std::unique_ptr<T> owned) : widget(std::move(owned)) {}

    /// Stashes this node's address into outRef so it can be mutated
    /// imperatively later (React ref={} style). Valid for as long as the
    /// tree that ends up owning this widget is alive.
    Node& Ref(T*& outRef) {
        outRef = widget.get();
        return *this;
    }
    Node& Width(float width) {
        widget->SetWidth(width);
        return *this;
    }
    Node& Height(float height) {
        widget->SetHeight(height);
        return *this;
    }
    Node& Grow(float grow) {
        widget->SetGrow(grow);
        return *this;
    }
    Node& Shrink(float shrink) {
        widget->SetShrink(shrink);
        return *this;
    }
    /// Registers a click callback (mouse only). See Widget::SetOnClick.
    Node& OnClick(std::function<void()> callback) {
        widget->SetOnClick(std::move(callback));
        return *this;
    }
    /// Registers an activate callback (mouse click or keyboard Enter/
    /// Space). See Widget::SetOnActivate.
    Node& OnActivate(std::function<void()> callback) {
        widget->SetOnActivate(std::move(callback));
        return *this;
    }
    /// Registers a hover-change callback. See Widget::SetOnHoverChange.
    Node& OnHoverChange(std::function<void(bool)> callback) {
        widget->SetOnHoverChange(std::move(callback));
        return *this;
    }
    /// Registers a key-press callback. See Widget::SetOnKeyPress.
    Node& OnKeyPress(std::function<void(int)> callback) {
        widget->SetOnKeyPress(std::move(callback));
        return *this;
    }
    /// Registers a key-down callback. See Widget::SetOnKeyDown.
    Node& OnKeyDown(std::function<void(int)> callback) {
        widget->SetOnKeyDown(std::move(callback));
        return *this;
    }
    /// Registers a key-up callback. See Widget::SetOnKeyUp.
    Node& OnKeyUp(std::function<void(int)> callback) {
        widget->SetOnKeyUp(std::move(callback));
        return *this;
    }
    /// Sets a TextArea's wrap mode. See TextArea::SetWrapMode.
    Node& WrapMode(TextAreaWrapMode mode) {
        widget->SetWrapMode(mode);
        return *this;
    }
    /// Sets a TextArea's visible row count. See TextArea::SetVisibleRows.
    Node& VisibleRows(int rows) {
        widget->SetVisibleRows(rows);
        return *this;
    }
    /// Registers a TextArea Shift+Enter submit callback. See
    /// TextArea::SetOnSubmit.
    Node& OnSubmit(std::function<void()> callback) {
        widget->SetOnSubmit(std::move(callback));
        return *this;
    }
    /// Sets a MenuItem's leading icon. See MenuItem::SetIcon.
    Node& Icon(Texture2D texture) {
        widget->SetIcon(texture);
        return *this;
    }
    /// Marks a MenuItem disabled (or re-enables it, passing false). See
    /// MenuItem::SetDisabled.
    Node& Disabled(bool disabled = true) {
        widget->SetDisabled(disabled);
        return *this;
    }

    operator std::unique_ptr<Widget>() { return std::move(widget); }

   private:
    std::unique_ptr<T> widget;
};

/// Constructs a T in place and wraps it in a Node<T> for tree-literal use.
/// The building block Text()/Btn()/Row()/Column() are defined in terms of.
template <typename T, typename... Args>
Node<T> MakeNode(Args&&... args) {
    return Node<T>(std::make_unique<T>(std::forward<Args>(args)...));
}

/// Collects a Row()/Column() parameter pack of Node<...> children into the
/// vector Container's constructor wants. Not usually called directly.
template <typename... NodesT>
std::vector<std::unique_ptr<Widget>> Children(NodesT&&... nodes) {
    std::vector<std::unique_ptr<Widget>> result;
    result.reserve(sizeof...(nodes));
    (result.push_back(std::unique_ptr<Widget>(std::forward<NodesT>(nodes))),
     ...);
    return result;
}

/// Tree-literal factory for a Label.
inline Node<Label> Text(std::string text) {
    return MakeNode<Label>(std::move(text));
}

/// Tree-literal factory for a Button.
inline Node<Button> Btn(std::string text) {
    return MakeNode<Button>(std::move(text));
}

/// Tree-literal factory for a TextBox.
inline Node<TextBox> Input(std::string initialText = "") {
    return MakeNode<TextBox>(std::move(initialText));
}

/// Tree-literal factory for a TextArea.
inline Node<TextArea> Textarea(std::string initialText = "") {
    return MakeNode<TextArea>(std::move(initialText));
}

/// Tree-literal factory for a Spacer, e.g. Space().Grow(1) to push later
/// siblings to the far end of a Row/Column. Named Space() rather than
/// Spacer() so it doesn't shadow the UI::Spacer class name (a function and
/// a class sharing a name in the same scope makes the class inaccessible
/// via unqualified lookup thereafter).
inline Node<Spacer> Space() { return MakeNode<Spacer>(); }

/// Tree-literal factory for a MenuItem — one row inside a Menu()'s
/// dropdown. Chain `.Icon(texture)`/`.Disabled()` and use `.OnActivate()`
/// for its action (see MenuItem.h for why `.OnClick()` is reserved).
inline Node<MenuItem> Item(std::string text) {
    return MakeNode<MenuItem>(std::move(text));
}

/// Tree-literal factory for a MenuSeparator — a thin divider between
/// Item()s inside a Menu()'s dropdown.
inline Node<MenuSeparator> Separator() { return MakeNode<MenuSeparator>(); }

/// Designated-initializer property bag for Row()/Column(), e.g.
/// Row({.justify = Justify::SpaceBetween, .gap = 4}, ...).
struct ContainerProps {
    Justify justify = Justify::Start;
    Align align = Align::Stretch;
    float gap = 0.0f;
    /// Uniform padding shorthand, applied to any side not overridden below
    /// (CSS `padding: Npx` shorthand).
    float padding = 0.0f;
    std::optional<float> paddingTop = std::nullopt;
    std::optional<float> paddingRight = std::nullopt;
    std::optional<float> paddingBottom = std::nullopt;
    std::optional<float> paddingLeft = std::nullopt;
    bool reverse = false;
    /// Overflow::Visible (default) keeps today's shrink-to-fit behavior.
    /// Overflow::Scroll lets main-axis content overflow behind a clipped,
    /// scrollable, overlay-scrollbar viewport instead.
    Overflow overflow = Overflow::Visible;
    /// Painted behind children when set. See Container::backgroundColor.
    std::optional<Color> backgroundColor = std::nullopt;
};

/// Resolves a ContainerProps' padding fields (uniform shorthand + per-side
/// overrides) into the four concrete values Container's constructor wants.
inline Padding ResolvePadding(const ContainerProps& props) {
    return Padding{
        props.paddingTop.value_or(props.padding),
        props.paddingRight.value_or(props.padding),
        props.paddingBottom.value_or(props.padding),
        props.paddingLeft.value_or(props.padding),
    };
}

/// Tree-literal factory for a Row-direction Container (or RowReverse, via
/// props.reverse). See the tree-literal example above.
template <typename... NodesT>
Node<Container> Row(ContainerProps props, NodesT&&... children) {
    Direction dir = props.reverse ? Direction::RowReverse : Direction::Row;
    return MakeNode<Container>(
        dir,
        props.justify,
        props.align,
        props.gap,
        ResolvePadding(props),
        props.overflow,
        props.backgroundColor,
        Children(std::forward<NodesT>(children)...)
    );
}

/// Tree-literal factory for a Column-direction Container (or ColumnReverse, via
/// props.reverse).
template <typename... NodesT>
Node<Container> Column(ContainerProps props, NodesT&&... children) {
    Direction dir =
        props.reverse ? Direction::ColumnReverse : Direction::Column;
    return MakeNode<Container>(
        dir,
        props.justify,
        props.align,
        props.gap,
        ResolvePadding(props),
        props.overflow,
        props.backgroundColor,
        Children(std::forward<NodesT>(children)...)
    );
}

/// Tree-literal factory for a MenuBarItem — a top-level MenuBar title
/// (e.g. "File") whose dropdown holds the given Item()/Separator()
/// children, e.g.:
///
///   UI::MenuBar(
///       UI::Menu("File",
///           UI::Item("New").OnActivate([] { ... }),
///           UI::Separator(),
///           UI::Item("Exit").OnActivate([] { ... })));
///
/// Named Menu() to read naturally at the call site rather than
/// MenuBarItem() — the class name describes what it *is* (one bar item
/// owning a popup); the factory name describes what it *reads like* (a
/// menu, the way Win32/most toolkits call it).
template <typename... NodesT>
Node<MenuBarItem> Menu(std::string label, NodesT&&... items) {
    return MakeNode<MenuBarItem>(
        std::move(label), Children(std::forward<NodesT>(items)...)
    );
}

/// Tree-literal factory for a MenuStrip — a horizontal bar of Menu()
/// items. See MenuBar.h for the keyboard/mouse interaction model (the
/// runtime class is named MenuStrip, not MenuBar, so it doesn't hide this
/// factory from unqualified lookup — see MenuBar.h).
template <typename... NodesT>
Node<MenuStrip> MenuBar(NodesT&&... menus) {
    return MakeNode<MenuStrip>(Children(std::forward<NodesT>(menus)...));
}

}  // namespace UI
