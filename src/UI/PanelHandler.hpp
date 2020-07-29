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
    /**
     * If returns a window then show that as a panel. If it is managed panel then
     * PanelHandler will take ownership of it.
     * @return window or nullptr
     */
    virtual wxWindow* ShowPanel() = 0;

    /**
     * If returns true then remove the panel. If it is managed panel then
     * delete the window.
     * @return true to remove the panel
     */
    virtual bool HidePanel() = 0;
};

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
        wxString name;              // key for config, art and translations
        int id;                     // command id
        PanelCreatorFn creator;     // creator function
        bool managed;               // create/destroy automatically
        Panel* panel = nullptr;     // the managed panel
        wxWindow* window = nullptr; // the panel window
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
    Entry* Register(const wxString& name, int id, PanelCreatorFn creator);

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

    bool ShowPanel(Entry &entry);
    bool ClosePanel(Entry &entry);

    [[nodiscard]] Entry* FindEntry(int id) noexcept;
    [[nodiscard]] Entry* FindEntry(wxWindow* wnd) noexcept;

private:
    void OnPaneWillClose(wxAuiNotebookEvent& event);
    void OnPaneClosed(wxAuiNotebookEvent &event);
    void HandleMenuEvents(wxCommandEvent& event);

    wxAuiNotebook* m_panelArea = nullptr;
    wxAuiManager* m_aui = nullptr;
    StringMap<Entry> m_entries;
    int m_visibleCount = 0;

    wxDECLARE_EVENT_TABLE();
};

}
