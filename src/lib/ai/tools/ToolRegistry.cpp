//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "ToolRegistry.hpp"
using namespace fbide;
using namespace fbide::ai;

void ToolRegistry::add(std::unique_ptr<Tool> tool) {
    m_tools.push_back(std::move(tool));
}

auto ToolRegistry::descriptors() const -> std::vector<AiTool> {
    std::vector<AiTool> result;
    result.reserve(m_tools.size());
    for (const auto& tool : m_tools) {
        result.push_back(tool->descriptor());
    }
    return result;
}

void ToolRegistry::invoke(AiToolCall call, Tool::ResultHandler handler) {
    for (const auto& tool : m_tools) {
        if (tool->descriptor().name == call.name) {
            tool->invoke(std::move(call), std::move(handler));
            return;
        }
    }
    // Unknown tool — surface the failure so the model can pick a
    // different one rather than retry the same call.
    auto callId = std::move(call.id);
    auto callName = std::move(call.name);
    handler(AiToolResult {
        .toolUseId = std::move(callId),
        .content = wxString::Format("Tool '%s' is not registered.", callName),
        .isError = true,
    });
}
