//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "App.hpp"
#include "Context.hpp"
#include "FileAssociations.hpp"
#include "FileAssociationsLinux.hpp"
#include "InstanceHandler.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "config/Value.hpp"
#include "config/Version.hpp"
#include "document/DocumentManager.hpp"
#include "document/FileSession.hpp"
#include "ui/UIManager.hpp"
#include "update/UpdateManager.hpp"
#ifdef __WXMSW__
#include <windows.h>
#endif
using namespace fbide;

namespace {

constexpr auto kHelpText = R"(Usage: fbide [options] [files...]

Options:
  --config <path>     Use the specified config file.
  --ide <path>        Override the IDE resources directory
                      (default: <fbide-binary-dir>/ide).
  --log-path <path>   Write the application log to <path>
                      (default: <user-data-dir>/logs/fbide_<version>.log).
  --cfg=<spec>        Print a config value to stdout and exit.
                      <spec> is `[category:]path[.*|/|/*]`.
                      Categories: config (default), locale, shortcuts,
                      keywords, layout. `.` and `/` are interchangeable
                      path separators. A trailing `*` or `/` enumerates
                      every leaf key under the path (one per line).
                      Examples:
                        --cfg=compiler.path
                        --cfg=locale:dialogs.find.title
                        --cfg=*                # all keys in config
                        --cfg=editor/          # all keys under editor
                        --cfg=locale:dialogs.*
  --restore-state-from <p>
                      Restore editor state from the snapshot at <p> on startup,
                      then delete it. Used internally to carry documents across
                      a restart.
  --wait-for-pid <id> Block startup (before any config is loaded) until the
                      process with id <id> has exited.
  --new-window        Open a new window even if another instance is running.
  --verbose           Enable verbose logging.
  --version           Print fbide and wxWidgets version and exit.
  --help              Show this help and exit.
)";

#ifdef __WXMSW__
/// Inject a synthetic Enter into the parent console's input queue. Workaround
/// for the `/SUBSYSTEM:WINDOWS` UX wart: the shell doesn't wait for a GUI
/// child, so it prints its next prompt immediately and our AttachConsole +
/// WriteFile output prints on top of it. Posting Enter makes the shell consume
/// the (empty) line and redraw a fresh prompt below our text. Skipped when
/// stdin isn't a console (e.g. piped/redirected).
void pokeParentConsole() {
    const HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == nullptr || hIn == INVALID_HANDLE_VALUE) {
        return;
    }
    if (GetFileType(hIn) != FILE_TYPE_CHAR) {
        return;
    }
    INPUT_RECORD events[2] = {};
    events[0].EventType = KEY_EVENT;
    events[0].Event.KeyEvent.bKeyDown = TRUE;
    events[0].Event.KeyEvent.wRepeatCount = 1;
    events[0].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    events[0].Event.KeyEvent.uChar.AsciiChar = '\r';
    events[1] = events[0];
    events[1].Event.KeyEvent.bKeyDown = FALSE;
    DWORD written = 0;
    WriteConsoleInput(hIn, events, 2, &written);
}
#endif

/// Write `text` followed by a newline to the host's stdout/stderr. Goes
/// through the raw OS handle on Windows because `/SUBSYSTEM:WINDOWS` builds
/// don't have CRT-bound streams even when the shell redirects. If no handle
/// is attached (Explorer launch with no parent console), attach to the
/// parent and retry, then poke an Enter so the shell redraws its prompt.
void writeLineTo(const wxString& text, const bool toStderr) {
    const auto utf8 = text.ToStdString(wxConvUTF8) + '\n';
#ifdef __WXMSW__
    const DWORD stdHandle = toStderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    auto write = [&](const HANDLE h) {
        DWORD written = 0;
        return h != nullptr && h != INVALID_HANDLE_VALUE
            && WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) != 0;
    };
    if (write(GetStdHandle(stdHandle))) {
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS) != 0) {
        write(GetStdHandle(stdHandle));
        pokeParentConsole();
    }
#else
    auto& stream = toStderr ? std::cerr : std::cout;
    stream << utf8;
    stream.flush();
