//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide::ai {

/// Role of a message within an AI conversation.
enum class AiRole : std::uint8_t {
    System,
    User,
    Assistant
};

/// One message in an AI conversation.
struct AiMessage {
    AiRole role;      ///< Who authored the message.
    wxString content; ///< Message text.
};

/// One block of the system prompt. The `cacheable` flag is a hint to
/// providers that support prompt caching (currently only Anthropic) —
/// stable content (base prompt, file context) is marked true so the
/// provider can attach a cache breakpoint; volatile content (open-tab
/// buffer snapshots) stays false. Providers without caching ignore the
/// flag and concatenate via `joinSystem`.
struct AiContent {
    wxString text;
    bool cacheable = false;
};

/// Concatenate `blocks` with `"\n\n"` separators, skipping empty
/// blocks. Used by providers that lack prompt caching to collapse the
/// structured form back to a single string.
[[nodiscard]] auto joinSystem(const std::vector<AiContent>& blocks) -> wxString;

/// Default cap on assistant reply length. Providers map this onto their
/// `max_tokens` (or equivalent) request field.
inline constexpr int kDefaultMaxTokens = 1024;

/// Provider-neutral request: a model name plus the conversation so far.
/// Carries no vendor-specific fields — each `AiProvider` maps this onto
/// its own wire format.
struct AiRequest {
    wxString model;                    ///< Model identifier.
    std::vector<AiContent> system;     ///< Optional system prompt, one block per
                                       ///< section. Providers that support prompt
                                       ///< caching emit a cache breakpoint after
                                       ///< each cacheable block; the rest fold
                                       ///< the vector back to a string via
                                       ///< `joinSystem`.
    std::vector<AiMessage> messages;   ///< Conversation, oldest first.
    int maxTokens = kDefaultMaxTokens; ///< Reply length cap.
};

/// Provider-neutral response.
struct AiResponse {
    bool ok = false; ///< True when `text` holds a valid reply.
    wxString text;   ///< Assistant reply (set when `ok`).
    wxString error;  ///< Human-readable failure reason (set when not `ok`).
};

} // namespace fbide::ai
