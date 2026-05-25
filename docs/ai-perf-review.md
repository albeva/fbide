# ai/ + markdown/ memory and conversion review

Follow-up to [ai-review.md](ai-review.md) and [chat-review.md](chat-review.md).
This pass is narrowly scoped: identify avoidable allocations, redundant
string conversions (wxString ↔ std::string / std::string_view), and
storage that lives in more places than it needs to.

Important context: **`wxString` in wx 3.3 on macOS is `typedef wxStdString`
— `std::basic_string`, not copy-on-write.** Every `wxString` copy /
assignment is a real allocation + memcpy. That makes per-tick copies of
streaming text genuinely expensive, not the "refcount bumps" they'd be
under classical wx 2.x.

## Findings

Numbered for cross-reference, ordered by impact.

1. **Streaming accumulator is stored twice.** `AiManager::m_pendingAccumulator`
   and `AiChatPanel::m_streaming` both grow byte-for-byte with the
   in-flight reply. For a 50 KB reply that's 100 KB of duplicate
   wide-char storage during streaming.

2. **The pipeline copies the reply on every render tick.** Throttle
   tick (default 150 ms) builds `ChatViewMessage::markdown` (copy from
   `m_streaming`) → `MarkdownDocument::m_markdown` (copy in
   `setMarkdown`). For a 50 KB reply that's ~1 MB/s of allocation +
   copy during the streaming window.

3. **`LaidScrollBlock::{patchSearch, patchReplace}` duplicate text
   already in the source markdown.** Previously deferred as
   `chat-review.md` TODO #1b. For a multi-KB patch this doubles
   storage that lives as long as the laid doc.

4. **`AiContext` re-reads attached files on every send.** Sync
   `wxFFile::ReadAll` per `FileContextItem` / `EditTargetItem` per
   `sendMessage`. 100 KB include × 10 messages = 1 MB of disk I/O + 1
   MB of wxString allocation for unchanged content.

5. **`resolveCodeBlockText` re-parses the entire message per click.**
   Full md4c parse on every Copy / Insert / Run button press, just to
   pull one fenced block's text out.

6. **`AiContext::appendTo` builds intermediate wxStrings.**
   `out += "\n--- File: " + pathWx + " ---\n"` allocates two
   temporaries per file per send.

7. **`AiManager::patchKey` allocates twice for a hash.** Builds a
   concatenated wxString, then converts to UTF-8 string, then hashes.
   Two big allocations to produce a 64-bit value.

8. **`Patch.cpp::findUtf8` re-converts the needle per call.**
   `findPatchMatch` calls it twice in the trailing-newline fallback
   path — two UTF-8 conversions of the same wxString.

9. **Per-token `std::string` allocation in every web-stream provider.**
   `delta.value("text", "")` returns nlohmann's string by value (a
   copy out of internal storage). Then `wxString::FromUTF8(...)`
   allocates the wxString. One std::string allocation per token,
   throughout the streaming hot path.

10. **`MarkdownImageCache` calls `url.utf8_string()` on every
    `get`/`contains`/`insertReady`.** Low call frequency, real
    allocation; tiny absolute cost.

## Plan

Four phases, ordered roughly by risk × win. Each phase is independent;
phases internally can land in any order.

### Phase 1 — hot-path quick wins (local, low risk)

Each item is a small change in one file, no API changes, no behaviour
change.

