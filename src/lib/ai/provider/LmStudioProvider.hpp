//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "WebStreamProvider.hpp"

namespace fbide::ai {

/**
 * AI provider backed by a local LM Studio server.
 *
 * LM Studio exposes an OpenAI-compatible REST surface at
 * `/v1/chat/completions` with `text/event-stream` deltas. Inherits
 * `WebStreamProvider` for HTTP + streaming + busy-state scaffolding.
 * API key is optional — LM Studio doesn't validate by default, but a
 * `Bearer` header is sent when one is configured so users running it
 * behind a proxy can still authenticate.
 *
 * **Threading:** UI thread only.
 */
class LmStudioProvider final : public WebStreamProvider {
public:
    NO_COPY_AND_MOVE(LmStudioProvider)

    /// Construct with the LM Studio server base URL (e.g.
    /// `http://localhost:1234`) and an optional API key. An empty key
    /// disables the `Authorization` header — matching the default
    /// out-of-the-box configuration.
    LmStudioProvider(wxString endpoint, wxString apiKey);

    /// Map `AiRole` onto OpenAI's `messages[].role`. Exposed for tests.
    /// LM Studio carries the system prompt as a `system`-role message
    /// in the array, so System gets its own mapping.
    [[nodiscard]] static auto roleToString(AiRole role) -> const char*;

    /// Parse one SSE line from LM Studio's `/v1/chat/completions` stream
    /// into delta / error / usage events through `sink`. Exposed for
    /// tests.
    static void parseStreamLine(std::string_view line, StreamLineConsumer& sink);

protected:
    [[nodiscard]] auto buildUrl(const AiRequest& request) const -> wxString override;
    void applyHeaders(wxWebRequest& request) const override;
    [[nodiscard]] auto buildBody(const AiRequest& request) const -> std::string override;
    void parseLine(std::string_view line, StreamLineConsumer& sink) const override;
    [[nodiscard]] auto httpErrorMessage(int status) const -> wxString override;
    [[nodiscard]] auto unauthorizedMessage() const -> wxString override;
    [[nodiscard]] auto requestFailedMessage(const wxString& detail) const -> wxString override;

private:
    wxString m_endpoint; ///< LM Studio server base URL (no trailing slash).
    wxString m_apiKey;   ///< Optional Bearer token — empty disables auth.
};

} // namespace fbide::ai
