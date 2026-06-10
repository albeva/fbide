//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileAssociationsLinux.hpp"

#ifdef __WXGTK__

using namespace fbide;

namespace {

/// User data root: $XDG_DATA_HOME, or ~/.local/share when it is unset/empty.
auto xdgDataHome() -> wxString {
    wxString dir;
    if (wxGetEnv("XDG_DATA_HOME", &dir) && !dir.empty()) {
        return dir;
    }
    return wxGetHomeDir() + "/.local/share";
}

/// Single-quote a path for embedding in a /bin/sh command line.
auto shellQuote(const wxString& path) -> wxString {
    wxString escaped = path;
    escaped.Replace("'", "'\\''");
    return "'" + escaped + "'";
}

/// Recursively copy every file under `src` into `dst`, preserving the relative
/// layout, creating directories and overwriting existing files. No-op if `src`
/// is absent.
void copyTree(const wxString& src, const wxString& dst) {
    if (!wxDirExists(src)) {
        return;
    }
    wxArrayString files;
    wxDir::GetAllFiles(src, &files);
    for (const auto& file : files) {
        wxFileName target(file);
        target.MakeRelativeTo(src);
        const wxString destPath = dst + "/" + target.GetFullPath();
        wxFileName::Mkdir(wxPathOnly(destPath), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        wxCopyFile(file, destPath, /*overwrite*/ true);
    }
}

/// A stamp identifying the integrated AppImage by path and modification time,
/// so a moved or upgraded AppImage re-integrates while an unchanged one does not.
auto integrationStamp(const wxString& appImage) -> wxString {
    const auto mtime = static_cast<long long>(wxFileModificationTime(appImage));
    return appImage + "|" + wxString::Format("%lld", mtime);
}

/// First line of `path`, or empty if it does not exist / cannot be read.
auto readFirstLine(const wxString& path) -> wxString {
    wxTextFile file(path);
    if (!file.Exists() || !file.Open()) {
        return {};
    }
    const wxString line = file.GetLineCount() > 0 ? file[0] : wxString {};
    file.Close();
    return line;
}

/// Overwrite `path` with a single `line` (Unix line endings), creating it if
/// needed.
void writeSingleLine(const wxString& path, const wxString& line) {
    wxTextFile file(path);
    if (file.Exists() ? file.Open() : file.Create()) {
        file.Clear();
        file.AddLine(line);
        file.Write(wxTextFileType_Unix);
        file.Close();
    }
}

/// Copy the bundled .desktop to `dst`, rewriting its `Exec=` to launch the
/// AppImage by absolute path. Inside the AppImage `Exec=fbide` resolves only via
/// AppRun's PATH; the user's menu launcher has no such PATH, so it must point at
/// $APPIMAGE directly.
void installDesktopEntry(const wxString& src, const wxString& dst, const wxString& appImage) {
    if (!wxCopyFile(src, dst, /*overwrite*/ true)) {
        return;
    }
    wxString quoted = appImage;
    quoted.Replace("\\", "\\\\");
    quoted.Replace("\"", "\\\"");
    const wxString execLine = "Exec=\"" + quoted + "\" %F";

    wxTextFile file(dst);
    if (!file.Open()) {
        return;
    }
    for (size_t line = 0; line < file.GetLineCount(); ++line) {
        if (file[line].StartsWith("Exec=")) {
            file[line] = execLine;
        }
    }
    file.Write(wxTextFileType_Unix);
    file.Close();
}

} // namespace

void FileAssociationsLinux::ensureRegistered() {
    // Only meaningful for an AppImage: its bundled share/ tree is invisible to
    // the system until copied into the user's data dir. A packaged install gets
    // these files from the package manager instead.
    wxString appImage;
    if (!wxGetEnv("APPIMAGE", &appImage) || appImage.empty()) {
        return;
    }

    // Bundled assets sit beside the binary at <exe>/../share (= $APPDIR/usr/share).
    wxFileName shareDir(wxStandardPaths::Get().GetExecutablePath());
    shareDir.SetFullName(""); // drop the executable name -> .../usr/bin/
    shareDir.RemoveLastDir(); // .../usr/
    shareDir.AppendDir("share");
    const wxString shareSrc = shareDir.GetPath();

    const wxString desktopSrc = shareSrc + "/applications/fbide.desktop";
    if (!wxFileExists(desktopSrc)) {
        return; // not the expected AppImage layout; nothing to integrate.
    }

    // Skip when this exact AppImage was already integrated.
    const wxString dataHome = xdgDataHome();
    const wxString stampFile = dataHome + "/fbide/appimage-integrated";
    const wxString stamp = integrationStamp(appImage);
    if (readFirstLine(stampFile) == stamp) {
        return;
    }

    // Icons (apps + mimetypes) and the MIME-type definitions.
    copyTree(shareSrc + "/icons", dataHome + "/icons");
    copyTree(shareSrc + "/mime", dataHome + "/mime");

    // Desktop entry, pointed at this AppImage.
    const wxString appsDir = dataHome + "/applications";
    wxFileName::Mkdir(appsDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    installDesktopEntry(desktopSrc, appsDir + "/fbide.desktop", appImage);

    // Refresh the freedesktop caches so the new types, handler and icons are
    // picked up. Best-effort and asynchronous — the files above are already in
    // place, the tools may be absent, and none of this should block startup.
    const wxString refresh
        = "update-desktop-database " + shellQuote(appsDir) + " >/dev/null 2>&1; "
                                                             "update-mime-database "
        + shellQuote(dataHome + "/mime") + " >/dev/null 2>&1; "
                                           "gtk-update-icon-cache -t -f "
        + shellQuote(dataHome + "/icons/hicolor") + " >/dev/null 2>&1";
    wxArrayString argv;
    argv.Add("/bin/sh");
    argv.Add("-c");
    argv.Add(refresh);
    wxExecute(argv, wxEXEC_ASYNC);

    // Record success last, so a failure part-way through retries next launch.
    wxFileName::Mkdir(dataHome + "/fbide", wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    writeSingleLine(stampFile, stamp);
}

#endif // __WXGTK__
