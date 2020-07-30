//
//  CmdManager.hpp
//  fbide
//
//  Created by Albert on 05/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#pragma once
#include "app_pch.hpp"

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

    /**
     * Get ID from command name. If command does
     * not yet exist a default entry will be created
     */
    int GetId(const wxString& name);

    /**
     * check if id exists for the given name
     */
    bool IdExists(const wxString& name) const;

    /**
     * Find entry. Will return a nullptr if it doesn't
     * exist
     */
    const Entry* FindEntry(int id) const;

    /**
     * Find entry. Will return a nullptr if it doesn't
     * exist
     */
    const Entry* FindEntry(const wxString& name) const;

    /**
     * Get the entry. If doesn't exist a default
     * one will be created
     */
    Entry& GetEntry(const wxString& name);

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
    Entry* GetEntry(int id) noexcept;

    StringMap<int> m_idMap;                    // id <-> name map
    std::unordered_map<int, Entry> m_entryMap; // id <-> entry map
};

} // namespace fbide
