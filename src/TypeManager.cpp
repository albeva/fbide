//
//  TypeManager.cpp
//  fbide
//
//  Created by Albert on 06/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "TypeManager.hpp"
using namespace fbide;


/**
 * Register document creator
 */
void TypeManager::Register(const wxString & name, CreatorFn creator)
{
    if (IsRegistered(name)) {
        throw std::invalid_argument("type '"s + name + "' is already registered");
    }
    m_types.emplace(std::make_pair(name, Type{creator}));
}


/**
 * Bind the alias
 */
void TypeManager::BindAlias(const wxString & alias, const wxString & name, bool overwrite)
{
    if (!IsRegistered(name)) {
        throw std::invalid_argument("Can't bind an alias to nonexisting type '"s + name + "'");
    }
    
    if (IsRegistered(alias) && !overwrite) {
        throw std::invalid_argument("Can't bind an alias because type '"s + alias + "' already exists");
    }
    
    m_aliases[alias] = name;
}


TypeManager::Type * TypeManager::GetType(const wxString & name) noexcept
{
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
