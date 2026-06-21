# `src/lib/markdown/`

Reusable markdown rendering pipeline. Two consumers today:
`MarkdownView` (the standalone read-only widget) and `AiChatView` (the
single-surface chat bubble stack). Both share the same parser, layout
engine and paint primitives.

## Layers

```
        ┌────────────────────────────────────────────────────┐
        │ src/lib/markdown/                                  │
        │                                                    │
        │ Markdown.{hpp,cpp}        — md4c-backed parser     │
        │   parseMarkdown(text) -> MdDoc                     │
        │   resolveCodeBlockText(text, idx) -> wxString      │
        │                                                    │
        │ MarkdownLayout.{hpp,cpp}  — layout engine          │
        │   layoutMarkdown(doc, width, …) -> LaidOutDoc      │
        │   types: TextStyle, PaintLine, PaintRun,           │
        │          MarkdownPalette, LaidScrollBlock, …       │
        │                                                    │
        │ MarkdownImageCache.{hpp,cpp} — async image cache   │
        │   wxWebRequest + LRU eviction                      │
        │                                                    │
        │ MarkdownDocument.{hpp,cpp} — state container       │
        │   bundles source + parsed + laid                   │
        │   setMarkdown(text, width, …) -> bool changed      │
        │                                                    │
        │ MarkdownRenderer.{hpp,cpp} — paint primitives      │
        │   fontFor(style, body, mono, themed) -> wxFont     │
        │   paintLineBackground(gc, line, …)                 │
        │   paintLineText(gc, line, …, PaintRunState&)       │
        │   MeasurementEntry — persistent measurer cache     │
        │                                                    │
        │ MarkdownView.{hpp,cpp}    — drop-in viewer widget  │
        │   wxScrolled<wxPanel>; setMarkdown(text), events   │
        └────────────────────────────────────────────────────┘
```

`MarkdownView` and `AiChatView` both build on **MarkdownDocument** (for
per-document parse/layout caching) and **MarkdownRenderer** (for the
per-line paint primitives). The renderer is stateless apart from a
host-owned `PaintRunState` cache that lets `SetFont` /
`SetTextForeground` traffic stay at per-style rather than per-run.

## Performance: single-pass paint

Both consumers paint **all visible content in one pass** through the
shared off-screen buffer:

- `AiChatView` walks its bubble stack in one `onPaint` and draws each
  bubble's lines via `paintLineBackground` + `paintLineText`. Bubble
  decoration (rounded rect + role tint), the floating code-action bar
  and the applied-patch veil stay on the chat side — none of them
  spawn child windows.

- `MarkdownView` walks the laid-out lines of its single document in
  one `onPaint` against its own off-screen buffer. No child windows.

This is deliberate: a list of N child `MarkdownView`s inside a scroller
would mean N paint events per scroll frame plus blit overhead, which
showed up as scroll judder under streaming. Keeping every consumer on
a single surface preserves scroll performance.

## Adding a new consumer

Embed `MarkdownView` directly when you just need a scrollable formatted
text panel. Reach for `MarkdownDocument` + `MarkdownRenderer` only when
you need a custom paint surface (multi-bubble, gallery view, etc.) —
that's the boundary the chat view sits on.

Link clicks emit `MARKDOWN_LINK_CLICKED` (URL on the event's string
field). Bind it on the view (or any parent in the chain) to intercept;
if no handler claims the event, `wxLaunchDefaultBrowser` runs as a
default so standalone use needs no extra wiring.
