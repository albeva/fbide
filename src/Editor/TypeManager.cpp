//
//  TypeManager.cpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//

#include "TypeManager.hpp"
#include "Config/Config.hpp"
#include "App/Manager.hpp"
using namespace fbide;

/**
 * Register document creator
 */
void TypeManager::Register(const wxString& name, CreatorFn creator) {
    if (IsRegistered(name)) {
        throw std::invalid_argument("type '"s + name + "' is already registered");
    }

    const Config* config = nullptr;
    std::vector<wxString> exts;

    if (name.SubString(0, 4) == "text/") {
        auto path = "Editor." + name.Right(name.length() - 5);
        config = GetConfig().Get(path);
        if (config) {
            auto es = config->Get("exts");
            if (es && es->IsArray()) {
                for (auto& ext : const_cast<Config*>(es)->AsArray()) {
                    if (ext.IsString()) {
                        exts.push_back(ext.AsString());
                    }
                }
            }
        }
    }

    m_types.emplace(std::make_pair(name, Type{ name, exts, config ? *config : Config::Empty, creator }));
}


/**
 * Bind the alias
 */
void TypeManager::BindAlias(const wxString& alias, const wxString& name, bool overwrite) {
    if (!IsRegistered(name)) {
        throw std::invalid_argument("Can't bind an alias to nonexisting type '"s + name + "'");
    }

    if (IsRegistered(alias) && !overwrite) {
        throw std::invalid_argument("Can't bind an alias because type '"s + alias + "' already exists");
    }

    m_aliases[alias] = name;
}


/**
 * Bind extensions to the name
 */
void TypeManager::BindExtensions(const wxString& name, const wxString& exts) {
    auto type = GetType(name);
    if (type == nullptr) {
        throw std::invalid_argument("Type '"s + name + "' not found");
    }

    wxStringTokenizer tokenizer(exts, ";,: ");
    while (tokenizer.HasMoreTokens()) {
        auto ext = tokenizer.GetNextToken();
        auto iter = std::find(type->exts.begin(), type->exts.end(), ext);
        if (iter == type->exts.end()) {
            type->exts.push_back(ext);
        }
    }
}


/**
 * Create Document from type
 */
Document* TypeManager::CreateFromType(const wxString& name) {
    auto type = GetType(name);
    if (type == nullptr) {
        throw std::invalid_argument("Type '"s + name + "' not found");
    }
    return type->creator(*type);
}


/**
 * find type for the given name. Return nullptr if none found
 */
TypeManager::Type* TypeManager::GetType(const wxString& name) noexcept {
    auto type = m_types.find(name);
    if (type != m_types.end()) {
        return &type->second;
    }

    auto alias = m_aliases.find(name);
    if (alias != m_aliases.end()) {
        return GetType(alias->second);
    }

    return nullptr;
}
