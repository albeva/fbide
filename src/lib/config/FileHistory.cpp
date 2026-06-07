//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileHistory.hpp"
#include "document/DocumentPath.hpp"
using namespace fbide;

void FileHistory::load(const std::filesystem::path& path) {
    m_path = path;
    wxLogVerbose("file history: %s", path.string());
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return;
    }
    wxFFileInputStream stream(toWxString(path));
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
    const auto pathWx = toWxString(m_path);
    wxFileOutputStream stream(pathWx);
    if (!stream.IsOk() || !ini.Save(stream)) {
        wxLogError("Failed to save file history '%s'", pathWx);
    }
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