- **T1** Anthropic / Gemini / Ollama parsers: replace
  `delta.value("text", "")` (returns `std::string` by value) with
  `delta.template get_ptr<const std::string*>()`-style access that
  yields a borrowed view, then `wxString::FromUTF8(view.data(),
  view.size())`. Eliminates one std::string allocation per token in
  every web-stream provider. *(#9)*
- **T2** `Patch.cpp::findPatchMatch`: hoist `needle.utf8_string()`
  into the caller; `findUtf8` takes `std::string_view`. Trim in
  std::string land for the fallback retry. *(#8)*
- **T3** `AiManager::patchKey`: hash incrementally over the UTF-8
  bytes of `search`, a separator, and `replace`, instead of
  concat-then-convert-then-hash. *(#7)*

### Phase 2 — kill the streaming duplicate

- **T4** `AiManager` exposes `[[nodiscard]] auto pendingReply() const
  -> const wxString&`. `AiChatPanel` drops `m_streaming` entirely —
  the panel's `onChunk` lambda still fires (so the render throttle
  can flip `m_dirty`) but doesn't accumulate. `renderConversation`
  uses `manager.pendingReply()` instead of `m_streaming`. *(#1)*

### Phase 3 — per-send + cleanup

- **T5** `AiContext` file content cache. `FileContextItem` and
  `EditTargetItem` track `(mtime, wxString content)` and skip the
  read when mtime matches. Read-error / missing file evicts. *(#4)*
- **T6** `AiContext::appendTo` (all three subclasses): split
  `out += "..." + pathWx + "..."` into three separate `+=` calls.
  Single-line touch per subclass. *(#6)*

### Phase 4 — source byte ranges (parser-level)

Higher-touch, gated on a design decision below.

- **T7** Store `(sourceStart, sourceEnd)` byte offsets in
  `LaidScrollBlock` for patches instead of the verbatim
  `patchSearch` / `patchReplace` wxStrings. The parser already
  knows these positions in `splitPatchBlocks`. Callers
  (`AiManager::applyPatch`, `isPatchApplied`, the chat view's
  overlay paint) read the text from the source markdown on demand
  via `wxString::Mid(start, end - start)`. *(#3)*
- **T8** Same mechanism for code fences: store the source range
  rather than re-parsing on every Copy / Insert / Run click —
  `resolveCodeBlockText(markdown, blockIndex)` becomes a substring
  rather than a full md4c parse. *(#5)*

**Open design question for Phase 4.** `splitPatchBlocks` calls
`stripCr(line)` while accumulating `patchSearch` / `patchReplace`,
so the currently-stored text is normalised to `\n`-only. A literal
source byte range would re-include any `\r` bytes from the source.
Options:
- **(a)** Stop normalising `\r\n` in the parser. Accepts `\r` bytes
  in patch text. Affects how `applyPatch`'s SEARCH text matches the
  buffer — probably benign since chat replies from API providers
  emit `\n`-only, but needs a test.
- **(b)** Detect `\r` in the patch span at parse time. If absent
  (the common case), store the byte range; if present, fall back to
  the wxString. Hybrid storage in `LaidScrollBlock`.
- **(c)** Defer Phase 4 until we have a measurement that says the
  per-block patch duplication actually matters in practice (most
  chat sessions have at most a handful of patches at any time).

Recommend (c) unless instrumentation shows it matters, with (a) as the
first try if it does.

### Skipped

- **#10** `MarkdownImageCache` keys as `std::string`. Call frequency
  is per-image during layout, not per token. Switching the map's key
  type to wxString requires a hasher + churns the file; the win is
  too small to be worth the diff.
- **#2** The per-tick `ChatViewMessage::markdown` / `MarkdownDocument::m_markdown`
  copies. Fixing them in earnest means turning `ChatViewMessage::markdown`
  into a non-owning view, but the streaming bubble and the "**Error:**"
  / `_Thinking…_` bubbles need owned wxStrings. A `variant<wxString,
  const wxString*>` is more complexity than the win justifies. Phase
  2 (T4) kills the most expensive duplicate; this finer one stays as a
  note.

## Critical review of the plan

Things I want to flag before turning these into TODOs:

- **T1 may have a nlohmann gotcha**: `get_ptr<const std::string*>()`
  returns `nullptr` when the node isn't a string. The new code must
  null-check (`if (auto* p = ...; p != nullptr) ...`) before
  constructing the wxString. The current code's `.value("text", "")`
  silently returns empty on a non-string — keep that semantics.
- **T4 — what if the panel was relying on m_streaming for something
  other than rendering?** Grep says only render path uses it; safe.
  But: `AiChatPanel::submitPrompt` clears `m_streaming` before
  starting the send. With T4, that clear moves to `AiManager`
  alongside the accumulator clear. The state-machine ordering needs
  to be obvious.
- **T5 — cache invalidation correctness**: must compare mtime by
  inequality (`!=`), not chronologically (`<`). Backup-restore tools
  can move mtime backwards.
- **T7 / T8** are real refactors, not one-liners. The parser
  (`Markdown.cpp`'s `splitPatchBlocks`) currently builds `patchSearch`
  via `+=`; switching to byte ranges means tracking the offset of
  each line at scan time. Not hard, but bigger than the other items
  here.
- **Tests**: Phase 1 items all preserve behaviour; existing parser /
  patch tests cover them. T4 is harder to unit-test directly (UI
  state machine); rely on the existing 685 tests staying green +
  manual confirmation. T5 wants real tests (cache hit, mtime miss,
  missing file). T7/T8 are covered by `applyPatch` and
  `resolveCodeBlockText` tests if added.

---

# TODOs

Each item is shippable on its own; phases group by purpose but don't
impose ordering.

## Phase 1 — hot-path quick wins

- [ ] **#9** Replace `delta.value("text", "")` with a borrowed
      `std::string*` view in all three web-stream parsers
      (`AnthropicProvider::parseStreamLine`,
      `OllamaProvider::parseStreamLine`,
      `GeminiProvider::parseStreamLine`). Null-check before
      constructing the `wxString`. Existing parser tests cover the
      behaviour; no new tests needed.
- [ ] **#8** Hoist `needle.utf8_string()` to a local in
      `findPatchMatch`; `findUtf8` takes `std::string_view`. Trim
      with `pop_back()` for the trailing-newline retry.
- [ ] **#7** Replace `(search + sep + replace).utf8_string()` in
      `AiManager::patchKey` with an incremental hash over the three
      pieces' UTF-8 bytes separately.

## Phase 2 — drop the duplicate accumulator

- [ ] **#1** Add `AiManager::pendingReply() const -> const wxString&`
      returning `m_pendingAccumulator`. Drop `AiChatPanel::m_streaming`.
      Panel's `onChunk` lambda flips `m_dirty` only.
      `renderConversation` reads from `manager.pendingReply()` for the
      streaming bubble. Move the panel's pre-send `m_streaming.clear()`
      out (already happens inside `sendMessage` for the accumulator).

## Phase 3 — per-send wins + cleanup

- [ ] **#4** `FileContextItem` and `EditTargetItem`: cache
      `(std::filesystem::file_time_type mtime, wxString content)`;
      re-read only when `mtime` changed or the cache is empty. Evict
      on read failure. Stat is cheap (one syscall per file per send).
      Tests: cache hit returns cached bytes; mtime change forces
      reread; missing file falls back to "<could not read file>".
- [ ] **#6** `AiContext::appendTo` (all three subclasses): split
      `out += "..." + pathWx + "..."` into three separate `+=`
      statements. Trivial; one commit.

## Phase 4 — source byte ranges (deferred, design first)

- [ ] **#3** *(deferred)* `LaidScrollBlock::{patchSearch,
      patchReplace}` → source byte ranges. Resolve text on demand
      from `MarkdownDocument::markdown()`. Requires settling the
      `\r\n` normalisation question above.
- [ ] **#5** *(deferred)* `resolveCodeBlockText` lookup via byte
      range instead of re-parse. Folds in naturally with #3 if the
      `LaidScrollBlock` is the shared carrier of the byte range.

## Skipped

- [x] **#10** *(rejected)* `MarkdownImageCache` wxString-keyed map.
      Frequency too low; API churn not justified.
- [x] **#2** *(deferred)* Per-tick `ChatViewMessage::markdown` copy.
      Fixing properly needs a `variant`/borrowed-view design that
      costs more in complexity than it saves. Re-evaluate if
      streaming throughput becomes a measured bottleneck.

## Suggested execution order

1. **#9, #8, #7** in any order — quick, local, no API touch.
2. **#1** — small but touches both `AiManager` and `AiChatPanel`.
3. **#6, #4** — `AiContext` only. #6 first (one-liner), then #4
   (real cache).
4. *(Decision point)* Whether to take Phase 4 (#3, #5) depends on
   whether anyone reports patch text feeling heavy. Default: skip
   unless profiling motivates it.

## TDD fit

| Item | TDD fit? |
|------|----------|
| #9   | Existing parser tests cover the byte-identical behaviour — no new tests, just a green-bar refactor. |
| #8   | `FindPatchMatch.*` tests cover it. |
| #7   | `isPatchApplied` tests cover the (search, replace) → bool round-trip. Hash value changing is irrelevant — only equivalence matters. |
| #1   | Manual / regression. No unit-testable observable. |
| #4   | Yes — pure cache logic. Worth tests for hit / mtime-miss / read-failure paths. |
| #6   | No behaviour change. |
| #3, #5 | Existing `applyPatch` / `resolveCodeBlockText` tests cover. |
