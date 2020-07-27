//
// Created by Albert on 7/27/2020.
//
#pragma once
#include "app_pch.hpp"

namespace fbide {

class Config;

/**
 * Handle panel show / hide
 */
class Panel {
public:
    virtual ~Panel();
    virtual bool Show() = 0;
    virtual bool Hide() = 0;
};

/**
 * Manage panel area.
 * Register panel providers. Similar to Toolbar and menu system
 *
 * Each registered panel
 * - has name that can be looked up from lang
 * - icon that can be fetched from ArtProvider
 * - options:
 *   * closable
 * - panel UI can still be loaded, but may be detached / reattached
 * - panels can be toggled from toolbar / menu
 */
class PanelHandler final: public wxEvtHandler {
    NON_COPYABLE(PanelHandler)
public:

    /**
     * Create (or return existing) instance of Panel*
     */
    using PanelCreatorFn = std::function<Panel*()>;

    /**
    * Registered type information
    */
    struct Entry {
        wxString name;          // key for config, art and translations
        int id;                 // command id
        const Config& config;   // optional Config object
        PanelCreatorFn creator; // creator function
        bool managed;           // create/destroy automatically
        Panel* panel;           // the managed panel
    };

    /**
     * Check that T is extended from Panel
     */
    template<typename T>
    using CheckPanel = std::enable_if_t<is_extended_from<Panel, T>(), int>;

    PanelHandler(wxAuiManager* aui);
    ~PanelHandler();

    /**
     * Register panel provider
     *
     * @param name is used as key for config, art and translations
     * @param id wx command ID
     * @param creator
     */
    void Register(const wxString& name, int id, PanelCreatorFn creator);

    /**
     * Is given panel registered?
     */
    [[nodiscard]] inline bool IsRegistered(const wxString& name) const noexcept {
        return m_entries.find(name) != m_entries.end();
    }

    //    /**
    //     * Register automatic panel provider.
    //     *
    //     * @tparam T class that inherits from Panel
    //     * @param name is used as key for config, art and translations
    //     * @param id wx command id
    //     */
    //    template<typename T, CheckPanel<T> = 0>
    //    void Register(const wxString& name, int id) {
    //        Register(name, id, []() -> Panel* { return new T(); })
    //    }

    inline wxAuiNotebook* GetPanelArea() { return m_panelArea; }

private:
    void OnPaneClose(wxAuiNotebookEvent& event);
    void OnCmdCheck(wxCommandEvent& event);

    wxAuiNotebook* m_panelArea = nullptr;
    wxAuiManager* m_aui = nullptr;
    StringMap<Entry> m_entries;
    std::unordered_map<int, Entry&> m_idLookup;
    int m_visibleCount = 0;

    wxDECLARE_EVENT_TABLE();
};

}
