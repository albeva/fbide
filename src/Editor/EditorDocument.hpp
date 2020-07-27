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
class ILexerSdk;

/**
     * Editor base class backed by a document
     */
class EditorDocument : public Document {
public:
    // freebasic type
    static const wxString Freebasic;
    static const wxString Plain;

    EditorDocument(const TypeManager::Type& type);
    virtual ~EditorDocument();

    /**
         * Instantiate the document
         */
    virtual void Create() override;

    /**
         * Load specified file. Will Create the instance
         */
    virtual void Load(const wxString& filename) override;

    /**
         * Save the document
         */
    virtual void Save(const wxString& filename) override;

    /**
         * Get underlying editor instance
         */
    StyledEditor& GetEditor() { return m_editor; }

private:
    // bound editor
    StyledEditor m_editor;
    std::unique_ptr<ILexerSdk> m_lexerIface;
};

} // namespace fbide