#endif
}

void writeLine(const wxString& text) { writeLineTo(text, /*toStderr=*/false); }
void writeErrLine(const wxString& text) { writeLineTo(text, /*toStderr=*/true); }

/// Resolve the application log file path. Honours `--log-path` when set,
/// otherwise falls back to `<user-data-dir>/logs/fbide_<version>.log` so
/// packaged builds (AppImage, .app bundles, signed Windows installers)
/// don't try to write next to a read-only executable, and so different
/// installed versions write to distinct files. Creates the parent dir.
auto resolveLogPath(const wxString& cliLogPath) -> wxString {
    wxFileName logFile;
    if (!cliLogPath.IsEmpty()) {
        logFile.Assign(cliLogPath);
    } else {
        logFile.AssignDir(wxStandardPaths::Get().GetUserDataDir());
        logFile.AppendDir("logs");
        logFile.SetFullName(wxString::Format("fbide_%s.log", Version::fbide().asString()));
    }
    logFile.Normalize(wxPATH_NORM_ENV_VARS | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_ABSOLUTE);
    wxFileName::Mkdir(logFile.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return logFile.GetFullPath();
}

/// True when `path` lives anywhere under the platform's temp directory.
/// Used by the `--restore-state-from` cleanup branch so we only delete files
/// FBIde itself dropped into temp space (the language-restart flow);
/// user-supplied `.fbs` files passed on the command line stay put.
auto isInsideTempDir(const wxString& path) -> bool {
    wxFileName session(path);
    session.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_LONG);

    wxFileName tempDir(wxStandardPaths::Get().GetTempDir(), wxEmptyString);
    tempDir.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_LONG);

    auto sessionPath = session.GetPath();
    auto tempPath = tempDir.GetPath();
#ifdef __WXMSW__
    sessionPath.MakeLower();
    tempPath.MakeLower();
#endif
    return !tempPath.IsEmpty() && sessionPath.StartsWith(tempPath);
}

/// Recursively collect every leaf under `node`, prefixing with `prefix`
/// (dot-separated). Each emitted entry is formatted `path=value` — sorted
/// later so the path prefix orders the listing.
void collectLeafEntries(const Value& node, const wxString& prefix, std::vector<wxString>& out) {
    if (!node.isTable()) {
        if (!prefix.IsEmpty()) {
            out.push_back(prefix + "=" + node.value_or(wxString {}));
        }
        return;
    }
    for (const auto& [key, child] : node.entries()) {
        const wxString sub = prefix.IsEmpty() ? key : (prefix + "." + key);
        collectLeafEntries(*child, sub, out);
    }
}

} // namespace

auto App::OnExit() -> int {
    // Flush clipboard so text copied from fbide (e.g. Format dialog "Copy"
    // output) remains available in other applications after we close.
    if (wxTheClipboard->Open()) {
        wxTheClipboard->Flush();
        wxTheClipboard->Close();
    }
    // Detach + delete the log target before the stream it borrows dies.
    delete wxLog::SetActiveTarget(nullptr);
    m_logStream.reset();
    return wxApp::OnExit();
}

void App::initAppearance() {
    // Enable light/dark mode appearance.
    // wxWidgets 3.3.x dark mode on Windows is partial — some controls
    // never repaint. SetAppearance(Dark) returns AppearanceResult; log
    // when it fails so misbehaving installs leave a breadcrumb in the
    // log.
    const auto appearance = m_context->getConfigManager().config().get_or("appearance", "").Lower();
    if (appearance.IsEmpty()) {
        return;
    }

    auto target = Appearance::System;
    if (appearance == "dark") {
        target = Appearance::Dark;
    } else if (appearance == "light") {
        target = Appearance::Light;
    } else if (appearance == "system") {
        target = Appearance::System;
    } else {
        wxLogWarning("Unknown 'appearance' value '%s' — expected light/dark/system", appearance);
        return;
    }

    const auto result = SetAppearance(target);
    if (result != AppearanceResult::Ok) {
        const wxString reason = (result == AppearanceResult::CannotChange)
                                  ? "appearance can no longer be changed (already shown windows)"
                                  : "wx returned Failure";
        wxLogWarning("SetAppearance('%s') did not apply: %s", appearance, reason);
    }
}

