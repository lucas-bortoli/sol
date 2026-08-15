#include <raylib.h>

#include <string>

#include "Assets.h"
#include "Lib/UI/UI.h"
#include "Palette.h"
#include "WindowManager.h"

/// Appends a new "<item text>  [x]" row to `itemList`, with the [x] button
/// removing the whole row on click — demonstrates Container::AppendChild
/// plus the self-removal pattern documented on Widget::Remove(): the
/// remove callback is attached *after* construction (via SetOnActivate on
/// the raw pointers grabbed from .Ref()) so it can capture the row's
/// address by value rather than by reference to a short-lived local.
void AddTodoItem(UI::Container* itemList, const std::string& text) {
    UI::Container* rowRef = nullptr;
    UI::Button* removeButtonRef = nullptr;

    itemList->AppendChild(
        UI::Row(
            {.justify = UI::Justify::SpaceBetween, .gap = 6},
            UI::Text(text).Grow(1),
            UI::Btn("x").Shrink(0).Ref(removeButtonRef)
        )
            .Ref(rowRef)
    );

    removeButtonRef->SetOnActivate([rowRef] { rowRef->Remove(); });
}

int main() {
    InitWindow(640, 400, "Sol");
    SetTargetFPS(30);
    HideCursor();

    Assets::Initialize();
    WM::internal::Initialize();

    auto todoWindow = WM::WindowCreate();
    WM::WindowSetSize(todoWindow, {280, 320});
    WM::WindowSetPosition(todoWindow, {16, 16});
    WM::WindowSetTitle(todoWindow, "Todo List");

    UI::TextBox* newItemInput = nullptr;
    UI::Container* itemList = nullptr;

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

    WM::WindowSetContent(
        todoWindow,
        UI::Column(
            {.gap = 6, .padding = 8},
            UI::Text("Todo List"),
            UI::Row(
                {.gap = 4},
                UI::Input("").Ref(newItemInput).Grow(1),
                UI::Btn("Add").OnActivate(addFromInput).Shrink(0)
            ),
            UI::Column({.gap = 4, .overflow = UI::Overflow::Scroll})
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

        WM::internal::ProcessEvents();
        WM::internal::Draw();

        // lastly: the cursor
        if (IsCursorOnScreen()) {
            DrawTexture(Assets::CursorDefault, mousePos.x, mousePos.y, WHITE);
            DrawPixel(mousePos.x, mousePos.y, RED_400);
        }

        EndDrawing();
    }

    WM::WindowDestroy(todoWindow);

    WM::internal::Cleanup();
    Assets::Cleanup();

    CloseWindow();

    return 0;
}
