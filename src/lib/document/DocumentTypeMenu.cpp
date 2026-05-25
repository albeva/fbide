//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "DocumentTypeMenu.hpp"
#include "app/Context.hpp"
using namespace fbide;

auto DocumentTypeMenu::build(Context& ctx, const DocumentType current) -> std::unique_ptr<wxMenu> {
    auto menu = std::make_unique<wxMenu>();
    for (std::size_t i = 0; i < kDocumentTypes.size(); i++) {
        const auto type = kDocumentTypes[i];
        const auto key = documentTypeKey(type);
        auto label = ctx.tr(wxString("statusbar.type.") + wxString::FromUTF8(key.data(), key.size()));
        if (label.empty()) {
            label = wxString::FromUTF8(key.data(), key.size());
        }
        const int id = kIdBase + static_cast<int>(i);
        auto* item = menu->AppendRadioItem(id, label);
        if (type == current) {
            item->Check(true);
        }
    }
    return menu;
}

auto DocumentTypeMenu::typeFromId(const int id) -> std::optional<DocumentType> {
    const auto offset = id - kIdBase;
    if (offset < 0 || offset >= static_cast<int>(kDocumentTypes.size())) {
        return std::nullopt;
    }
    return kDocumentTypes[static_cast<std::size_t>(offset)];
}
