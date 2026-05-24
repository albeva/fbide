# AI module review — `src/lib/ai/`

A senior-level review of everything under `src/lib/ai/`: providers,
`AiManager`, `AiContext`, `AiChatPanel`, `ContextTagBar`, and the chat
view + highlighter + action bar. Complements [chat-review.md](chat-review.md),
which focused on layout / rendering inside the chat view; this one focuses
on the rest — providers, domain state, and architecture.

Overall hygiene is good: `NO_COPY_AND_MOVE` consistent, `Unowned<T>` /
`make_unowned` consistent, trailing-return everywhere, `[[nodiscard]]` on
observers, designated initializers, `std::exchange` to guarantee one-shot
callbacks. Biggest leverage points: a shared base for the three HTTP/SSE
providers, splitting `AiChatView`, and lifting patch lifecycle out of the
view into `AiManager`.

## Correctness / bugs

1. **`m_appliedPatches` never cleared and survives `AiManager::clear()`.**
   `AiChatView::autoApplyPatches` (`AiChatView.cpp:1423-1438`) inserts a
   per-patch text key into `m_appliedPatches` and never removes it. After
   "clear conversation" the set still holds keys from the prior session, so
   a textually identical SEARCH/REPLACE in a *new* conversation is silently
   skipped under live-edit. Also a slow memory leak across long sessions.
   **Action:** clear the set when the history shrinks (or hook
   `AiManager::clear`).

2. **Live-edit may apply mid-stream patches that are still in flux.**
   `setMessages` runs `autoApplyPatches` on every streaming throttle tick.
   A patch can match at chunk N but its surrounding text mutate by chunk
   N+1 (next chunk extends the SEARCH text); the second variant generates
   a new key and may re-apply on top of the partial edit.
   **Action:** auto-apply only when md4c reports the fence closed; or
   keep a per-(bubble, blockIndex) "fence-closed" flag that flips once.

3. **`AiManager::clear()` doesn't reset `ClaudeCliProvider::m_sessionId`.**
   The provider's `request.messages.size() <= 1` heuristic
   (`ClaudeCliProvider.cpp:34-36`) catches the common case by accident.
   Load-bearing on heuristics is fragile.
   **Action:** add an optional `AiProvider::reset()` hook; call it from
   `AiManager::clear`.

4. **`CodeHighlighter` leaks the lexer if its ctor body throws.**
   `m_lexer(FBSciLexer::Create())` runs before
   `configureFbWordlists(...)`. If the latter throws, `m_lexer` never gets
   `Release()`'d.
   **Action:** wrap in `std::unique_ptr<Scintilla::ILexer5,
   ReleaseDeleter>` or do the configure in a try block.

5. **`linkAt` opens arbitrary URI schemes in `wxLaunchDefaultBrowser`.**
   A malicious model reply could embed `file://`, `vbscript:`, or a
   platform-specific scheme. Practical risk is low (user opts into the
   provider), but the fix is cheap.
   **Action:** whitelist `http:` / `https:` / `mailto:` before launching.

6. **Dead code in `AiChatView::showActionBar`** (`AiChatView.cpp:759-761`).
   `const int blockHeight = block.height; (void)blockHeight;` is the
   "reserved for later" pattern CLAUDE.local.md says to avoid; `(void)mode;`
   is wrong (`mode` IS used at line 750).
   **Action:** delete both lines.

## Architecture / code-churn

7. **Three near-identical HTTP+stream providers.** Anthropic, Gemini,
   Ollama all carry: MI of `wxEvtHandler + AiProvider`, identical members
   (`m_request`, `m_buffer`, `m_onChunk`, `m_onComplete`, `m_busy`,
   `m_streamError`), identical `onRequestState` switch, identical
   `finish()`, identical dtor, identical line loop in `consumeBuffer`
   (modulo SSE-vs-NDJSON and the per-chunk JSON shape). Three concrete
   callers — the explicit "second caller" threshold from the project's
   "no premature abstraction" rule is met.
   **Action:** extract a `WebStreamProvider` base owning the scaffolding,
   with virtual hooks `buildUrl`, `applyHeaders`, `buildBody(const
   AiRequest&) -> std::string`, `onLine(std::string_view,
   ChunkHandler&)`, `httpErrorMessage(int)`. Concrete providers collapse
   to ~40 lines each.

