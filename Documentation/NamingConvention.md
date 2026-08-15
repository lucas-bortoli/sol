# Naming convention

Chosen to mirror raylib's public API and a win32-flavored background,
rather than the lowercase std-style casing common elsewhere in C++.

## Identifiers

- Types, classes, structs, functions, and methods: PascalCase.
- Namespaces: PascalCase too — UI, WM, Assets — not lowercase like std
  or boost. Initialisms go all-caps (ui becomes UI); ordinary words stay
  regular PascalCase (assets becomes Assets).
- Local variables and parameters: camelCase.
- Member fields: camelCase with an m\_ prefix, e.g. m_handleCounter.
- Constants: k prefix, e.g. kContentInsetTop.

Namespaces stay PascalCase rather than std-style lowercase so they read
consistently with the PascalCase types and functions they qualify
(GUI::Button, not gui::Button) — matching raylib's own casing.

## Files and directories

Filenames are PascalCase, matching the type or function they declare —
Widget.h, WindowManager.cpp, Bdf.h — namespace or not.

Directories are PascalCase and mirror the namespace or library they
hold: Src/Lib/UI corresponds to the UI namespace. A namespace that's one
cohesive unit (WM) stays a flat file pair; a family of types (UI) gets
its own directory. Top-level layout follows suit: Src, Tests,
Documentation, Vendor. Src/Assets holds runtime asset files alongside
the unrelated Src/Assets.h/.cpp source pair.

The one exception is main.cpp, kept lowercase as the universal
entry-point convention rather than part of this project's own style.

## Adding something new

- New namespace, family of types: own PascalCase directory under Src/Lib.
- New namespace, single cohesive unit: flat PascalCase file pair under Src.
- New free function, no namespace: still a PascalCase filename.
