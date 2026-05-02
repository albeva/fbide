# FBIde {#mainpage}

FBIde is an open-source IDE for the FreeBASIC compiler, built on
wxWidgets 3.3 and C++23. It is a ground-up rewrite of the original
FBIde, replicating the same feature set with clean modern code.

This documentation pairs an API reference (every class, struct, enum,
and free function in `src/`) with long-form subsystem pages that
explain how the pieces fit together. Start with @ref architecture for
the high-level shape, then dive into the subsystem you care about.

## Subsystems

- @subpage architecture — Context-as-locator, ownership graph, app lifecycle.
- @subpage commands — Layout-driven menu/toolbar wiring + dispatch.
- @subpage documents — Document, DocumentManager, save/reload, encoding/EOL.
- @subpage analyses — Throttled lex+parse pipeline, SymbolTable, recycling.
- @subpage editor — Editor surface: themes, transforms, folding, hotspots.
- @subpage compiler — Compile/Run/QuickRun lifecycle, error nav, fbc probe.
- @subpage format — Lexer to Tree to Renderer formatter pipeline.
- @subpage settings — SettingsDialog tabs + ConfigManager hot-reload chain.
- @subpage theming — Theme schema, .fbt/.ini, ThemeCategory dispatch.
- @subpage ui — UIManager / SideBarManager / freeze locks.
