# AI tools arc — summary and test plan

What landed on `ai-integration` from the plan in `docs/ai-tools-plan.md`,
and how to exercise each piece before merging.

## Summary of changes

### Phase 0 — preparation (no behaviour change)
- New `AiContent` struct (`text` + `cacheable` flag) and `joinSystem`
  helper in `AiTypes.{hpp,cpp}`. `AiManager::sendMessage` builds a
  vector of blocks instead of mutating a flat `wxString`.

### Phase 1 — Anthropic prompt caching
- `AiContext::buildBlocks()` returns one `AiContent` per attached
  item; file / edit-target items are `cacheable=true`, buffer
  snapshots are `cacheable=false`.
- `AiRequest::system` is now `std::vector<AiContent>`. Non-caching
  providers (Gemini, Ollama, ClaudeCli) collapse it with `joinSystem`.
- `AiProvider::supportsPromptCaching()` capability flag.
- `AnthropicProvider::serializeBody` emits the array form with
  `cache_control: ephemeral` on cacheable blocks when any are present,
  capped at the API's 4 breakpoints. Falls back to the string form
  byte-identical to the pre-caching path when nothing is cacheable.

### Phase 2 — tool scaffold + `read_file`
- New `AiTool`, `AiToolCall`, `AiToolResult` types in `AiTypes.hpp`;
  `AiMessage` gains `toolCalls`/`toolResults`; `AiRequest` gains
  `tools`.
- `AiProvider::send` signature gains a `ToolCallHandler`. New
  `supportsTools()` capability flag.
- `StreamLineConsumer::onToolCall` and `onUsage` virtuals added so
  provider parsers can forward events through the same sink as text
  deltas.
- New `src/lib/ai/tools/`:
  - `ToolRegistry.{hpp,cpp}` — async dispatch by tool name.
  - `ReadFileTool.{hpp,cpp}` — `read_file(path)` scoped to the active
    doc's directory subtree or open tabs, 256 KB cap, `..` rejected.
- New `ToolDispatchLoop` class drives multi-round conversations:
  send → collect text + tool_use → invoke tools → re-send. Caps at
  10 rounds. `AiManager::sendMessage` delegates to it.
- `AnthropicProvider` serializes the `tools` array, `tool_use` and
  `tool_result` content blocks; parses streamed `tool_use` blocks via
  a stateful SSE assembler keyed by content-block index.
- `[ai] enable_tools` config flag (default `true`).
- Chat view renders tool calls as a blockquote strip
  (`> \`name(args)\``). Synthetic user messages carrying only tool
  results are hidden from the chat.

### Phase 3 — `apply_patch` tool
- New `tools/ApplyPatchTool.{hpp,cpp}` exposing
  `apply_patch(search, replace)`. Gated on agent mode AND a pinned
  edit target; the tool itself enforces both. Returns
  `{status: applied|no_match}`; `no_match` sets `isError=true`.
- Tool-aware system prompt: when agent mode is on AND the provider
  supports tools, the model gets a short "use apply_patch" rubric.
  Non-tool providers keep the SEARCH/REPLACE-blocks rubric.

### Phase 4 — cancel + token readout
- `AiProvider::cancel()` virtual; `WebStreamProvider::cancel()` aborts
  the in-flight `wxWebRequest`. `ToolDispatchLoop::cancel` forwards.
- Send button doubles as Cancel while a request is in flight (label
  swaps, click handler routes to `AiManager::cancel`).
- `AiResponse` and `AiMessage` gain `inputTokens`/`outputTokens`.
- `AnthropicProvider` parses `usage.input_tokens` from
  `message_start` and `usage.output_tokens` from each `message_delta`.
- Chat view renders an italic `↑N ↓M` footer on assistant messages
  with non-zero usage.

### Phase 5 — `compile` tool
- New `tools/CompileTool.{hpp,cpp}` exposing `compile()`. Triple-gated:
  agent mode + "Allow compile" checkbox + per-turn cap (≤3 calls per
  `sendMessage`). Async; returns
  `{status: ok|failed|cancelled, output: <16 KB truncated>}`.
