//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Manages recent file history, persisted to history.local.ini.
class FileHistory final {
public:
    NO_COPY_AND_MOVE(FileHistory)
    FileHistory() = default;

    /// Load history from an INI file.
    void load(const std::filesystem::path& path);

    /// Save history to the loaded path.
    void save();

    /// Add a file to the history.
    void addFile(const std::filesystem::path& path);

    /// Get a file from history. Returns nullopt when the index is out
    /// of range or the file no longer exists on disk.
    [[nodiscard]] auto getFile(std::size_t idx) const -> std::optional<std::filesystem::path>;

    /// Get the underlying wxFileHistory for menu integration.
    [[nodiscard]] auto getHistory() -> wxFileHistory& { return m_history; }

private:
    std::filesystem::path m_path;              ///< Path to the backing INI file.
    wxFileHistory m_history { 9, wxID_FILE1 }; ///< Underlying wx history (9 slots from `wxID_FILE1`).
};

} // namespace fbide