auto App::OnInit() -> bool {
    auto cli = parseCli();
    if (cli.parseFailed) {
        std::exit(EXIT_FAILURE);
    }

    if (cli.helpRequested) {
        showHelp();
        std::exit(EXIT_SUCCESS);
    }

    if (cli.versionRequested) {
        showVersion();
        std::exit(EXIT_SUCCESS);
    }

    m_newWindow = cli.newWindow;
    m_verbose = cli.verbose;
    m_configPath = cli.configPath;
    m_idePath = cli.idePath;
    m_logPath = cli.logPath;
    if (cli.verbose) {
        wxLog::SetVerbose(true);
    }

    // If asked, block until the predecessor process has exited. The
    // language-change restart flow uses this so the new instance only
    // touches config/locale files after the previous one has fully
    // released them. Polling via wxProcess::Exists is portable; cap
    // the wait so a misbehaving caller can't hang us indefinitely.
    if (cli.waitForPid > 0) {
        constexpr int maxWaitMs = 5000;
        constexpr int pollMs = 25;
        for (int waited = 0; waited < maxWaitMs && wxProcess::Exists(cli.waitForPid); waited += pollMs) {
            wxMilliSleep(pollMs);
        }
    }

    const auto fbidePath = getFbidePath();

    // Open the log file unbuffered (`std::unitbuf` flushes after each
    // `<<`, so `wxLogStream` lands every record on disk before returning)
    // and disable wxLog's repetition counter so duplicate messages don't
    // sit in memory waiting to be coalesced. Together these ensure the
    // last few records survive a crash.
    const auto logPath = resolveLogPath(cli.logPath);
    // `wxLogStream` borrows the stream without owning it, so App keeps it
    // alive and tears it down in OnExit after detaching the log target.
    m_logStream = std::make_unique<std::ofstream>(logPath.ToStdString(), std::ios::app);
    *m_logStream << std::unitbuf;
    wxLog::SetRepetitionCounting(false);
    wxLog::SetActiveTarget(new wxLogStream(m_logStream.get()));

    // Construct context with parsed CLI overrides — `--ide` flows into
    // ConfigManager so subsequent config/locale/theme lookups resolve
    // against the overridden resource directory by default.
    m_context = std::make_unique<Context>(*this, fbidePath, cli.idePath, cli.configPath);

    // --cfg=<spec>: print value, exit. No window, no IPC, no splash.
    if (!cli.cfgKey.IsEmpty()) {
        writeLine(resolveCfg(cli.cfgKey));
        std::exit(EXIT_SUCCESS);
    }

    // Single instance: if another FBIde is running, forward files and exit
    if (!m_newWindow) {
        m_instanceHandler = std::make_unique<InstanceHandler>(*m_context);
        if (m_instanceHandler->isAnotherRunning()) {
            m_instanceHandler->sendFiles(cli.files);
            return false;
        }
    }

    initAppearance();
    showSplash();

    const auto& configManager = m_context->getConfigManager();
    m_context->getFileHistory().load(configManager.historyPath());

    // Build the shared FB keyword tables before any editor / Intellisense lexes.
    lexer::setFbKeywords(m_context->getConfigManager().keywords().at("groups"));

#ifdef __WXOSX__
    // Opt out of macOS automatic window tabbing — we manage our own AUI
    // notebook, so the OS tab bar (and its "Show Tab Bar" View-menu items)
    // is redundant. Must run before the main frame is ordered front.
    OSXEnableAutomaticTabbing(false);
#endif

    m_context->getUIManager().createMainFrame();
#ifdef __WXMSW__
    // Register per-user associations so .bas/.bi/.fbs show FBIde's icons and open
    // with FBIde. ensureRegistered() self-skips on installed builds (the installer
    // records a marker under Software\FBIde and owns the associations, honouring
    // the user's per-type choices in the setup wizard); portable (zip) builds
    // carry no marker and register.
    FileAssociations::ensureRegistered();
#elifdef __WXGTK__
    // AppImage self-integration: publish the desktop entry, MIME types and
    // icons into ~/.local/share so .bas/.bi/.fbs associate with FBIde.
    FileAssociationsLinux::ensureRegistered();
#endif
    openFiles(cli.files);
    if (!cli.restoreStateFrom.IsEmpty()) {
        // A throwaway snapshot from a restart that had no active session: load it
        // as a session to reopen the documents, then close the session (leaving
        // them as loose documents) and delete the temp file.
        auto& docManager = m_context->getDocumentManager();
        docManager.startSession(cli.restoreStateFrom);
        docManager.closeSession();
        if (isInsideTempDir(cli.restoreStateFrom)) {
            wxRemoveFile(cli.restoreStateFrom);
        }
    }
    // First launch (no config overlay yet): try to locate a bundled or
    // PATH fbc silently so the IDE works out of the box — installers ship
    // fbc next to fbide.exe. Only when that finds nothing do we fall back
    // to the interactive "compiler missing" prompt. Later launches go
    // straight to the prompt-if-missing check.
    auto& compilerManager = m_context->getCompilerManager();
    const bool firstRunConfigured = configManager.isFirstRun() && compilerManager.detectCompilerOnFirstRun();
    if (!firstRunConfigured) {
        compilerManager.checkCompilerOnStartup();
    }
    m_context->getUpdateManager().checkOnStartup();
    return true;
}

