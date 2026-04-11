//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {

/// Application configuration.
/// Loads/saves legacy INI prefs files via wxFileConfig.
class Config final {
public:
    /// Initialize with resolved binary path.
    explicit Config(const wxString& binaryPath);

    /// Load settings from a legacy INI file.
    void load(const wxString& filePath);

    /// Save current settings to an INI file.
    void save() const;

    /// Reset all settings to defaults.
    void reset();

    /// Get resolved binary directory path.
    [[nodiscard]] auto getFbidePath() const -> const wxString& { return m_fbideDir; }

    /// Get IDE config folder path (with trailing separator).
    [[nodiscard]] auto getIdePath() const -> const wxString& { return m_ideDir; }

    /// Override IDE config folder path.
    void setIdePath(const wxString& path);

    /// Get current working directory (with trailing separator).
    [[nodiscard]] auto getCwd() const -> const wxString& { return m_cwd; }

    /// Set current working directory.
    void setCwd(const wxString& path);

    /// Resolve path to a fully qualified path.
    [[nodiscard]] auto resolvePath(const wxString& path) const -> wxString;

    /// Get platform specific default config file name.
    [[nodiscard]] static auto getDefaultConfigFileName() -> wxString;

    // -- [general] editor preferences --

    [[nodiscard]] auto autoIndent() const -> bool { return m_autoIndent; }
    void set_autoIndent(bool val) { m_autoIndent = val; }

    [[nodiscard]] auto syntaxHighlight() const -> bool { return m_syntaxHighlight; }
    void set_syntaxHighlight(bool val) { m_syntaxHighlight = val; }

    [[nodiscard]] auto longLine() const -> bool { return m_longLine; }
    void set_longLine(bool val) { m_longLine = val; }

    [[nodiscard]] auto whiteSpace() const -> bool { return m_whiteSpace; }
    void set_whiteSpace(bool val) { m_whiteSpace = val; }

    [[nodiscard]] auto lineNumbers() const -> bool { return m_lineNumbers; }
    void set_lineNumbers(bool val) { m_lineNumbers = val; }

    [[nodiscard]] auto indentGuide() const -> bool { return m_indentGuide; }
    void set_indentGuide(bool val) { m_indentGuide = val; }

    [[nodiscard]] auto braceHighlight() const -> bool { return m_braceHighlight; }
    void set_braceHighlight(bool val) { m_braceHighlight = val; }

    [[nodiscard]] auto showExitCode() const -> bool { return m_showExitCode; }
    void set_showExitCode(bool val) { m_showExitCode = val; }

    [[nodiscard]] auto folderMargin() const -> bool { return m_folderMargin; }
    void set_folderMargin(bool val) { m_folderMargin = val; }

    [[nodiscard]] auto displayEOL() const -> bool { return m_displayEOL; }
    void set_displayEOL(bool val) { m_displayEOL = val; }

    [[nodiscard]] auto currentLine() const -> bool { return m_currentLine; }
    void set_currentLine(bool val) { m_currentLine = val; }

    [[nodiscard]] auto activePath() const -> bool { return m_activePath; }
    void set_activePath(bool val) { m_activePath = val; }

    [[nodiscard]] auto tabSize() const -> int { return m_tabSize; }
    void set_tabSize(int val) { m_tabSize = val; }

    [[nodiscard]] auto edgeColumn() const -> int { return m_edgeColumn; }
    void set_edgeColumn(int val) { m_edgeColumn = val; }

    [[nodiscard]] auto language() const -> const wxString& { return m_language; }
    void set_language(const wxString& val) { m_language = val; }

    // -- [paths] --

    [[nodiscard]] auto compilerPath() const -> const wxString& { return m_compilerPath; }
    void set_compilerPath(const wxString& val) { m_compilerPath = val; }

    [[nodiscard]] auto syntaxFile() const -> const wxString& { return m_syntaxFile; }
    void set_syntaxFile(const wxString& val) { m_syntaxFile = val; }

    [[nodiscard]] auto themeFile() const -> wxString { return m_themeFile + ".fbt"; }
    void set_themeFile(const wxString& val) { m_themeFile = val; }

    [[nodiscard]] auto helpFile() const -> const wxString& { return m_helpFile; }
    void set_helpFile(const wxString& val) { m_helpFile = val; }

    [[nodiscard]] auto terminal() const -> const wxString& { return m_terminal; }
    void set_terminal(const wxString& val) { m_terminal = val; }

    // -- [compiler] --

    [[nodiscard]] auto compileCommand() const -> const wxString& { return m_compileCommand; }
    void set_compileCommand(const wxString& val) { m_compileCommand = val; }

    [[nodiscard]] auto runCommand() const -> const wxString& { return m_runCommand; }
    void set_runCommand(const wxString& val) { m_runCommand = val; }

    // -- [editor] window state --

    [[nodiscard]] auto floatBars() const -> bool { return m_floatBars; }
    void set_floatBars(bool val) { m_floatBars = val; }

    [[nodiscard]] auto splashScreen() const -> bool { return m_splashScreen; }
    void set_splashScreen(bool val) { m_splashScreen = val; }

    [[nodiscard]] auto windowX() const -> int { return m_windowX; }
    void set_windowX(int val) { m_windowX = val; }

    [[nodiscard]] auto windowY() const -> int { return m_windowY; }
    void set_windowY(int val) { m_windowY = val; }

    [[nodiscard]] auto windowW() const -> int { return m_windowW; }
    void set_windowW(int val) { m_windowW = val; }

    [[nodiscard]] auto windowH() const -> int { return m_windowH; }
    void set_windowH(int val) { m_windowH = val; }

private:
    wxString m_fbideDir;
    wxString m_ideDir;
    wxString m_cwd;
    wxString m_configPath;

    // [general]
    bool m_autoIndent = false;
    bool m_syntaxHighlight = false;
    bool m_longLine = false;
    bool m_whiteSpace = false;
    bool m_lineNumbers = false;
    bool m_indentGuide = false;
    bool m_braceHighlight = false;
    bool m_showExitCode = false;
    bool m_folderMargin = false;
    bool m_displayEOL = false;
    bool m_currentLine = false;
    bool m_activePath = true;
    int m_tabSize = 4;
    int m_edgeColumn = 80;
    wxString m_language = "english";

    // [paths]
    wxString m_compilerPath;
    wxString m_syntaxFile = "fbfull.lng";
    wxString m_themeFile = "classic";
    wxString m_helpFile;
    wxString m_terminal;

    // [compiler]
    wxString m_compileCommand = R"("<$fbc>" "<$file>")";
    wxString m_runCommand = R"("<$file>" <$param>)";

    // [editor]
    bool m_floatBars = false;
    bool m_splashScreen = false;
    int m_windowX = 50;
    int m_windowY = 50;
    int m_windowW = 350;
    int m_windowH = 200;
};

} // namespace fbide
