#pragma once

#include <raylib.h>

#include <cstddef>
#include <string>
#include <utility>

#include "Utf8.h"

namespace ui {

/// Maximum gap, in seconds, between two clicks at the same character for
/// the second to count as part of a double/triple-click sequence rather
/// than starting a fresh single click.
inline constexpr double kMultiClickIntervalSeconds = 0.4;

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

/// Coarse character classes used to decide where a "word" starts/ends for
/// double-click selection — a maximal run of the same class counts as one
/// word, so double-clicking a run of punctuation or whitespace selects
/// that whole run too, not just alphanumerics.
enum class CharClass {
    Whitespace,
    Word,
    Punctuation,
};

/// Classifies a single Unicode codepoint for word-boundary purposes.
/// Anything above ASCII (i.e. any non-ASCII letter, in any script) is
/// treated as a word character rather than punctuation, since this is a
/// coarse heuristic, not full Unicode word segmentation.
inline CharClass ClassifyCodepoint(int codepoint) {
    if (codepoint == ' ' || codepoint == '\t' || codepoint == '\n' ||
        codepoint == '\r') {
        return CharClass::Whitespace;
    }
    if ((codepoint >= '0' && codepoint <= '9') ||
        (codepoint >= 'a' && codepoint <= 'z') ||
        (codepoint >= 'A' && codepoint <= 'Z') || codepoint == '_' ||
        codepoint > 127) {
        return CharClass::Word;
    }
    return CharClass::Punctuation;
}

/// Returns the byte range of the maximal same-CharClass run containing
/// `byteIndex` — i.e. the word (or whitespace/punctuation run) a
/// double-click at that position should select. `byteIndex` must be
/// codepoint-aligned; if it's at the very end of `text`, the run ending
/// there is used instead (so double-clicking just past the last character
/// still selects it). Returns {0, 0} for an empty string.
inline ByteRange WordRangeAt(const std::string& text, size_t byteIndex) {
    if (text.empty()) return {0, 0};

    size_t anchor = byteIndex < text.size()
                        ? byteIndex
                        : PrevCodepointBoundary(text, text.size());

    int anchorSize = 0;
    int anchorCodepoint = GetCodepointNext(text.c_str() + anchor, &anchorSize);
    CharClass cls = ClassifyCodepoint(anchorCodepoint);

    size_t start = anchor;
    while (start > 0) {
        size_t prev = PrevCodepointBoundary(text, start);
        int prevSize = 0;
        int prevCodepoint = GetCodepointNext(text.c_str() + prev, &prevSize);
        if (ClassifyCodepoint(prevCodepoint) != cls) break;
        start = prev;
    }

    size_t end = anchor + static_cast<size_t>(anchorSize);
    while (end < text.size()) {
        int nextSize = 0;
        int nextCodepoint = GetCodepointNext(text.c_str() + end, &nextSize);
        if (ClassifyCodepoint(nextCodepoint) != cls) break;
        end += static_cast<size_t>(nextSize);
    }

    return {start, end};
}

}  // namespace ui
