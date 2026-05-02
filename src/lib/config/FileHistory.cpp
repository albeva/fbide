//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileHistory.hpp"
using namespace fbide;

void FileHistory::load(const wxString& path) {
    m_path = path;
    if (!wxFileExists(path)) {
        return;
    }
    wxFFileInputStream stream(path);
    const wxFileConfig ini(stream);
    m_history.Load(ini);

    // Prune entries whose files no longer exist on disk. Walk back-to-front
    // so removals don't shift indices we haven't visited yet.
    for (std::size_t idx = m_history.GetCount(); idx > 0; idx--) {
        if (!wxFileExists(m_history.GetHistoryFile(idx - 1))) {
            m_history.RemoveFileFromHistory(idx - 1);
        }
    }
}

void FileHistory::save() {
    if (m_path.empty()) {
        return;
    }
    wxFileConfig ini(wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString, 0);
    m_history.Save(ini);
    wxFileOutputStream stream(m_path);
    ini.Save(stream);
}

void FileHistory::addFile(const wxString& path) {
    m_history.AddFileToHistory(path);
}

auto FileHistory::getFile(const std::size_t idx) const -> std::optional<wxString> {
    if (idx >= m_history.GetCount()) {
        return {};
    }
    auto file = m_history.GetHistoryFile(idx);
    if (not wxFileExists(file)) {
        return {};
    }
    return file;
}
