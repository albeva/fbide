//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "App.hpp"
#include "Context.hpp"
#include "InstanceHandler.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "config/FileHistory.hpp"
#include "config/Value.hpp"
#include "config/Version.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "document/FileSession.hpp"
#include "ui/UIManager.hpp"
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
  --load-session <p>  Load the .fbs session at <p> on startup, then delete it.
                      Used internally for the language-change restart flow.
  --wait-for-pid <id> Block startup (before any config is loaded) until the
                      process with id <id> has exited. Used internally so the
                      replacement process spawned by a language-change
                      restart only opens after the previous one is gone.
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
void writeLineTo(const wxString& text, bool toStderr) {
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
    return wxApp::OnExit();
}

void App::initAppearance() {
    // Enable light/dark mode appearance
    // EXTREMELY buggy on windows, not reccomended enabling this
    const auto appearance = m_context->getConfigManager().config().get_or("appearance", "").Lower();
    if (appearance == "dark") {
        SetAppearance(Appearance::Dark);
    } else if (appearance == "light") {
        SetAppearance(Appearance::Light);
    } else if (appearance == "system") {
        SetAppearance(Appearance::System);
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
        for (int waited = 0;
             waited < maxWaitMs && wxProcess::Exists(static_cast<int>(cli.waitForPid));
             waited += pollMs) {
            wxMilliSleep(pollMs);
        }
    }

    const auto fbidePath = getFbidePath();

#if FBIDE_DEBUG_BUILD
    wxLog::SetVerbose(true);
    wxLogWindow* logWindow = make_unowned<wxLogWindow>(nullptr, "Debug Log", false, false);
    wxLog::SetActiveTarget(logWindow);
    if (cli.cfgKey.IsEmpty()) {
        logWindow->Show();
    }
#else
    wxLog::SetActiveTarget(new wxLogStream(new std::ofstream((fbidePath / "app.log").ToStdString(), std::ios::app)));
#endif

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

    showSplash();
    initAppearance();

    const auto& configManager = m_context->getConfigManager();
    m_context->getFileHistory().load(configManager.getIdeDir() / "history.ini");

    m_context->getUIManager().createMainFrame();
    openFiles(cli.files);
    if (!cli.loadSession.IsEmpty()) {
        m_context->getFileSession().load(cli.loadSession);
        wxRemoveFile(cli.loadSession);
    }
    m_context->getCompilerManager().checkCompilerOnStartup();
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
        if (arg.StartsWith("--cfg=")) {
            opts.cfgKey = arg.Mid(6);
            if (opts.cfgKey.IsEmpty()) {
                writeErrLine("fbide: --cfg= requires a key");
                opts.parseFailed = true;
                return opts;
            }
            continue;
        }
        if (arg == "--load-session") {
            index += 1;
            if (index >= args.GetCount()) {
                writeErrLine("fbide: --load-session requires a path argument");
                opts.parseFailed = true;
                return opts;
            }
            opts.loadSession = args[index];
            continue;
        }
        if (arg == "--wait-for-pid") {
            index += 1;
            if (index >= args.GetCount()) {
                writeErrLine("fbide: --wait-for-pid requires a numeric pid argument");
                opts.parseFailed = true;
                return opts;
            }
            if (!args[index].ToLong(&opts.waitForPid) || opts.waitForPid <= 0) {
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

void App::showHelp() const {
    writeLine(kHelpText);
}

void App::showVersion() const {
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

    auto& root = m_context->getConfigManager().get(cat);
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
    auto& docManager = m_context->getDocumentManager();
    for (const auto& file : files) {
        docManager.openFile(file);
    }
}

void App::scheduleRestart(std::function<void()> commitConfig) {
    CallAfter([this, commit = std::move(commitConfig)]() {
        // Save the open documents to a temp session that the
        // replacement process will load via `--load-session`. If the
        // user cancels an in-flight save dialog, FileSession returns
        // false and we abort — locale (or any other deferred config
        // change) stays unchanged.
        const auto sessionPath = m_context->getConfigManager().getIdeDir() / "restart-session.fbs";
        if (!m_context->getFileSession().save(sessionPath)) {
            return;
        }

        // Apply any in-memory config changes the caller wanted to
        // commit only on a confirmed restart.
        if (commit) {
            commit();
        }

        // Spawn the replacement asynchronously. `--wait-for-pid`
        // makes the new instance block before any config / locale
        // load until this process has actually exited — keeps the
        // handoff precise without resorting to a sleep.
        const auto exe = wxStandardPaths::Get().GetExecutablePath();
        wxExecute(
            wxString::Format(
                R"("%s" --new-window --wait-for-pid %lu --load-session "%s")",
                exe, wxGetProcessId(), sessionPath
            ),
            wxEXEC_ASYNC
        );

        // Trigger the normal close path. Mark documents not-modified
        // so `prepareToQuit` doesn't re-prompt — FileSession already
        // covered them. `UIManager::onClose` persists window
        // geometry / config / file history on its way out.
        for (const auto& doc : m_context->getDocumentManager().getDocuments()) {
            doc->setModified(false);
        }
        m_context->getUIManager().getMainFrame()->Close(true);
    });
}

void App::showSplash() {
    if (m_context->getConfigManager().config().get_or("general.splashScreen", true)) {
        wxImage::AddHandler(make_unowned<wxPNGHandler>());
        const auto splashPath = m_context->getConfigManager().absolute("splash.png");
        if (const wxBitmap bmp(splashPath, wxBITMAP_TYPE_PNG); bmp.IsOk()) {
            make_unowned<wxSplashScreen>(
                bmp,
                wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT,
                1000, nullptr, wxID_ANY
            );
            wxYield();
        }
    }
}
