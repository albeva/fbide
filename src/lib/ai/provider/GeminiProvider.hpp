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

protected:
    [[nodiscard]] auto buildUrl(const AiRequest& request) const -> wxString override;
    void applyHeaders(wxWebRequest& request) const override;
    [[nodiscard]] auto buildBody(const AiRequest& request) const -> std::string override;
    void parseLine(std::string_view line, const StreamDeltaSink& onDelta, const StreamErrorSink& onError) const override;
    [[nodiscard]] auto httpErrorMessage(int status) const -> wxString override;
    [[nodiscard]] auto unauthorizedMessage() const -> wxString override;

private:
    wxString m_apiKey; ///< Gemini API key.
};

} // namespace fbide::ai
