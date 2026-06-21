//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileSession.hpp"
#include "Document.hpp"
#include "DocumentManager.hpp"
#include "DocumentPath.hpp"
#include "TextEncoding.hpp"
#include "app/Context.hpp"
#include "command/CommandManager.hpp"
#include "config/ConfigManager.hpp"
#include "editor/Editor.hpp"
#include "sidebar/SideBarManager.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {

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

/// The local file to bind `m_config` to at construction: the `.fbs` path when
/// it already holds v3 INI — so the config parses it on construction — or empty
/// to stay purely in-memory. A new file has nothing to load; a legacy text file
/// is left for `loadLegacy` (binding it would trip wxFileConfig's INI parser and
/// log spurious errors). Both are written fresh as v3 on save.
auto localSessionFile(const wxString& path) -> wxString {
    return wxFileExists(path) && isIniFormat(path) ? path : wxString {};
}

void applyScrollAndCursor(Editor* editor, const long scroll, const long cursor) {
    editor->ScrollToLine(static_cast<int>(scroll));
    editor->SetCurrentPos(static_cast<int>(cursor));
    editor->SetSelectionStart(static_cast<int>(cursor));
    editor->SetSelectionEnd(static_cast<int>(cursor));
}

} // namespace

FileSession::FileSession(Context& ctx, wxString path)
: m_ctx(ctx)
, m_path(std::move(path))
, m_config(wxEmptyString, wxEmptyString, localSessionFile(m_path), wxEmptyString, 0, wxConvUTF8) {
    // `save` streams the file out itself, so stop wxFileConfig from also
    // flushing its bound copy when it is destroyed.
    m_config.DisableAutoSave();
}

FileSession::~FileSession() {
    // A destructor must not let exceptions escape. `save` already logs I/O
    // failures and a missed session snapshot is non-fatal, so swallow anything.
    try {
        save();
        m_path.clear();
        updateUi(false);
    } catch (const std::exception& ex) {
        wxLogError("Failed to save FileSession. %s", ex.what());
    } catch (...) {
        wxLogError("Failed to save FileSession. Unknown exception");
    }
}

auto FileSession::getName() const -> wxString {
    return wxFileName(m_path).GetName();
}

auto FileSession::getConfig() -> wxConfigBase& {
    return m_config;
}

auto FileSession::sessionDir() const -> std::filesystem::path {
    return toFsPath(m_path).parent_path();
}

auto FileSession::relative(const std::filesystem::path& path) const -> wxString {
    if (path.empty()) {
        return {};
    }
    const auto dir = sessionDir();
    std::error_code ec;
    const auto absolute = std::filesystem::weakly_canonical(path, ec);
    const auto& canonical = ec ? path : absolute;
    if (!dir.empty()) {
        std::error_code relEc;
        const auto rel = std::filesystem::relative(canonical, dir, relEc);
        if (!relEc && !rel.empty() && !rel.native().starts_with(std::filesystem::path { ".." }.native())) {
            // A bare "." here means the path IS the session folder — kept verbatim.
            return wxString::FromUTF8(rel.generic_string());
        }
    }
    return wxString::FromUTF8(canonical.generic_string());
}

auto FileSession::relative(const wxString& path) const -> wxString {
    return path.empty() ? wxString {} : relative(toFsPath(path));
}

auto FileSession::resolve(const std::filesystem::path& stored) const -> std::filesystem::path {
    if (stored.empty()) {
        return {}; // no value stored
    }
    if (stored == std::filesystem::path { "." }) {
        return sessionDir(); // "." is the session folder itself
    }
    if (stored.is_absolute()) {
        return stored;
    }
    return sessionDir() / stored;
}

auto FileSession::resolve(const wxString& stored) const -> wxString {
    return toWxString(resolve(toFsPath(stored)));
}

void FileSession::load() {
    updateUi(true);

    // no session
    if (not wxFileExists(m_path)) {
        return;
    }

    // load the state
    if (isIniFormat(m_path)) {
        loadV3();
    } else {
        loadLegacy();
    }
}

void FileSession::updateUi(const bool loaded) {
    m_isLoaded = loaded;
    if (auto* entry = m_ctx.getCommandManager().find(+CommandId::SessionClose)) {
        entry->enabled = loaded;
        entry->update();
    }
    m_ctx.getUIManager().updateTitle();
}

