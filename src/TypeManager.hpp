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
        typedef Document * (*CreatorFn)( );
        
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
        void Register(const wxString & name, CreatorFn creator);
        
        /**
         * Check if type is registered
         */
        inline bool IsRegistered(const wxString & name) const noexcept
        {
            return m_types.find(name) != m_types.end()
                || m_aliases.find(name) != m_aliases.end();
        }
        
        /**
         * Bind file extensions to the type. Exts are separated by semicolon.
         * e.g. BindExtensions("source/freebasic", "bas;bi");
         */
        void BindExtensions(const wxString & name, const wxString & exts);
        
        /**
         * Bind alias to a type. Main usage is for binfing "default"
         */
        void BindAlias(const wxString & alias, const wxString & name, bool overwrite = false);
        
    private:
        
        struct Type {
            CreatorFn              creator;
            std::vector<wxString>  exts;
            Type(CreatorFn creator) : creator(creator) {}
        };
        
        
        /**
         * Gets the Type struct from name. This will
         * resolve aliases. Will return nullptr if none found
         */
        Type * GetType(const wxString &) noexcept;
        
        std::unordered_map<wxString, Type>     m_types;
        std::unordered_map<wxString, wxString> m_aliases;
    };
    
}