auto App::parseCli() const -> CliOptions {
    CliOptions opts;
    auto args = argv.GetArguments();

    for (std::size_t index = 1; index < args.GetCount(); index++) {
        const auto& arg = args[index];

        if (arg == "--help") {
            opts.helpRequested = true;
            return opts;
        }
        if (arg == "--version") {
            opts.versionRequested = true;
            return opts;
        }
        if (arg == "--new-window") {
            opts.newWindow = true;
            continue;
        }
        if (arg == "--verbose") {
            opts.verbose = true;
            continue;
        }
        if (arg == "--config") {
            index += 1;
            if (index >= args.GetCount()) {
                writeErrLine("fbide: --config requires a path argument");
                opts.parseFailed = true;
                return opts;
            }
            opts.configPath = args[index];
            continue;
        }
        if (arg == "--ide") {
            index += 1;
            if (index >= args.GetCount()) {
                writeErrLine("fbide: --ide requires a path argument");
                opts.parseFailed = true;
                return opts;
            }
            opts.idePath = args[index];
            continue;
        }
        if (arg == "--log-path") {
            index += 1;
            if (index >= args.GetCount()) {
                writeErrLine("fbide: --log-path requires a path argument");
                opts.parseFailed = true;
                return opts;
            }
            opts.logPath = args[index];
            continue;
        }
        if (arg.StartsWith("--cfg=")) {
            opts.cfgKey = arg.Mid(6);
            if (opts.cfgKey.IsEmpty()) {
                writeErrLine("fbide: --cfg= requires a key");
                opts.parseFailed = true;
                return opts;
            }
            continue;
        }
        if (arg == "--restore-state-from") {
            index += 1;
            if (index >= args.GetCount()) {
                writeErrLine("fbide: --restore-state-from requires a path argument");
                opts.parseFailed = true;
                return opts;
            }
            opts.restoreStateFrom = args[index];
            continue;
        }
        if (arg == "--wait-for-pid") {
            index += 1;
            if (index >= args.GetCount()) {
                writeErrLine("fbide: --wait-for-pid requires a numeric pid argument");
                opts.parseFailed = true;
                return opts;
            }
            if (!args[index].ToInt(&opts.waitForPid) || opts.waitForPid <= 0) {
                writeErrLine(wxString::Format("fbide: --wait-for-pid expected a positive integer, got '%s'", args[index]));
                opts.parseFailed = true;
                return opts;
            }
            continue;
        }
        if (arg.StartsWith("-")) {
            writeErrLine(wxString::Format("fbide: unknown option: %s", arg));
            opts.parseFailed = true;
            return opts;
        }

        wxFileName fileName(arg);
        fileName.Normalize(wxPATH_NORM_ENV_VARS | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_ABSOLUTE);
        opts.files.Add(fileName.GetAbsolutePath());
    }
    return opts;
}

