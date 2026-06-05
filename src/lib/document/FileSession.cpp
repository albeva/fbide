//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileSession.hpp"
#include "Document.hpp"
#include "DocumentManager.hpp"
#include "DocumentNotebook.hpp"
#include "DocumentPath.hpp"
#include "TextEncoding.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "editor/Editor.hpp"
#include "ui/UIManager.hpp"
#include "workspace/WorkspaceManager.hpp"
using namespace fbide;

namespace {

constexpr auto SESSION_EXT = "fbs";
constexpr auto FILE_GROUP_PREFIX = "file_";

/// Cheap format probe: if the first non-blank line starts with '[' the file
/// is v3 INI, otherwise fall back to the legacy text parser.
auto isIniFormat(const std::filesystem::path& path) -> bool {
    wxTextFile file(toWxString(path));
    if (!file.Open()) {
        return false;
    }
    for (size_t i = 0; i < file.GetLineCount(); i++) {
        wxString line = wxString(file[i]).Trim().Trim(false);
        if (!line.empty()) {
            return line.StartsWith("[");
        }
    }
    return false;
}

void applyScrollAndCursor(Editor* editor, const long scroll, const long cursor) {
    editor->ScrollToLine(static_cast<int>(scroll));
    editor->SetCurrentPos(static_cast<int>(cursor));
    editor->SetSelectionStart(static_cast<int>(cursor));
    editor->SetSelectionEnd(static_cast<int>(cursor));
}

/// Produce the path string to store for a file. If `filePath` lives anywhere
/// under `sessionDir` (nested at any depth), return a relative path rooted at
/// `sessionDir`. Otherwise return an absolute path. Result always uses forward
/// slashes (via `generic_string()`) for cross-platform portability.
auto pathForSession(const std::filesystem::path& filePath,
    const std::filesystem::path& sessionDir) -> std::string {
    std::error_code ec;
    const auto absolute = std::filesystem::weakly_canonical(filePath, ec);
    const auto& canonical = ec ? filePath : absolute;

    if (!sessionDir.empty()) {
        std::error_code relEc;
        const auto rel = std::filesystem::relative(canonical, sessionDir, relEc);
        if (!relEc && !rel.empty() && !rel.native().starts_with(std::filesystem::path { ".." }.native())) {
            return rel.generic_string();
        }
    }
    return canonical.generic_string();
}

/// Resolve a stored path on load. Relative paths are rooted at the session
/// folder; absolute paths pass through unchanged.
auto resolveStoredPath(const wxString& storedPath,
    const std::filesystem::path& sessionDir) -> std::filesystem::path {
    const auto stored = toFsPath(storedPath);
    if (stored.is_absolute()) {
        return stored;
    }
    return sessionDir / stored;
}

} // namespace

FileSession::FileSession(Context& ctx)
: m_ctx(ctx) {}

void FileSession::load(const std::filesystem::path& path, const bool addToHistory) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return;
    }
    if (isIniFormat(path)) {
        loadV3(path);
    } else {
        loadLegacy(path);
    }

    if (addToHistory) {
        m_ctx.getFileHistory().addFile(path);
    }
}

auto FileSession::save(const std::filesystem::path& path) -> bool {
    // Pure path snapshot — modified buffers are NOT auto-saved here.
    // Callers that need to flush dirty state (Save Session menu, the
    // restart flow) must drive the user-facing save/close prompts
    // through `DocumentManager::closeAllFiles` (or equivalent) before
    // writing the session.

    wxFileConfig cfg(wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString, 0);

    const auto sessionDir = path.parent_path();
    cfg.Write("/session/version", Version);
    cfg.Write("/session/selectedTab", m_ctx.getDocumentManager().notebook().GetSelection());

    size_t fileIndex = 0;
    for (auto* doc : m_ctx.getWorkspaceManager().documents()) {
        if (doc->isNew() || !doc->hasView()) {
            continue;
        }
        const auto group = wxString::Format("/%s%03zu", FILE_GROUP_PREFIX, fileIndex);
        cfg.SetPath(group);
        cfg.Write("path", wxString::FromUTF8(pathForSession(doc->getFilePath(), sessionDir)));
        doc->setSessionAttributes(cfg);
        fileIndex++;
    }

    const auto pathWx = toWxString(path);
    wxFFileOutputStream outStream(pathWx);
    if (!outStream.IsOk()) {
        wxLogError("Failed to open '%s' for writing", pathWx);
        return false;
    }
    cfg.Save(outStream, wxConvUTF8);
    return true;
}

