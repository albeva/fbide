# Chat view + markdown rendering review

A focused review of `src/lib/ai/chat/*` and `src/lib/ai/AiChatPanel.cpp`. The
architecture is solid — per-message layout cache, dirty-rect paint,
baseline-aligned text, throttled streaming. Most findings are sharpenings,
not rewrites.

## Memory

1. **Source text duplicated.** `LaidCodeBlock::code` and
   `LaidPatchBlock::{search,replace}` (`ChatLayout.hpp:113-127`) hold the
   verbatim snippet text, but the same bytes already live in
   `ChatViewMessage::markdown` and (for patches) in `m_appliedPatches`
   keys. On long code-heavy replies this is ~3× the snippet text in RAM.
   **Action:** drop those fields and resolve the text on demand when the
   action bar needs it (clipboard copy / patch apply).

2. **`m_imageCache` is unbounded.** Every `Ready` entry keeps a `wxBitmap`
   for the rest of the session. No LRU, no size cap. A long session that
   scrolls through many image URLs leaks slowly.
   **Action:** LRU cap (e.g. 32 ready entries or N total pixels).

3. **Off-screen bubbles keep their full `LaidOutDoc`.** `m_items` only
   grows. For threads with hundreds of bubbles, each carries vectors of
   `PaintLine` / `PaintRun` (wxString per run).
   **Action (deferred):** evict `doc` for bubbles outside a viewport
   window; re-lay on scroll into view. Bigger change — only worth it
   under real pressure.

4. **`PaintRun.text` is wxString per token.** Code blocks split
   per-character on overflow (`ChatLayout.cpp:476`) produce many tiny
   wxStrings. Fine for typical content; allocator-pressure candidate if
   ever profiled. Not a fix-now.

## Rendering / caching

5. **`DcMeasurer` and its font-style cache rebuilt on every
   `relayout()`** (`AiChatView.cpp:303-306`). Each streaming throttle
   tick throws away the per-style font + lineHeight + spaceWidth
   memoisation just built.
   **Action:** hoist `DcMeasurer` to a `std::unique_ptr` member;
   invalidate on `resolveFonts()` / `refreshTheme()`.

6. **Image-cache "ready" listener relayouts the entire view.** Multiple
   images settling at once → multiple `relayout()` + `Refresh()`
   back-to-back.
   **Action:** coalesce via a `m_imagesDirty` flag and a single
   `CallAfter([this]{ relayout(); Refresh(); })`.

7. **`Refresh()` is whole-window.** During streaming only the last
   bubble changes meaningfully. The dirty-rect paint short-circuits
   non-visible lines, but the bubble round-rect + every visible line
   still repaints each tick.
   **Action (optional):** track the streaming bubble's previous bottom
   and `RefreshRect` only the changed band.

8. **`autoApplyPatches` iterates every laid bubble each call**
   (`AiChatView.cpp:1054`). `m_appliedPatches.insert(...).second`
   short-circuits, but the lookup still runs on every streaming tick
   under live-edit.
   **Action (optional):** track first-unchecked-bubble index, resume
   from there.

9. **Per-bubble style cache reset.** `paintMessage` resets style/colour
   caches at the start of each message; adjacent bubbles in a multi-
   message paint don't share. Minor — within-bubble it pays off.

## Cleanup

10. **`paintMessage` is ~180 lines** (`AiChatView.cpp:425-604`),
    interleaving background fills, table border math, baseline
    computation, and run drawing.
    **Action:** extract `paintLineBackground`, `paintTableBorders`,
    `paintLineRuns`.

11. **md4c callbacks are file-scope `static extern "C"`**
    (`Markdown.cpp:220+`). Pre-existing tidy noise.
    **Action:** wrap in `namespace { extern "C" { ... } }` or suppress
    at function level.

12. **`Engine::imageCellLabel` doesn't use Engine state**
    (`ChatLayout.cpp:667`).
    **Action:** move to a free function in the anonymous namespace.

13. **`makeChatGraphicsContext` runs through D2D for the measure DC.**
    Measurement doesn't need D2D; only paint does.
    **Action (optional):** use the default renderer for the measure DC.

14. **`paintMessage::ascentForRun` is named like a getter but mutates
    state** (`currentStyle`, `currentAscent`, `styleSet`).
    **Action:** rename to `currentAscentFor` or split out the side
    effects explicitly.

15. **Patch pre-scan lives inside `parseMarkdown`**
    (`Markdown.cpp:488-577`). Coherent pre-pass but couples the parser
    to a proposal format that has nothing to do with markdown.
    **Action (optional):** extract a `splitPatchBlocks(text)` step;
    keep `parseMarkdown` pure markdown.

## Reliability

16. **Synchronous `wxImage::LoadFile` on the UI thread for up to 5 MiB**
    (`ChatImageCache.cpp:122`). A large slow JPEG decode (50-200 ms)
    freezes the UI.
    **Action:** tighten the byte cap to ~2 MiB (chat images shouldn't
    be photos) OR push decode to `wxThread` and post the bitmap back
    via `CallAfter`.

17. **`wxWebRequest` follows redirects.** `allowedScheme` only checks
    the initial URL; an HTTPS URL that 302-redirects to a different
    scheme could be honoured.
    **Action:** verify wx redirect policy; disallow cross-scheme
    redirects if needed.

