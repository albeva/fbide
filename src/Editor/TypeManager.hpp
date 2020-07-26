//
//  TypeManager.hpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

class Document;
class Config;

/**
     * Manage file loaders and assist in creating
     * load/save dialogs
     */
class TypeManager final : NonCopyable {
public:
    struct Type;

    /**
         * Signature for function that can create a Document
         */
    typedef Document* (*CreatorFn)(const Type& type);

    /**
         * Registered type information
         */
    struct Type {
        wxString name;              // mime style name
        std::vector<wxString> exts; // file extensions
        const Config& config;       // optional Config object
        CreatorFn creator;          // creator function
    };

    /**
         * Register a subclass of Document with the TypeManager
         */
    template<typename T>
    inline void Register(const wxString& name) {
        static_assert(std::is_base_of<Document, T>::value && !std::is_same<Document, T>::value,
            "Registered type must be subclass of Document");
        Register(name, [](const Type& type) -> Document* { return new T(type); });
    }

    /**
         * Register a document creator function
         */
    void Register(const wxString& name, CreatorFn creator);

    /**
         * Check if type is registered
         */
    inline bool IsRegistered(const wxString& name) const noexcept {
        return m_types.find(name) != m_types.end()
               || m_aliases.find(name) != m_aliases.end();
    }

    /**
         * Bind file extensions to the type. Exts are separated by semicolon.
         * e.g. BindExtensions("source/freebasic", "bas;bi");
         */
    void BindExtensions(const wxString& name, const wxString& exts);

    /**
         * Bind alias to a type. E.g. useful to bind "default"
         */
    void BindAlias(const wxString& alias, const wxString& name, bool overwrite = false);

    /**
         * Create document from type
         */
    Document* CreateFromType(const wxString& name);

private:
    /**
         * Gets the Type struct from name. This will
         * resolve aliases. Will return nullptr if none found
         */
    Type* GetType(const wxString&) noexcept;

    StringMap<Type> m_types;
    StringMap<wxString> m_aliases;
};

} // namespace fbide
