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
 * AI provider backed by the Anthropic Messages API.
 *
 * Inherits `WebStreamProvider` for the HTTP + streaming + busy-state
 * scaffolding; only the URL, headers, body shape, and per-line parser
 * are provider-specific. Authentication uses the `x-api-key` header.
 *
 * **Threading:** UI thread only.
 */
class AnthropicProvider final : public WebStreamProvider {
public:
    NO_COPY_AND_MOVE(AnthropicProvider)

    /// Construct with the Anthropic API key (`x-api-key` header value).
    explicit AnthropicProvider(wxString apiKey);

    /// Map `AiRole` onto Anthropic's `messages[].role`. Exposed for tests.
    /// System folds to user — Anthropic carries the system prompt as a
    /// top-level field, so the System role never appears in `messages`.
    [[nodiscard]] static auto roleToString(AiRole role) -> const char*;

    /// Parse one SSE line from Anthropic's `/v1/messages` stream into
    /// delta / error events through `sink`. Exposed for tests so the
    /// dispatch can be exercised without spinning up a real request.
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
    wxString m_apiKey; ///< Anthropic API key.
};

} // namespace fbide::ai
