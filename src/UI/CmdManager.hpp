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
#pragma once
#include "pch.h"

namespace fbide {

/**
 * Command manager manages "commands" that can be executed
 * in the editor. For example command to "undo" change in the text
 * editor. It acts as a unifying place between UI description
 * menu and toolbars
 */
class CmdManager final {
    NON_COPYABLE(CmdManager)
public:
    // Entry type
    enum class Type {
        Normal,
        Check,
        Menu
    };

    // Command entry
    struct Entry {
        int id = wxID_ANY;
        Type type = Type::Normal;
        bool checked = false;
        bool enabled = true;
        wxObject* object = nullptr;
    };

    CmdManager();
    ~CmdManager() = default;

    /**
     * Get ID from command name. If command does
     * not yet exist a default entry will be created
     */
    int GetId(const wxString& name);

    /**
     * check if id exists for the given name
     */
    [[nodiscard]] bool IdExists(const wxString& name) const;

    /**
     * Find entry. Will return a nullptr if it doesn't
     * exist
     */
    [[nodiscard]] const Entry* FindEntry(int id) const;

    /**
     * Find entry. Will return a nullptr if it doesn't
     * exist
     */
    [[nodiscard]] const Entry* FindEntry(const wxString& name) const;

    /**
     * Get the entry. If doesn't exist a default
     * one will be created
     */
    [[nodiscard]] Entry& GetEntry(const wxString& name);

    /**
     * Register new entry. Will return the ID of the created
     * entry
     */
    int Register(const wxString& name, const Entry& entry);

    /**
     * Toggle the checkboxes. For Type::Check
     */
    void Check(int id, bool state);

    /**
     * Enable / Disable command state
     */
    void Enable(int id, bool state);

    [[nodiscard]] bool IsEnabled(int id) const noexcept;
    [[nodiscard]] bool IsChecked(int id) const noexcept;

private:
    /**
     * Find entry with given ID. If not found return null
     */
    [[nodiscard]] Entry* GetEntry(int id) noexcept;

    StringMap<int> m_idMap;                    // id <-> name map
    std::unordered_map<int, Entry> m_entryMap; // id <-> entry map
};

} // namespace fbide
