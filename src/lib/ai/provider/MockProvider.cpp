//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "MockProvider.hpp"
using namespace fbide;
using namespace fbide::ai;

namespace {
// Emission cadence and chunk size for the simulated stream.
constexpr int kTickMs = 40;
constexpr std::size_t kChunkChars = 6;

// Canned reply set — one per command. Lower-case / under-indented code
// is deliberate: it lets the reformat + keyword-case pipeline show its
// work. Pick which to send by typing the command (e.g. `table`) into
// the chat. `help` lists the menu.

const auto kHelpReply = wxString::FromUTF8(
    R"(**Mock provider** — type one of these as your prompt:

- `help` / `list` — this message.
- `short` — single-line reply.
- `long` — long multi-section reply.
- `text` — plain prose, no markdown.
- `fb` — FreeBASIC code sample.
- `bigcode` — long FreeBASIC sample (~70 lines, several wide lines).
- `json` — JSON code sample.
- `table` — markdown table.
- `emoji` — text with emoji.
- `tasks` — GFM task-list checkboxes.
- `setext` — `===` / `---` underline headings.
- `images` — inline images from the FBIde screenshot gallery.
- `patch` — SEARCH/REPLACE proposal card.
- `all` — every reply above, concatenated with rules.
- `allf` / `all fast` — same as `all`, dumped instantly (no streaming).

Anything else gets the default mixed reply.
)"
);

const auto kShortReply = wxString::FromUTF8(
    R"(Got it — short reply, single line.)"
);

const auto kTextReply = wxString::FromUTF8(
    R"(Plain prose with no markdown — just a paragraph long enough to wrap a few times so the bubble width and line spacing get exercised. No headings, no fences, no lists, no links. The mock provider is a good place to confirm that ordinary text still looks right after we tweak font sizing or bubble padding, without any markup features getting in the way.

Second paragraph, separated by a blank line, to verify inter-paragraph spacing matches what you'd expect from a real reply.)"
);

const auto kFbReply = wxString::FromUTF8(
    R"(A small **FreeBASIC** snippet — first 10 Fibonacci numbers:

```freebasic
dim as integer a, b, t, n
a = 0 : b = 1
for n = 1 to 10
print a
t = a + b : a = b : b = t
next n
```

`Dim` declares the loop variables; the body advances the pair `(a, b)` each step.
)"
);

const auto kBigCodeReply = wxString::FromUTF8(
    R"(A longer **FreeBASIC** sample — single-file linked-list implementation,
~70 lines. Useful for exercising tall code-block layout, the action
bar's attached-↔-pinned transition while scrolling, and horizontal
overflow when `wrapCodeBlocks = false`.

```freebasic
'' Linked-list demo — append, prepend, iterate, free.
'' Single-file FreeBASIC; the long literal at the bottom keeps a
'' line long enough to force horizontal overflow when wrapping is
'' disabled in the markdown view.

#include once "crt/stdlib.bi"

type Node
    value as integer
    nxt   as Node ptr
end type

type List
    head as Node ptr
    tail as Node ptr
    size as integer
end type

sub list_init(byref lst as List)
    lst.head = 0
    lst.tail = 0
    lst.size = 0
end sub

sub list_append(byref lst as List, byval value as integer)
    dim as Node ptr node_ = callocate(sizeof(Node))
    node_->value = value
    node_->nxt   = 0
    if lst.tail = 0 then
        lst.head = node_
        lst.tail = node_
    else
        lst.tail->nxt = node_
        lst.tail = node_
    end if
    lst.size += 1
end sub

sub list_prepend(byref lst as List, byval value as integer)
    dim as Node ptr node_ = callocate(sizeof(Node))
    node_->value = value
    node_->nxt   = lst.head
    lst.head = node_
    if lst.tail = 0 then
        lst.tail = node_
    end if
    lst.size += 1
end sub

sub list_print(byref lst as List)
    dim as Node ptr cur = lst.head
    do while cur <> 0
        print using "###"; cur->value;
        if cur->nxt <> 0 then print " -> ";
        cur = cur->nxt
    loop
    print
end sub

sub list_free(byref lst as List)
    dim as Node ptr cur = lst.head
    do while cur <> 0
        dim as Node ptr nxt = cur->nxt
        deallocate cur
        cur = nxt
    loop
    lst.head = 0
    lst.tail = 0
    lst.size = 0
end sub

'' --- demo -----------------------------------------------------------------
dim as List numbers
list_init(numbers)
for n as integer = 1 to 12
    list_append(numbers, n * n)
next n
list_prepend(numbers, -1)
print "list size: " & numbers.size
list_print(numbers)
'' Long single-statement line so horizontal scroll has something to scroll past — keep this on one line so it overflows the bubble width when wrapping is disabled.
print "first squared plus last squared = " & (numbers.head->value + numbers.tail->value)
'' Deliberately wide diagnostic line — exercises horizontal overflow in [markdown] wrapCodeBlocks=false mode and the scrollbar's thumb travel.
print "[diagnostic] node="; numbers.head; " head_value="; numbers.head->value; " tail="; numbers.tail; " tail_value="; numbers.tail->value; " size="; numbers.size; " bytes_per_node="; sizeof(Node)
'' Another deliberately wide formatted line — long ascii table row with many columns to demonstrate horizontal scroll behaviour in the chat code-block renderer.
print using "| ###### | ###### | ###### | ###### | ###### | ###### | ###### | ###### | ###### | ###### |"; 1; 4; 9; 16; 25; 36; 49; 64; 81; 100
'' Long string literal — equally good for showing that string-token horizontal scrolling works without breaking the syntax-highlighted run boundaries.
dim as string note = "This is a deliberately long string literal — it should stay on a single visual line in horizontal-scroll mode, and only wrap when [markdown] wrapCodeBlocks=true. Use it to stress the wrap / no-wrap layout switch."
print note
list_free(numbers)
```

`callocate` zero-fills the allocation, so we don't have to memset the
node fields by hand; `deallocate` frees them on tear-down.
)"
);

