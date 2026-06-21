//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FileAssociations.hpp"

#ifdef __WXMSW__
#include <array>

#include <wx/msw/registry.h>
#include <wx/stdpaths.h>

#include <windows.h>
#include <shlobj.h> // SHChangeNotify

using namespace fbide;

namespace {

/// One associable document type. `iconIndex` is the position of its icon in the
/// executable's icon resources. Those are sorted by name in the PE (see
/// src/lib/rc/app.rc), so the order is: appicon=0, doc_bas=1, doc_bi=2,
/// doc_fbs=3. Keep this in sync with app.rc if the icon names change.
struct DocType {
    const char* ext;     ///< extension, e.g. ".bas"
    const char* progId;  ///< per-user ProgID, e.g. "FBIde.bas"
    const char* desc;    ///< Explorer type description
    int iconIndex;       ///< icon index into the exe's resources
};

constexpr std::array<DocType, 3> kDocTypes { {
    { ".bas", "FBIde.bas", "FreeBASIC Source File", 1 },
    { ".bi", "FBIde.bi", "FreeBASIC Header File", 2 },
    { ".fbs", "FBIde.fbs", "FBIde Session", 3 },
} };

/// Create `path` under HKCU if needed and set value `name` to `value`, but only
/// when it differs from what's already there. Returns true if it wrote anything.
auto setIfChanged(const wxString& path, const wxString& name, const wxString& value) -> bool {
    wxRegKey key(wxRegKey::HKCU, path);
    if (!key.Create()) {
        return false;
    }
    wxString current;
    if (key.HasValue(name) && key.QueryValue(name, current) && current == value) {
        return false;
    }
    key.SetValue(name, value);
    return true;
}

/// True when FBIde was placed by the Windows installer, which records an
/// `Installed` marker under `Software\FBIde` and owns the file associations
/// (registered from the setup wizard, honouring the user's per-type choices).
/// Checked in both hives: an all-users install writes HKLM, a per-user install
/// writes HKCU. When set, the runtime must not self-register, or it would
/// re-assert .bas/.bi every launch and undo an opt-out.
auto installedByInstaller() -> bool {
    const wxRegKey hklm(wxRegKey::HKLM, "Software\\FBIde");
    const wxRegKey hkcu(wxRegKey::HKCU, "Software\\FBIde");
    return hklm.HasValue("Installed") || hkcu.HasValue("Installed");
}

} // namespace

void FileAssociations::ensureRegistered() {
    // Installed builds let the installer own the associations — skip runtime
    // self-registration there. Portable (zip) builds carry no marker and
    // self-register the per-user associations below.
    if (installedByInstaller()) {
        return;
    }

    const wxString exe = wxStandardPaths::Get().GetExecutablePath();
    const wxString classes = "Software\\Classes\\";
    bool changed = false;

    for (const auto& type : kDocTypes) {
        const wxString progId = classes + type.progId;
        const wxString icon = "\"" + exe + "\"," + wxString::Format("%d", type.iconIndex);
        const wxString command = "\"" + exe + "\" \"%1\"";

        changed |= setIfChanged(progId, wxEmptyString, type.desc);
        changed |= setIfChanged(progId + "\\DefaultIcon", wxEmptyString, icon);
        changed |= setIfChanged(progId + "\\shell\\open\\command", wxEmptyString, command);
        // Point the extension at our ProgID. The extension's icon + open verb
        // come from its effective ProgID, so this is what actually shows the
        // icon. It's the per-user *class* default; the user's explicit
        // default-app choice (the OS-protected UserChoice) still wins if set.
        changed |= setIfChanged(classes + type.ext, wxEmptyString, type.progId);
        // Also list it in the Open-with menu.
        changed |= setIfChanged(classes + type.ext + "\\OpenWithProgids", type.progId, wxEmptyString);
    }

    if (changed) {
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
}

#endif // __WXMSW__
