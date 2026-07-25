//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"
#include "config/Value.hpp"

namespace fbide {

class Context;
class AsyncProcess;

#ifdef __WXMSW__

/// Target architecture of an `fbc` build. Windows-only feature, so the
/// only two cases are the 32- and 64-bit Windows targets.
enum class FbcArch : std::uint8_t { Win32, Win64 };

/// A usable `fbc` executable plus the architecture it targets.
struct FbcVariant final {
    std::filesystem::path exe; ///< Absolute path to the fbc binary.
    FbcArch arch;              ///< Target architecture (from file name or `--version`).
};

/// Auto-detection of an installed FreeBASIC compiler (Windows only).
///
/// `parseArch` and `buildCompilerValue` are pure and static so they can be
/// unit tested without real binaries; `run()` drives the interactive flow
/// (dialogs, PATH search, probing fbc, applying the result) on top of them.
class FbcAutoDetect final {
public:
    NO_COPY_AND_MOVE(FbcAutoDetect)

    /// Construct against the application context (config + locale).
    explicit FbcAutoDetect(Context& ctx);
    ~FbcAutoDetect() = default;

    /// Run the full interactive detection flow: confirm overwrite when the
    /// current compiler settings differ from the shipped defaults, locate
    /// fbc (system PATH with a browse option, or a folder picker), detect
    /// its variants, and build the `[compiler]` subtree to install. Returns
    /// `nullopt` when the user cancels, or when no valid compiler is found
    /// (an error message is shown in that case).
    auto run(wxWindow* parent) -> std::optional<Value>;

    /// Extract the target architecture from an `fbc --version` line.
    /// Returns `nullopt` when neither a 32- nor 64-bit marker is present.
    [[nodiscard]] static auto parseArch(const wxString& versionLine) -> std::optional<FbcArch>;

    /// Build the `[compiler]` config subtree from detected variants: a
    /// canonical Default (OS-appropriate binary, generic command, hidden
    /// from the menu) plus one GUI/Console configuration pair per available
    /// architecture. The console configuration matching the canonical
    /// architecture is marked active.
    [[nodiscard]] static auto buildCompilerValue(std::span<const FbcVariant> variants, bool osIs64) -> Value;

    /// Silent (no-UI) first-run detection. Probes `exeDir` (where the
    /// Windows installer bundles fbc next to fbide.exe) first, then the
    /// system PATH. Returns the `[compiler]` subtree built from the first
    /// folder that yields usable variants, or `nullopt` when none are
    /// found. No dialogs — distinct from `run()`, which is interactive.
    [[nodiscard]] static auto detectSilently(const std::filesystem::path& exeDir, bool osIs64) -> std::optional<Value>;

private:
    Context& m_ctx;
};

/// Async, non-blocking first-run fbc detection. Probes candidate `fbc`
/// binaries via `AsyncProcess` (`--version`) instead of a blocking
/// `wxExecute`, so startup never spins a nested event loop (issue #127).
/// Chained: one probe in flight at a time, in the same priority order as
/// `FbcAutoDetect::detectSilently` — the first folder (exe dir, then PATH)
/// that yields usable variants wins. `onDone` fires exactly once, on the UI
/// thread, with the built `[compiler]` subtree or `nullopt`.
class FbcAsyncDetect final {
public:
    NO_COPY_AND_MOVE(FbcAsyncDetect)

    using OnDone = std::function<void(std::optional<Value>)>;

    FbcAsyncDetect(std::filesystem::path exeDir, bool osIs64, OnDone onDone);
    ~FbcAsyncDetect();

    /// Begin detection. Returns immediately; `onDone` runs later (or, when
    /// there is nothing to probe, on the next event-loop tick).
    void run();

    /// Sever the completion callback so a late probe can't touch a freed
    /// owner. The in-flight probe still self-completes and is detached.
    void detach();

private:
    struct Candidate final {
        std::filesystem::path exe;
        std::optional<FbcArch> archHint; ///< Known from the file name; nullopt for plain fbc.exe.
    };

    void beginFolder();
    void probeNextCandidate();
    void onProbed(const wxString& versionLine);
    void finish(std::optional<Value> result);

    std::vector<std::filesystem::path> m_folders; ///< Exe dir then PATH hit.
    std::size_t m_folderIdx = 0;
    std::vector<Candidate> m_candidates; ///< Existing candidates in the current folder.
    std::size_t m_candIdx = 0;
    std::vector<FbcVariant> m_variants; ///< Usable variants accumulated for the current folder.
    bool m_osIs64;
    OnDone m_onDone;
    AsyncProcess* m_process = nullptr; ///< In-flight `--version` probe (detached on teardown).
};

#endif // __WXMSW__

} // namespace fbide