const auto kJsonReply = wxString::FromUTF8(
    R"(Example JSON payload — note the non-FreeBASIC fence renders in the
system monospace font, not the editor theme font:

```json
{
  "name": "fbide",
  "version": "0.6.0",
  "features": ["editor", "compiler", "ai"],
  "experimental": true
}
```
)"
);

const auto kTableReply = wxString::FromUTF8(
    R"(Feature status:

| Feature  | Status | Notes                  |
|----------|--------|------------------------|
| Editor   | done   | Scintilla-based        |
| Compiler | wired  | calls `fbc`            |
| AI chat  | beta   | mock + live providers  |

A right-aligned numeric table:

| ID | Lines |
|---:|------:|
|  1 |    42 |
|  2 |  1024 |
| 17 |     7 |
)"
);

const auto kTasksReply = wxString::FromUTF8(
    R"(### v0.6 release checklist

- [x] Implement task lists in the markdown renderer
- [x] Add image rendering through `MarkdownImageCache`
- [x] Cover setext headings (`===` / `---` underlines)
- [ ] Polish drag-selection auto-scroll past the edges
- [ ] Wire selection support into the help / about panes
- [X] Update the mock provider's canned replies

Mixed lists work too:

1. Plan the work
2. - [ ] Sub-item that hasn't been started
   - [x] Sub-item that's done
3. Ship it
)"
);

const auto kSetextReply = wxString::FromUTF8(
    R"(Setext-style headings — the `===` and `---` underlines below count
as H1 and H2 just like `#` and `##` would.

Top-level heading
=================

This is the body of the first section. Setext H1 should render at the
same size as a `#` heading and pick up the heading bold + extra spacing
from the layout engine.

Smaller heading
---------------

Setext H2 mirrors `##`. The `---` form is also how horizontal rules are
written, but the parser disambiguates: a `---` *underneath text*  is a
setext heading, a `---` on its own (with blank lines around it) is a
horizontal rule.

---

(That last rule above is a real horizontal rule, for contrast.)
)"
);

const auto kImagesReply = wxString::FromUTF8(
    R"(A few screenshots from the [FBIde gallery](https://fbide.freebasic.net/):

![About dialog](https://fbide.freebasic.net/images/screenshots/about.png)

The classic about box — the smallest image in the gallery, good for
checking that small images render at their natural size and don't get
scaled up to fill the bubble.

![Dark theme](https://fbide.freebasic.net/images/screenshots/dark-theme.png)

Editor running with the dark theme. Wider images scale down to fit the
bubble while keeping their aspect ratio.

![Code formatter](https://fbide.freebasic.net/images/screenshots/code-formatter.png)

Before / after panes of the FreeBASIC code formatter.

![Integrated file browser](https://fbide.freebasic.net/images/screenshots/integrated-file-browser.png)

The sidebar file tree alongside the editor.
)"
);

const auto kPatchReply = wxString::FromUTF8(
    R"(Tightening the greeting — replace the literal name with the parameter:

<<<<<<< SEARCH greeter.bas
print "Hello, world!"
=======
print "Hello, " & name & "!"
>>>>>>> REPLACE

And one more, swapping the loop bound to a named constant:

<<<<<<< SEARCH greeter.bas
for i = 1 to 10
=======
for i = 1 to kMaxGreetings
>>>>>>> REPLACE
)"
);

const auto kEmojiReply = wxString::FromUTF8(
    R"(Status report 🚀

- ✅ Tests pass
- ⚠️ One flake on the macOS `gui` label
- 🐛 One open bug in the lexer
- 📦 Build size down by 3%

Coffee level: ☕☕☕ — still going.
)"
);

const auto kLongReply = wxString::FromUTF8(
    R"(# Long mock reply

This canned response is deliberately verbose so the chat surface gets
exercised across **headings**, *emphasis*, lists, fenced code, inline
`code`, a [link](https://freebasic.net), and a blockquote — useful for
spotting layout regressions in a single message.

## Background

FBIde is a lightweight IDE for FreeBASIC. The AI chat panel renders
markdown locally; the provider just hands back text. That means we can
test the entire render pipeline offline by typing a command into the
mock provider.

### A code block

```freebasic
sub greet(byval name as string)
print "Hello, " & name & "!"
end sub

greet("world")
```

### A nested list

1. First top-level item.
   - Nested bullet.
   - Another nested bullet.
2. Second top-level item.
   1. Nested ordered item.
   2. Another one.
3. Third top-level item — long enough text that this line will wrap
   inside the bubble, so the continuation indent for ordered lists
   gets a visible check.

> A short blockquote so the rule + indent treatment can be inspected.
> Two lines, to see the bar extend.

---

That dash run above should render as a horizontal rule.
)"
);

/// Concatenation of every canned reply (except the default), with help
/// first and the rest separated by horizontal rules so the boundaries
/// are visible in the rendered bubble. Lets us eyeball every render
/// path in a single bubble.
const auto kAllReply
    = kHelpReply + "\n---\n\n"
    + kShortReply + "\n\n---\n\n"
    + kTextReply + "\n\n---\n\n"
    + kFbReply + "\n---\n\n"
    + kBigCodeReply + "\n---\n\n"
    + kJsonReply + "\n---\n\n"
    + kTableReply + "\n---\n\n"
    + kEmojiReply + "\n---\n\n"
    + kTasksReply + "\n---\n\n"
    + kSetextReply + "\n---\n\n"
    + kImagesReply + "\n---\n\n"
    + kPatchReply + "\n---\n\n"
    + kLongReply;

const auto kDefaultReply = wxString::FromUTF8(
    R"(Here is a **FreeBASIC** example — a `For` loop that prints numbers:

```freebasic
dim i as integer
for i = 1 to 10
print "value: "; i
next i
```

A few notes:

- `Dim` declares the variable `i`.
- `For ... Next` repeats the body until the counter passes the end value.
- This reply came from the **mock provider** — no AI service was contacted.

Type `help` for the full menu of canned responses.
)"
);

