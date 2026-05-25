//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompileTool.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

constexpr auto kSchema = R"({"type":"object","properties":{}})";

constexpr auto kDescription = "Compile the pinned edit target with the configured "
                              "FreeBASIC compiler. Returns JSON with status "
                              "\"ok\"|\"failed\"|\"cancelled\" and the captured "
                              "compiler output (truncated to 16 KB). Use this to "
                              "verify edits before reporting back to the user.";

/// Wire up a JSON-shaped tool result for the model. `output` is the
/// truncated compiler text; `status` is one of "ok|failed|cancelled".
auto formatResult(const wxString& toolUseId, const wxString& status, const wxString& output, const bool isError) -> AiToolResult {
    const json payload {
        { "status", status.utf8_string() },
        { "output", output.utf8_string() },
    };
    return AiToolResult {
        .toolUseId = toolUseId,
        .content = wxString::FromUTF8(payload.dump()),
        .isError = isError,
    };
}

} // namespace

CompileTool::CompileTool(Hooks hooks)
: m_hooks(std::move(hooks)) {}

auto CompileTool::descriptor() const -> AiTool {
    return AiTool {
        .name = kName,
        .description = kDescription,
        .inputSchemaJson = kSchema,
    };
}

auto CompileTool::truncateOutput(const wxArrayString& output) -> wxString {
    // Join the lines into one buffer up-front — fbc reports line-by-
    // line, but the model wants a single text block to read.
    wxString joined;
    for (const auto& line : output) {
        joined += line;
        joined += "\n";
    }
    const auto bytes = joined.utf8_string().size();
    if (bytes <= kMaxOutputBytes) {
        return joined;
    }
    // Head + tail of roughly equal size, leaving room for the marker.
    constexpr std::size_t kMarkerReserve = 64;
    const std::size_t budget = kMaxOutputBytes - kMarkerReserve;
    const std::size_t headBytes = budget / 2;
    const std::size_t tailBytes = budget - headBytes;
    const auto raw = joined.utf8_string();
    auto head = wxString::FromUTF8(raw.substr(0, headBytes));
    auto tail = wxString::FromUTF8(raw.substr(raw.size() - tailBytes));
    const auto skipped = raw.size() - headBytes - tailBytes;
    return head + wxString::Format("\n[truncated %zu bytes]\n", skipped) + tail;
}

void CompileTool::invoke(AiToolCall call, ResultHandler handler) {
    // Gate sequence: agent → allow-compile → per-turn cap → ready.
    // Order chosen so the most specific message lands first; the model
    // adjusts faster when it knows exactly which gate failed.
    if (!m_hooks.isAgentMode || !m_hooks.isAgentMode()) {
        handler(formatResult(call.id, "failed",
            "Agent mode is off — compile is unavailable. Ask the user to toggle it.", true));
        return;
    }
    if (!m_hooks.isAllowCompile || !m_hooks.isAllowCompile()) {
        handler(formatResult(call.id, "failed",
            "The 'Allow compile' option is off — compile is unavailable. Ask the user to tick it.", true));
        return;
    }
    if (!m_hooks.tryBumpCompileCount || !m_hooks.tryBumpCompileCount()) {
        handler(formatResult(call.id, "failed",
            "Compile invocation cap for this turn was reached. Try a different approach.", true));
        return;
    }
    if (m_hooks.validateReady) {
        if (const auto issue = m_hooks.validateReady(); !issue.empty()) {
            handler(formatResult(call.id, "failed", issue, true));
            return;
        }
    }

    auto callId = std::move(call.id);
    if (!m_hooks.startCompilation) {
        handler(formatResult(callId, "failed", "Compile host is not wired up.", true));
        return;
    }
    m_hooks.startCompilation([callId, handler = std::move(handler)](const bool ok, wxArrayString output) {
        const auto text = CompileTool::truncateOutput(output);
        const wxString status = ok ? "ok" : "failed";
        // A `[cancelled]` sentinel from BuildTask's destructor — surface
        // as "cancelled" status so the model doesn't treat it as a real
        // build failure (the user explicitly killed the process).
        const bool cancelled = output.size() == 1 && output[0] == "[cancelled]";
        if (cancelled) {
            handler(formatResult(callId, "cancelled", text, true));
            return;
        }
        handler(formatResult(callId, status, text, !ok));
    });
}
