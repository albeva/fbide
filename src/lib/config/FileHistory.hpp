//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Manages recent file history, persisted to history.ini.
class FileHistory final {
public:
    /// Load history from an INI file.
    void load(const wxString& path);

    /// Save history to the loaded path.
    void save();

    /// Add a file to the history.
    void addFile(const wxString& path);

    /// Get a file from history
    auto getFile(std::size_t idx) const -> std::optional<wxString>;

    /// Get the underlying wxFileHistory for menu integration.
    [[nodiscard]] auto getHistory() -> wxFileHistory& { return m_history; }

private:
    wxString m_path;
    wxFileHistory m_history { 9, wxID_FILE1 };
};

} // namespace fbide
