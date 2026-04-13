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
    static constexpr auto SESSION_EXT = "fbt";
    static constexpr auto LANGUAGE_EXT = "lng";
    static constexpr auto THEME_EXT = "fbt";

    /// Initialize with resolved binary path.
    explicit Config(const wxString& binaryPath);

    /// Load settings from a legacy INI file.
    void load(const wxString& filePath);

    /// Save current settings to an INI file.
    void save() const;

    /// Get array with all the langauges found
    [[nodiscard]] auto getAllLanguages() const -> std::vector<wxString>;

    /// Get array with all the themes found
    [[nodiscard]] auto getAllThemes() const -> std::vector<wxString>;

    /// Get all available fixed width fonts
    [[nodiscard]] static auto getAllFixedWidthFonts() -> std::vector<wxString>;

    /// Get resolved binary directory path.
    [[nodiscard]] auto getAppPath() const -> const wxString& { return m_fbideDir; }

    /// Get IDE config folder path (with trailing separator).
    [[nodiscard]] auto getAppSettingsPath() const -> const wxString& { return m_ideDir; }

    /// Get current working directory (with trailing separator).
    [[nodiscard]] auto getCwd() const -> const wxString& { return m_cwd; }

    /// Resolve path to a fully qualified path.
    [[nodiscard]] auto resolvePath(const wxString& path) const -> wxString;

    /// Get platform specific default config file name.
    [[nodiscard]] static auto getPlatformConfigFileName() -> wxString;

    // -- [general] editor preferences --

    [[nodiscard]] auto getAutoIndent() const -> bool { return m_autoIndent; }
    void setAutoIndent(const bool val) { m_autoIndent = val; }

    [[nodiscard]] auto getSyntaxHighlight() const -> bool { return m_syntaxHighlight; }
    void setSyntaxHighlight(const bool val) { m_syntaxHighlight = val; }

    [[nodiscard]] auto getLongLine() const -> bool { return m_longLine; }
    void setLongLine(const bool val) { m_longLine = val; }

    [[nodiscard]] auto getWhiteSpace() const -> bool { return m_whiteSpace; }
    void setWhiteSpace(const bool val) { m_whiteSpace = val; }

    [[nodiscard]] auto getLineNumbers() const -> bool { return m_lineNumbers; }
    void setLineNumbers(const bool val) { m_lineNumbers = val; }

    [[nodiscard]] auto getIndentGuide() const -> bool { return m_indentGuide; }
    void setIndentGuide(const bool val) { m_indentGuide = val; }

    [[nodiscard]] auto getBraceHighlight() const -> bool { return m_braceHighlight; }
    void setBraceHighlight(const bool val) { m_braceHighlight = val; }

    [[nodiscard]] auto getShowExitCode() const -> bool { return m_showExitCode; }
    void setShowExitCode(const bool val) { m_showExitCode = val; }

    [[nodiscard]] auto getFolderMargin() const -> bool { return m_folderMargin; }
    void setFolderMargin(const bool val) { m_folderMargin = val; }

    [[nodiscard]] auto getDisplayEOL() const -> bool { return m_displayEOL; }
    void setDisplayEOL(const bool val) { m_displayEOL = val; }

    [[nodiscard]] auto getCurrentLine() const -> bool { return m_currentLine; }
    void setCurrentLine(const bool val) { m_currentLine = val; }

    [[nodiscard]] auto getTabSize() const -> int { return m_tabSize; }
    void setTabSize(const int val) { m_tabSize = val; }

    [[nodiscard]] auto getEdgeColumn() const -> int { return m_edgeColumn; }
    void setEdgeColumn(const int val) { m_edgeColumn = val; }

    [[nodiscard]] auto getLanguage() const -> const wxString& { return m_language; }
    void setLanguage(const wxString& val) { m_language = val; }

    // -- [paths] --

    [[nodiscard]] auto getCompilerPath() const -> const wxString& { return m_compilerPath; }
    void setCompilerPath(const wxString& val) { m_compilerPath = val; }

    /// Resolve the compiler path to an absolute path and validate it exists.
    /// On Windows, normalizes the path and checks existence.
    /// Returns empty string on failure.
    [[nodiscard]] auto getCompilerFullPath() const -> wxString;

    [[nodiscard]] auto getSyntaxFile() const -> const wxString& { return m_syntaxFile; }
    void setSyntaxFile(const wxString& val) { m_syntaxFile = val; }

    [[nodiscard]] auto getTheme() const -> wxString { return m_themeFile; }
    [[nodiscard]] auto getThemePath() const -> wxString;
    void setTheme(const wxString& val) { m_themeFile = val; }

    [[nodiscard]] auto getHelpFile() const -> const wxString& { return m_helpFile; }
    void setHelpFile(const wxString& val) { m_helpFile = val; }

    /// Get the default system terminal command.
    [[nodiscard]] static auto getTerminal() -> wxString;

    // -- [compiler] --

    [[nodiscard]] auto getCompileCommand() const -> const wxString& { return m_compileCommand; }
    void setCompileCommand(const wxString& val) { m_compileCommand = val; }

    [[nodiscard]] auto getRunCommand() const -> const wxString& { return m_runCommand; }
    void setRunCommand(const wxString& val) { m_runCommand = val; }

    // -- [editor] window state --

    [[nodiscard]] auto getSplashScreen() const -> bool { return m_splashScreen; }
    void setSplashScreen(const bool val) { m_splashScreen = val; }

    // TODO: consolidate this into wxSize
    [[nodiscard]] auto getWindowX() const -> int { return m_windowX; }
    void setWindowX(const int val) { m_windowX = val; }

    [[nodiscard]] auto getWindowY() const -> int { return m_windowY; }
    void setWindowY(const int val) { m_windowY = val; }

    // TODO: consolidate this into wxPoint
    [[nodiscard]] auto getWindowW() const -> int { return m_windowW; }
    void setWindowW(const int val) { m_windowW = val; }

    [[nodiscard]] auto getWindowH() const -> int { return m_windowH; }
    void setWindowH(const int val) { m_windowH = val; }

private:
    wxString m_fbideDir;
    wxString m_ideDir;
    wxString m_cwd;
    wxString m_configPath;

    // [general]
    bool m_autoIndent = true;
    bool m_syntaxHighlight = true;
    bool m_longLine = false;
    bool m_whiteSpace = false;
    bool m_lineNumbers = true;
    bool m_indentGuide = false;
    bool m_braceHighlight = true;
    bool m_showExitCode = false;
    bool m_folderMargin = false;
    bool m_displayEOL = false;
    bool m_currentLine = true;
    int m_tabSize = 4;
    int m_edgeColumn = 80;
    wxString m_language = "english";

    // [paths]
    wxString m_compilerPath;
    wxString m_syntaxFile = "fbfull.lng";
    wxString m_themeFile = "classic";
    wxString m_helpFile;

    // [compiler]
    wxString m_compileCommand = R"("<$fbc>" "<$file>")";
    wxString m_runCommand = R"(<$terminal> "<$file>" <$param>)";

    // [editor]
    bool m_splashScreen = true;
    int m_windowX = wxDefaultCoord;
    int m_windowY = wxDefaultCoord;
    int m_windowW = 0;
    int m_windowH = 0;
};

} // namespace fbide