8. **`AiChatView` is 1461 LOC and the SRP fault-line is clear.**
   Painting, selection, action-bar placement, per-block scrollbar +
   drag + wheel, link clicks, patch lifecycle, image-cache binding,
   hover routing, double-click word selection, Ctrl-C/A — all in one
   class. Plausible decomposition:
   - **`ChatSelection`** — `m_selectionMessage`, `m_selection`, drag
     state, copy-to-clipboard, double-click word, remap-across-resize.
     The remap at `onSize` (`AiChatView.cpp:217-237`) is non-trivial and
     worth isolating for tests.
   - **`BlockScrollController`** — per-block horizontal scrollbar
     geometry, hit-test, hover, drag, wheel routing. The
     `kScrollbarHeight=6` / `kScrollbarMinThumb=24` constants are
     redeclared in three methods (`paintMessage`, `scrollbarAt`,
     `onMotion`) — one home for the geometry.
   - **`ActionBarPlacement`** — `showActionBar` / `hideActionBar` and
     scroll-tracking attach-vs-pin logic.
   - **Patch handling belongs in `AiManager`**, not the view.
     `applyPatch`, `autoApplyPatches`, `m_appliedPatches` are domain
     operations, and `AiContext::editTarget()` already lives at that
     layer.

9. **`AiManager` constructor is half config-parser**
   (`AiManager.cpp:30-88`). The provider-selection switch belongs in a
   factory.
   **Action:** extract `namespace { auto makeProvider(const wxString&
   kind, const ConfigSection&) -> std::unique_ptr<AiProvider>; }`. Ctor
   shrinks to ~10 lines; adding the next provider only touches the
   factory; unit-testable in isolation.

## Memory

10. **`MockProvider::send` pre-slices the canned reply into a
    `std::vector<wxString>`** (`MockProvider.cpp:483-490`). For
    `kAllReply` (concatenation of every reply, 6-char chunks) that's
    thousands of small wide-char allocations.
    **Action:** replace `m_chunks` + `m_index` with one `wxString reply`
    + `std::size_t cursor`; emit `reply.Mid(cursor, kChunkChars)` per
    tick.

11. **`m_appliedPatches` keys are full patch text** (`AiChatView.hpp:242`).
    Cheap to switch to a 64-bit hash key (FNV-1a or `std::hash`). Minor
    in practice but bounded.

12. **`consumeBuffer` is O(n²) on long streams.** `m_buffer.substr(0,
    pos)` + `m_buffer.erase(0, pos+1)` allocates + shifts on every line
    in three providers.
    **Action:** swap for a `std::size_t m_bufferConsumed` cursor with
    periodic compaction. Fix once in the base class from #7.

## Project conventions

13. **Redundant std/wx includes in provider headers.**
    `AnthropicProvider.hpp`, `GeminiProvider.hpp`, `OllamaProvider.hpp`
    each include `<string>` and `<wx/webrequest.h>` — both live in
    `pch.hpp`. Project rule (CLAUDE.local.md): "headers go in pch.hpp".
    **Action:** delete the four redundant includes.

14. **Redundant includes in `AiChatView.hpp`.** `<unordered_set>` and
    `<wx/scrolwin.h>` — both reachable via pch.
    **Action:** delete.

15. **Snake-case slip.** `AiChatView.cpp:310` — `const bool
    rebuilt_layout = ...`. Project is camelCase.
    **Action:** rename (`documentRebuilt` / `relaidLayout`).

16. **`AiManager::context()` has no const overload.** Add a
    `[[nodiscard]] auto context() const -> const AiContext&` overload.

17. **`AiChatPanel::renderConversation()` is declared `const` but it
    mutates the chat view** via the (non-owning) `m_output` pointer.
    Strictly legal because constness doesn't propagate through pointers,
    but misleading.
    **Action:** drop the `const`.

## Tests

Only `CodeHighlighterTests.cpp` covers anything under `ai/`. Concrete,
isolated, cheap-to-test targets:

18. **`AiContext::setEditTarget`** — replacement / clearing behaviour;
    keeps `FileContextItem`s while removing the previous
    `EditTargetItem`; empty-path clears.
