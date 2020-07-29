//
//  Editor.hpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"

#include "Document.hpp"
#include "StyledEditor.hpp"

namespace fbide {

class Editor;

/**
 * Editor base class backed by a text document
 */
class EditorDocument: public Document {
    NON_COPYABLE(EditorDocument)
public:

    static const wxString TypeId;

    EditorDocument(const TypeManager::Type& type);
    virtual ~EditorDocument();

    /**
     * Instantiate the document
     */
    virtual void Create();

    /**
     * Load specified file. Will Create the instance
     */
    void Load(const wxString& filename) final;

    /**
     * Save the document
     */
    void Save(const wxString& filename) final;

    /**
     * Get underlying editor instance
     */
    inline StyledEditor& GetEditor() { return m_editor; }

private:
    // bound editor
    StyledEditor m_editor;
};

} // namespace fbide
