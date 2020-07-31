//
//  Editor.hpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
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