19. **`AiContext::removeAt`** — index ≥ size is a no-op.
20. **`MockProvider::pickReply`** — every command in the menu
    (`MockProvider.cpp:411-462`); case-insensitive `normalise`; unknown
    prompt falls through to `kDefaultReply`.
21. **SSE / NDJSON line parsing** — after the base-class extraction
    from #7, the per-line parser is a pure function over `std::string`.
    Coverage targets: split-across-chunk boundaries, broken JSON,
    missing `data:` prefix, partial trailing line, multiple events per
    feed.
22. **`roleToString` per provider** — three different mappings for
    `AiRole::System`; a silent regression here drops messages.
23. **`applyPatch` trailing-newline fallback** (`AiChatView.cpp:1404-1412`)
    — two-stage SEARCH retry that breaks easily under refactor.
24. **`makeProvider` factory** (after #9) — every supported `kind`
    string, missing-key handling, default fallback.

## Smaller items / notes

25. **`CodeActionBar` round-trips an enum through `void*
    SetClientData`** (`CodeActionBar.cpp:32-39`). Works, but
    `wxClientData` is the cleaner wx idiom.
26. **`AiChatView::onPaint` allocates a fresh `wxGraphicsContext` per
    paint** via `makeChatGraphicsContext`. Correct for the Direct2D
    flush story (the comment block at the top of the function explains
    the sequencing), just worth noting on first read.
27. **`wxString::FromUTF8` runs per token delta.** Unavoidable at the wx
    boundary; flagged as profiling lead, not a fix.

---

# TODOs (derived, independent)

Numbered to match the findings above. Each item is shippable on its own.

## Bugs / correctness

- [ ] **#1** Clear `m_appliedPatches` when the conversation is
      cleared. Hook `AiManager::clear` (or invalidate from
      `setMessages` when history shrinks).
- [ ] **#2** Gate live-edit auto-apply on a closed-fence signal so
      mid-stream patches don't apply against partial SEARCH text.
- [ ] **#3** Add `AiProvider::reset()` (default no-op); `AiManager::clear`
      calls it; `ClaudeCliProvider` drops `m_sessionId` in its override.
- [ ] **#4** Wrap `CodeHighlighter::m_lexer` in a smart pointer with a
      `Release()` deleter so a throwing ctor body doesn't leak.
- [ ] **#5** Whitelist `http:` / `https:` / `mailto:` in `linkAt`
      before `wxLaunchDefaultBrowser`.
- [ ] **#6** *(tidy)* Delete the `(void)blockHeight` and `(void)mode`
      lines in `AiChatView::showActionBar`.

## Architecture

- [ ] **#7** Extract `WebStreamProvider` base class for Anthropic /
      Gemini / Ollama. Hooks: `buildUrl`, `applyHeaders`, `buildBody`,
      `onLine`, `httpErrorMessage`. Migration order: Anthropic first
      (smallest delta), then Ollama, then Gemini.
- [ ] **#8a** Extract `ChatSelection` from `AiChatView`.
- [ ] **#8b** Extract `BlockScrollController` from `AiChatView` — owns
      the `kScrollbarHeight` / `kScrollbarMinThumb` constants (delete
      the three duplicate declarations).
- [ ] **#8c** Extract `ActionBarPlacement` from `AiChatView`.
- [ ] **#8d** Move patch lifecycle (`applyPatch`, `autoApplyPatches`,
      `m_appliedPatches`) to `AiManager`. Consolidates with #1, #2.
