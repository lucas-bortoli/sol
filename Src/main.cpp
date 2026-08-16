#include <raylib.h>

#include <optional>
#include <string>

#include "Assets.h"
#include "Lib/UI/UI.h"
#include "Palette.h"
#include "Shell.h"
#include "WindowManager.h"

/// Minimal four-function calculator state machine: tracks the value typed
/// so far, a pending operator, and the left-hand operand it applies to.
/// Digit/operator/equals all funnel through here so the display label is
/// the only thing UI callbacks need to update.
class CalculatorState {
   public:
    void PressDigit(char digit) {
        if (justEvaluated) {
            entry.clear();
            justEvaluated = false;
        }
        if (digit == '.' && entry.find('.') != std::string::npos) return;
        if (entry == "0" && digit != '.') entry.clear();
        entry.push_back(digit);
    }

    void PressOperator(char op) {
        if (!pendingOp) {
            leftOperand = CurrentValue();
        } else if (!justEvaluated) {
            leftOperand = Apply(leftOperand, CurrentValue(), *pendingOp);
        }
        pendingOp = op;
        entry.clear();
        justEvaluated = false;
    }

    void PressEquals() {
        if (!pendingOp) return;
        leftOperand = Apply(leftOperand, CurrentValue(), *pendingOp);
        entry = FormatValue(leftOperand);
        pendingOp = std::nullopt;
        justEvaluated = true;
    }

    void PressClear() {
        entry.clear();
        leftOperand = 0.0;
        pendingOp = std::nullopt;
        justEvaluated = false;
    }

    std::string Display() const { return entry.empty() ? "0" : entry; }

   private:
    double CurrentValue() const { return entry.empty() ? 0.0 : std::stod(entry); }

    static double Apply(double lhs, double rhs, char op) {
        switch (op) {
            case '+':
                return lhs + rhs;
            case '-':
                return lhs - rhs;
            case '*':
                return lhs * rhs;
            case '/':
                return rhs == 0.0 ? 0.0 : lhs / rhs;
        }
        return rhs;
    }

    static std::string FormatValue(double value) {
        std::string text = std::to_string(value);
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
        return text.empty() ? "0" : text;
    }

    std::string entry;
    double leftOperand = 0.0;
    std::optional<char> pendingOp = std::nullopt;
    bool justEvaluated = false;
};

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
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(640, 400, "Sol");
    SetTargetFPS(30);
    HideCursor();

    Assets::Initialize();
    WM::internal::Initialize();

    auto todoWindow = WM::WindowCreate();
    WM::WindowSetSize(todoWindow, {280, 350});
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
            {.gap = 6},
            UI::MenuBar(
                UI::Menu(
                    "File",
                    UI::Item("New List").OnActivate([] { printf("[Menu] File > New List\n"); }),
                    UI::Item("Open...").Icon(Assets::IconOpenExternal).OnActivate([] {
                        printf("[Menu] File > Open...\n");
                    }),
                    UI::Separator(),
                    UI::Item("Save").Icon(Assets::IconSave).Disabled(),
                    UI::Item("Exit").OnActivate([] {
                        printf("[Menu] File > Exit\n");
                        CloseWindow();
                    })
                ),
                UI::Menu(
                    "Edit",
                    UI::Item("Cut").Disabled(),
                    UI::Item("Copy").Disabled(),
                    UI::Item("Paste").Disabled(),
                    UI::Separator(),
                    UI::Item("Clear Completed").OnActivate([] { printf("[Menu] Edit > Clear Completed\n"); })
                )
            ),
            UI::Column(
                {.gap = 6, .padding = 8},
                UI::Text("Todo List"),
                UI::Row(
                    {.gap = 4},
                    UI::Input("").Ref(newItemInput).Grow(1),
                    UI::Btn("Add").OnActivate(addFromInput).Shrink(0)
                ),
                UI::Column({.gap = 4, .overflow = UI::Overflow::Scroll}).Ref(itemList).Grow(1)
            )
                .Grow(1)
        )
    );

    AddTodoItem(itemList, "Build the UI toolkit");
    AddTodoItem(itemList, "Wire up the window manager");
    AddTodoItem(itemList, "Ship a demo app");

    auto calculatorWindow = WM::WindowCreate();
    WM::WindowSetSize(calculatorWindow, {220, 300});
    WM::WindowSetPosition(calculatorWindow, {320, 16});
    WM::WindowSetTitle(calculatorWindow, "Calculator");

    static CalculatorState calculatorState;
    UI::Label* calculatorDisplay = nullptr;

    auto refreshDisplay = [&calculatorDisplay] { calculatorDisplay->SetText(calculatorState.Display()); };

    auto digitButton = [&refreshDisplay](char digit) -> std::unique_ptr<UI::Widget> {
        return UI::Btn(std::string(1, digit)).Grow(1).OnActivate([&refreshDisplay, digit] {
            calculatorState.PressDigit(digit);
            refreshDisplay();
        });
    };
    auto operatorButton = [&refreshDisplay](char op, std::string label) -> std::unique_ptr<UI::Widget> {
        return UI::Btn(std::move(label)).Grow(1).OnActivate([&refreshDisplay, op] {
            calculatorState.PressOperator(op);
            refreshDisplay();
        });
    };

    WM::WindowSetContent(
        calculatorWindow,
        UI::Column(
            {.gap = 6, .padding = 8},
            UI::Text("0").Ref(calculatorDisplay),
            UI::Row({.gap = 4}, digitButton('7'), digitButton('8'), digitButton('9'), operatorButton('/', "/"))
                .Shrink(0),
            UI::Row({.gap = 4}, digitButton('4'), digitButton('5'), digitButton('6'), operatorButton('*', "*"))
                .Shrink(0),
            UI::Row({.gap = 4}, digitButton('1'), digitButton('2'), digitButton('3'), operatorButton('-', "-"))
                .Shrink(0),
            UI::Row(
                {.gap = 4},
                digitButton('0'),
                digitButton('.'),
                UI::Btn("C").Grow(1).OnActivate([&refreshDisplay] {
                    calculatorState.PressClear();
                    refreshDisplay();
                }),
                operatorButton('+', "+")
            )
                .Shrink(0),
            UI::Btn("=").OnActivate([&refreshDisplay] {
                calculatorState.PressEquals();
                refreshDisplay();
            })
        )
    );

    while (!WindowShouldClose()) {
        Shell::ProcessEvents();

        BeginDrawing();
        ClearBackground(RAYWHITE);

        Shell::Draw();

        WM::internal::ProcessEvents();
        WM::internal::Draw();

        // lastly: the cursor
        auto mousePos = GetMousePosition();
        if (IsCursorOnScreen()) {
            DrawTexture(Assets::CursorDefault, mousePos.x, mousePos.y, WHITE);
            DrawPixel(mousePos.x, mousePos.y, RED_400);
        }

        EndDrawing();
    }

    WM::WindowDestroy(todoWindow);
    WM::WindowDestroy(calculatorWindow);

    WM::internal::Cleanup();
    Assets::Cleanup();

    CloseWindow();

    return 0;
}
