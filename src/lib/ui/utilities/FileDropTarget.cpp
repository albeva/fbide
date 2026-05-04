//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileDropTarget.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/Value.hpp"
#include "document/DocumentManager.hpp"

using namespace fbide;

namespace {
constexpr auto kAllPatternsKey = "all";
constexpr auto kGlobSeparator = ";";
} // namespace

FileDropTarget::FileDropTarget(Context& ctx)
: m_ctx(ctx) {}

auto FileDropTarget::OnDropFiles(wxCoord /*x*/, wxCoord /*y*/, const wxArrayString& filenames) -> bool {
    bool opened = false;
    auto& docManager = m_ctx.getDocumentManager();
    for (const auto& path : filenames) {
        if (!isSupported(path)) {
            continue;
        }
        if (docManager.openFile(path) != nullptr) {
            opened = true;
        }
    }
    return opened;
}

auto FileDropTarget::isSupported(const wxString& filename) const -> bool {
    const auto& patterns = m_ctx.getConfigManager().config().at("filePatterns");
    if (!patterns.isTable()) {
        return false;
    }

    const wxString name = wxFileName(filename).GetFullName();
    for (const auto& [key, value] : patterns.entries()) {
        if (key == kAllPatternsKey) {
            continue;
        }
        const auto globs = value->value_or(wxString {});
        if (globs.IsEmpty()) {
            continue;
        }
        wxStringTokenizer tokenizer(globs, kGlobSeparator);
        while (tokenizer.HasMoreTokens()) {
            const auto glob = tokenizer.GetNextToken().Trim(true).Trim(false);
            if (glob.IsEmpty()) {
                continue;
            }
            if (wxMatchWild(glob, name, false)) {
                return true;
            }
        }
    }
    return false;
}
