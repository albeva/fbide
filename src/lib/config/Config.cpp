//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "Config.hpp"
#include <wx/fileconf.h>
#include <wx/wfstream.h>

namespace fbide {

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
        // TODO: log an error?
        exit(EXIT_FAILURE);
    }
    reset();

    wxFFileInputStream stream(filePath);
    if (!stream.IsOk()) {
        return;
    }

    wxFileConfig ini(stream);

    // [general]
    ini.SetPath("/general");
    autoIndent = ini.ReadBool("autoindent", autoIndent);
    syntaxHighlight = ini.ReadBool("syntaxhighlight", syntaxHighlight);
    longLine = ini.ReadBool("borderline", longLine);
    whiteSpace = ini.ReadBool("whitespaces", whiteSpace);
    lineNumbers = ini.ReadBool("linenumbers", lineNumbers);
    indentGuide = ini.ReadBool("indentguide", indentGuide);
    braceHighlight = ini.ReadBool("bracehighlight", braceHighlight);
    showExitCode = ini.ReadBool("showexitcode", showExitCode);
    folderMargin = ini.ReadBool("foldermargin", folderMargin);
    displayEOL = ini.ReadBool("Displayeol", displayEOL);
    currentLine = ini.ReadBool("lightcursorline", currentLine);
    activePath = ini.ReadBool("ActivePath", activePath);
    tabSize = static_cast<int>(ini.ReadLong("tabsize", tabSize));
    edgeColumn = static_cast<int>(ini.ReadLong("edgecolumn", edgeColumn));
    language = ini.Read("language", language);

    // [paths]
    ini.SetPath("/paths");
    compilerPath = ini.Read("fbc", compilerPath);
    syntaxFile = ini.Read("syntax", syntaxFile);
    if (syntaxFile.empty()) {
        syntaxFile = "fbfull.lng";
    }
    themeFile = ini.Read("theme", themeFile);
    if (themeFile.empty()) {
        themeFile = "classic";
    }
    helpFile = ini.Read("helpfile", helpFile);
    terminal = ini.Read("terminal", terminal);

    // [compiler]
    ini.SetPath("/compiler");
    compileCommand = ini.Read("command", compileCommand);
    runCommand = ini.Read("runprototype", runCommand);

    // [editor]
    ini.SetPath("/editor");
    floatBars = ini.ReadBool("floatbars", floatBars);
    splashScreen = ini.ReadBool("splashscreen", splashScreen);
    windowX = static_cast<int>(ini.ReadLong("winx", windowX));
    windowY = static_cast<int>(ini.ReadLong("winy", windowY));
    windowW = static_cast<int>(ini.ReadLong("winw", windowW));
    windowH = static_cast<int>(ini.ReadLong("winh", windowH));
}

void Config::save() const {
    wxFileInputStream existingStream(m_configPath);
    wxFileConfig ini(existingStream);
    wxFileOutputStream outStream(m_configPath);

    // [general]
    ini.SetPath("/general");
    ini.Write("autoindent", autoIndent);
    ini.Write("syntaxhighlight", syntaxHighlight);
    ini.Write("borderline", longLine);
    ini.Write("whitespaces", whiteSpace);
    ini.Write("linenumbers", lineNumbers);
    ini.Write("indentguide", indentGuide);
    ini.Write("bracehighlight", braceHighlight);
    ini.Write("showexitcode", showExitCode);
    ini.Write("foldermargin", folderMargin);
    ini.Write("Displayeol", displayEOL);
    ini.Write("lightcursorline", currentLine);
    ini.Write("ActivePath", activePath);
    ini.Write("tabsize", static_cast<long>(tabSize));
    ini.Write("edgecolumn", static_cast<long>(edgeColumn));
    ini.Write("language", language);

    // [paths]
    ini.SetPath("/paths");
    ini.Write("fbc", compilerPath);
    ini.Write("syntax", syntaxFile);
    ini.Write("theme", themeFile);
    ini.Write("helpfile", helpFile);
    ini.Write("terminal", terminal);

    // [compiler]
    ini.SetPath("/compiler");
    ini.Write("command", compileCommand);
    ini.Write("runprototype", runCommand);

    // [editor]
    ini.SetPath("/editor");
    ini.Write("floatbars", floatBars);
    ini.Write("splashscreen", splashScreen);
    ini.Write("winx", static_cast<long>(windowX));
    ini.Write("winy", static_cast<long>(windowY));
    ini.Write("winw", static_cast<long>(windowW));
    ini.Write("winh", static_cast<long>(windowH));

    ini.Save(outStream);
}

void Config::reset() {
    // Reset to defaults by assigning a fresh default-initialized instance,
    // preserving resolved paths.
    auto fbideDir = m_fbideDir;
    auto ideDir = m_ideDir;
    auto cwd = m_cwd;
    auto configPath = m_configPath;
    *this = Config("");
    m_fbideDir = fbideDir;
    m_ideDir = ideDir;
    m_cwd = cwd;
    m_configPath = configPath;
}

void Config::setIdePath(const wxString& path) {
    wxFileName dir = wxFileName::DirName(path);
    dir.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    m_ideDir = dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME);
}

void Config::setCwd(const wxString& path) {
    wxFileName dir = wxFileName::DirName(path);
    dir.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    m_cwd = dir.GetPath(wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME);
}

auto Config::resolvePath(const wxString& path) const -> wxString {
    if (wxIsAbsolutePath(path)) {
        return path;
    }

    // cwd
    wxFileName fn(path);
    fn.MakeAbsolute(getCwd());
    if (fn.Exists()) {
        return fn.GetFullPath();
    }

    // ./ide/
    fn = path;
    fn.MakeAbsolute(getIdePath());
    if (fn.Exists()) {
        return fn.GetFullPath();
    }

    // maybe look in PATHs?
    // TODO: log an error?
    return path;
}

auto Config::getDefaultConfigFileName() -> wxString {
#ifdef __WXMSW__
    return "prefs_win32.ini";
#else
    return "prefs_linux.ini";
#endif
}

} // namespace fbide
