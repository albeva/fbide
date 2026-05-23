//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include <cstdint>

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

/// Provider-neutral request: a model name plus the conversation so far.
/// Carries no vendor-specific fields — each `AiProvider` maps this onto
/// its own wire format.
struct AiRequest {
    wxString model;                  ///< Model identifier.
    wxString system;                 ///< Optional system prompt.
    std::vector<AiMessage> messages; ///< Conversation, oldest first.
    int maxTokens = 1024;            ///< Reply length cap.
};

/// Provider-neutral response.
struct AiResponse {
    bool ok = false; ///< True when `text` holds a valid reply.
    wxString text;   ///< Assistant reply (set when `ok`).
    wxString error;  ///< Human-readable failure reason (set when not `ok`).
};

} // namespace fbide::ai
