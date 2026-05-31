//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FbcAutoDetect.hpp"
#include <wx/dirdlg.h>
#include <wx/filefn.h>
#include <wx/richmsgdlg.h>
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "utils/PathConversions.hpp"
using namespace fbide;

#ifdef __WXMSW__

namespace {
/// Compile-command template fragments shared by every generated config.
/// Stored raw — the INI writer escapes the embedded quotes on save.
constexpr auto kDefaultCompile = R"("<$fbc>" "<$file>")";
constexpr auto kDefaultRun = R"("<$file>" <$param>)";
constexpr auto kDefaultTerminal = "cmd /C";

/// One generated GUI/Console pair is driven by this descriptor.
struct ArchPlan final {
    FbcArch arch;
    const char* target; ///< `-target` value: win32 / win64.
    const char* label;  ///< Display-name prefix: Win32 / Win64.
};

/// `-s` subsystem variants, in the order they appear in the list.
struct SubsystemPlan final {
    const char* flag;    ///< `-s` value: gui / console.
    const char* label;   ///< Display-name suffix: GUI / Console.
    bool isConsole;      ///< Console is the one promoted to active.
};

/// Executable names probed for an fbc compiler, in priority order.
constexpr std::array<const char*, 3> kFbcNames { "fbc.exe", "fbc32.exe", "fbc64.exe" };

/// True when `folder` contains any fbc executable (by name).
auto folderHasFbc(const std::filesystem::path& folder) -> bool {
    return std::ranges::any_of(kFbcNames, [&folder](const char* name) {
        std::error_code ec;
        return std::filesystem::exists(folder / name, ec);
    });
}

/// Search the system PATH for an fbc executable; return its containing
/// folder, or nullopt when none is on the PATH.
auto findInPath() -> std::optional<std::filesystem::path> {
    wxPathList paths;
    paths.AddEnvList("PATH");
    for (const auto* name : kFbcNames) {
        const wxString found = paths.FindAbsoluteValidPath(name);
        if (!found.empty()) {
            return toFsPath(found).parent_path();
        }
    }
    return std::nullopt;
}

/// Run `<exe> --version` and return its first output line — empty when the
/// binary cannot be run. Serves as the default `Probe`.
auto probeVersion(const std::filesystem::path& exe) -> wxString {
    wxArrayString output;
    wxExecute("\"" + toWxString(exe) + "\" --version", output);
    return output.empty() ? wxString {} : output[0];
}

/// True when the current `[compiler]` config diverges from the shipped
/// pristine defaults: any user-defined configuration, or any of the four
/// canonical fields edited away from baseline.
auto hasExistingSettings(ConfigManager& cfg) -> bool {
    const auto& cur = cfg.config().at("compiler");
    const auto& base = cfg.baseline(ConfigManager::Category::Config).at("compiler");
    const bool hasUserConfig = std::ranges::any_of(cur.entries(), [](const auto& entry) {
        return entry.second->isTable();
    });
    if (hasUserConfig) {
        return true;
    }
    constexpr std::array<const char*, 4> fields { "path", "compileCommand", "runCommand", "terminal" };
    return std::ranges::any_of(fields, [&cur, &base](const char* key) {
        return cur.get_or(key, wxString {}) != base.get_or(key, wxString {});
    });
}

} // namespace

auto FbcAutoDetect::parseArch(const wxString& versionLine) -> std::optional<FbcArch> {
    const auto lower = versionLine.Lower();
    if (lower.Contains("win64") || lower.Contains("64bit")) {
        return FbcArch::Win64;
    }
    if (lower.Contains("win32") || lower.Contains("32bit")) {
        return FbcArch::Win32;
    }
    return std::nullopt;
}

auto FbcAutoDetect::detectVariants(const std::filesystem::path& folder, const Probe& probe) -> std::vector<FbcVariant> {
    struct Candidate final {
        const char* name = nullptr;
        std::optional<FbcArch> archHint = std::nullopt; ///< Known from the file name; nullopt for plain fbc.exe.
    };
    // Named variants first so they win over a plain fbc.exe of the same arch.
     constexpr std::array<Candidate, 3> candidates { {
        { .name = "fbc64.exe", .archHint = FbcArch::Win64 },
        { .name = "fbc32.exe", .archHint = FbcArch::Win32 },
        { .name = "fbc.exe", .archHint = std::nullopt },
    } };

    std::vector<FbcVariant> result;
    const auto haveArch = [&result](FbcArch arch) {
        return std::ranges::any_of(result, [arch](const FbcVariant& existing) { return existing.arch == arch; });
    };

    for (const auto& [name, archHint] : candidates) {
        const auto exe = folder / name;
        std::error_code ec;
        if (!std::filesystem::exists(exe, ec)) {
            continue;
        }
        // Must run and report a version — confirms it is a real fbc binary.
        const auto version = probe(exe);
        if (version.empty()) {
            continue;
        }
        const auto arch = archHint.has_value() ? archHint : parseArch(version);
        if (!arch.has_value() || haveArch(*arch)) {
            continue;
        }
        result.push_back({ .exe = exe, .arch = *arch });
    }
    return result;
}

