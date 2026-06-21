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
 * A `wxGenericDirCtrl` that can be "focused" on a single folder: while a focus
 * root is set the tree shows just that folder as its root, instead of the whole
 * filesystem (drives / home). Clearing it restores the full view.
 *
 * Re-rooting is done by overriding the virtual `SetupSections` — the hook that
 * normally populates the hidden root with drive/home sections — to emit the
 * focus folder as the sole section, then rebuilding via `ReCreateTree`.
 */
class FocusableDirCtrl final : public wxGenericDirCtrl {
public:
    using wxGenericDirCtrl::wxGenericDirCtrl;

    /// Set (or clear, when empty) the folder shown as the tree root. Rebuilds
    /// the tree and expands the focused folder to reveal its contents.
    void setFocusRoot(const wxString& path);

    /// The current focus root, or empty when showing the full filesystem.
    [[nodiscard]] auto focusRoot() const -> const wxString& { return m_focusRoot; }

protected:
    /// Emit the focus folder as the only section when focused; otherwise the
    /// platform default sections (drives / home).
    void SetupSections() override;

private:
    wxString m_focusRoot; ///< Folder shown as the root; empty = full filesystem.
};

} // namespace fbide
