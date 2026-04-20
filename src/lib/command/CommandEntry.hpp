//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/**
 * Command entries are used to generate and manage toolbar / menu entries.
 *
 * wxItemKind::wxITEM_DROPDOWN is considered a menu or submenu, while
 * on the toolbar it is a dropdown.
 */
struct CommandEntry final {

    /// Possible controls that can bind to command entry
    using Bind = std::variant<wxMenu*, wxMenuItem*, wxToolBarToolBase*, wxAuiManager*>;

    /// Get control from binds for a given type
    template<typename T>
    [[nodiscard]] auto get() const -> T* {
        for (const auto& bind : binds) {
            if (std::holds_alternative<T*>(bind)) {
                return std::get<T*>(bind);
            }
        }
        return nullptr;
    }

    /// Get control from binds for a given type
    template<typename T>
    void remove() {
        auto it = std::ranges::find_if(binds, [](const T& x) {
            return std::holds_alternative<T*>(x);
        });

        if (it != binds.end()) {
            binds.erase(it);
        }
    }

    /// Set enabled state and update bound controls
    void setEnabled(bool state);

    /// Set checked state and update bound controls
    void setChecked(bool state);

    /// update bound controls
    void update();

    wxWindowID id = wxID_ANY;
    wxString name;
    wxItemKind kind = wxITEM_NORMAL;
    bool enabled = true;
    bool checked = false;
    std::vector<Bind> binds = {};
};

} // fbide
