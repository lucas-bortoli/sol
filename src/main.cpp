#include <raylib.h>

#include <string>

#include "assets.h"
#include "lib/ui/UI.h"
#include "palette.h"
#include "window_manager.h"

/// Appends a new "<item text>  [x]" row to `itemList`, with the [x] button
/// removing the whole row on click — demonstrates Container::AppendChild
/// plus the self-removal pattern documented on Widget::Remove(): the
/// remove callback is attached *after* construction (via SetOnActivate on
/// the raw pointers grabbed from .Ref()) so it can capture the row's
/// address by value rather than by reference to a short-lived local.
void AddTodoItem(ui::Container* itemList, const std::string& text) {
    ui::Container* rowRef = nullptr;
    ui::Button* removeButtonRef = nullptr;

    itemList->AppendChild(
        ui::Row(
            {.justify = ui::Justify::SpaceBetween, .gap = 6},
            ui::Text(text).Grow(1),
            ui::Btn("x").Shrink(0).Ref(removeButtonRef)
        )
            .Ref(rowRef)
    );

    removeButtonRef->SetOnActivate([rowRef] { rowRef->Remove(); });
}

int main() {
    InitWindow(640, 400, "Sol");
    SetTargetFPS(60);
    HideCursor();

    assets::Initialize();
    wm::internal::Initialize();

    auto todoWindow = wm::WindowCreate();
    wm::WindowSetSize(todoWindow, {280, 320});
    wm::WindowSetPosition(todoWindow, {16, 16});
    wm::WindowSetTitle(todoWindow, "Todo List");

    ui::TextBox* newItemInput = nullptr;
    ui::Container* itemList = nullptr;

    // Captures newItemInput/itemList by reference: safe because both
    // outlive the whole program (they're set once, right below, and never
    // reassigned — same pattern as every other Ref()'d-pointer callback
    // in this file), even though this lambda is defined before either
    // pointer is actually assigned.
    auto addFromInput = [&newItemInput, &itemList] {
        std::string text = newItemInput->GetText();
        if (text.empty()) return;
        AddTodoItem(itemList, text);
        newItemInput->SetText("");
    };

    wm::WindowSetContent(
        todoWindow,
        ui::Column(
            {.gap = 6, .padding = 8},
            ui::Text("Todo List"),
            ui::Row(
                {.gap = 4},
                ui::Input("").Ref(newItemInput).Grow(1),
                ui::Btn("Add").OnActivate(addFromInput).Shrink(0)
            ),
            ui::Column({.gap = 4, .overflow = ui::Overflow::Scroll})
                .Ref(itemList)
                .Grow(1)
        )
    );

    AddTodoItem(itemList, "Build the UI toolkit");
    AddTodoItem(itemList, "Wire up the window manager");
    AddTodoItem(itemList, "Ship a demo app");

    while (!WindowShouldClose()) {
        auto mousePos = GetMousePosition();

        BeginDrawing();
        ClearBackground(RAYWHITE);

        wm::internal::ProcessEvents();
        wm::internal::Draw();

        // lastly: the cursor
        if (IsCursorOnScreen()) {
            DrawTexture(assets::CursorDefault, mousePos.x, mousePos.y, WHITE);
            DrawPixel(mousePos.x, mousePos.y, RED_400);
        }

        EndDrawing();
    }

    wm::WindowDestroy(todoWindow);

    wm::internal::Cleanup();
    assets::Cleanup();

    CloseWindow();

    return 0;
}