void App::showHelp() {
    writeLine(kHelpText);
}

void App::showVersion() {
    writeLine(wxString::Format(
        "fbide %s (wxWidgets %s)",
        Version::fbide().asString(),
        Version::wxWidgets().asString()
    ));
}

auto App::resolveCfg(const wxString& spec) const -> wxString {
    using Cat = ConfigManager::Category;

    wxString prefix;
    wxString key;
    if (const auto colon = spec.Find(':'); colon != wxNOT_FOUND) {
        prefix = spec.Mid(0, static_cast<std::size_t>(colon)).Lower();
        key = spec.Mid(static_cast<std::size_t>(colon) + 1);
    } else {
        key = spec;
    }

    auto cat = Cat::Config;
    if (!prefix.IsEmpty()) {
        if (prefix == "config") {
            cat = Cat::Config;
        } else if (prefix == "locale") {
            cat = Cat::Locale;
        } else if (prefix == "shortcuts") {
            cat = Cat::Shortcuts;
        } else if (prefix == "keywords") {
            cat = Cat::Keywords;
        } else if (prefix == "layout") {
            cat = Cat::Layout;
        } else {
            writeErrLine(wxString::Format("fbide: unknown --cfg category: %s", prefix));
            return {};
        }
    }

    // Detect enumeration markers: bare `*`, trailing `*`, or trailing `/`.
    // Strip the marker; what remains is the (possibly empty) prefix path.
    bool enumerate = false;
    if (key == "*") {
        key.clear();
        enumerate = true;
    } else if (key.EndsWith("/*") || key.EndsWith(".*")) {
        key = key.Mid(0, key.length() - 2);
        enumerate = true;
    } else if (key.EndsWith("/") || key.EndsWith(".")) {
        key = key.Mid(0, key.length() - 1);
        enumerate = true;
    }
    // Accept `/` as a path separator alongside `.` for ergonomics.
    key.Replace("/", ".");

    const auto& root = m_context->getConfigManager().get(cat);
    const auto& node = key.IsEmpty() ? root : root.at(key);

    if (!enumerate) {
        return node.value_or(wxString {});
    }

    std::vector<wxString> entries;
    collectLeafEntries(node, key, entries);
    std::ranges::sort(entries);

    wxString out;
    for (const auto& entry : entries) {
        if (!out.IsEmpty()) {
            out += '\n';
        }
        out += entry;
    }
    return out;
}

auto App::getFbidePath() -> wxString {
    const auto& sp = GetTraits()->GetStandardPaths();
    return wxPathOnly(sp.GetExecutablePath());
}

void App::openFiles(const wxArrayString& files) {
    for (const auto& file : files) {
        m_pendingFiles.Add(file);
    }

    // The notebook only exists once createMainFrame() has run. Files
    // forwarded by a second instance can arrive during the splash
    // screen (showSplash's wxYield pumps IPC events), before that —
    // hold them until OnInit reaches the open call after the frame is
    // built, which drains the whole queue at once.
    if (m_context->getUIManager().getMainFrame() == nullptr) {
        return;
    }

    auto& docManager = m_context->getDocumentManager();
    for (const auto& file : m_pendingFiles) {
        docManager.openFile(file);
    }
    m_pendingFiles.Clear();
}

#ifdef __WXOSX__
void App::MacOpenFiles(const wxArrayString& fileNames) {
    // macOS can deliver the open-document event while OnInit is
    // still running (when Finder launches FBIde fresh by double-
    // clicking a .bas/.bi/.fbs file): the Apple event is queued
    // before m_context is built. Re-dispatch through CallAfter so
    // the actual file open runs on a later event-loop tick, by
    // which point OnInit has finished and the document manager
    // exists. When fired against an already-running instance
    // (Dock-drop, open-with on a running app), this is a one-tick
    // detour at worst.
    CallAfter([this, fileNames]() {
        if (m_context) {
            openFiles(fileNames);
        }
    });
}
#endif

