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
 * AI provider backed by a local Ollama server (`/api/chat`).
 *
 * Inherits `WebStreamProvider` for the HTTP + streaming + busy-state
 * scaffolding. No API key — the server is whatever the user configured
 * locally; the model runs on the user's machine.
 *
 * **Threading:** UI thread only.
 */
class OllamaProvider final : public WebStreamProvider {
public:
    NO_COPY_AND_MOVE(OllamaProvider)

    /// Construct with the Ollama server base URL (e.g. `http://localhost:11434`).
    explicit OllamaProvider(wxString endpoint);

    /// Map `AiRole` onto Ollama's `messages[].role`. Exposed for tests.
    /// Ollama carries the system prompt as a `system`-role message in
    /// the array, so System gets its own mapping (unlike Anthropic).
    [[nodiscard]] static auto roleToString(AiRole role) -> const char*;

    /// Parse one NDJSON line from Ollama's `/api/chat` stream into
    /// delta / error events through `sink`. Exposed for tests.
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
    wxString m_endpoint; ///< Ollama server base URL (no trailing slash).
};

} // namespace fbide::ai