void FileSession::showLoadDialog() {
    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("files.loadTitle"),
        "", wxString(".") + SESSION_EXT,
        m_ctx.getConfigManager().filePattern("session"),
        wxFD_FILE_MUST_EXIST
    );
    if (dlg.ShowModal() == wxID_OK) {
        load(toFsPath(dlg.GetPath()));
    }
}

void FileSession::showSaveDialog() {
    const auto& dm = m_ctx.getDocumentManager();
    if (dm.getCount() == 0) {
        return;
    }

    wxFileDialog dlg(
        m_ctx.getUIManager().getMainFrame(),
        m_ctx.tr("files.sessionSaveTitle"),
        "", wxString(".") + SESSION_EXT,
        m_ctx.getConfigManager().filePattern("session"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );
    if (dlg.ShowModal() == wxID_OK) {
        (void)save(toFsPath(dlg.GetPath()));
    }
}

void FileSession::loadV3(const std::filesystem::path& path) {
    const auto pathWx = toWxString(path);
    wxFFileInputStream stream(pathWx);
    if (!stream.IsOk()) {
        wxLogError("Failed to open session '%s' for reading", pathWx);
        return;
    }
    wxFileConfig cfg(stream, wxConvUTF8);

    const auto thaw = m_ctx.getUIManager().freeze();
    auto& workspace = m_ctx.getWorkspaceManager();
    const auto sessionDir = path.parent_path();

    // Collect file groups. wxFileConfig's iteration order is not guaranteed,
    // so sort — zero-padded names yield insertion order.
    std::vector<wxString> groups;
    cfg.SetPath("/");
    wxString group;
    long cookie = 0;
    for (bool ok = cfg.GetFirstGroup(group, cookie); ok; ok = cfg.GetNextGroup(group, cookie)) {
        if (group.StartsWith(FILE_GROUP_PREFIX)) {
            groups.push_back(group);
        }
    }
    std::ranges::sort(groups);

    for (const auto& g : groups) {
        cfg.SetPath("/" + g);

        wxString storedPath;
        cfg.Read("path", &storedPath);
        if (storedPath.empty()) {
            continue;
        }
        const auto filePath = resolveStoredPath(storedPath, sessionDir);
        std::error_code fsEc;
        if (!std::filesystem::exists(filePath, fsEc)) {
            continue;
        }

        auto* doc = workspace.openFile(filePath);
        if (doc == nullptr) {
            continue;
        }
        // cfg's path is the current file group — Document reads its state from it.
        doc->loadSessionAttributes(cfg);
    }

    // Restore selected tab
    cfg.SetPath("/session");
    long selectedTab = 0;
    cfg.Read("selectedTab", &selectedTab, 0L);
    auto& notebook = m_ctx.getDocumentManager().notebook();
    if (selectedTab >= 0 && static_cast<size_t>(selectedTab) < notebook.GetPageCount()) {
        notebook.SetSelection(static_cast<size_t>(selectedTab));
    }
}

void FileSession::loadLegacy(const std::filesystem::path& path) {
    wxTextFile file(toWxString(path));
    if (!file.Open() || file.GetLineCount() == 0) {
        return;
    }

    const auto thaw = m_ctx.getUIManager().freeze();
    auto& workspace = m_ctx.getWorkspaceManager();

    constexpr auto v2Header = "<fbide:session:version = \"0.2\"/>";
    const bool isV2 = wxString(file[0]).Trim().Trim(false).Lower() == v2Header;
    const size_t startLine = isV2 ? 2 : 1;

    unsigned long selectedTab = 0;
    file[isV2 ? 1 : 0].ToULong(&selectedTab);

    for (size_t i = startLine; i < file.GetLineCount(); i++) {
        const wxString filePathWx = file[i];
        if (filePathWx.empty() || !wxFileExists(filePathWx)) {
            if (isV2) {
                i += 2; // skip scroll / cursor lines
            }
            continue;
        }

        auto* doc = workspace.openFile(toFsPath(filePathWx));
        if (doc != nullptr && isV2 && i + 2 < file.GetLineCount()) {
            unsigned long scroll = 0;
            unsigned long cursor = 0;
            file[i + 1].ToULong(&scroll);
            file[i + 2].ToULong(&cursor);
            applyScrollAndCursor(doc->getEditor(), static_cast<long>(scroll), static_cast<long>(cursor));
        }

        if (isV2) {
            i += 2;
        }
    }

    auto& notebook = m_ctx.getDocumentManager().notebook();
    if (selectedTab < notebook.GetPageCount()) {
        notebook.SetSelection(selectedTab);
    }
}
