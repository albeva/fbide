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
#include "CmdManager.hpp"
using namespace fbide;

/**
 * Setup Command manager with defaults
 */
CmdManager::CmdManager() {
    Register("new", { wxID_NEW });
    Register("open", { wxID_OPEN });
    Register("save", { wxID_SAVE });
    Register("quit", { wxID_EXIT });
    Register("undo", { wxID_UNDO });
    Register("redo", { wxID_REDO });
    Register("cut", { wxID_CUT });
    Register("copy", { wxID_COPY });
    Register("paste", { wxID_PASTE });
    Register("find", { wxID_FIND });
    Register("replace", { wxID_REPLACE });
    Register("goto", { wxNewId() });
    Register("about", { wxID_ABOUT });
}

/**
 * Get ID from command name. If command does
 * not yet exist a default entry will be created
 */
int CmdManager::GetId(const wxString& name) {
    auto iter = m_idMap.find(name);
    if (iter != m_idMap.end()) {
        return iter->second;
    }
    return Register(name, {});
}

/**
 * check if id exists for the given name
 */
bool CmdManager::IdExists(const wxString& name) const {
    return m_idMap.find(name) != m_idMap.end();
}

/**
 * Find entry. Will return a nullptr if it doesn't
 * exist
 */
const CmdManager::Entry* CmdManager::FindEntry(int id) const {
    auto iter = m_entryMap.find(id);
    if (iter == m_entryMap.end()) {
        return nullptr;
    }
    return &iter->second;
}

/**
 * Find entry based on name. Will return a nullptr if it doesn't
 * exist
 */
const CmdManager::Entry* CmdManager::FindEntry(const wxString& name) const {
    auto iter = m_idMap.find(name);
    if (iter == m_idMap.end()) {
        return nullptr;
    }

    return FindEntry(iter->second);
}

/**
 * Get the entry. If doesn't exist a default
 * one will be created
 */
CmdManager::Entry& CmdManager::GetEntry(const wxString& name) {
    int id = 0;
    auto iter = m_idMap.find(name);
    if (iter == m_idMap.end()) {
        id = Register(name, Entry{::wxNewId()});
    } else {
        id = iter->second;
    }
    return m_entryMap[id];
}

/**
 * Register new entry. Will return the ID of the created
 * entry
 */
int CmdManager::Register(const wxString& name, const Entry& entry) {
    // the ID
    auto id = entry.id == wxID_ANY || entry.id == 0 ? ::wxNewId() : entry.id;

    // check name registered?
    auto nameIter = m_idMap.find(name);
    if (nameIter != m_idMap.end()) {
        LOG_ERROR("Command '" + name + "' is already registered with CmdMgr");
        return nameIter->second;
    }

    // check id registered
    auto idIter = m_entryMap.find(id);
    if (idIter != m_entryMap.end()) {
        LOG_ERROR("Cmd ID for '" + name + "' is duplicated");
        return id;
    }

    // insert into map
    auto iter = m_entryMap.emplace(std::make_pair(id, entry));
    iter.first->second.id = id;

    // register id <-> name map
    m_idMap[name] = id;

    // done
    return id;
}

/**
 * Toggle the checkboxes. For Type::Check
 */
void CmdManager::Check(int id, bool state) {
    auto *entry = GetEntry(id);
    if (entry == nullptr) {
        return;
    }

    if (entry->type != Type::Check) {
        return;
    }

    entry->checked = state;
}

/**
 * Enable / Disable command state
 */
void CmdManager::Enable(int id, bool state) {
    auto *entry = GetEntry(id);
    if (entry == nullptr) {
        return;
    }

    if (entry->enabled == state) {
        return;
    }

    entry->enabled = state;
}

/**
 * Find entry with given ID. If not found return null
 */
CmdManager::Entry* CmdManager::GetEntry(int id) noexcept {
    auto entry = m_entryMap.find(id);
    if (entry == m_entryMap.end()) {
        return nullptr;
    }
    return &entry->second;
}

bool CmdManager::IsEnabled(int id) const noexcept {
    if (const auto *entry = FindEntry(id)) {
        return entry->enabled;
    }
    return false;
}

bool CmdManager::IsChecked(int id) const noexcept {
    if (const auto *entry = FindEntry(id)) {
        return entry->checked;
    }
    return false;
}
