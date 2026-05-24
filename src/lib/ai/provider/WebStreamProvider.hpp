//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AiProvider.hpp"
#include "StreamParsers.hpp"

namespace fbide::ai {

/**
 * Common scaffolding for AI providers that talk to an HTTPS endpoint over
 * `wxWebRequest` with line-delimited streaming (SSE for Anthropic/Gemini,
 * NDJSON for Ollama).
 *
 * Concrete subclasses describe the request shape and the per-line parser
 * via four hooks. The base handles the in-flight bookkeeping
 * (`m_busy`, the request handle, the completion callback), the buffer +
 * line-splitting, and the `wxWebRequest` event lifecycle. Handles one
 * in-flight request at a time.
 *
 * **Threading:** UI thread only.
 */
class WebStreamProvider : public wxEvtHandler, public AiProvider {
public:
    NO_COPY_AND_MOVE(WebStreamProvider)

    WebStreamProvider();
    ~WebStreamProvider() override;

    /// Send `request` to the configured endpoint. See `AiProvider::send`.
    void send(const AiRequest& request, ChunkHandler onChunk, ResponseHandler onComplete) final;

protected:
    /// Build the absolute URL for `request`. Called once at the start of
    /// every `send`. `request.model` is the only field most providers
    /// fold into the URL.
    [[nodiscard]] virtual auto buildUrl(const AiRequest& request) const -> wxString = 0;

    /// Apply provider-specific headers (API key, version) to the request
    /// already created by the base. Method, content-type, and storage
    /// mode are set by the base — the hook should only add headers.
    virtual void applyHeaders(wxWebRequest& request) const = 0;

    /// Build the request body as a serialised JSON string. Encoding into
    /// `wxWebRequest::SetData` is handled by the base.
    [[nodiscard]] virtual auto buildBody(const AiRequest& request) const -> std::string = 0;

    /// Parse one already-split line of streamed response into delta /
    /// error events. The base has stripped a trailing `\r` from `line`
    /// (so the SSE hosts don't need to repeat the strip).
    virtual void parseLine(std::string_view line, const StreamDeltaSink& onDelta, const StreamErrorSink& onError) const = 0;

    /// Human-readable error string for an HTTP non-2xx status code.
    [[nodiscard]] virtual auto httpErrorMessage(int status) const -> wxString = 0;

    /// Message used when the server returns HTTP 401 / Unauthorized.
    /// Defaults to a generic string; providers that authenticate (API
    /// key, etc.) override to surface the relevant credential hint.
    [[nodiscard]] virtual auto unauthorizedMessage() const -> wxString;

    /// Message used when `wxWebRequest` reports State_Failed (DNS,
    /// connection, TLS …). The base appends the raw description from
    /// `wxWebRequestEvent::GetErrorDescription`; providers can override
    /// to add a service-specific hint.
    [[nodiscard]] virtual auto requestFailedMessage(const wxString& detail) const -> wxString;

private:
    /// `wxWebRequest` state transition — terminal states finish the request.
    void onRequestState(wxWebRequestEvent& event);
    /// Incoming response body chunk — buffered and parsed line by line.
    void onRequestData(wxWebRequestEvent& event);
    /// Drain whole lines from the buffer, invoking `parseLine` per line.
    /// Uses a consumed-cursor instead of `erase(0, n)` so streaming a
    /// long reply stays amortised-O(n) rather than O(n²).
    void consumeBuffer();
    /// Invoke the completion handler exactly once and reset request state.
    void finish(AiResponse response);

    wxWebRequest m_request;       ///< Current request — one at a time.
    ChunkHandler m_onChunk;       ///< Streaming delta callback.
    ResponseHandler m_onComplete; ///< Pending completion callback.
    std::string m_buffer;         ///< Unparsed response bytes.
    std::size_t m_consumed = 0;   ///< Cursor into `m_buffer` — bytes already parsed.
    wxString m_streamError;       ///< Error reported via a stream event.
    bool m_busy = false;          ///< True while a request is in flight.
};

} // namespace fbide::ai
