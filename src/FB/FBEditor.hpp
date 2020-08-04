/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "pch.h"
#include "Editor/TextDocument.hpp"

namespace fbide {

struct StyleEntry;
namespace FB::Parser {
class SourceLexer;
}

class FBEditor final: public TextDocument {
    NON_COPYABLE(FBEditor)
public:
    // Editor mime type
    static const wxString TypeId;

    explicit FBEditor(const TypeManager::Type& type);
    ~FBEditor() final;
    void CreateDocument() final;

private:
    void OnModified(wxStyledTextEvent& event);
    void OnStyleNeeded(wxStyledTextEvent& event);

    void LoadConfiguration(const Config& config);
    void LoadTheme();
    void LoadStyle(int nr, const StyleEntry&);

    std::unique_ptr<FB::Parser::SourceLexer> m_sourceLexer;

    wxDECLARE_EVENT_TABLE(); // NOLINT
};

} // namespace fbide
