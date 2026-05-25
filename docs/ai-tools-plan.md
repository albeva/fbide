# AI tools arc — implementation plan

Six-phase plan to extend the existing AI integration with prompt caching,
tool use, an in-loop compile tool, and supporting UX. Each phase is
independently shippable and test-driven.

Scope: `src/lib/ai/`. No changes outside the AI module except for tool
implementations that bridge into `CompilerManager` / `DocumentManager`.

## Locked-in decisions

- **No `search_workspace` tool.** fbide has no project/workspace model.
- **`read_file` is scoped** to the active document's directory subtree and
  open tabs. No `..` escape, no absolute paths outside that root.
- **`apply_patch` operates only on the pinned `EditTargetItem`.** Refuses
  when nothing is pinned.
- **`compile` is opt-in per session** via an "Allow compile" checkbox
  under the prompt box, in the same row as agent toggle / live-edit.
  **Not persisted** — starts off every session, including after restart.
- **No model picker UI**, no conversation persistence, no keychain, no
  log redaction. All explicitly deferred.
- **No `run` tool.** Compile only; executing model-chosen binaries is
  out of scope.
- **Stream-parsed SEARCH/REPLACE stays as fallback** for non-tool
  providers. Tool-capable providers stop receiving the SEARCH/REPLACE
  rubric in the system prompt — model uses `apply_patch` instead.

## Cross-phase architecture deltas

### `AiTypes.hpp`

- Replace `AiRequest::system` (flat `wxString`) with
  `std::vector<AiContent> system;` where
  `struct AiContent { wxString text; bool cacheable = false; };`.
  Providers that don't cache concatenate.
- Add tool types:
  ```cpp
  struct AiTool {
      wxString name;
      wxString description;
      wxString inputSchemaJson; // raw JSON schema text
  };
  struct AiToolCall {
      wxString id;
      wxString name;
      wxString argumentsJson;
  };
  struct AiToolResult {
      wxString toolUseId;
      wxString content;
      bool isError = false;
  };
  ```
- Extend `AiMessage` with optional `std::vector<AiToolCall> toolCalls`
  (assistant) and `std::vector<AiToolResult> toolResults` (user).
- Add `std::vector<AiTool> tools` to `AiRequest`.

### `AiProvider`

- Add capability flags (default `false`):
  ```cpp
  virtual auto supportsPromptCaching() const -> bool { return false; }
  virtual auto supportsTools() const -> bool { return false; }
  ```
- Extend `send` signature with a `ToolCallHandler`:
  ```cpp
  using ToolCallHandler = std::function<void(AiToolCall)>;
  virtual void send(const AiRequest&, ChunkHandler, ToolCallHandler,
                    ResponseHandler) = 0;
  ```
  Existing 3-arg call sites get a wrapper supplying a no-op tool handler.
  `MockProvider`/`Gemini`/`Ollama`/`ClaudeCli` keep their current text-only
  paths; only `AnthropicProvider` actually dispatches tool calls in v1.

---

## Phase 1 — Prompt caching (Anthropic)

**Goal.** Mark stable parts of each request as cacheable; follow-up turns
in a conversation reuse the cached prefix at ~10% cost.

### Changes

- `AiContext::buildText()` returns `std::vector<AiContent>` instead of a
  flat `wxString`. One block per item: base prompt + agent rubric is one
  block; each `FileContextItem` / `EditTargetItem` is its own block, all
  marked `cacheable = true`. `BufferContextItem` is **not cacheable**
  (changes per keystroke).
- `AiManager::sendMessage` builds the `system` vector from
  `AiContext::buildText()` + the configured system prompt.
- `AnthropicProvider::buildBody`:
  - When `system.size() == 1` and not cacheable, emit the legacy string
    form for compatibility.
  - Otherwise emit `system: [{ type: "text", text: "...",
    cache_control: { type: "ephemeral" } } ...]`. Cap at the first 3
    cacheable file blocks + the system block (Anthropic ≤4 breakpoints).
- Other providers get a `joinSystem(blocks)` helper that concatenates.

### Tests

