# Locale Re-Translation Plan

Re-create 11 missing locale files using ISO 639-1 codes, basing
translations on the **current** `en.ini` (the source of truth) and
the *current behaviour* of the IDE. Use the legacy `.fbl` files at
`C:\Users\Albert\Developer\FBIde0.4.6r4\IDE\lang\` only as a reference
for wording — never copy keys, never translate strings that no longer
exist, and re-word anything that's stale.

## Languages

ISO file → legacy reference → original author credit:

| ISO file  | Old `.fbl`         | Original author    |
|-----------|--------------------|--------------------|
| `pt.ini`  | `portuguese.fbl`   | v!ct0r             |
| `de.ini`  | `deutsch.fbl`      | Mecki              |
| `fr.ini`  | `french.fbl`       | MystikShadows      |
| `nl.ini`  | `dutch.fbl`        | Dutchtux           |
| `ru.ini`  | `russian.fbl`      | E. Gerfanow        |
| `zh.ini`  | `chinesesimpXP.fbl`| Rojalus Kele       |
| `el.ini`  | `greek.fbl`        | Drakontas          |
| `ja.ini`  | `japanese.fbl`     | Shion              |
| `ro.ini`  | `roumanian.fbl`    | N. Panaitoiu       |
| `fi.ini`  | `finnish.fbl`      | lurah              |
| `sk.ini`  | `slovak.fbl`       | etko               |

`en.ini` and `et.ini` are already in place. The other legacy locales
(`bulgarian`, `indonesian`, `italian`, `spanish`, `chinesesimp9X`)
have no listed author and are out of scope for this pass.

## File header convention

Each file starts with the same shape as `en.ini` / `et.ini`:

```ini
; ============================================================================
; <Native language name> locale
; Original author: <name from table above>
; ============================================================================
name=<native name>
author=Claude AI
date=<YYYY-MM-DD of the translation>
version=0.5.0
```

`name=` is the **endonym** — what speakers of the language call it,
e.g. `Deutsch`, `Français`, `Português`, `Русский`, `中文`, `Ελληνικά`,
`日本語`, `Română`, `Suomi`, `Slovenčina`, `Nederlands`. The Settings
dropdown shows this value (sorted alphabetically — see
`GeneralPage::loadLanguageOptions`).

UTF-8 without BOM, LF line endings (matches `en.ini`).

## Translation policy

1. **English locale is the spec.** Every key in `en.ini` should
   appear in the new file. Don't carry over keys that the legacy
   `.fbl` had but the new IDE no longer uses.
2. **Legacy strings as wording reference.** Where a key has an
   obvious old-fbl analogue (File menu, Edit menu, common verbs),
   reuse the old phrasing and only adapt to the new context.
   Example: `[commands/save] name = &Save` → German uses Mecki's
   "&Speichern" verbatim because the menu item hasn't changed.
3. **New strings get fresh translations.** Anything new (keyword
   case dialog, format dialog re-indent / re-format toggles,
   sub-function browser bucket labels, `--cfg` help text, language
   restart prompt, theme migration messages) is translated against
   the English source. Where the English uses a placeholder
   (`{file}`, `{name}`, `%s`) preserve it byte-for-byte.
4. **Don't translate proper nouns / format markers.** `FBIde`,
   `wxWidgets`, `FreeBASIC`, `fbc`, file extensions, hotkey markers
   like `&`, `Ctrl+`, version placeholders.
5. **Hotkey accelerators.** Every menu / button name with `&` in
   the English version needs an `&` in the translation, but it
   marks a sensible key for the translated word — don't blindly
   copy the position. Avoid clashes within the same menu.
6. **Punctuation conventions.** Match the language: French keeps
   the non-breaking space before `:` and `?`; German keeps
   capitalised nouns; Russian / Greek / Chinese / Japanese drop
   the trailing `...` ellipsis only if the language convention
   prefers `…` — pick one and be consistent across the file.
7. **Length budget.** Some controls (toolbar tooltips, status bar
   fields) are width-sensitive. If the natural translation is much
   longer than English, prefer the shorter near-equivalent.

## Workflow per locale

For each ISO target:

1. Open `en.ini` and copy it verbatim as the starting skeleton —
   this guarantees no key is missed.
2. Update the header (`name`, `author`, `date`, comment line with
   "Original author: ...").
3. Walk the file top-to-bottom; for each value:
   - Look up the matching legacy string in the old `.fbl` (numeric
     keys → see `english.fbl` for the legend).
   - If the legacy string still applies, reuse it as the basis;
     edit for the current phrasing only when the English changed
     meaningfully.
   - If the key is new, translate from English using context
     comments above the section in `en.ini`.
4. Save as `resources/ide/locales/<iso>.ini`, UTF-8 without BOM.
5. Smoke-check: `fbide --cfg=locale:dialogs.settings.title --config <path-with-locale-set>` should print the
   translated title, validating both the parser and the encoding.
6. Visual check: open Settings, switch language, restart, confirm
   menus / dialogs / sidebar render without truncation or mojibake.

## Order of work

Recommended order — group by script complexity so latin-script
files settle the layout first, then non-latin scripts validate
font / rendering:

1. `de.ini` (Deutsch)
2. `nl.ini` (Nederlands)
3. `fr.ini` (Français)
4. `pt.ini` (Português)
5. `ro.ini` (Română)
6. `sk.ini` (Slovenčina)
7. `fi.ini` (Suomi)
8. `el.ini` (Ελληνικά)
9. `ru.ini` (Русский)
10. `ja.ini` (日本語)
11. `zh.ini` (中文)

Each locale adds one row to the language dropdown — verify the
sort still places it where the endonym dictates after each step.

## Out of scope

- No live UI refresh logic — language change goes through
  `App::scheduleRestart` (already implemented).
- No locale-specific keyboard layout / accelerator remapping —
  `shortcuts_<plat>.ini` stays as-is.
- No bidi / RTL support — the IDE currently assumes LTR; if a
  future locale needs RTL, that's a separate effort.
- Bulgarian / Indonesian / Italian / Spanish (no listed author).
