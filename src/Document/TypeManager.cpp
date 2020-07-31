/*
 * This file is part of fbide project, an open source IDE
 * for FreeBASIC.
 * https://github.com/albeva/fbide
 * http://fbide.freebasic.net
 * Copyright (C) 2020 Albert Varaksin
 *
 * fbide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fbide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 */
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
        const int prefiexLen = 5;
        auto path = "Editor.Types." + name.Right(name.length() - prefiexLen);
        config = GetConfig().Get(path);
        if (config != nullptr) {
            const auto *es = config->Get("exts");
            if (es != nullptr && es->IsArray()) {
                for (const auto& ext : es->AsArray()) {
                    if (ext.IsString()) {
                        exts.push_back(ext.AsString());
                    }
                }
            }
        }
    }

    m_types.emplace(std::make_pair(name, Type{ name, exts, config == nullptr ? Config::Empty : *config, creator }));
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
    auto *type = GetType(name);
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
    auto *type = GetType(name);
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
