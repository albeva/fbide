//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "DocumentType.hpp"

namespace fbide {
class Context;

/// Popup menu for the status bar document-type selector. Each item maps
/// 1:1 to an entry in `kDocumentTypes` so the selection event can be
/// decoded without keeping per-menu state.
class DocumentTypeMenu {
public:
    /// Type radio items: kIdBase + index-in-kDocumentTypes.
    static constexpr int kIdBase = wxID_HIGHEST + 10300;

    /// Build the type popup with `current` checked. Labels come from the
    /// active locale via `ctx.tr("statusbar/type/<key>")`; if a translation
    /// is missing the bare key is shown.
    [[nodiscard]] static auto build(Context& ctx, DocumentType current) -> std::unique_ptr<wxMenu>;

    /// Decode a menu selection id back to a DocumentType. Returns nullopt
    /// when the id is outside the type range.
    [[nodiscard]] static auto typeFromId(int id) -> std::optional<DocumentType>;
};

} // namespace fbide
