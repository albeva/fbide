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
/// All path properties are stored as-is from the INI file.
/// Use binaryPath/idePath for resolved absolute locations.
class Config final {
public:
    /// Initialize with resolved binary path.
    /// idePath defaults to binaryPath + "IDE/".
    explicit Config(const wxString& binaryPath);

    /// Load settings from a legacy INI file.
    /// Resets to defaults before loading.
    void load(const wxString& filePath);

    /// Save current settings to an INI file.
    void save(const wxString& filePath) const;

    /// Reset all settings to defaults.
    void reset();

    /// Get resolved binary directory path (with trailing separator).
    [[nodiscard]] auto getBinaryPath() const -> const wxString& { return m_binaryPath; }

    /// Get IDE config folder path (with trailing separator).
    [[nodiscard]] auto getIdePath() const -> const wxString& { return m_idePath; }

    /// Override IDE config folder path.
    void setIdePath(const wxString& path);

    /// Get current working directory (with trailing separator).
    [[nodiscard]] auto getCwd() const -> const wxString& { return m_cwd; }

    /// Set current working directory.
    void setCwd(const wxString& path);

    // -- [general] editor preferences --
    bool autoIndent = false;
    bool syntaxHighlight = false;
    bool longLine = false;
    bool whiteSpace = false;
    bool lineNumbers = false;
    bool indentGuide = false;
    bool braceHighlight = false;
    bool showExitCode = false;
    bool folderMargin = false;
    bool displayEOL = false;
    bool currentLine = false;
    bool activePath = true;
    int tabSize = 4;
    int edgeColumn = 80;
    wxString language = "english";

    // -- [paths] --
    wxString compilerPath;
    wxString syntaxFile = "fbfull.lng";
    wxString themeFile = "classic";
    wxString helpFile;
    wxString terminal;

    // -- [compiler] --
    wxString compileCommand = R"("<$fbc>" "<$file>")";
    wxString runCommand = R"("<$file>" <$param>)";

    // -- [editor] window state --
    bool floatBars = false;
    bool splashScreen = false;
    int windowX = 50;
    int windowY = 50;
    int windowW = 350;
    int windowH = 200;

private:
    wxString m_binaryPath;
    wxString m_idePath;
    wxString m_cwd;
};

} // namespace fbide
