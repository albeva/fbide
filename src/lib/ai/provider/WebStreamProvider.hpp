//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "AiProvider.hpp"

namespace fbide::ai {

/**
 * Sink interface for `WebStreamProvider::parseLine` implementations.
 *
 * Avoids the per-line `std::function` construction that a callback-pair
 * API would force at every loop iteration. `WebStreamProvider` itself
 * implements this interface (privately) and just passes `*this` to the
 * parser; tests subclass it directly.
 */
class StreamLineConsumer {
public:
    NO_COPY_AND_MOVE(StreamLineConsumer)

    StreamLineConsumer() = default;
    virtual ~StreamLineConsumer() = default;

    /// Append `delta` to the in-flight reply. Called once per text
    /// fragment in a streamed line (Gemini parts emit multiple).
    virtual void onDelta(const wxString& delta) = 0;

    /// Report a stream-level error from the provider's response shape
    /// (not the HTTP status — that lands in `httpErrorMessage`). At
    /// most one error per request typically; later calls overwrite.
    virtual void onError(const wxString& message) = 0;

    /// Hand off one fully-assembled `tool_use` block parsed from the
    /// stream. Default no-op — only providers that emit tool calls
    /// override (currently Anthropic). The host loop dispatches the
    /// call after the response completes; ordering within a turn
    /// matches the stream's arrival order. By value so the sink can
    /// move the call into its owner's queue without copying.
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    virtual void onToolCall(AiToolCall /*call*/) {}
};

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
// Multiple inheritance: `wxEvtHandler` (event sink for wxWebRequest
// state/data events) plus the `AiProvider` interface. Both have
// non-pure-virtual members; the combination is intentional because
// `wxWebRequest`'s event binding requires a `wxEvtHandler` subclass.
// NOLINTNEXTLINE(misc-multiple-inheritance)
class WebStreamProvider
: public wxEvtHandler,
  public AiProvider {
public:
    NO_COPY_AND_MOVE(WebStreamProvider)

    WebStreamProvider();
    ~WebStreamProvider() override;

    /// Send `request` to the configured endpoint. See `AiProvider::send`.
    /// Tool support is provider-specific — base `WebStreamProvider`
    /// forwards `onToolCall` only when the concrete provider opted in
    /// via `supportsTools()` (Anthropic does, Gemini/Ollama don't yet).
    void send(const AiRequest& request, ChunkHandler onChunk, ToolCallHandler onToolCall, ResponseHandler onComplete) final;

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

    /// Parse one already-split line of streamed response, dispatching
    /// delta / error events through `sink`. The base has stripped a
    /// trailing `\r` from `line` so SSE hosts don't need to repeat it.
    virtual void parseLine(std::string_view line, StreamLineConsumer& sink) const = 0;

    /// Human-readable error string for an HTTP non-2xx status code.
    [[nodiscard]] virtual auto httpErrorMessage(int status) const -> wxString = 0;

    /// Message used when the server returns HTTP 401 / Unauthorized.
    /// Pure virtual so every provider names its credential source —
    /// the user needs a hint at where to look, not just "Unauthorized."
    [[nodiscard]] virtual auto unauthorizedMessage() const -> wxString = 0;

    /// Message used when `wxWebRequest` reports State_Failed (DNS,
    /// TLS, etc). Pure virtual so every provider can fold in the
    /// service-specific hint that turns a generic network failure
    /// into actionable feedback. `detail` is the raw description
    /// from `wxWebRequestEvent::GetErrorDescription`.
    [[nodiscard]] virtual auto requestFailedMessage(const wxString& detail) const -> wxString = 0;

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

    /// Adaptor that forwards parser events into the owning provider's
    /// in-flight callback / error state. Lives as a member so the
    /// `parseLine` call can pass a `StreamLineConsumer&` without
    /// publicly exposing the consumer interface on `WebStreamProvider`.
    class Sink final : public StreamLineConsumer {
    public:
        NO_COPY_AND_MOVE(Sink)
        explicit Sink(WebStreamProvider& owner)
        : m_owner(owner) {}
        ~Sink() override = default;
        void onDelta(const wxString& delta) override;
        void onError(const wxString& message) override;
        void onToolCall(AiToolCall call) override;

    private:
        WebStreamProvider& m_owner;
    };

    wxWebRequest m_request;       ///< Current request — one at a time.
    ChunkHandler m_onChunk;       ///< Streaming delta callback.
    ToolCallHandler m_onToolCall; ///< Tool-use callback (no-op for providers without tools).
    ResponseHandler m_onComplete; ///< Pending completion callback.
    std::string m_buffer;         ///< Unparsed response bytes.
    std::size_t m_consumed = 0;   ///< Cursor into `m_buffer` — bytes already parsed.
    wxString m_streamError;       ///< Error reported via a stream event.
    Sink m_sink { *this };        ///< Adaptor passed to `parseLine`.
    bool m_busy = false;          ///< True while a request is in flight.
};

} // namespace fbide::ai