void App::scheduleRestart(std::function<void()> commitConfig) {
    CallAfter([this, commit = std::move(commitConfig)]() {
        auto& dm = m_context->getDocumentManager();

        // Check for either active session, or create temporary one
        const FileSession* session = dm.getSession();
        const bool isTemporarySession = session == nullptr;
        if (isTemporarySession) {
            session = dm.startSession(wxFileName::CreateTempFileName("fbide_session"));
        }
        const wxString sessionPath = session->getPath();

        // Close session, will cause it to be saved
        dm.closeSession();

        // Now ask the user to close every document. This raises the
        // standard "Save / Don't Save / Cancel" prompt for any still-
        // modified buffer (notably untitled ones, which the snapshot
        // skipped). A Cancel here aborts the restart entirely so the
        // user keeps editing — drop the temp file we just wrote.
        if (!dm.closeAllFiles()) {
            return;
        }

        // Apply any in-memory config changes the caller wanted to
        // commit only on a confirmed restart (e.g. locale path swap).
        if (commit) {
            commit();
        }

        // Spawn the replacement asynchronously. `--wait-for-pid`
        // makes the new instance block before any config / locale
        // load until this process has actually exited — keeps the
        // handoff precise without resorting to a sleep. Replay any
        // path overrides the original launch carried so the new
        // instance lands on the same config / IDE dir (and inherits
        // verbose logging if it was on).
        const auto exe = wxStandardPaths::Get().GetExecutablePath();
        wxString cmd = wxString::Format(
            R"("%s" --new-window --wait-for-pid %d)",
            exe, static_cast<int>(wxGetProcessId())
        );

        // append (restore) session args
        if (isTemporarySession) {
            cmd += " --restore-state-from";
        }
        cmd += wxString::Format(R"( "%s")", sessionPath);

        // reload config
        if (!m_configPath.IsEmpty()) {
            cmd += wxString::Format(R"( --config "%s")", m_configPath);
        }
        if (!m_idePath.IsEmpty()) {
            cmd += wxString::Format(R"( --ide "%s")", m_idePath);
        }
        if (!m_logPath.IsEmpty()) {
            cmd += wxString::Format(R"( --log-path "%s")", m_logPath);
        }
        if (m_verbose) {
            cmd += " --verbose";
        }

        wxExecute(cmd, wxEXEC_ASYNC);

        auto* frame = m_context->getUIManager().getMainFrame();
        frame->Close(true);
        frame->Destroy();
        Exit();
    });
}

void App::showSplash() const {
    if (not m_context->getConfigManager().config().get_or("general.splashScreen", true)) {
        return;
    }

    wxImage::AddHandler(make_unowned<wxPNGHandler>());
    const auto splashPath = m_context->getConfigManager().absolute("splash.png");
    wxBitmap bmp { splashPath, wxBITMAP_TYPE_PNG };
    if (not bmp.IsOk()) {
        return;
    }

    {
        wxFontInfo fontInfo { 11 };
#if wxUSE_PRIVATE_FONTS
        // Use the bundled Arimo font (ide/) so the version looks the same
        // regardless of installed system fonts. Unavailable on wx builds
        // without private-font support (e.g. some wxGTK configs); there we
        // fall back to the default face.
        if (wxFont::AddPrivateFont(m_context->getConfigManager().absolute("Arimo.ttf"))) {
            fontInfo.FaceName("Arimo");
        }
#endif

        wxMemoryDC dc { bmp };
        dc.SetFont(fontInfo);
        dc.SetTextForeground(wxColour(210, 210, 210));
        const auto version = Version::fbide().asString();
        const auto extent = dc.GetTextExtent(version);
        constexpr int margin = 10;
        dc.DrawText(version, margin, bmp.GetHeight() - extent.GetHeight() - margin + 5);
    }

    make_unowned<wxSplashScreen>(
        bmp,
        wxSPLASH_CENTRE_ON_PARENT | wxSPLASH_TIMEOUT,
        1000, nullptr, wxID_ANY
    );
    wxYield();
}
