//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "app/App.hpp"

#ifdef __WXGTK__
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

auto trim(const std::string& str) -> std::string {
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// An AppImage bundles its own GTK; on non-GNOME desktops (e.g. KDE) the bundled
// GTK can't auto-detect the host theme and falls back to Adwaita, whose dark
// scrollbar rendering shows a faint vertical "glow" over the editor. The host's
// theme name lives in ~/.config/gtk-3.0/settings.ini and its files are on the
// host (reachable via XDG_DATA_DIRS), so exporting GTK_THEME before GTK starts
// makes the bundle render the host theme instead. Only runs inside an AppImage
// and never overrides an explicit GTK_THEME.
void useHostGtkThemeInAppImage() {
    if (std::getenv("APPIMAGE") == nullptr || std::getenv("GTK_THEME") != nullptr) {
        return;
    }
    const char* const home = std::getenv("HOME");
    if (home == nullptr) {
        return;
    }
    const char* const xdgConfig = std::getenv("XDG_CONFIG_HOME");
    const std::string configDir = (xdgConfig != nullptr && xdgConfig[0] != '\0')
                                    ? std::string(xdgConfig)
                                    : std::string(home) + "/.config";

    std::ifstream ini(configDir + "/gtk-3.0/settings.ini");
    if (!ini) {
        return;
    }
    std::string themeName;
    bool preferDark = false;
    for (std::string line; std::getline(ini, line);) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key == "gtk-theme-name") {
            themeName = value;
        } else if (key == "gtk-application-prefer-dark-theme") {
            preferDark = (value == "true" || value == "1");
        }
    }
    if (themeName.empty()) {
        return;
    }

    // The bundled GTK searches XDG_DATA_DIRS/themes for the theme files. The
    // AppImage's GTK hook can replace XDG_DATA_DIRS with bundle-only paths, so
    // append the standard host dirs (where /usr/share/themes/<Theme> lives) if
    // they're missing — otherwise the named theme can't be found and GTK falls
    // back to Adwaita regardless.
    const char* const dataDirsEnv = std::getenv("XDG_DATA_DIRS");
    std::string dataDirs = dataDirsEnv != nullptr ? dataDirsEnv : "";
    for (const std::string hostDir : { "/usr/local/share", "/usr/share" }) {
        // Membership test with ':'-padding so a path matches only as a whole
        // entry, never as a substring of a longer one.
        if (((":" + dataDirs + ":").find(":" + hostDir + ":") == std::string::npos)) {
            dataDirs += (dataDirs.empty() ? "" : ":") + hostDir;
        }
    }
    setenv("XDG_DATA_DIRS", dataDirs.c_str(), /*overwrite=*/1);

    const std::string gtkTheme = preferDark ? themeName + ":dark" : themeName;
    setenv("GTK_THEME", gtkTheme.c_str(), /*overwrite=*/0);
}

} // namespace
#endif

#ifdef __WXGTK__
// Custom entry point so the host GTK theme is applied before GTK initialises.
// On wxGTK the standard entry is just `main()` -> `wxEntry()`, so this matches
// what wxIMPLEMENT_APP would generate, with the env tweak prepended. Other
// platforms keep wxIMPLEMENT_APP (Windows needs its WinMain).
auto main(int argc, char** argv) -> int {
    useHostGtkThemeInAppImage();
    return wxEntry(argc, argv);
}
wxIMPLEMENT_APP_NO_MAIN(fbide::App);
#else
wxIMPLEMENT_APP(fbide::App);
#endif
