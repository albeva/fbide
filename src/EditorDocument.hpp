//
//  Editor.hpp
//  fbide
//
//  Created by Albert on 09/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

#include "Document.hpp"
#include "Editor.hpp"

namespace fbide {
    
    class Editor;
    
    /**
     * Editor base class backed by a document
     */
    class EditorDocument : public Document
    {
    public:
        
        /**
         * Instantiate the document
         */
        virtual void Create() override;
        
        /**
         * Load specified file. Will Create the instance
         */
        virtual void Load(const wxString & filename) override;
        
        /**
         * Save the document
         */
        virtual void Save(const wxString & filename) override;
        
        /**
         * Get underlying editor instance
         */
        Editor & GetEditor() { return m_editor; }
        
    private:
        
        // bound editor
        Editor m_editor;
    };
    
}