- Extend `AnthropicProviderTests` with body-shape assertions for:
  - flat string path (single non-cacheable block);
  - array path with `cache_control` on the right entries;
  - 4-breakpoint cap.
- New `AiContextTests` cases for block emission per item type.

### Risks

Low. Wire change local to `AnthropicProvider::buildBody`. Min cacheable
size (1024 tokens) silently no-ops below threshold — acceptable.

---

## Phase 2 — Tool scaffold + `read_file`

**Goal.** Model can pull additional context on demand without the user
pre-attaching every file.

### Changes

- Type additions per cross-phase deltas.
- New `src/lib/ai/tools/`:
  - `ToolRegistry.{hpp,cpp}` — owns a `std::vector<std::unique_ptr<Tool>>`,
    looks up by name, dispatches by `AiToolCall`. Sync API for v1
    (`auto invoke(const AiToolCall&) -> AiToolResult`).
  - `ReadFileTool.{hpp,cpp}` — implements `read_file(path: string)`:
    - Resolves relative to active doc's directory, or matches an open
      tab title; rejects `..` escape and out-of-root absolute paths.
    - Size cap 256 KB; returns text with a header `=== <path> ===`.
    - Returns `AiToolResult{ isError = true }` on out-of-scope or read
      failure with a human-readable message.
- `AiManager` gains `runTurn()` (replaces the in-line streaming dispatch
  in `sendMessage`):
  1. Send request with current `tools` from registry (only when
     `m_provider->supportsTools()`).
  2. Collect streamed text chunks via `ChunkHandler` (existing
     behaviour preserved for the caller).
  3. Collect `AiToolCall`s via the new `ToolCallHandler`.
  4. On `onComplete`:
     - If no tool calls, append assistant message, fire caller's
       `onComplete`, done.
     - Otherwise append assistant message (with tool calls), invoke
       each tool, append a user message holding the `AiToolResult`s,
       re-enter `runTurn()`. Hard cap 10 rounds; on exceeding, fire
       error to caller.
- `AnthropicProvider`:
  - `buildBody`: serialize `tools` array; serialize assistant messages
    with tool_use content blocks; serialize user messages with
    tool_result content blocks.
  - `parseLine`: handle `content_block_start type=tool_use`,
    accumulate `input_json_delta` into per-block JSON buffer, finalize
    on `content_block_stop`. Emit assembled `AiToolCall` through the
    new handler.
- `AiChatView`: render tool-call strip as a one-line block in the
  assistant bubble — `> read_file("Foo.bi") → 412 bytes`.
- Config: `[ai] enable_tools = true` (default `true`); set `false` to
  globally disable.

### Tests

- New `ToolDispatchTests`: drive `AiManager` with a `MockProvider`
  configured to emit a canned tool call sequence; assert round-trip.
- `ReadFileToolTests`: in-scope path, out-of-scope path, `..` escape,
  absolute outside root, open tab by name, missing file, oversize file.
- `AnthropicProviderTests`: tool serialization in body; parse a
  captured SSE fixture containing tool_use deltas.

### Risks

Medium. SSE parsing becomes stateful (per-block JSON buffer). Mitigated
by captured fixtures and a small state machine in `parseLine`.

---

## Phase 3 — `apply_patch` tool

**Goal.** Replace stream-parsed SEARCH/REPLACE with a real tool that
gives the model structured success/failure feedback.

### Changes

- New `tools/ApplyPatchTool.{hpp,cpp}`:
  - Signature: `apply_patch(search: string, replace: string)`.
  - Requires `m_context.editTarget() != nullptr` and agent mode on;
    refuses otherwise with `isError = true`.
  - Delegates to existing `AiManager::applyPatch(search, replace,
    recordAlways=true)`.
  - Returns JSON-shaped text: `{"status":"applied|no_match|ambiguous",
    "match_count":N, "line":L, "match_kind":"exact|trimmed_newline"}`.
- `AiManager::buildSystemPrompt`: when provider supports tools AND agent
  mode is on, drop the SEARCH/REPLACE rubric from the system prompt
  (model uses the tool). When provider lacks tool support, rubric stays.
