#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Button.h"
#include "Label.h"
#include "Panel.h"
#include "Widget.h"

namespace ui {

// ---------------------------------------------------------- tree literals --
//
// Widget/Panel/Label/Button are the runtime widget classes; everything in
// this file is the declarative surface used to actually describe a UI,
// e.g.:
//
//   ui::Label* scoreValue = nullptr;
//   std::unique_ptr<ui::Widget> root = ui::Column(
//       {.gap = 6, .padding = 8},
//       ui::Text("Player Stats"),
//       ui::Row(
//           {.justify = ui::Justify::SpaceBetween},
//           ui::Text("Score"),
//           ui::Text("0").Ref(scoreValue)),
//       ui::Btn("Level Up"));
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
/// vector Panel's constructor wants. Not usually called directly.
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

/// Designated-initializer property bag for Row()/Column(), e.g.
/// Row({.justify = Justify::SpaceBetween, .gap = 4}, ...).
struct PanelProps {
    Justify justify = Justify::Start;
    Align align = Align::Stretch;
    float gap = 0.0f;
    float padding = 0.0f;
    bool reverse = false;
};

/// Tree-literal factory for a Row-direction Panel (or RowReverse, via
/// props.reverse). See the tree-literal example above.
template <typename... NodesT>
Node<Panel> Row(PanelProps props, NodesT&&... children) {
    Direction dir = props.reverse ? Direction::RowReverse : Direction::Row;
    return MakeNode<Panel>(
        dir,
        props.justify,
        props.align,
        props.gap,
        props.padding,
        Children(std::forward<NodesT>(children)...)
    );
}

/// Tree-literal factory for a Column-direction Panel (or ColumnReverse, via
/// props.reverse).
template <typename... NodesT>
Node<Panel> Column(PanelProps props, NodesT&&... children) {
    Direction dir =
        props.reverse ? Direction::ColumnReverse : Direction::Column;
    return MakeNode<Panel>(
        dir,
        props.justify,
        props.align,
        props.gap,
        props.padding,
        Children(std::forward<NodesT>(children)...)
    );
}

}  // namespace ui