---

# TODO list (derived)

Numbered to match the findings above. Each one is independent.

- [x] **#1**  Drop `LaidCodeBlock::{code,lang}`; resolve on demand. *(code blocks
      done; patch half deferred — `autoApplyPatches` would re-parse every
      message every streaming tick without a separate dedup-key strategy.)*
- [x] **#2**  LRU eviction in `ChatImageCache` (cap on Ready entries).
- [x] **#5**  Hoist measurement cache onto `AiChatView`.
- [x] **#6**  Coalesce image-ready relayouts via `CallAfter`.
- [x] **#10** Split `AiChatView::paintMessage` into focused helpers.
- [x] **#14** Rename `ascentForRun` → `selectRunFont` (side effects are
      now in the name + doc).
- [x] **#16** Tighten byte cap (5 → 2 MiB). Off-thread decode still
      possible if 2 MiB proves too lax.
- [x] **#17** Defence-in-depth scheme check on the post-redirect URL.
- [x] **#11** *(tidy)* md4c callbacks moved into the anonymous namespace.
- [x] **#12** *(tidy)* `imageCellLabel` extracted to a free function.
- [x] **#15** *(tidy)* Patch pre-scan extracted from `parseMarkdown`.
- [ ] **#3**  *(deferred)* Evict off-screen bubble `LaidOutDoc`s.
- [ ] **#7**  *(deferred)* `RefreshRect` the streaming band only.
- [ ] **#1b** *(deferred)* Drop `LaidPatchBlock::{target,search,replace}` —
      needs a hash-based dedup-key strategy first.

---

# TDD candidates (proposed implementation order)

Pick items where a failing test can drive the implementation honestly —
new observable behaviour or a regression-prone refactor.

## Round 1 — strong fit for TDD

### A. `ChatImageCache` LRU eviction  (#2)
Pure new behaviour; isolated; easy to unit-test without GUI. Add a
constructor param `maxReadyEntries` (default 32). Add a `ChatImageCacheTests`
TU and inject a deterministic test seam:
- entries can be "force-completed" without an HTTP round-trip, or
- the cache exposes an `insertForTest(url, bitmap)` helper.

Test list (TDD order):
1. `EvictsOldestOnceCapReached` — cap=3; add 4 ready entries; first URL is
   gone (`get` returns Loading and starts a new request).
2. `MarkingEntryReadyTouchesLruPosition` — adding to a full cache after a
   `get(oldest)` evicts the next-oldest.
3. `FailedEntriesAreNotCountedAgainstCap` — cap of 1; one Failed + one
   Ready coexist (failed entries are cheap, no bitmap).
4. `ClearAllResetsLruBookkeeping` — after `clearAll`, capacity is fully
   available again.

### B. Drop duplicated snippet text  (#1)
Refactor with test safety net. New tests assert the *behaviour* (clipboard
copy works, patch apply works) so the refactor can proceed without
regressing.

Test list (TDD order):
1. `CopyCodeReturnsExactSnippetFromMarkdown` — drive a synthetic message
   through `AiChatView::setMessages`, simulate the "copy" path, assert the
   resolved text. Requires factoring the snippet-resolution out of
   `onCopyCode` into a pure function `resolveCodeBlockText(markdown, idx)`.
2. `ApplyPatchSearchesExactBlockFromMarkdown` — similar shape; assert the
   resolved (search, replace) pair matches the source.
3. Once both tests pass *with* the current duplicated fields, remove the
   fields and re-route the action bar handlers through the new resolver.
   Tests stay green or the refactor is wrong.

This refactor needs `LaidCodeBlock` and `LaidPatchBlock` to carry an
*index* into the parsed `MdDoc` (or the source byte range) rather than
the text itself.

## Round 2 — moderate fit

### C. Coalesce image-ready relayouts  (#6)
Testable but needs a small seam. The natural test is "N ready callbacks
in one event-loop tick produce 1 relayout". That requires either
- counting relayouts via a friend hook, or
- decoupling: a `ChatLayoutCoalescer` class with `notifyDirty()` + `flush()`
  driven by an injected scheduler.

Not strong-fit because the production wiring is `wxEvtHandler::CallAfter`
and the unit test would need a fake scheduler. Possible but adds test
infrastructure for one feature. **Defer unless we add more coalesced
work later.**

## Not a TDD fit

- **#5 Hoist `DcMeasurer`** — pure internal refactor. `DcMeasurer` lives
  in the anonymous namespace of `AiChatView.cpp`; observability is
  "fewer allocations / faster relayout", which is a profiling concern,
  not a unit test.
- **#10 Split `paintMessage`** — no behavioural change; visual diff is
  the only honest verifier.
- **#11 / #12 / #14 / #15** — cleanups with no behavioural change.
- **#16 Async decode** — testing the off-thread decode path requires
  GUI / event-loop integration; not a unit test.
- **#17 Redirect policy** — testable only against a real or stubbed
  HTTP server.

## Suggested execution order

1. **A (LRU eviction)** — lowest risk, isolated, ships visible memory
   relief.
2. **B (drop duplicated text)** — bigger structural change, gated by
   the new resolver tests so the refactor is safe.
3. After that, the non-TDD items in priority: **#5 → #6 → #16 → #10
   → #14 → #11/#12**.
