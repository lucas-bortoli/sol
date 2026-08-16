#pragma once

#include <raylib.h>

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

namespace UI {

/// Fixed back-to-front ordering of independent z-bands. An item only ever
/// reorders within its own layer — bringing it to front never lets it
/// paint over, or steal a click from, a later-listed layer. Bands with no
/// registered items yet (Menu, Notification) are declared up front so
/// future call sites don't need a LayerStacker API change to use them.
enum class Layer {
    Background,
    Windows,
    Shell,
    Menu,
    Notification,
};

/// A single global z-order + click-ownership registry (see
/// GlobalLayerStacker() below). Any widget that can visually float
/// independent of normal parent-child containment — a window's content
/// root, eventually a popup menu or toast — registers a layerToken here
/// instead of relying on tree position for draw order or hit-testing.
class LayerStacker {
   public:
    using ItemId = unsigned int;

    /// Anything paintable via DrawAll() implements this — Widget already
    /// has a virtual Draw(), so a Widget IS-A Drawable for free; WM's
    /// Window (chrome + content, not itself a Widget) implements it
    /// directly. A non-owning pointer, exactly like Widget::parent — the
    /// registrant is responsible for Unregister()-ing before it's
    /// destroyed.
    class Drawable {
       public:
        virtual ~Drawable() = default;
        virtual void Draw() const = 0;
    };

    /// Registers a new item at the front (top) of `layer`'s own stack,
    /// returning its ItemId ("layerToken"). `drawable` is asked to Draw()
    /// by DrawAll() when it's this item's turn to paint — the registry
    /// owns nothing about *how* an item draws itself, only when, and never
    /// takes ownership of `drawable`.
    ItemId Register(Layer layer, Drawable& drawable);
    /// Unregisters `id`. No-op if already unregistered — safe to call from
    /// a widget's destructor unconditionally.
    void Unregister(ItemId id);
    /// Refreshes the screen-space rect `id` currently occupies, used for
    /// hit-testing. Call once per frame, before TopmostAt/IsTopmostAt, for
    /// every registered item still on screen.
    void SetBounds(ItemId id, Rectangle bounds);

    /// Moves `id` to the front of its own layer's stack.
    void BringToFront(ItemId id);
    /// Moves `id` to the back of its own layer's stack.
    void SendToBack(ItemId id);
    /// Swaps `id` with the next item above it in its own layer's stack.
    void BringForward(ItemId id);
    /// Swaps `id` with the next item below it in its own layer's stack.
    void SendBackward(ItemId id);

    /// The frontmost registered item whose last-set bounds contain `point`,
    /// scanning layers back-to-front (Notification before Windows before
    /// Background) and, within a layer, its stack front-to-back.
    std::optional<ItemId> TopmostAt(Vector2 point) const;
    /// Convenience: TopmostAt(point) == id — what an item asks to find out
    /// whether a click is actually directed at it, not just within its
    /// hitbox.
    bool IsTopmostAt(ItemId id, Vector2 point) const;

    /// Calls every registered item's Draw() once, back-to-front (every
    /// Background item, then every Windows item, then Shell, ...) — the
    /// single place paint order is decided app-wide.
    void DrawAll() const;

    /// `layer`'s own items, front-to-back (topmost first) — for callers
    /// that need their own custom hit-testing beyond a simple bounds check
    /// (e.g. a resize-border region extending outside an item's
    /// registered bounds) but still want it done in current z-order.
    std::vector<ItemId> ItemsFrontToBack(Layer layer) const;

   private:
    static constexpr size_t kLayerCount = 5;

    struct Entry {
        Layer layer;
        Drawable* drawable;
        Rectangle bounds{};
    };

    /// Front = back of the vector, matching per-layer draw/hit-test order.
    std::vector<ItemId>& Stack(Layer layer) {
        return stacks[static_cast<size_t>(layer)];
    }

    std::array<std::vector<ItemId>, kLayerCount> stacks;
    std::unordered_map<ItemId, Entry> entries;
    ItemId nextId = 1;
};

/// The app-wide LayerStacker instance. A plain accessor (not a
/// FakeInput-style swappable seam like CurrentInput()) — LayerStacker holds
/// no OS/raylib state, so tests construct their own local instance instead
/// of swapping this one.
LayerStacker& GlobalLayerStacker();

}  // namespace UI
