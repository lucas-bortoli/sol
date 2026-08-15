#pragma once

#include <cstddef>
#include <string>
#include <utility>

namespace ui {

/// A byte range [start, end) into a UTF-8 string, always normalized so
/// start <= end regardless of which end an anchor/caret pair started from.
struct ByteRange {
    size_t start;
    size_t end;
};

/// Orders an (anchor, caret) pair into a normalized ByteRange, since
/// either one can be the smaller value depending on which direction the
/// selection was extended from.
inline ByteRange NormalizeSelection(size_t anchor, size_t caret) {
    return anchor < caret ? ByteRange{anchor, caret} : ByteRange{caret, anchor};
}

/// Applies one arrow/Home/End-style caret movement with standard
/// text-field selection semantics, shared by TextBox and TextArea (both
/// have an identical caretByteIndex/selectionAnchor pair):
///   - Shift held: runs `moveFn` normally, leaving `selectionAnchor`
///     untouched — this is what extends a selection.
///   - Not held, and a selection is currently active: collapses the caret
///     to the selection's start or end (per `collapseToStart`) instead of
///     also running `moveFn` — pressing Left with an active selection
///     jumps to its start rather than moving one further character left,
///     matching every mainstream text editor.
///   - Not held, no active selection: runs `moveFn` normally.
/// In the two "not held" cases, `selectionAnchor` is resynced to the
/// resulting `caretByteIndex` afterward, so the selection is gone for the
/// next keypress.
template <typename MoveFn>
void ApplySelectableMovement(
    size_t& caretByteIndex,
    size_t& selectionAnchor,
    bool shiftHeld,
    bool collapseToStart,
    MoveFn&& moveFn
) {
    if (shiftHeld) {
        moveFn();
        return;
    }
    if (caretByteIndex != selectionAnchor) {
        ByteRange range = NormalizeSelection(selectionAnchor, caretByteIndex);
        caretByteIndex = collapseToStart ? range.start : range.end;
    } else {
        moveFn();
    }
    selectionAnchor = caretByteIndex;
}

/// Normalizes clipboard text before it's spliced into a text widget:
/// collapses "\r\n" and lone "\r" to "\n", and — when `allowNewlines` is
/// false (TextBox, which must stay single-line) — replaces any newline
/// with a space instead of introducing one. TextArea passes true and keeps
/// newlines as hard line breaks.
inline std::string SanitizePastedText(
    const std::string& clipboard, bool allowNewlines
) {
    std::string out;
    out.reserve(clipboard.size());
    for (size_t i = 0; i < clipboard.size(); i++) {
        char c = clipboard[i];
        if (c == '\r') {
            bool followedByNewline =
                i + 1 < clipboard.size() && clipboard[i + 1] == '\n';
            if (!followedByNewline) out.push_back(allowNewlines ? '\n' : ' ');
            continue;
        }
        if (c == '\n') {
            out.push_back(allowNewlines ? '\n' : ' ');
            continue;
        }
        out.push_back(c);
    }
    return out;
}

}  // namespace ui
