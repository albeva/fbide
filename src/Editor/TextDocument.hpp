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
#include "Document/Document.hpp"

namespace fbide {

/**
 * Editor base class backed by a text document
 */
class TextDocument: public wxStyledTextCtrl, public Document {
    NON_COPYABLE(TextDocument)
public:

    static const wxString TypeId;

    explicit TextDocument(const TypeManager::Type& type);
    ~TextDocument() override;

    /**
     * Instantiate the document
     */
    void CreateDocument() override;

    /**
     * LoadDocument specified file. Will CreateDocument the instance
     */
    void LoadDocument(const wxString& filename) override;

    /**
     * SaveDocument the document
     */
    void SaveDocument(const wxString& filename) override;

};

} // namespace fbide
