//
//  TypeManager.hpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once

namespace fbide {
    
    class Document;
    
    
    /**
     * Create new Document instance
     */
    template<typename T> Document * DocumentCreator()
    {
        return new T();
    }
    
    
    /**
     * Manage file loaders and assist in creating
     * load/save dialogs
     */
    class TypeManager : NonCopyable
    {
    public:
        
        /**
         * Signature for function that can create a Document
         */
        using Creator = Document*();
        
        /**
         * Register a subclass of Document with the TypeManager
         */
        template<typename T>
        inline void Register(const wxString & name)
        {
            static_assert(std::is_base_of<Document, T>::value &&
                          std::is_same<Document, T>::value == false,
                          "Registered type must be subclass of Document");
            Register(name, DocumentCreator<T>);
        }
        
        /**
         * Register a document creator function
         */
        void Register(const wxString & name, Creator creator);
        
    };
    
}
