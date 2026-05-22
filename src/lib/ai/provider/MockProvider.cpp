//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "MockProvider.hpp"
using namespace fbide;

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
- `json` — JSON code sample.
- `table` — markdown table.
- `emoji` — text with emoji.

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

/// Pick a canned reply based on the user's last message. Unknown
/// prompts get the default mixed reply.
auto pickReply(const AiRequest& request) -> const wxString& {
    if (request.messages.empty()) {
        return kDefaultReply;
    }
    const wxString key = normalise(request.messages.back().content);
    if (key == "help" || key == "list" || key == "?") {
        return kHelpReply;
    }
    if (key == "short") {
        return kShortReply;
    }
    if (key == "long") {
        return kLongReply;
    }
    if (key == "text") {
        return kTextReply;
    }
    if (key == "fb" || key == "freebasic") {
        return kFbReply;
    }
    if (key == "json") {
        return kJsonReply;
    }
    if (key == "table") {
        return kTableReply;
    }
    if (key == "emoji") {
        return kEmojiReply;
    }
    return kDefaultReply;
}

} // namespace

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
    // is exercised just like a real provider.
    const wxString& reply = pickReply(request);
    m_chunks.clear();
    for (std::size_t pos = 0; pos < reply.length(); pos += kChunkChars) {
        m_chunks.push_back(reply.Mid(pos, kChunkChars));
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
