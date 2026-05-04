//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class ConfigManager;

/**
 * Command entries are used to generate and manage toolbar / menu entries.
 *
 * wxItemKind::wxITEM_DROPDOWN is considered a menu or submenu, while
 * on the toolbar it is a dropdown.
 */
struct CommandEntry final {

    /// Possible controls that can bind to a command entry.
    /// `wxAuiToolBar*` carries the parent toolbar — wxAuiToolBarItem
    /// has no enable / toggle of its own, those go through the parent
    /// keyed by entry id.
    using Bind = std::variant<wxMenu*, wxMenuItem*, wxToolBarToolBase*, wxAuiToolBar*, wxAuiManager*, ConfigManager*>;

    /// Get the bound control of type `T*`, or `nullptr` if none exists.
    template<typename T>
    [[nodiscard]] auto get() const -> T* {
        for (const auto& bind : binds) {
            if (std::holds_alternative<T*>(bind)) {
                return std::get<T*>(bind);
            }
        }
        return nullptr;
    }

    /// Remove the first bind of type `T*` (no-op if not present).
    template<typename T>
    void remove() {
        auto it = std::ranges::find_if(binds, [](const T& x) {
            return std::holds_alternative<T*>(x);
        });

        if (it != binds.end()) {
            binds.erase(it);
        }
    }

    /// Set the broad enabled state and refresh every bound control.
    void setEnabled(bool state);

    /// Set forced-disabled override and update bound controls. When true the
    /// command is shown disabled regardless of `enabled` (used for fine-
    /// grained per-editor state like CanUndo/CanPaste while `enabled` tracks
    /// the broader UI state from UIManager::applyState).
    void setForceDisabled(bool state);

    /// Set the checked state and refresh every bound control.
    void setChecked(bool state);

    /// Push the current state onto every bound control.
    void update();

    /// Effective enabled state — `enabled` masked by `forceDisabled`.
    [[nodiscard]] auto isEnabled() const -> bool { return !forceDisabled && enabled; }

    wxWindowID id = wxID_ANY;          ///< wx event id (zero/`wxID_ANY` triggers `wxNewId()`).
    wxString name;                     ///< Stable internal name (matches layout/locale/shortcuts keys).
    wxItemKind kind = wxITEM_NORMAL;   ///< Item kind (Normal, Check, Dropdown).
    bool enabled = true;               ///< Broad enabled gate (set by `UIManager::applyState`).
    bool forceDisabled = false;        ///< Per-editor mask (set by `DocumentManager::syncEditCommands`).
    bool checked = false;              ///< Checked state for `wxITEM_CHECK` entries.
    std::vector<Bind> binds = {};      ///< Bound UI controls.
};

} // namespace fbide
