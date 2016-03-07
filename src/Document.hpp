//
//  Document.hpp
//  fbide
//
//  Created by Albert on 07/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

namespace fbide {
    
    /**
     * Document represents a file (usually) that is loaded. It can have
     * some backing file, UI, etc. Examples are source files (.bas, .bi),
     * projects (which contain other documents), workspaces (which contain
     * projects). Documents are usually created by TypeManager
     */
    class Document : NonCopyable
    {
    public:
        
        /**
         * Create new Document.
         */
        Document() {}
        
//        /**
//         * Create new blank instance
//         */
//        virtual void Create() = 0;
//        
//        /**
//         * Load specified file. Will Create the instance
//         */
//        virtual void LoadFile(const wxString & filename) = 0;
        
    private:
        
        // parent document
        Document * m_parent{nullptr};
    };
    
}