- `AiChatView`: render apply_patch tool calls with same visual cue as
  parsed SEARCH/REPLACE blocks (search + replace diff strip); Apply /
  Reject buttons hidden — action already happened.

### Tests

- `ApplyPatchToolTests`: no edit target pinned, agent mode off, no
  match, single match success, multiple matches (ambiguous), Scintilla
  undo grouping verified.

### Risks

Low–medium. Most apply machinery exists. Main risk is double-application
during the migration; gated by `supportsTools()` so unsupported
providers keep stream-parsed path.

---

## Phase 4 — Cancel + token readout

**Goal.** Ship the UX prerequisites for the compile loop before Phase 5
lands. Without these the compile loop is opaque.

### Cancel

- Add `AiProvider::cancel()` virtual (default no-op).
- `WebStreamProvider::cancel()` calls `wxWebRequest::Cancel()` and
  marks the request as cancelled so `onComplete` is not double-fired.
- `AiManager::cancel()` cancels the provider AND aborts the tool
  dispatch loop (clears pending state, fires caller's `onComplete`
  with `ok=false, error="cancelled"`).
- `AiChatPanel`: send button doubles as cancel while a request is in
  flight (label switches "Send" ↔ "Cancel").

### Token readout

- Extend `AiResponse` with `int inputTokens = 0; int outputTokens = 0;`
  populated by providers that report usage (Anthropic, Gemini, Ollama
  all do).
- `AnthropicProvider::parseLine`: read `usage.input_tokens` and
  `usage.output_tokens` from `message_start` / `message_delta` events.
- `AiChatView`: append a small token readout to the assistant bubble
  footer: `↑ 1,234  ↓ 567`. No cost estimate in v1.

### Tests

- `AnthropicProviderTests`: usage parsing from fixture.
- `AiChatPanelTests` (if exists, else skip): cancel toggles button
  label, second click cancels.

### Risks

Low. Cancel needs to interact cleanly with mid-tool-call state.

---

## Phase 5 — `compile` tool

**Goal.** Model can verify its edits by triggering a build and reading
the output. The full edit → compile → fix loop.

### Changes

- New "Allow compile" checkbox in `AiChatPanel`, bottom row under the
  prompt box, right of "live-edit". Greyed when agent mode is off.
  **Not persisted** — pure runtime state on the panel; resets every
  session.
- New `tools/CompileTool.{hpp,cpp}`:
  - Signature: `compile()` — no args.
  - Targets the pinned `EditTargetItem` if present, else active
    document, else refuses.
  - Saves the target's buffer to disk first (Scintilla edits don't
    touch disk; fbc reads disk). Honor `DocumentManager`'s save flow;
    refuse if save fails or user cancels a dirty-prompt.
  - Bridges into `CompilerManager` via a new headless variant of
    `compile()` that returns a future/callback with `BuildTask`'s
    captured output and exit status. Concretely: add
    `CompilerManager::compileHeadless(Document&, std::function<void(
    bool ok, int exitCode, wxArrayString output)>)` that creates a
    `BuildTask` configured not to pop the compiler-log dialog.
  - Truncates output to 16 KB, preserving head + tail with a
    `[truncated N bytes]` marker.
  - Returns JSON-shaped text: `{"status":"ok|failed","exit_code":N,
    "output":"..."}`.
- `ToolRegistry`: invocation becomes async — `invoke` takes a
  completion callback instead of returning. `AiManager::runTurn` waits
  for all tool callbacks before re-entering the loop.
- Per-turn cap: ≤3 `compile` invocations per turn (in addition to the
  10-round overall cap).
- Cancellation: `AiManager::cancel()` calls
  `CompilerManager::killProcess()` if a compile is in flight.

### Tests

- `CompileToolTests`: no target / target dirty (saves first) / save
  cancelled / success / failure / per-turn cap enforced.

### Risks

Medium. First async tool; first tool with disk side-effects. Need to
verify `BuildTask`'s output capture is complete by the time the
completion callback fires.

---

## Phase 6 — Sturdier patch matching (deferred)