/// Lowercase + strip surrounding whitespace. Used to match the user's
/// prompt against a canned command.
auto normalise(const wxString& text) -> wxString {
    wxString trimmed = text;
    trimmed.Trim(true).Trim(false);
    return trimmed.Lower();
}

} // namespace

auto MockProvider::pickReply(const AiRequest& request) -> PickedReply {
    if (request.messages.empty()) {
        return { .text = kDefaultReply };
    }
    const wxString key = normalise(request.messages.back().content);
    if (key == "allf" || key == "all fast") {
        return { .text = kAllReply, .fast = true };
    }
    if (key == "help" || key == "list" || key == "?") {
        return { .text = kHelpReply };
    }
    if (key == "short") {
        return { .text = kShortReply };
    }
    if (key == "long") {
        return { .text = kLongReply };
    }
    if (key == "text") {
        return { .text = kTextReply };
    }
    if (key == "fb" || key == "freebasic") {
        return { .text = kFbReply };
    }
    if (key == "bigcode" || key == "big" || key == "long fb") {
        return { .text = kBigCodeReply };
    }
    if (key == "json") {
        return { .text = kJsonReply };
    }
    if (key == "table") {
        return { .text = kTableReply };
    }
    if (key == "emoji") {
        return { .text = kEmojiReply };
    }
    if (key == "tasks" || key == "todo") {
        return { .text = kTasksReply };
    }
    if (key == "setext" || key == "headings") {
        return { .text = kSetextReply };
    }
    if (key == "images" || key == "gallery") {
        return { .text = kImagesReply };
    }
    if (key == "patch") {
        return { .text = kPatchReply };
    }
    if (key == "all") {
        return { .text = kAllReply };
    }
    return { .text = kDefaultReply };
}

MockProvider::MockProvider() {
    m_timer.SetOwner(this);
    Bind(wxEVT_TIMER, &MockProvider::onTick, this);
}

void MockProvider::send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) {
    if (m_busy) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }

    // Slice the canned reply into small chunks so the streaming UI path
    // is exercised just like a real provider. `allf` / `all fast` skip
    // the slicing and emit the whole reply in one chunk for paste-style
    // performance comparison against the streamed path.
    const auto picked = pickReply(request);
    const wxString& reply = picked.text;
    m_chunks.clear();
    if (picked.fast) {
        m_chunks.push_back(reply);
    } else {
        for (std::size_t pos = 0; pos < reply.length(); pos += kChunkChars) {
            m_chunks.push_back(reply.Mid(pos, kChunkChars));
        }
    }
    m_index = 0;
    m_onChunk = std::move(onChunk);
    m_onComplete = std::move(onComplete);
    m_busy = true;
    m_timer.Start(kTickMs);
}

void MockProvider::onTick(wxTimerEvent& /*event*/) {
    if (m_index < m_chunks.size()) {
        m_onChunk(m_chunks[m_index++]);
        return;
    }

    m_timer.Stop();
    m_busy = false;
    m_onChunk = nullptr;
    if (auto handler = std::exchange(m_onComplete, nullptr)) {
        handler(AiResponse { .ok = true, .text = {}, .error = {} });
    }
}