void FileSession::loadV3() {
    // `m_config` was bound to the `.fbs` at construction, so it already holds
    // the parsed v3 INI — read straight out of it.
    auto& cfg = m_config;

    const auto thaw = m_ctx.getUIManager().freeze();
    auto& dm = m_ctx.getDocumentManager();

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
        const auto filePath = resolve(toFsPath(storedPath));
        std::error_code fsEc;
        if (!std::filesystem::exists(filePath, fsEc)) {
            continue;
        }

        auto* doc = dm.openFile(filePath);
        if (doc == nullptr) {
            continue;
        }
        auto* editor = doc->getEditor();

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
        // Restore user-overridden document type (e.g. user picked "Bash"
        // for an extensionless file last session). Auto-detected types
        // aren't persisted, so absence here means "trust the derived type".
        wxString typeKey;
        if (cfg.Read("type", &typeKey) && !typeKey.empty()) {
            if (const auto type = documentTypeFromKey(typeKey.ToStdString()); type.has_value()) {
                doc->setType(*type);
            }
        }
        // Pinned compiler config. Stored verbatim — validation against
        // the live catalog is deferred until first compile/run, so
        // momentarily-missing slugs don't lose the pointer if the user
        // is about to re-add them.
        wxString configSlug;
        if (cfg.Read("configuration", &configSlug) && !configSlug.empty()) {
            doc->setConfiguration(configSlug);
        }
        // setEncoding / setEolMode flip the meta-dirty flag — clear.
        doc->setModified(false);

        long scroll = 0;
        long cursor = 0;
        cfg.Read("scroll", &scroll, 0L);
        cfg.Read("cursor", &cursor, 0L);
        applyScrollAndCursor(editor, scroll, cursor);

        if (m_ctx.getConfigManager().config().get_or("editor.folderMargin", false)) {
            wxString folds;
            if (cfg.Read("folds", &folds)) {
                editor->Colourise(editor->GetEndStyled(), -1);
                for (const auto& str : wxSplit(folds, ',')) {
                    int line;
                    if (str.ToInt(&line)) {
                        const auto last = editor->GetLastChild(line, -1);
                        if (last > line) {
                            editor->SetFoldExpanded(line, false);
                            editor->HideLines(line + 1, last);
                        }
                    }
                }
            }
        }
    }

    // Restore selected tab
    cfg.SetPath("/session");
    long selectedTab = 0;
    cfg.Read("selectedTab", &selectedTab, 0L);
    auto* notebook = m_ctx.getUIManager().getNotebook();
    if (selectedTab >= 0 && static_cast<size_t>(selectedTab) < notebook->GetPageCount()) {
        notebook->SetSelection(static_cast<size_t>(selectedTab));
    }

    // Restore the file browser + sidebar state (the sidebar owns the format).
    m_ctx.getSideBarManager().load(*this);

    // we loaded, success!
    return;
}

void FileSession::loadLegacy() {
    wxTextFile file(m_path);
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

    return;
}

void FileSession::save() {
    const auto& dm = m_ctx.getDocumentManager();

    // Pure path snapshot — modified buffers are NOT auto-saved here. Callers
    // that need unsaved changes flushed must drive a save / close flow (e.g.
    // `DocumentManager::closeAllFiles`) before this object is destroyed.

    // Rewrite from scratch: clear whatever was loaded (or left by a prior save)
    // so stale file_NNN / sidebar groups don't survive, then stream out fresh.
    m_config.DeleteAll();
    auto& cfg = m_config;

    const auto* notebook = m_ctx.getUIManager().getNotebook();
    cfg.Write("/session/version", Version);
    cfg.Write("/session/selectedTab", notebook->GetSelection());

    size_t fileIndex = 0;
    for (const auto& doc : dm.getDocuments()) {
        if (doc->isNew()) {
            continue;
        }
        auto* editor = doc->getEditor();
        const auto group = wxString::Format("/%s%03zu", FILE_GROUP_PREFIX, fileIndex);
        cfg.SetPath(group);
        cfg.Write("path", relative(doc->getFilePath()));
        cfg.Write("scroll", editor->GetFirstVisibleLine());
        cfg.Write("cursor", editor->GetCurrentPos());
        cfg.Write("encoding", wxString(doc->getEncoding().toString()));
        cfg.Write("eolMode", wxString(doc->getEolMode().toString()));
        // Only persist the type when the user has explicitly overridden it.
        // Otherwise it can be re-derived from the path on next load.
        if (doc->isTypeOverridden()) {
            const auto typeKey = documentTypeKey(doc->getType());
            cfg.Write("type", wxString::FromUTF8(typeKey.data(), typeKey.size()));
        }
        // Pinned compiler config — written only when the doc is pinned
        // to a non-default configuration. An empty optional means
        // "follow active", which doesn't need on-disk representation.
        if (const auto& slug = doc->getConfiguration(); slug.has_value()) {
            cfg.Write("configuration", *slug);
        }

        // Store code folds
        if (m_ctx.getConfigManager().config().get_or("editor.folderMargin", false)) {
            editor->Colourise(editor->GetEndStyled(), -1);
            wxString folds;
            const auto lines = editor->GetLineCount();
            folds.reserve(static_cast<std::size_t>(std::min(1, lines / 5)));
            for (int line = 0; line < lines; line++) {
                if (not editor->GetFoldExpanded(line)) {
                    if (not folds.empty()) {
                        folds += ",";
                    }
                    folds += std::to_string(line);
                }
            }
            if (not folds.empty()) {
                cfg.Write("folds", folds);
            }
        }

        fileIndex++;
    }

    // File browser + sidebar state — the sidebar owns its own format, reaching
    // back through this session for the config and the path mapping.
    m_ctx.getSideBarManager().store(*this);

    wxFFileOutputStream outStream(m_path);
    if (!outStream.IsOk()) {
        wxLogError("Failed to open '%s' for writing", m_path);
    } else {
        cfg.Save(outStream, wxConvUTF8);
    }
}
