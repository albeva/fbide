//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Config.hpp"
using namespace fbide;

Config::Config(const wxString& binaryPath) {
    wxFileName path(binaryPath);
    path.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    m_fbideDir = path.GetAbsolutePath();
    m_ideDir = m_fbideDir + wxFileName::GetPathSeparator() + "IDE" + wxFileName::GetPathSeparator();
    m_cwd = wxGetCwd() + wxFileName::GetPathSeparator();
}

void Config::load(const wxString& filePath) {
    m_configPath = filePath;
    if (!wxFileExists(m_configPath)) {
        exit(EXIT_FAILURE);
    }

    wxFFileInputStream stream(filePath);
    if (!stream.IsOk()) {
        return;
    }

    wxFileConfig ini(stream);

    // [general]
    ini.SetPath("/general");
    m_autoIndent = ini.ReadBool("autoindent", m_autoIndent);
    m_syntaxHighlight = ini.ReadBool("syntaxhighlight", m_syntaxHighlight);
    m_longLine = ini.ReadBool("borderline", m_longLine);
    m_whiteSpace = ini.ReadBool("whitespaces", m_whiteSpace);
    m_lineNumbers = ini.ReadBool("linenumbers", m_lineNumbers);
    m_indentGuide = ini.ReadBool("indentguide", m_indentGuide);
    m_braceHighlight = ini.ReadBool("bracehighlight", m_braceHighlight);
    m_showExitCode = ini.ReadBool("showexitcode", m_showExitCode);
    m_folderMargin = ini.ReadBool("foldermargin", m_folderMargin);
    m_displayEOL = ini.ReadBool("Displayeol", m_displayEOL);
    m_currentLine = ini.ReadBool("lightcursorline", m_currentLine);
    m_tabSize = static_cast<int>(ini.ReadLong("tabsize", m_tabSize));
    m_edgeColumn = static_cast<int>(ini.ReadLong("edgecolumn", m_edgeColumn));
    m_language = ini.Read("language", m_language);

    // [paths]
    ini.SetPath("/paths");
    m_compilerPath = ini.Read("fbc", m_compilerPath);
    m_syntaxFile = ini.Read("syntax", m_syntaxFile);
    if (m_syntaxFile.empty()) {
        m_syntaxFile = "fbfull.lng";
    }
    m_themeFile = ini.Read("theme", m_themeFile);
    if (m_themeFile.empty()) {
        m_themeFile = "classic";
    }
    m_helpFile = ini.Read("helpfile", m_helpFile);

    // [compiler]
    ini.SetPath("/compiler");
    m_compileCommand = ini.Read("command", m_compileCommand);
    m_runCommand = ini.Read("runprototype", m_runCommand);

    // [editor]
    ini.SetPath("/editor");
    m_splashScreen = ini.ReadBool("splashscreen", m_splashScreen);
    m_windowX = static_cast<int>(ini.ReadLong("winx", m_windowX));
    m_windowY = static_cast<int>(ini.ReadLong("winy", m_windowY));
    m_windowW = static_cast<int>(ini.ReadLong("winw", m_windowW));
    m_windowH = static_cast<int>(ini.ReadLong("winh", m_windowH));
}

void Config::save() const {
    wxFileInputStream existingStream(m_configPath);
    wxFileConfig ini(existingStream);
    wxFileOutputStream outStream(m_configPath);

    // [general]
    ini.SetPath("/general");
    ini.Write("autoindent", m_autoIndent);
    ini.Write("syntaxhighlight", m_syntaxHighlight);
    ini.Write("borderline", m_longLine);
    ini.Write("whitespaces", m_whiteSpace);
    ini.Write("linenumbers", m_lineNumbers);
    ini.Write("indentguide", m_indentGuide);
    ini.Write("bracehighlight", m_braceHighlight);
    ini.Write("showexitcode", m_showExitCode);
    ini.Write("foldermargin", m_folderMargin);
    ini.Write("Displayeol", m_displayEOL);
    ini.Write("lightcursorline", m_currentLine);
    ini.Write("tabsize", static_cast<long>(m_tabSize));
    ini.Write("edgecolumn", static_cast<long>(m_edgeColumn));
    ini.Write("language", m_language);

    // [paths]
    ini.SetPath("/paths");
    ini.Write("fbc", m_compilerPath);
    ini.Write("syntax", m_syntaxFile);
    ini.Write("theme", m_themeFile);
    ini.Write("helpfile", m_helpFile);

    // [compiler]
    ini.SetPath("/compiler");
    ini.Write("command", m_compileCommand);
    ini.Write("runprototype", m_runCommand);

    // [editor]
    ini.SetPath("/editor");
    ini.Write("splashscreen", m_splashScreen);
    ini.Write("winx", static_cast<long>(m_windowX));
    ini.Write("winy", static_cast<long>(m_windowY));
    ini.Write("winw", static_cast<long>(m_windowW));
    ini.Write("winh", static_cast<long>(m_windowH));

    ini.Save(outStream);
}

auto Config::getAllLanguages() const -> std::vector<wxString> {
    std::vector<wxString> languages;
    const wxString langDir = getAppSettingsPath() + "lang/";
    if (const wxDir dir(langDir); dir.IsOpened()) {
        wxString filename;
        if (dir.GetFirst(&filename, "*.fbl", wxDIR_FILES)) {
            do {
                languages.emplace_back(wxFileName(filename).GetName());
            } while (dir.GetNext(&filename));
        }
    }
    return languages;
}

auto Config::getAllThemes() const -> std::vector<wxString> {
    std::vector<wxString> themes;
    if (const wxDir themeDir(getAppSettingsPath()); themeDir.IsOpened()) {
        wxString filename;
        if (themeDir.GetFirst(&filename, "*.fbt", wxDIR_FILES)) {
            do {
                themes.emplace_back(wxFileName(filename).GetName());
            } while (themeDir.GetNext(&filename));
        }
    }
    return themes;
}

auto Config::getAllFixedWidthFonts() -> std::vector<wxString> {
    wxFontEnumerator fontEnum;
    auto fontList = fontEnum.GetFacenames(wxFONTENCODING_SYSTEM, true);
    fontList.Sort();
    return fontList;
}

auto Config::resolvePath(const wxString& path) const -> wxString {
    // already a full path
    if (wxIsAbsolutePath(path)) {
        return path;
    }

    // check against fbide path
    wxFileName fn(path);
    fn.MakeAbsolute(getAppPath());
    if (fn.Exists()) {
        return fn.GetFullPath();
    }

    // check against fbide settings path
    fn = path;
    fn.MakeAbsolute(getAppSettingsPath());
    if (fn.Exists()) {
        return fn.GetFullPath();
    }

    // check against current working dir
    fn = path;
    fn.MakeAbsolute(getCwd());
    if (fn.Exists()) {
        return fn.GetFullPath();
    }

    // no idea.
    return path;
}

auto Config::getCompilerFullPath() const -> wxString {
    wxFileName path(m_compilerPath);
    path.MakeAbsolute(getAppPath());
    return path.GetFullPath();
}

auto Config::getTerminal() -> wxString {
#ifdef __WXMSW__
    return "cmd.exe";
#elif defined(__WXOSX__)
    return "open -a Terminal";
#else
    return "x-terminal-emulator";
#endif
}

auto Config::getPlatformConfigFileName() -> wxString {
#ifdef __WXMSW__
    return "prefs_win32.ini";
#else
    return "prefs_linux.ini";
#endif
}

auto Config::getThemePath() const -> wxString {
    return resolvePath(m_themeFile + "." + THEME_EXT);
}
