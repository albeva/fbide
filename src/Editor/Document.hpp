//
//  Document.hpp
//  fbide
//
//  Created by Albert on 07/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "TypeManager.hpp"
#include "app_pch.hpp"


namespace fbide {

/**
 * Document represents a file (usually) that is loaded. It can have
 * some backing file, UI, etc. Examples are source files (.bas, .bi),
 * projects (which contain other documents), workspaces (which contain
 * projects). Documents are usually created by TypeManager
 */
class Document {
public:
    Document(const TypeManager::Type& type);
    virtual ~Document();

    /**
     * Get unique document id
     */
    int GetId() const {
        return m_id;
    }

    /**
     * Get document type
     */
    const TypeManager::Type& GetType() const {
        return m_type;
    }

    /**
     * Instantiate the document
     */
    virtual void Create() = 0;

    /**
     * Load specified file. Will Create the instance
     */
    virtual void Load(const wxString& filename) = 0;

    /**
     * Save the document
     */
    virtual void Save(const wxString& filename) = 0;

    /**
     * Set document filename
     */
    virtual void SetFilename(const wxString& filename);

    /**
     * Get filename
     */
    virtual const wxString& GetFilename() const {
        return m_filename;
    }

    /**
     * Set document title
     */
    virtual void SetTitle(const wxString& title);


    /**
     * Get title
     */
    virtual const wxString& GetTitle() const {
        return m_title;
    }

protected:
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
