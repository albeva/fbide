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
 * AI provider backed by the Google Gemini API (Generative Language API).
 *
 * Inherits `WebStreamProvider` for the HTTP + streaming + busy-state
 * scaffolding. Authentication uses the `x-goog-api-key` header; the
 * URL is constructed per-model from `streamGenerateContent?alt=sse`.
 *
 * **Threading:** UI thread only.
 */
class GeminiProvider final : public WebStreamProvider {
public:
    NO_COPY_AND_MOVE(GeminiProvider)

    /// Construct with the Google AI Studio API key.
    explicit GeminiProvider(wxString apiKey);

    /// Map `AiRole` onto Gemini's `contents[].role`. Exposed for tests.
    /// Gemini uses `model` for assistant and carries the system prompt
    /// as `systemInstruction`, so System never appears here.
    [[nodiscard]] static auto roleToString(AiRole role) -> const char*;

    /// Parse one SSE line from Gemini's `streamGenerateContent?alt=sse`
    /// into delta / error events through `sink`. A line may carry
    /// multiple `parts[*].text` fragments — each emits its own delta.
    /// Exposed for tests.
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
    wxString m_apiKey; ///< Gemini API key.
};

} // namespace fbide::ai
