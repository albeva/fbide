//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Lang.hpp"
using namespace fbide;

void Lang::load(const wxString& filePath) {
    clear();
    m_strings.resize(maxId + 1);

    wxFFileInputStream stream(filePath);
    if (!stream.IsOk()) {
        return;
    }

    wxFileConfig config(stream);
    config.SetPath("/FBIde");

    wxString value;
    for (int index = 0; index <= maxId; index++) {
        wxString key;
        key << index;
        if (config.Read(key, &value)) {
            value.Trim();
            // Legacy language files have trailing '|' on file dialog filters
            if (value.EndsWith("|")) {
                value.RemoveLast();
            }
            m_strings[static_cast<size_t>(index)] = value;
        }
    }
}

void Lang::clear() {
    m_strings.clear();
}

auto Lang::get(LangId id) const -> const wxString& {
    static const wxString empty;
    const auto index = static_cast<size_t>(id);
    if (index < m_strings.size()) {
        return m_strings[index];
    }
    return empty;
}