auto FbcAutoDetect::buildCompilerValue(std::span<const FbcVariant> variants, bool osIs64) -> Value {
    // Resolve the binary for each architecture (absent when not installed).
    std::optional<std::filesystem::path> exe32;
    std::optional<std::filesystem::path> exe64;
    for (const auto& [exe, arch] : variants) {
        if (arch == FbcArch::Win32) {
            exe32 = exe;
        } else {
            exe64 = exe;
        }
    }

    // Canonical architecture: prefer the OS architecture, fall back to the
    // other when the matching binary is not installed.
    std::optional<FbcArch> canonicalArch;
    if (osIs64) {
        if (exe64.has_value()) {
            canonicalArch = FbcArch::Win64;
        } else if (exe32.has_value()) {
            canonicalArch = FbcArch::Win32;
        }
    } else {
        if (exe32.has_value()) {
            canonicalArch = FbcArch::Win32;
        } else if (exe64.has_value()) {
            canonicalArch = FbcArch::Win64;
        }
    }

    Value compiler;
    if (canonicalArch.has_value()) {
        const auto& exe = (*canonicalArch == FbcArch::Win64) ? exe64 : exe32;
        compiler["path"] = toWxString(*exe);
    }
    compiler["compileCommand"] = wxString { kDefaultCompile };
    compiler["runCommand"] = wxString { kDefaultRun };
    compiler["terminal"] = wxString { kDefaultTerminal };
    compiler["showInMenu"] = false; // Default is hidden from the menu.

    static constexpr std::array<ArchPlan, 2> kArchOrder { {
        { .arch = FbcArch::Win32, .target = "win32", .label = "Win32" },
        { .arch = FbcArch::Win64, .target = "win64", .label = "Win64" },
    } };
    static constexpr std::array<SubsystemPlan, 2> kSubsystems { {
        { .flag = "gui", .label = "GUI", .isConsole = false },
        { .flag = "console", .label = "Console", .isConsole = true },
    } };

    int next = 1;
    wxString activeSlug;
    for (const auto& archPlan : kArchOrder) {
        const auto& exe = (archPlan.arch == FbcArch::Win32) ? exe32 : exe64;
        if (!exe.has_value()) {
            continue;
        }
        for (const auto& [flag, label, isConsole] : kSubsystems) {
            const auto slug = wxString::Format("cfg-%d", next);
            auto& cfg = compiler[slug];
            cfg["name"] = wxString { archPlan.label } + " " + label;
            cfg["path"] = toWxString(*exe);
            cfg["compileCommand"] = wxString { R"("<$fbc>" -target )" } + archPlan.target + " -s " + flag + R"( "<$file>")";
            cfg["showInMenu"] = true;
            cfg["order"] = next;
            if (isConsole && canonicalArch.has_value() && archPlan.arch == *canonicalArch) {
                activeSlug = slug;
            }
            ++next;
        }
    }

    if (!activeSlug.empty()) {
        compiler["active"] = activeSlug;
    }
    compiler["nextSlugIndex"] = next;
    return compiler;
}


FbcAutoDetect::FbcAutoDetect(Context& ctx)
: m_ctx(ctx) {}

auto FbcAutoDetect::run(wxWindow* parent) -> std::optional<Value> {
    auto& cfg = m_ctx.getConfigManager();
    const Value& loc = cfg.locale().at("dialogs.settings.compiler");
    const auto tr = [&loc](const wxString& key) { return loc.get_or(key, key); };

    // 1. Warn before overwriting settings the user has customised.
    if (hasExistingSettings(cfg)) {
        const auto answer = wxMessageBox(tr("autoOverwriteConfirm"), tr("autoDetect"), wxYES_NO | wxICON_WARNING, parent);
        if (answer != wxYES) {
            return std::nullopt;
        }
    }

    // Folder picker that insists on a folder actually containing fbc.
    const auto browse = [&]() -> std::optional<std::filesystem::path> {
        for (;;) {
            wxDirDialog dlg(parent, tr("autoSelectFolder"), wxEmptyString, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
            if (dlg.ShowModal() != wxID_OK) {
                return std::nullopt;
            }
            auto folder = toFsPath(dlg.GetPath());
            if (folderHasFbc(folder)) {
                return folder;
            }
            wxMessageBox(tr("autoNoFbc"), tr("autoDetect"), wxICON_ERROR | wxOK, parent);
        }
    };

    // 2. Locate fbc: offer a PATH hit (use / browse / cancel), else browse.
    std::optional<std::filesystem::path> folder;
    if (const auto inPath = findInPath()) {
        wxRichMessageDialog dlg(parent, tr("autoFoundInPath"), tr("autoDetect"), wxYES_NO | wxCANCEL | wxICON_QUESTION);
        dlg.SetYesNoLabels(tr("autoUseFound"), tr("autoBrowse"));
        switch (dlg.ShowModal()) {
        case wxID_YES:
            folder = inPath;
            break;
        case wxID_NO:
            folder = browse();
            break;
        default:
            return std::nullopt; // Cancel
        }
    } else {
        folder = browse();
    }
    if (!folder.has_value()) {
        return std::nullopt;
    }

    // 3. Detect variants in the chosen folder; bail when none are usable.
    const auto variants = detectVariants(*folder, [](const std::filesystem::path& exe) { return probeVersion(exe); });
    if (variants.empty()) {
        wxMessageBox(tr("autoNoFbc"), tr("autoDetect"), wxICON_ERROR | wxOK, parent);
        return std::nullopt;
    }

    // 4. Build the [compiler] subtree to install.
    return buildCompilerValue(variants, wxIsPlatform64Bit());
}

#endif // __WXMSW__
