#include "LayerStacker.h"

#include <algorithm>

namespace UI {

LayerStacker::ItemId LayerStacker::Register(Layer layer, Drawable& drawable) {
    ItemId id = nextId++;
    entries.emplace(id, Entry{layer, &drawable, Rectangle{}});
    Stack(layer).push_back(id);
    return id;
}

void LayerStacker::Unregister(ItemId id) {
    auto it = entries.find(id);
    if (it == entries.end()) return;

    std::vector<ItemId>& stack = Stack(it->second.layer);
    stack.erase(std::remove(stack.begin(), stack.end(), id), stack.end());
    entries.erase(it);
}

void LayerStacker::SetBounds(ItemId id, Rectangle bounds) {
    auto it = entries.find(id);
    if (it == entries.end()) return;
    it->second.bounds = bounds;
}

void LayerStacker::BringToFront(ItemId id) {
    auto it = entries.find(id);
    if (it == entries.end()) return;

    std::vector<ItemId>& stack = Stack(it->second.layer);
    auto pos = std::find(stack.begin(), stack.end(), id);
    if (pos == stack.end() || pos + 1 == stack.end()) return;
    stack.erase(pos);
    stack.push_back(id);
}

void LayerStacker::SendToBack(ItemId id) {
    auto it = entries.find(id);
    if (it == entries.end()) return;

    std::vector<ItemId>& stack = Stack(it->second.layer);
    auto pos = std::find(stack.begin(), stack.end(), id);
    if (pos == stack.end() || pos == stack.begin()) return;
    stack.erase(pos);
    stack.insert(stack.begin(), id);
}

void LayerStacker::BringForward(ItemId id) {
    auto it = entries.find(id);
    if (it == entries.end()) return;

    std::vector<ItemId>& stack = Stack(it->second.layer);
    auto pos = std::find(stack.begin(), stack.end(), id);
    if (pos == stack.end() || pos + 1 == stack.end()) return;
    std::iter_swap(pos, pos + 1);
}

void LayerStacker::SendBackward(ItemId id) {
    auto it = entries.find(id);
    if (it == entries.end()) return;

    std::vector<ItemId>& stack = Stack(it->second.layer);
    auto pos = std::find(stack.begin(), stack.end(), id);
    if (pos == stack.end() || pos == stack.begin()) return;
    std::iter_swap(pos, pos - 1);
}

std::optional<LayerStacker::ItemId> LayerStacker::TopmostAt(Vector2 point
) const {
    for (size_t layerIndex = kLayerCount; layerIndex-- > 0;) {
        const std::vector<ItemId>& stack = stacks[layerIndex];
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            const Entry& entry = entries.at(*it);
            if (CheckCollisionPointRec(point, entry.bounds)) return *it;
        }
    }
    return std::nullopt;
}

bool LayerStacker::IsTopmostAt(ItemId id, Vector2 point) const {
    return TopmostAt(point) == id;
}

void LayerStacker::DrawAll() const {
    for (size_t layerIndex = 0; layerIndex < kLayerCount; layerIndex++) {
        for (ItemId id : stacks[layerIndex]) {
            entries.at(id).drawable->Draw();
        }
    }
}

std::vector<LayerStacker::ItemId> LayerStacker::ItemsFrontToBack(Layer layer
) const {
    const std::vector<ItemId>& stack = stacks[static_cast<size_t>(layer)];
    return std::vector<ItemId>(stack.rbegin(), stack.rend());
}

LayerStacker& GlobalLayerStacker() {
    static LayerStacker instance;
    return instance;
}

}  // namespace UI