- [ ] **#9** Extract `makeProvider(kind, config) ->
      std::unique_ptr<AiProvider>` factory from `AiManager` ctor.

## Memory

- [ ] **#10** `MockProvider`: replace pre-sliced `m_chunks` with a
      single `wxString` + cursor.
- [ ] **#11** Key `m_appliedPatches` on a 64-bit hash rather than full
      patch text. (Folds into #8d.)
- [ ] **#12** Replace `m_buffer.erase(0, pos+1)` with a consumed-cursor
      pattern. (Fix once in #7's base class.)

## Conventions

- [ ] **#13** Remove redundant `<string>` / `<wx/webrequest.h>` from
      `AnthropicProvider.hpp`, `GeminiProvider.hpp`, `OllamaProvider.hpp`.
- [ ] **#14** Remove redundant `<unordered_set>` / `<wx/scrolwin.h>`
      from `AiChatView.hpp`.
- [ ] **#15** Rename `rebuilt_layout` to camelCase.
- [ ] **#16** Add `const` overload for `AiManager::context()`.
- [ ] **#17** Drop `const` from `AiChatPanel::renderConversation()`.

## Tests

- [ ] **#18** `AiContext::setEditTarget` tests (replace / clear / keep
      siblings).
- [ ] **#19** `AiContext::removeAt` out-of-range no-op test.
- [ ] **#20** `MockProvider::pickReply` table-driven test covering
      every command + the default fall-through. Requires factoring
      `pickReply` out of the anonymous namespace or via a friend.
- [ ] **#21** SSE / NDJSON parser tests (after #7's extraction).
- [ ] **#22** `roleToString` per provider — `System` handling especially.
- [ ] **#23** `applyPatch` trailing-newline fallback test. Requires
      factoring out a pure `findPatchTarget(buffer, search) -> int`
      step that doesn't depend on `wxStyledTextCtrl`.
- [ ] **#24** `makeProvider` factory tests (after #9).

## Tidy / notes (defer unless touched)

- [x] **#25** *(rejected)* `CodeActionBar`'s `void*` round-trip stays.
      Tried 484ce02, reverted in bf2ab10 — a `wxClientData` subclass
      adds one heap allocation per button at construction; the existing
      `toVoidPtr` / `toMode` helpers pay zero. Type-erased pattern is
      local to the bar and acceptable for the win.
- [ ] **#26** *(note only)* Per-paint `wxGraphicsContext` allocation —
      profiling lead if Retina paints get slow.
- [ ] **#27** *(note only)* `wxString::FromUTF8` per token delta —
      profiling lead.

---

# Suggested execution order

Some items unblock or simplify others; some are pure cleanups that
can ride along whenever the file is touched.

## Phase 1 — quick correctness + hygiene wins (low risk)

Independent, mostly one-file changes. Land in any order.

1. **#6** delete dead code in `showActionBar`.
2. **#13** + **#14** remove redundant includes.
3. **#15** rename `rebuilt_layout`.
4. **#16** const overload for `context()`.
5. **#17** drop misleading `const` on `renderConversation`.
6. **#4** lexer leak fix in `CodeHighlighter`.
7. **#5** URL scheme whitelist in `linkAt`.

## Phase 2 — provider consolidation (test-driven)

8. **#21 / #22 (partial)** write parser + role-mapping tests against
   the *current* providers first, so the refactor in #7 has a safety
   net.
9. **#7** extract `WebStreamProvider`. Migrate Anthropic first, then
   Ollama, then Gemini.
10. **#12** drops in for free as part of #7.
11. **#9** extract `makeProvider` factory.
12. **#24** tests for the factory.

## Phase 3 — view decomposition (test-driven)

13. **#23** test `applyPatch` trailing-newline fallback against
    current code, after extracting the pure search step.
14. **#8d** move patch lifecycle to `AiManager`. This unblocks #1, #2,
    and #11.
15. **#1** clear-on-clear, **#2** closed-fence gate, **#11** hash key —
    land together in `AiManager` now that the code is at the right
    layer.
16. **#3** `AiProvider::reset()` hook + `ClaudeCliProvider` override.
17. **#8a → #8b → #8c** extract `ChatSelection`,
    `BlockScrollController`, `ActionBarPlacement`. These are
    behaviour-preserving refactors; visual-diff is the honest verifier.

## Phase 4 — domain tests + smaller items

18. **#18**, **#19**, **#20** — `AiContext` and `MockProvider::pickReply`
    coverage.
19. **#10** `MockProvider` chunk slicing.
20. **#25** `wxClientData` cleanup (only when touching `CodeActionBar`).

## TDD fit summary

| Item       | TDD fit?                                                          |
|------------|-------------------------------------------------------------------|
| #1, #2, #11 | Yes once moved to `AiManager` (#8d) — pure domain logic.         |
| #7         | Yes — write SSE/NDJSON + role tests first (#21, #22).            |
| #9         | Yes — pure factory function.                                     |
| #18 – #24  | Yes by definition.                                               |
| #4, #5, #6, #13–17 | No — hygiene / one-line fixes.                           |
| #8a–#8c    | No — behaviour-preserving refactor; visual diff is the verifier. |
| #10, #25, #26, #27 | No — perf / API cleanup.                                 |
