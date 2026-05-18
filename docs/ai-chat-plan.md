# AI Chat — implementation plan

Proof-of-concept AI chat integration for FBIde. Deliberate scope expansion
beyond the original FBIde feature set, approved by the maintainer.

## Locked design decisions

- **Module** `lib/ai/`.
- **Backend** — abstract `AiProvider` with provider-neutral
  `AiRequest`/`AiResponse`. Three providers implemented, selected via
  `[ai] provider`:
  - `anthropic` — Anthropic Messages API (cloud, API key).
  - `ollama` — local Ollama server (free, no key).
  - `claude-cli` — spawns the `claude` CLI in print mode; uses the
    Claude Code login (e.g. a Max subscription), no API key. Prompt on
    stdin, `--resume <session-id>` for conversation continuity.
  OpenAI/Gemini can be added later as further subclasses.
- **HTTP** — `wxWebRequest` (no new dependency, async).
- **Chat UI** — dockable `wxPanel` on the right, AUI pane. Conversation
  rendered as a single `wxHtmlWindow`; markdown converted to HTML.
- **Markdown** — maddy (header-only, via CMake `FetchContent`, like toml11).
- **Command** — `CommandId::AiChat`, `viewAiChat` `wxITEM_CHECK` toggle pane
  (same pattern as `viewBrowser`), `F7` shortcut.
- **Features v1** — chat + code actions (Explain / Refactor on editor
  selection).
- **Context** — attach files only (`AiContextItem` kept extensible for
  future directory/symbol types).

## Deferred (not in PoC)

Directories as context, settings UI, OS keychain. API key stored as
`[ai] key=` in the prefs INI as plaintext — security risk, flagged with
a TODO.

Response streaming is implemented — all three providers stream the
reply incrementally (`AsyncProcess` gained a line-streaming mode for the
claude-cli provider; the HTTP providers use `wxWebRequest` data events).

## Phases

### Phase 1 — command + empty pane (no AI logic)
- `CommandId::AiChat` enum value.
- `CommandEntry { .name="viewAiChat", .kind=wxITEM_CHECK }`. Pure toggle —
  no event-table entry or handler needed (the `wxAuiManager` bind shows/
  hides the pane automatically).
- `lib/ai/AiChatPanel` stub — `wxHtmlWindow` + `wxTextCtrl` + send button.
- `UIManager::createLayout` — `AddPane(...).Right().BestSize(320,-1).Hide()`.
- `layout.ini` view menu, `shortcuts_*.ini` `viewAiChat=F7`, locale strings.
- `CMakeLists.txt` sources.
- Checkpoint: F7 toggles the empty panel; builds and runs.

### Phase 2 — AI core
- maddy via `FetchContent`.
- `AiRequest` / `AiResponse` — provider-neutral types.
- `AiProvider` abstract base.
- `AnthropicProvider` — `wxWebRequest` POST, parse `content[0].text`.
- `AiManager` — Context service, owns provider + conversation history.
- API key read from `[ai] key=`.

### Phase 3 — chat wired
- Panel send -> `AiManager` -> provider -> async response ->
  markdown-to-HTML via maddy -> `wxHtmlWindow`.

### Phase 4 — file context
- `AiContextItem` (abstract) / `FileContextItem` / `AiContext`.
- Context bar UI (Add file, removable list); files snapshotted at send.

### Phase 5 — code actions
- New `CommandId`s for Explain / Refactor; editor selection context menu
  routes the selection through `AiManager`.

### Phase 6
- `change-log.md` entry.

## Build risks

- `wxWebRequest` needs wxWidgets built with `wxUSE_WEBREQUEST` (on by
  default in 3.3).
- `wxHtmlWindow` needs the wxHTML library linked.
