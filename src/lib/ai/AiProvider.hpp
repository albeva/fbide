//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AiTypes.hpp"

namespace fbide {

/**
 * Abstract AI model backend.
 *
 * Concrete providers (Anthropic, and later OpenAI / Gemini / Ollama) map
 * the provider-neutral `AiRequest` onto their wire format and the reply
 * back onto an `AiResponse`. Keeping this interface vendor-neutral is what
 * lets new backends drop in without touching `AiManager` or the UI.
 *
 * **Threading:** UI thread only. `send` is asynchronous — the handler is
 * invoked later on the UI thread.
 */
class AiProvider {
public:
    NO_COPY_AND_MOVE(AiProvider)

    /// Callback delivering the result; always invoked on the UI thread.
    using ResponseHandler = std::function<void(AiResponse)>;

    AiProvider() = default;
    virtual ~AiProvider() = default;

    /// Send `request`. `handler` runs exactly once when the reply or an
    /// error arrives. Implementations reject overlapping calls with an
    /// error response.
    virtual void send(const AiRequest& request, ResponseHandler handler) = 0;
};

} // namespace fbide
