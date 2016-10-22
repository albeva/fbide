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
     * Manage file loaders and assist in creating
     * load/save dialogs
     */
    class TypeManager : NonCopyable
    {
    public:
        
        /**
         * Signature for function that can create a Document
         */
        typedef Document*(*CreatorFn)();
        
        /**
         * Register a subclass of Document with the TypeManager
         */
        template<typename T>
        inline void Register(const wxString & name, std::vector<wxString> exts)
        {
            static_assert(std::is_base_of<Document, T>::value &&
                          !std::is_same<Document, T>::value,
                          "Registered type must be subclass of Document");
            Register(name, []() -> Document* { return new T; }, exts);
        }
        
        /**
         * Register a document creator function
         */
        void Register(const wxString & name, CreatorFn creator, std::vector<wxString> exts);
        
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
         * Bind alias to a type. E.g. useful to bind "default"
         */
        void BindAlias(const wxString & alias, const wxString & name, bool overwrite = false);
        
        /**
         * Create document from type
         */
        Document * CreateFromType(const wxString & name);
        
    private:
        
        struct Type
        {
            CreatorFn              creator;
            std::vector<wxString>  exts;
        };
        
        
        /**
         * Gets the Type struct from name. This will
         * resolve aliases. Will return nullptr if none found
         */
        Type * GetType(const wxString &) noexcept;
        
        StringMap<Type>     m_types;
        StringMap<wxString> m_aliases;
    };
    
}
