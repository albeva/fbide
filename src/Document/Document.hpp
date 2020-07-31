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
#include "TypeManager.hpp"
#include "pch.h"


namespace fbide {

/**
 * Document represents a file (usually) that is loaded. It can have
 * some backing file, UI, etc. Examples are source files (.bas, .bi),
 * projects (which contain other documents), workspaces (which contain
 * projects). Documents are usually created by TypeManager
 */
class Document {
    NON_COPYABLE(Document)
public:
    explicit Document(const TypeManager::Type& type);
    virtual ~Document();

    /**
     * Get unique document id
     */
    [[nodiscard]] int GetDocumentId() const noexcept {
        return m_id;
    }

    /**
     * Get document type
     */
    [[nodiscard]] const TypeManager::Type& GetDocumentType() const noexcept {
        return m_type;
    }

    /**
     * Instantiate the document
     */
    virtual void CreateDocument() = 0;

    /**
     * Load / Store
     */
    virtual void LoadDocument(const wxString& filename) = 0;
    virtual void SaveDocument(const wxString& filename) = 0;

    /**
     * File name backing the document.
     */
    virtual void SetDocumentFileName(const wxString& filename);

    [[nodiscard]] const wxString& GetDocumentFileName() const noexcept {
        return m_filename;
    }

    /**
     * Title that is visible in the document tab, window title, etc.
     */
    virtual void SetDocumentTitle(const wxString& title);

    [[nodiscard]] const wxString& GetDocumentTitle() const noexcept {
        return m_title;
    }

private:
    // document id
    int m_id;

    // backing file
    wxString m_filename;

    // document title
    wxString m_title;

    // document type. Hold by const &
    const TypeManager::Type& m_type;
};

} // namespace fbide