- `BuildTask::CompletionHandler` — fires after `onCompileFinished` or
  on early destruction (the dtor emits a `[cancelled]` sentinel so
  the dispatch loop doesn't hang when the task is replaced).
- `CompilerManager::compileHeadless(doc, handler)` — separate entry
  point that doesn't prompt for save and fires the handler with the
  raw fbc output.
- `AiChatPanel`: new `[ ] allow compile` checkbox on the bottom row
  under the prompt box. Greyed when agent mode is off, cleared when
  agent mode toggles off, **never persisted** (opt-in every session).
- `AiManager::cancel()` also calls `CompilerManager::killProcess()`.

### Phase 6 — patch matching hardening
- `findPatchMatch` gains a third fallback after exact and
  trimmed-newline: line-by-line comparison with leading/trailing
  whitespace stripped. Catches "model used 4 spaces, file uses 2" and
  trailing-whitespace drift.
- New `MatchKind` enum on `PatchMatch`
  (`Exact`/`TrimmedNewline`/`NormalizedWhitespace`). Not surfaced via
  `apply_patch` yet — the existing `applied|no_match` is enough for
  the dispatch loop's retry signal.
- Anchor-bracket fallback (the riskier mode) intentionally not
  implemented — the compile loop already gives the model
  self-correction.

### Incidentals
- `cmake/warnings.cmake`: `-Wno-missing-designated-field-initializers`
  for clang. LLVM 22 made this default-on under `-Wextra`; the project
  uses partial designated init throughout.
- `.clang-tidy`: `::nlohmann::basic_json` added to the
  `cppcoreguidelines-pro-bounds-avoid-unchecked-container-access`
  exclude list.
- `tests/CMakeLists.txt`: nlohmann include dir added.
- `ClaudeCliProvider`: AsyncProcess callback takes `ProcessResult` by
  const ref (pre-existing tidy finding fixed in passing).
- `EditTargetItem::label`: pencil glyph extracted to a named
  constant.

## Files added

```
src/lib/ai/AiTypes.cpp
src/lib/ai/ToolDispatchLoop.{hpp,cpp}
src/lib/ai/tools/ToolRegistry.{hpp,cpp}
src/lib/ai/tools/ReadFileTool.{hpp,cpp}
src/lib/ai/tools/ApplyPatchTool.{hpp,cpp}
src/lib/ai/tools/CompileTool.{hpp,cpp}
tests/unit/AiTypesTests.cpp
tests/unit/ReadFileToolTests.cpp
tests/unit/ToolRegistryTests.cpp
tests/unit/ToolDispatchLoopTests.cpp
tests/unit/ApplyPatchToolTests.cpp
tests/unit/CompileToolTests.cpp
docs/ai-tools-plan.md
docs/ai-tools-changes.md          (this file)
```

## Test plan

### 0. Prerequisites

- Configure an Anthropic provider in `~/.fbide/preferences.toml`
  (or the user's chosen path):
  ```toml
  [ai]
  active = "anthropic"

  [ai.anthropic]
  provider = "anthropic"
  model = "claude-sonnet-4-6"
  key = "sk-ant-..."
  ```
  Anthropic is the only provider with full tool + caching support.
  The other providers (Gemini, Ollama, ClaudeCli, Mock) fall back to
  text-only behaviour and exercise the legacy SEARCH/REPLACE path.

- Build via the `verify` skill or directly:
  ```
  cmake --build build/claude/clang/debug --target fbide
  ```

- Have a small FreeBASIC file to use as the edit target — anything
  that compiles. A throwaway example:
  ```basic
  ' hello.bas
  Sub greet()
    Print "hello"
  End Sub
  greet()
  ```
  And keep a second file with `#include "hello.bi"` style references
  so `read_file` has something to chase.

### 1. Smoke test (no tools)
- Open the chat panel (`F7`).
- Send a plain question like "what does this do?" with no attachments.
- **Expected:** reply streams in; assistant bubble appears; no tool
  strip; on completion, `↑N ↓M` footer shows the token counts.

### 2. Prompt caching (Anthropic)
- Attach a moderately large file (>~4 KB) via the `+` button.
- Send a message; note the `↑N` count.
- Send a second message in the same conversation.
- **Expected:** the second turn's `↑N` should drop sharply if caching
  hit. The Anthropic API also reports `cache_creation_input_tokens`
  and `cache_read_input_tokens` — the chat doesn't surface those yet,
  but you can verify by inspecting `[ai.anthropic]` request bodies
  (e.g. via a proxy) — system blocks should serialise as an array
  with `cache_control: {"type":"ephemeral"}` on the first cacheable
  entries.

### 3. `read_file` tool
- Open `hello.bas`. Don't attach anything.
- Send: "What's in the file I'm editing? List any includes you see."
- **Expected:** the assistant emits a `> read_file(...)` blockquote
  in its bubble, and the next round's text reflects the file's
  contents. If `hello.bas` references another file, the model can
  call `read_file` again to pull that one in.
- **Negative case:** ask the model to read `/etc/hosts`. The
  refusal will be surfaced as an `isError` result; the model should
  apologise rather than retry.

### 4. `apply_patch` tool
- Open a FreeBASIC file and toggle agent mode on (the `agent` button
  in the bottom row). The active doc is auto-pinned as the edit
  target. The "live-edit" and "allow compile" checkboxes ungrey.
- Send: "Change the greeting string to 'hi there'."
- **Expected:** the assistant emits a `> apply_patch(...)` strip,
  the edit lands in the editor (Scintilla undo wraps it as one
  action — `Ctrl+Z` reverts), and the follow-up text confirms.
- **Negative case:** turn agent mode off and ask for an edit. The
  model should respond textually without trying to call the tool
  (the tool isn't in its `tools` array). If it does call it
  speculatively, the in-tool gating returns
  "Agent mode is off — apply_patch is unavailable.".

### 5. Cancel
- Send a long request likely to stream for several seconds.
- While streaming, click the Send button (now labelled "Cancel").
- **Expected:** the request aborts; the assistant bubble stops
  growing; the button reverts to "Send"; the chat shows
  "**Error:** Request cancelled." in red.

### 6. Token readout
- Send any message.
- **Expected:** assistant bubble ends with an italic
  `↑12345 ↓678` line. Numbers are real — Anthropic's actual usage.
  Providers without usage reporting (Gemini/Ollama/ClaudeCli/Mock)
  leave the line out.

### 7. `compile` tool — happy path
- Open a FreeBASIC file that compiles cleanly. Save it.
- Toggle agent mode on, then tick "Allow compile" (it ungreys after
  agent mode is on).
- Send: "Compile this and tell me if it builds."
- **Expected:** `> compile()` strip appears; the project's status
  bar shows "Compiling..." briefly; the next round's text reports
  success.

### 8. `compile` tool — error feedback
- Introduce a deliberate fbc error in the file (e.g. `Print x` where
  `x` is undeclared). Save.
- Agent + Allow compile on. Send: "Fix any compile errors in this
  file."
- **Expected:** the model calls `compile()`, sees the failure
  output, calls `apply_patch` to fix, calls `compile()` again to
  verify, and reports success. The dispatch loop will cap at 10
  rounds and the compile sub-cap at 3 invocations per turn — if
  the model loops, the tool returns "Compile invocation cap for
  this turn was reached." and the model has to either back off or
  the loop terminates.

### 9. `compile` tool — dirty / untitled rejection
- Open an untitled buffer (`File > New`). Agent on, allow compile on.
- Send: "Compile this."
- **Expected:** the model gets back
  "The active document has never been saved." and surfaces that
  to the user.
- Same for a saved doc with unsaved changes — the model is told
  "The active document has unsaved changes." rather than the IDE
  popping a save dialog mid-stream.

### 10. `compile` cancellation
- Trigger a long compile (a file with heavy includes works).
- While the AI-triggered compile is running, hit the chat panel's
  Cancel button.
- **Expected:** `CompilerManager::killProcess()` aborts fbc; the
  tool reports a `cancelled` status; the dispatch loop unwinds and
  the chat ends with the cancellation error.

### 11. Session-only "Allow compile"
- Tick "Allow compile" during a session.
- Restart fbide.
- **Expected:** the checkbox is unchecked again. Opt-in every time
  is intentional — never persisted to config.

### 12. Whitespace-normalised patch matching
- File with 2-space indent. Agent on. Tell the model to change a
  specific indented line and (if it cooperates) deliberately encourage
  it to emit the SEARCH using 4-space indent. The `apply_patch` tool
  should still succeed via the normalised-whitespace fallback.
- Easier: trigger from the unit tests
  (`ctest -R FindPatchMatch.NormalisedWhitespace`).

### 13. Tool gating regression spot-checks
- Agent off, Allow compile off → only `read_file` exposed.
  Verify by sending "What tools do you have available?".
- Agent on, Allow compile off → `read_file` + `apply_patch`.
- Agent on, Allow compile on → `read_file` + `apply_patch` +
  `compile`.

### 14. `[ai] enable_tools = false`
- Set the config flag, restart.
- **Expected:** tools array is empty in every request; the model
  cannot use any of the three even in agent mode. The legacy
  SEARCH/REPLACE-blocks path takes over for edits when agent mode
  is on.

## Deferred — inside the arc, scoped down

These were listed as TODOs in `docs/ai-tools-plan.md` but were
intentionally deferred during implementation. Each is small enough
to land as a follow-up commit if real usage shows the need.

- **`ToolCallView` specialised renderer.** The chat view shows tool
  calls as a generic blockquote with the raw JSON arguments
  (P2.12 + the same path covers `apply_patch` and `compile`).
  Planned specialised renderers:
  - `apply_patch` → red/green diff strip mirroring the existing
    SEARCH/REPLACE block style.
  - `compile` → folded output block with first error highlighted,
    expandable on click.
  - `read_file` → "≡ path · N lines" pill.
  Requires non-trivial work in the 1349-line custom-painted
  `AiChatView` — better to do as one focused commit rather than
  smuggle into the arc.

- **`MatchKind` surfaced via `apply_patch`.** The tool result is
  still `{status: applied|no_match}`. Phase 6 added the enum but
  the wire format didn't follow. Trivial change: add
  `match_kind: exact|trimmed_newline|normalized_whitespace` to the
  JSON when applied — gives the model a hint to be more precise
  on the next round when normalisation kicked in.

- **Anchor-bracket patch matching.** The plan's P6.2 — first 2 +
  last 2 non-blank lines uniquely bracket a region. Intentionally
  skipped: too easy to span unintended content, and the compile
  loop already gives the model self-correction when a patch lands
  wrong. Revisit only if real usage shows patches missing often
  with no compile feedback to lean on.

- **Per-call tool confirmation.** No "are you sure you want to run
  this tool?" prompt; tools are either always-on (`read_file`,
  bounded and read-only) or session-toggled (`apply_patch`,
  `compile`). Worth adding if a tool with broader side-effects
  ever ships.

- **Pre-existing tidy findings exposed by my edits**, left for
  separate cleanup commits:
  - `AiChatPanel.cpp` — `AttachMenuId` unscoped enum with `int`
    base (`cppcoreguidelines-use-enum-class`,
    `performance-enum-size`).
  - `AiChatPanel.cpp` — internal-container `operator[]` use
    (`cppcoreguidelines-pro-bounds-avoid-unchecked-container-access`).
  - `MockProvider.cpp` — static initialisation may throw
    (`bugprone-throwing-static-initialization`).

## Future work — out of the arc

Items discussed when the arc was scoped, intentionally excluded
from this delivery. Listed roughly in priority order.

### Capability

- **Conversation persistence.** History is in-memory only; restart
  drops it. SQLite or one JSON file per conversation in
  `~/.fbide/ai-history/` plus a sidebar with conversation list +
  "new conversation" button would unlock "go back to yesterday's
  debug session." Non-trivial UI work — own arc.

- **Multi-provider tool support.** Only Anthropic carries tools
  through this arc. Gemini's tool wire format is similar but
  not identical (`functionDeclarations`,`functionCall`,
  `functionResponse`); Ollama's `tools` field uses the OpenAI
  shape. Once Anthropic is proven stable, Gemini is the next
  candidate. Ollama only worth doing if a local-tool-using model
  becomes practical.

- **Multi-provider usage parsing.** `onUsage` is wired through the
  sink for everyone but only Anthropic populates it. Gemini
  reports `usageMetadata.promptTokenCount` /
  `candidatesTokenCount`; Ollama's `prompt_eval_count` /
  `eval_count` lands in the NDJSON final message. Small per-
  provider parser additions.

- **Image attachments.** Drag a screenshot into the chat and have
  the model see it. Anthropic and Gemini both support vision via
  base64 content blocks. Needs an `ImageContextItem` in
  `AiContext`, schema-aware serialisation in providers, and drag-
  drop on the chat panel.

- **Compile error → AI seamless UX (#9 from the original
  suggestion).** Discussed and parked. Options were (a) inline
  affordance at the error site, (b) "Ask AI" link in the build
  output panel, (c) right-click "Explain with AI", (d) auto-
  attach last failure as a context chip. (b) + (d) is the
  highest-leverage combination if revisited.

- **`run` tool.** Compile-only is the right v1 stance. Adding a
  `run` tool means executing model-chosen binaries — needs per-
  invocation user confirmation at minimum. Probably worth
  shipping eventually for a true edit/verify/run loop.

### UX

- **Per-conversation system prompt override.** A second tab on the
  chat panel for "you are reviewing FreeBASIC for a learner",
  "you write succinct comments", etc. Doesn't change anything in
  the wire — just an extra `AiContent` block prepended to system.

- **`@filename` autocomplete in the input box.** Instead of (or
  alongside) the attach menu. Type `@fo` → dropdown of matching
  files in the active doc's directory / open tabs.

- **Streaming retry on transient 5xx.** Single-shot today.
  Exponential backoff for 502/503/504 inside `WebStreamProvider`
  would let an Anthropic blip not kill the conversation.

- **Cost estimate from token readout.** The `↑N ↓M` strip is
  there; multiplying by `cost_per_mtok` from a per-provider
  config field would give a per-turn `$0.0123` annotation. Trivial
  once anyone wants to see it.

- **Model picker UI.** Explicitly excluded from this arc (the user
  edits config to switch models). Worth a small dropdown next to
  the agent button when a use case demands it.

### Security / operability

- **Keychain-backed API keys.** Plaintext in `preferences.toml`
  today. macOS Security.framework keychain + an opaque ref in
  config would be the right shape. Excluded from this arc.

- **Key redaction in logs / error messages.** Pre-existing concern;
  `WebStreamProvider`'s 401 error path could echo the auth header
  back in a log if `wxWebRequest`'s error description includes the
  request header dump. Audit + redact. Skipped from this arc.

- **`AiChatPanel` cancel button localised label.** Currently
  hard-codes "Cancel". The send button uses
  `m_ctx.tr("panels.aichat.send")`. Add
  `panels.aichat.cancel` to the i18n table once translations are
  added.

- **Hash collision in `m_appliedPatches`.** Documented trade-off
  in `AiManager::patchKey` (size_t collision → one patch silently
  skipped, no data loss). Could store full text or a longer hash
  pair for true uniqueness; cost is multi-KB strings in memory.
  Unlikely to matter in practice.

### Things explicitly *not* on the roadmap (per conversation)

- **Symbol-aware context** (the original suggestion #10). Dropped:
  the symbol browser is keyword-driven, no real semantic index.
  Becomes viable only after intellisense lands.

### Testing / infrastructure

- **GUI tests on macOS.** Currently skipped (`ctest -LE gui`); the
  Windows / Linux CI surfaces. Out of scope for the arc.

- **`AiChatPanel` cancel-toggle test.** A UI test that
  asserts the button label flips on busy and back. Skipped because
  the chat panel needs a full wxApp / Context to instantiate; the
  cancel logic itself is covered indirectly via
  `ToolDispatchLoopTests.BailsWithErrorAfterReachingRoundCap` and
  `WebStreamProvider`'s `State_Cancelled` path.

- **End-to-end smoke test against a real Anthropic key.** Worth
  running before merging. The unit tests prove the wire shape
  and the local state machine — the live integration verifies the
  cache + tool-use + compile loop in the actual IDE.
