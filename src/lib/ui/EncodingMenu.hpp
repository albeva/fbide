//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "editor/TextEncoding.hpp"

namespace fbide {

/// Popup menus for status bar encoding + EOL selectors.
/// Menu items use dedicated command-id ranges so the selection event can
/// be decoded back into a TextEncoding / EolMode value without maintaining
/// external state.
class EncodingMenu {
public:
    /// EOL radio items: kEolIdBase + index-in-EolMode::all.
    static constexpr int kEolIdBase = wxID_HIGHEST + 10000;

    /// Encoding "Save with..." radio items: kEncodingSaveIdBase + index.
    static constexpr int kEncodingSaveIdBase = wxID_HIGHEST + 10100;

    /// Encoding "Reload with..." items: kEncodingReloadIdBase + index.
    static constexpr int kEncodingReloadIdBase = wxID_HIGHEST + 10200;

    /// Build EOL popup menu with current mode checked.
    /// Caller owns the returned menu (pass to wxWindow::PopupMenu).
    [[nodiscard]] static auto buildEolMenu(EolMode current) -> std::unique_ptr<wxMenu>;

    /// Build encoding popup menu with:
    ///   - Save-with-encoding radio items (current checked)
    ///   - Separator
    ///   - Reload-with-encoding submenu (labelled with `reloadLabel`)
    [[nodiscard]] static auto buildEncodingMenu(TextEncoding current, const wxString& reloadLabel) -> std::unique_ptr<wxMenu>;

    /// Decode selection event id back to an EolMode.
    /// Returns nullopt if the id is not in the EOL range.
    [[nodiscard]] static auto eolFromId(int id) -> std::optional<EolMode>;

    /// Decode save-with-encoding selection id.
    [[nodiscard]] static auto encodingSaveFromId(int id) -> std::optional<TextEncoding>;

    /// Decode reload-with-encoding selection id.
    [[nodiscard]] static auto encodingReloadFromId(int id) -> std::optional<TextEncoding>;
};

} // namespace fbide
