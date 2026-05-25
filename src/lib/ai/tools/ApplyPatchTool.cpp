//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ApplyPatchTool.hpp"
#include <nlohmann/json.hpp>
using namespace fbide;
using namespace fbide::ai;
using json = nlohmann::json;

namespace {

constexpr auto kSchema = R"({
"type":"object",
"properties":{
"search":{"type":"string","description":"Exact text from the edit target to find."},
"replace":{"type":"string","description":"Text to substitute in its place."}
},
"required":["search","replace"]
})";

constexpr auto kDescription = "Apply a SEARCH/REPLACE patch to the pinned edit "
                              "target. The search text must match the file "
                              "byte-for-byte (including indentation). Returns "
                              "JSON with status \"applied\" or \"no_match\".";

/// Pull a required string field from `args` into `out`. Returns false
/// and writes a human message into `error` if the field is missing or
/// not a string.
auto requireStringField(const json& args, const char* key, wxString& out, wxString& error) -> bool {
    const auto it = args.find(key);
    if (it == args.end() || !it->is_string()) {
        error = wxString::Format("'%s' argument is required and must be a string", key);
        return false;
    }
    const auto value = it->get<std::string>();
    out = wxString::FromUTF8(value);
    return true;
}

} // namespace

ApplyPatchTool::ApplyPatchTool(Hooks hooks)
: m_hooks(std::move(hooks)) {}

auto ApplyPatchTool::descriptor() const -> AiTool {
    return AiTool {
        .name = kName,
        .description = kDescription,
        .inputSchemaJson = kSchema,
    };
}

void ApplyPatchTool::invoke(AiToolCall call, ResultHandler handler) {
    // Gating — refuse with isError so the model can adjust rather than
    // retry forever.
    if (!m_hooks.isAgentMode || !m_hooks.isAgentMode()) {
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = "Agent mode is off — apply_patch is unavailable. Ask the user to toggle it.",
            .isError = true,
        });
        return;
    }
    if (!m_hooks.hasEditTarget || !m_hooks.hasEditTarget()) {
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = "No edit target pinned. Ask the user to pin a file as the edit target.",
            .isError = true,
        });
        return;
    }

    const auto args = json::parse(call.argumentsJson.utf8_string(), nullptr, false);
    if (args.is_discarded() || !args.is_object()) {
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = "Arguments must be a JSON object with 'search' and 'replace' strings.",
            .isError = true,
        });
        return;
    }
    wxString search;
    wxString replace;
    wxString fieldError;
    if (!requireStringField(args, "search", search, fieldError)
        || !requireStringField(args, "replace", replace, fieldError)) {
        handler(AiToolResult {
            .toolUseId = std::move(call.id),
            .content = fieldError,
            .isError = true,
        });
        return;
    }

    const bool ok = m_hooks.applyPatch && m_hooks.applyPatch(search, replace);
    const json result {
        { "status", ok ? "applied" : "no_match" },
    };
    handler(AiToolResult {
        .toolUseId = std::move(call.id),
        .content = wxString::FromUTF8(result.dump()),
        .isError = !ok,
    });
}
