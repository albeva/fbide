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
#include "LexerSdk.hpp"

namespace fbide {

class FBEditor final: public TextDocument, public ILexerSdk {
    NON_COPYABLE(FBEditor)
public:
    // Editor mime type
    static const wxString TypeId;

    explicit FBEditor(const TypeManager::Type& type);
    ~FBEditor() final;
    void CreateDocument() final;

    // fblexer communication
    void Log(const std::string& message) final;

private:
    void OnCharAdded(wxStyledTextEvent &event);

    static bool s_fbLexerLoaded; // NOLINT
    void LoadFBLexer();
    void LoadConfiguration(const Config& config);
    void LoadTheme(const Config &theme);

    wxDECLARE_EVENT_TABLE(); // NOLINT
};

} // namespace fbide