**Goal.** Reduce silent patch misses. Demoted because Phase 5's compile
loop already gives the model self-correction; this only saves turns.

Skip unless real usage shows patches missing often after Phase 5.

### Sketch (when needed)

Extend `findPatchMatch` with ordered fallbacks beyond the existing
trailing-newline retry:

1. Leading/trailing whitespace normalisation per line, with replacement
   reindented to match source's actual leading whitespace.
2. Unique-anchor bracket: first 2 + last 2 non-blank lines of `search`
   uniquely bracket a region in source; replace between. Gated by an
   opt-in flag; reports ambiguity when bracketed span exceeds 2× the
   search line count.

Add `MatchKind` enum to `PatchMatch` so callers can report kind to the
model.

---

## Out of scope (explicitly)

- Other providers getting tool support (Gemini/Ollama tool formats
  differ; revisit once Anthropic path is stable).
- Conversation persistence (#8).
- Model picker UI (#5).
- Compile error → AI seamless UX (#9) — subsumed by Phase 5's tool.
- Keychain / API key redaction (#11, #12).
- Image attachments.
- `@filename` autocomplete in the input box.
- `run` tool (executing built binaries).

---

## Open questions

- **Tool-call ordering when multiple per turn.** Anthropic can emit
  multiple `tool_use` blocks in one assistant message. v1 executes them
  in arrival order. Parallel execution unnecessary for v1 — `read_file`
  is microseconds, `compile` is naturally sequential.
- **Streaming display of tool_use args.** Show the strip with
  `(running...)` while partial JSON streams in, or wait until the call
  is fully assembled before rendering? Recommendation: wait — partial
  JSON in the UI is noisier than helpful.
- **Cancel during tool execution.** Once a tool callback is in flight
  (e.g. compile running), cancel should kill the tool and short-circuit
  the loop. `CompilerManager::killProcess()` handles compile; read_file
  is sync so cancellation is moot.

---

## Review

First-pass review of the plan above. Findings ordered by phase.

### Cross-cutting

- **`AiContent` rollout order matters.** Changing `AiRequest::system` from
  `wxString` to `std::vector<AiContent>` breaks every provider's
  `buildBody`/serialization at once. Mitigation: introduce
  `AiContent` + a free helper `joinSystem(const std::vector<AiContent>&)
  -> wxString` *first*, with `AiRequest::system` still a `wxString`
  computed via the helper at the `AiManager` boundary. Then in a
  separate step flip `AiRequest::system` to the vector and have
  non-cacheable providers call the helper internally. Two-step keeps
  each commit green.
- **Tool dispatch should be async from the start.** The plan has Phase 2
  use a sync `ToolRegistry::invoke` and Phase 5 refactor to async. That
  refactor is non-trivial (state spread across `runTurn`'s recursive
  calls). Cheaper to design async from the start: `invoke(call, ResultHandler)`
  with `read_file` invoking its handler synchronously. Phase 5 then adds
  `CompileTool` without touching the dispatch core.
- **Tool exposure vs registration.** Tools are always *registered* in
  `ToolRegistry` at `AiManager` construction; whether they appear in the
  request's `tools` array depends on runtime toggles (agent mode,
  allow-compile, `enable_tools` config). Keeps registry static, gating
  centralised in `AiManager::buildRequest`.
- **Synthetic tool messages persist in history.** Anthropic requires the
  tool_use/tool_result pair to be present in subsequent turns. The
  dispatch loop must append both to `m_history`. The existing
  `clear()` correctly drops them along with everything else.
- **3-arg `send()` wrapper unnecessary.** `AiManager` is the only
  caller. Just change its callsite to the 4-arg form. Drop the wrapper
  from the plan.

### Phase 1

- Anthropic accepts both `system: "..."` (string) and
  `system: [...]` (array). Plan's "use string when nothing is cacheable"
  is correct. Worth explicit assertion in tests for both shapes.
- Cap of 4 cache breakpoints is fine, but breakpoint *ordering* matters
  — Anthropic caches the longest matching prefix. Block order should be:
  base system prompt first, then file blocks in stable order
  (path-sorted, not insertion-sorted) so an attach/detach toggle of a
  later block doesn't bust caches for earlier ones.

### Phase 2

- **SSE state machine non-trivial.** `content_block_start type=tool_use`
  carries the tool name + id but the input arrives as
  `input_json_delta` events with partial JSON strings until
  `content_block_stop`. Per-block buffer must key by block index. Must
  also handle the case where `content_block_delta` for text and tool_use
  blocks interleave in the same message. Plan should explicitly call
  out a fixture-driven test for interleaved blocks.
- **`enable_tools` config flag** has two layers — `supportsTools()` AND
  `enable_tools=true`. Both must be true to send a `tools` array. Single
  predicate `AiManager::toolsEnabled()` keeps it tidy.
- **Tool-call rendering in the chat view.** Plan says "render as a
  one-line strip". The view currently renders message text with
  embedded code blocks. Tool calls aren't in the message text — they
  live on the `AiMessage` alongside content. Renderer needs a new path:
  walk `message.toolCalls` after rendering text. Worth flagging as new
  view code, not a one-line addition.

### Phase 3

- **`apply_patch` tool-call rendering** is more involved than Phase 2's
  generic strip — it needs to show the diff (search vs replace) so the
  user can verify. Introduce a `ToolCallView` interface with a
  per-tool specialisation: generic strip for `read_file`/`compile`,
  diff strip for `apply_patch`.
- **Two parallel patch-application paths during the transition** —
  stream-parsed (for non-tool providers) and tool-driven (for
  Anthropic). They share `AiManager::applyPatch`, so divergence risk is
  contained, but tests must cover both paths against the same fixture
  doc.

### Phase 4

- **Cancel button: muddled with send button.** Switching the same
  control's label is space-efficient but conventional Send/Cancel are
  separate (Send fades to grey, Cancel appears beside it). Revisit
  during implementation; not a plan blocker.
- **Token readout placement.** Bubble footer is fine but the streaming
  bubble doesn't have a footer yet — needs new layout code. Alternative:
  show in status-bar-like strip at the bottom of the panel. Pick during
  implementation.

### Phase 5

- **Untitled documents.** `CompilerManager::compile()` already prompts
  for save via `ensureSaved()`. The headless variant must honour the
  same flow but cannot pop dialogs from a tool callback — instead,
  refuse with `isError = true` and a message asking the user to save
  first. Save-on-compile silently is too surprising for an untitled
  doc.
- **Race with manual compile.** If the user manually triggers compile
  while a tool-driven compile is in flight, `CompilerManager` replaces
  `m_task`. The tool's callback never fires. Fix: tool subscribes to
  task completion via `BuildTask`'s callback, and if the task is
  replaced before completion the tool reports
  `status: "cancelled", error: "compile aborted"`. Requires `BuildTask`
  to invoke its callback on early destruction — verify or add.
- **Per-turn compile cap counter** lives on `AiManager`, reset at the
  start of each `sendMessage`. ≤3 compiles per call. Hard error to the
  model on the 4th attempt with `isError = true, content: "compile
  invocation cap reached for this turn"`.
- **`compileHeadless` should not replace existing `compile()`.** Add as
  a separate entry point that takes a callback and avoids dialog
  popups, to keep the existing UI compile flow untouched.

### Phase 6

No changes — deferred.

### Open question follow-ups

- **Streaming display of tool_use args:** confirmed — wait for full
  assembly before rendering. Partial JSON in the UI is noise.
- **Multiple parallel tool calls per turn:** serialise in arrival
  order. Anthropic's tool_use blocks already arrive sequentially.
  Parallelism gains nothing for `read_file` (fast) or `compile`
  (single-in-flight).

---

## Derived TODOs

Phase-by-phase, in dependency order. Each line is a single discrete
work item. `t:` prefix marks a test deliverable that ships with the
phase.

### Phase 0 — preparation (~no behaviour change)

- [ ] P0.1 Add `struct AiContent { wxString text; bool cacheable = false; };`
      to `AiTypes.hpp`.
- [ ] P0.2 Add free helper `auto joinSystem(const std::vector<AiContent>&)
      -> wxString` to `AiTypes.{hpp,cpp}`.
- [ ] P0.3 Change `AiManager::sendMessage` to build a
      `std::vector<AiContent>` from `AiContext` + system prompt, then
      flatten via `joinSystem` into the existing
      `AiRequest::system` (still `wxString`). No wire change.

### Phase 1 — prompt caching

- [ ] P1.1 Change `AiContext::buildText()` return type to
      `std::vector<AiContent>`; emit one block per item; cacheable
      true for `FileContextItem`/`EditTargetItem`, false for
      `BufferContextItem`.
- [ ] P1.2 Sort cacheable blocks by path before emission (stable cache
      prefix).
- [ ] P1.3 Flip `AiRequest::system` to `std::vector<AiContent>`. Update
      non-Anthropic providers to call `joinSystem` internally.
- [ ] P1.4 Add `AiProvider::supportsPromptCaching()` virtual, default
      `false`. Override `true` in `AnthropicProvider`.
- [ ] P1.5 `AnthropicProvider::buildBody`: emit string form when no
      cacheable blocks; otherwise emit array form with `cache_control`
      on cacheable blocks, capped at 4.
- [ ] t: P1.6 `AnthropicProviderTests`: assert string form (no cache),
      array form (some cacheable), 4-breakpoint cap.
- [ ] t: P1.7 `AiContextTests`: block emission per item type.

### Phase 2 — tool scaffold + read_file

- [ ] P2.1 Add `AiTool`, `AiToolCall`, `AiToolResult` to `AiTypes.hpp`.
- [ ] P2.2 Extend `AiMessage` with `toolCalls` / `toolResults` vectors.
- [ ] P2.3 Add `AiRequest::tools` vector.
- [ ] P2.4 Add `AiProvider::supportsTools()` virtual; replace `send`
      signature with 4-arg form (`ChunkHandler`, `ToolCallHandler`,
      `ResponseHandler`). Update `AiManager::sendMessage` callsite.
      Update `MockProvider`, `Gemini`, `Ollama`, `ClaudeCli`,
      `WebStreamProvider` to accept the new handler (no-op).
- [ ] P2.5 New `src/lib/ai/tools/ToolRegistry.{hpp,cpp}` with async
      `invoke(call, ResultHandler)` interface from the start.
- [ ] P2.6 New `src/lib/ai/tools/ReadFileTool.{hpp,cpp}`:
      resolve-against-active-doc-dir-or-open-tab, reject `..` and
      out-of-root absolute, size cap 256 KB.
- [ ] P2.7 Wire `ToolRegistry` into `AiManager`. Register `read_file`
      at construction.
- [ ] P2.8 New `AiManager::buildRequest()` helper that assembles tools
      based on gating predicates (`toolsEnabled()`, agent mode,
      allow-compile flags from chat panel).
- [ ] P2.9 Refactor `AiManager::sendMessage` into a multi-round
      dispatch: send → collect text + tool calls → on completion, if
      tool calls present, invoke via registry, append synthetic
      messages, re-enter. Hard cap 10 rounds.
- [ ] P2.10 `AnthropicProvider::buildBody`: serialize `tools` array,
       assistant tool_use content blocks, user tool_result content
       blocks.
- [ ] P2.11 `AnthropicProvider::parseLine`: tool_use SSE state machine
       (block-index-keyed JSON buffer; finalise on `content_block_stop`).
- [ ] P2.12 `AiChatView`: render `toolCalls` strip after message text.
       Generic format `> name(args) → summary`.
- [ ] P2.13 Add `[ai] enable_tools` config flag, default true.
- [ ] t: P2.14 `ReadFileToolTests`: in-scope, `..` escape, abs outside
       root, open-tab match, missing, oversize.
- [ ] t: P2.15 `ToolDispatchTests`: `AiManager` + `MockProvider`
       emitting canned tool calls; assert round-trip and 10-round cap.
- [ ] t: P2.16 `AnthropicProviderTests`: tool body serialization;
       parse fixture with interleaved text+tool_use SSE.

### Phase 3 — apply_patch tool

- [ ] P3.1 New `src/lib/ai/tools/ApplyPatchTool.{hpp,cpp}`. Gated on
      `editTarget != nullptr` and agent mode. Delegates to
      `AiManager::applyPatch(..., recordAlways=true)`. JSON-shaped
      result.
- [ ] P3.2 Register `apply_patch` in `ToolRegistry`. Gate inclusion in
      `buildRequest()` on agent mode.
- [ ] P3.3 `AiManager::buildSystemPrompt`: drop SEARCH/REPLACE rubric
      when `supportsTools()` AND `m_agentMode`. Keep rubric otherwise.
- [ ] P3.4 `AiChatView`: introduce `ToolCallView` abstraction;
      specialised renderer for `apply_patch` showing search/replace
      diff strip; generic renderer for `read_file`/others.
- [ ] t: P3.5 `ApplyPatchToolTests`: no target / agent off / no match
      / single match success / ambiguous / undo grouping.

### Phase 4 — cancel + token readout

- [ ] P4.1 Add `AiProvider::cancel()` virtual (no-op default).
- [ ] P4.2 `WebStreamProvider::cancel()`: call `wxWebRequest::Cancel()`;
      guard against double-`onComplete`.
- [ ] P4.3 `AiManager::cancel()`: cancel provider, abort tool dispatch
      loop, fire caller's `onComplete` with `error="cancelled"`.
- [ ] P4.4 `AiChatPanel`: cancel control while a request is in flight
      (button toggle or separate cancel button — TBD during impl).
- [ ] P4.5 Add `int inputTokens / outputTokens` to `AiResponse`.
- [ ] P4.6 `AnthropicProvider::parseLine`: populate usage from
      `message_start` / `message_delta` events.
- [ ] P4.7 `AiChatView`: render `↑N ↓M` token strip after streaming
      bubble completes.
- [ ] t: P4.8 `AnthropicProviderTests`: usage parsed correctly from
      fixture.

### Phase 5 — compile tool

- [ ] P5.1 Add "Allow compile" checkbox to `AiChatPanel`, bottom row
      under prompt. Greyed when agent mode off. Not persisted.
- [ ] P5.2 Add `CompilerManager::compileHeadless(Document&, callback)`
      — separate entry point, no dialog popup, callback receives
      `(bool ok, int exitCode, wxArrayString output)`.
- [ ] P5.3 Verify or extend `BuildTask` to fire its completion
      callback on early destruction (so a replaced task signals
      cancellation to subscribers).
- [ ] P5.4 New `src/lib/ai/tools/CompileTool.{hpp,cpp}`. Targets
      pinned edit target → active doc → refuse. Requires saved doc
      (refuse with isError if dirty/untitled). Calls
      `compileHeadless`. Truncates output to 16 KB head+tail.
- [ ] P5.5 Register `compile` in `ToolRegistry`. Gate inclusion in
      `buildRequest()` on agent mode AND `m_allowCompile` (new
      `AiManager` flag set by panel checkbox).
- [ ] P5.6 Per-turn compile cap: counter on `AiManager`, reset at start
      of each top-level `sendMessage`. Refuse on 4th attempt.
- [ ] P5.7 `AiManager::cancel()`: also call
      `CompilerManager::killProcess()`.
- [ ] P5.8 `AiChatView::ToolCallView` for `compile`: status + truncated
      output preview, expandable.
- [ ] t: P5.9 `CompileToolTests`: no target / target dirty / save
      cancelled / success / failure / per-turn cap / cancellation.

### Phase 6 — patch matching hardening (deferred)

- [ ] P6.1 Extend `findPatchMatch` with whitespace-normalised retry.
- [ ] P6.2 Extend `findPatchMatch` with anchor-bracket fallback (opt-in).
- [ ] P6.3 Add `MatchKind` enum, surface via `ApplyPatchTool` result.
- [ ] t: P6.4 `PatchTests` fixtures for new modes + ambiguity.

---

## Next step

Phase 0 + Phase 1 are the natural first commit pair — preparation +
caching with no UI change. Sign-off needed before coding.
