# Theming {#theming}

Themes drive how the editor looks: foreground / background per syntax
category, font choice, line-number gutter, selection, fold margin, and
brace-match decoration. The schema is fixed (one slot per
`ThemeCategory`) and editable through the Theme tab in the Settings
dialog.

## The Theme structure

`fbide::Theme` (`src/lib/config/Theme.hpp`) holds:

| Field                              | Notes                                                  |
|------------------------------------|--------------------------------------------------------|
| `m_categories[kThemeCategoryCount]`| Per-`ThemeCategory` `Entry { Colors, bold, italic, underlined }`. |
| `m_version`                        | Theme schema version (top of the file).                |
| `m_separator`                      | Column-line / margin separator color.                  |
| `m_font`, `m_fontSize`             | Editor font.                                           |
| `m_lineNumber`, `m_selection`, `m_foldMargin` | Extra `Colors` entries.                     |
| `m_brace`, `m_badBrace`            | Extra `Entry` entries (matched / mismatched).          |

`Entry` is `{ Colors, bold, italic, underlined }`. `Colors` is
`{ foreground, background }`. Both have defaulted `operator==` so the
Theme tab can detect "did this entry change since load".

## ThemeCategory + X-macros

`ThemeCategory` (`ThemeCategory.hpp`) is the master list of style
slots. The enum, the name lookup, the iteration arrays, and the
keyword-group sub-array are all driven by a single X-macro:

```cpp
#define DEFINE_THEME_CATEGORY(_)   \
    _(Default)                     \
    _(Comment)                     \
    _(MultilineComment)            \
    _(Number)                      \
    _(String)                      \
    _(StringOpen)                  \
    _(Identifier)                  \
    DEFINE_THEME_KEYWORD_GROUPS(_) \
    _(Operator)                    \
    _(Label)                       \
    _(Preprocessor)                \
    _(Error)
```

The `enum class`, `kThemeCategories[]`, `getThemeCategoryName(...)`,
and `kThemeKeywordCategories[]` are all generated from this list. To
add a new style slot, you add **one line** to the macro.

`Theme.hpp` mirrors the same X-macro pattern for the extra
properties (`Version`, `Separator`, `Font`, `FontSize`, `LineNumber`,
etc.) — `DEFINE_THEME_PROPERTY` generates the member, getter, and
setter for each entry. Adding a new top-level theme field means one
line in `DEFINE_THEME_PROPERTY` plus the matching read/write in the
INI loader.

## Loading

| Format       | Source                          | Mode                      |
|--------------|---------------------------------|---------------------------|
| `.ini` (v5+) | `resources/IDE/v2/themes/*.ini` | Read/write (canonical).   |
| `.fbt` (v4)  | Legacy fbide-old themes         | Read-only migration.      |

`Theme::load(path)` dispatches on the extension. The `.fbt` path
calls `loadV4` which reads the legacy format but does **not** store
the path — saving back round-trips through a v5 `.ini` instead.

`Version` is the first field in every saved file so future schema
changes can branch on it during load.

## Save round-trip

`Theme::save(newPath)` writes every member back to the INI file. The
member-by-member format makes diffs in version control reasonable —
adding a new theme entry shows up as one new section, not a
whole-file rewrite.

## Per-DocumentType apply

Editors apply themes by `DocumentType`:

```
Editor::applyTheme()
    │
    ▼
switch (m_docType) {
    case FreeBASIC:    applyFreebasicTheme();    // FBSciLexer + every category
    case Html:         applyHtmlTheme();         // wxSTC HTML lexer
    case Properties:   applyPropertiesTheme();   // wxSTC Props lexer
    case Text:         applyTextTheme();         // plain text styling
}
```

`applyStyle(stcId, entry, theme)` and `applyColors(stcId, colors,
theme)` are the helpers that translate `Entry` / `Colors` into wxSTC
style ids. The same `Theme` object feeds every editor.

## Recipe: add a new theme entry

For a new style slot (e.g. a new keyword group):

1. Add one line to `DEFINE_THEME_CATEGORY` in `ThemeCategory.hpp`.
   (Add it inside `DEFINE_THEME_KEYWORD_GROUPS` if it is one.)
2. Add the matching entry to every theme `.ini` under
   `resources/IDE/v2/themes/`.
3. Map the new `ThemeCategory` to a wxSTC style id in
   `Editor::applyFreebasicTheme` (or the relevant per-type method).
4. If it's a keyword group, add the keyword list to `keywords.ini`.

For a new top-level theme field (a new color triple, etc.):

1. Add one line to `DEFINE_THEME_PROPERTY` (or
   `DEFINE_THEME_EXTRA_PROPERTY`) in `Theme.hpp`.
2. Read it in the theme loader and write it in the saver.
3. Consume it in the editor where it applies.

## Cross-links

- @ref settings — Theme tab UI lives in `ThemePage`.
- @ref editor — `applyTheme` chain and per-DocumentType dispatch.
- @ref format — `HtmlRenderer` reads the active theme so HTML export
  matches the editor.
- @ref analyses — keyword groups in `Token.hpp` mirror the
  `DEFINE_THEME_KEYWORD_GROUPS` macro.
