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

// Canned reply — deliberately under-indented and lower-cased so the
// code-block reformat + keyword-case pipeline visibly does its job.
const auto kSampleReply = wxString::FromUTF8(
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
)"
);
} // namespace

MockProvider::MockProvider() {
    m_timer.SetOwner(this);
    Bind(wxEVT_TIMER, &MockProvider::onTick, this);
}

void MockProvider::send(const AiRequest& /*request*/, ChunkHandler onChunk, ResponseHandler onComplete) {
    if (m_busy) {
        onComplete(AiResponse { .ok = false, .text = {}, .error = "A request is already in progress." });
        return;
    }

    // Slice the canned reply into small chunks so the streaming UI path
    // is exercised just like a real provider.
    m_chunks.clear();
    for (std::size_t pos = 0; pos < kSampleReply.length(); pos += kChunkChars) {
        m_chunks.push_back(kSampleReply.Mid(pos, kChunkChars));
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
