//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileSession.hpp"
#include "Document.hpp"
#include "DocumentManager.hpp"
#include "TextEncoding.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "editor/Editor.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {

constexpr auto SESSION_EXT = "fbs";
constexpr auto FILE_GROUP_PREFIX = "file_";

/// Cheap format probe: if the first non-blank line starts with '[' the file
/// is v3 INI, otherwise fall back to the legacy text parser.
auto isIniFormat(const wxString& path) -> bool {
    wxTextFile file(path);
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

/// Normalise a path for session storage: always forward-slash separators,
/// even on Windows, so session files stay portable between platforms.
auto toPortablePath(const wxString& path) -> wxString {
    wxString out = path;
    out.Replace("\\", "/");
    return out;
}

/// Produce the path to store for a file. If `filePath` lives anywhere under
/// `sessionDir` (nested at any depth), return a relative path rooted at
/// `sessionDir`. Otherwise return an absolute path. Result always uses
/// forward slashes.
auto pathForSession(const wxString& filePath, const wxString& sessionDir) -> wxString {
    wxFileName fn(filePath);
    fn.MakeAbsolute();

    wxFileName rel = fn;
    if (rel.MakeRelativeTo(sessionDir)) {
        const auto relPath = rel.GetFullPath(wxPATH_UNIX);
        // `..` prefix means the file is outside the session folder subtree.
        if (!relPath.StartsWith("..")) {
            return toPortablePath(relPath);
        }
    }
    return toPortablePath(fn.GetFullPath());
}

/// Resolve a stored path on load. Relative paths are rooted at the session
/// folder; absolute paths pass through unchanged.
auto resolveStoredPath(const wxString& storedPath, const wxString& sessionDir) -> wxString {
    wxFileName fn(storedPath);
    if (fn.IsAbsolute()) {
        return fn.GetFullPath();
    }
    fn.MakeAbsolute(sessionDir);
    return fn.GetFullPath();
}

} // namespace

FileSession::FileSession(Context& ctx)
: m_ctx(ctx) {}

void FileSession::load(const wxString& path) {
    if (!wxFileExists(path)) {
        return;
    }
    if (isIniFormat(path)) {
        loadV3(path);
    } else {
        loadLegacy(path);
    }
}

auto FileSession::save(const wxString& path) -> bool {
    const auto& dm = m_ctx.getDocumentManager();

    // Pure path snapshot — modified buffers are NOT auto-saved here.
    // Callers that need to flush dirty state (Save Session menu, the
    // restart flow) must drive the user-facing save/close prompts
    // through `DocumentManager::closeAllFiles` (or equivalent) before
    // writing the session.

    wxFileConfig cfg(wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString, 0);

    const auto sessionDir = wxFileName(path).GetPath();
    const auto* notebook = m_ctx.getUIManager().getNotebook();
    cfg.Write("/session/version", Version);
    cfg.Write("/session/selectedTab", notebook->GetSelection());

    size_t idx = 0;
    for (const auto& doc : dm.getDocuments()) {
        if (doc->isNew()) {
            continue;
        }
        const auto* editor = doc->getEditor();
        const auto group = wxString::Format("/%s%03zu", FILE_GROUP_PREFIX, idx);
        cfg.Write(group + "/path", pathForSession(doc->getFilePath(), sessionDir));
        cfg.Write(group + "/scroll", editor->GetFirstVisibleLine());
        cfg.Write(group + "/cursor", editor->GetCurrentPos());
        cfg.Write(group + "/encoding", wxString(doc->getEncoding().toString()));
        cfg.Write(group + "/eolMode", wxString(doc->getEolMode().toString()));
        idx++;
    }

    wxFFileOutputStream outStream(path);
    if (!outStream.IsOk()) {
        wxLogError("Failed to open '%s' for writing", path);
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
        load(dlg.GetPath());
    }
}

void FileSession::showSaveDialog() {
    auto& dm = m_ctx.getDocumentManager();
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
        (void)save(dlg.GetPath());
    }
}

void FileSession::loadV3(const wxString& path) {
    wxFFileInputStream stream(path);
    if (!stream.IsOk()) {
        wxLogError("Failed to open session '%s' for reading", path);
        return;
    }
    wxFileConfig cfg(stream, wxConvUTF8);

    const auto thaw = m_ctx.getUIManager().freeze();
    auto& dm = m_ctx.getDocumentManager();
    const auto sessionDir = wxFileName(path).GetPath();

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
        if (!wxFileExists(filePath)) {
            continue;
        }

        auto* doc = dm.openFile(filePath);
        if (doc == nullptr) {
            continue;
        }

        // Optional metadata — overrides auto-detected values.
        wxString encKey;
        if (cfg.Read("encoding", &encKey) && !encKey.empty()) {
            if (const auto enc = TextEncoding::parse(encKey.ToStdString()); enc.has_value()) {
                doc->setEncoding(*enc);
            }
        }
        wxString eolKey;
        if (cfg.Read("eolMode", &eolKey) && !eolKey.empty()) {
            if (const auto eol = EolMode::parse(eolKey.ToStdString()); eol.has_value()) {
                doc->setEolMode(*eol);
            }
        }
        // setEncoding / setEolMode flip the meta-dirty flag — clear.
        doc->setModified(false);

        long scroll = 0;
        long cursor = 0;
        cfg.Read("scroll", &scroll, 0L);
        cfg.Read("cursor", &cursor, 0L);
        applyScrollAndCursor(doc->getEditor(), scroll, cursor);
    }

    // Restore selected tab
    cfg.SetPath("/session");
    long selectedTab = 0;
    cfg.Read("selectedTab", &selectedTab, 0L);
    auto* notebook = m_ctx.getUIManager().getNotebook();
    if (selectedTab >= 0 && static_cast<size_t>(selectedTab) < notebook->GetPageCount()) {
        notebook->SetSelection(static_cast<size_t>(selectedTab));
    }
}

void FileSession::loadLegacy(const wxString& path) {
    wxTextFile file(path);
    if (!file.Open() || file.GetLineCount() == 0) {
        return;
    }

    const auto thaw = m_ctx.getUIManager().freeze();
    auto& dm = m_ctx.getDocumentManager();

    constexpr auto v2Header = "<fbide:session:version = \"0.2\"/>";
    const bool isV2 = wxString(file[0]).Trim().Trim(false).Lower() == v2Header;
    const size_t startLine = isV2 ? 2 : 1;

    unsigned long selectedTab = 0;
    file[isV2 ? 1 : 0].ToULong(&selectedTab);

    for (size_t i = startLine; i < file.GetLineCount(); i++) {
        const auto filePath = file[i];
        if (filePath.empty() || !wxFileExists(filePath)) {
            if (isV2) {
                i += 2; // skip scroll / cursor lines
            }
            continue;
        }

        auto* doc = dm.openFile(filePath);
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

    auto* notebook = m_ctx.getUIManager().getNotebook();
    if (selectedTab < notebook->GetPageCount()) {
        notebook->SetSelection(selectedTab);
    }
}
