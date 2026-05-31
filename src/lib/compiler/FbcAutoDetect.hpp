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
/// The detection cores below are pure and static so they can be unit
/// tested without real binaries (the `--version` probe is injected). The
/// interactive `run()` driver — dialogs, PATH search, applying the
/// result — is added on top of these.
class FbcAutoDetect final {
public:
    /// Probe callback: given an fbc executable, return the first line of
    /// its `--version` output, or empty when the binary cannot be run.
    /// Injected so `detectVariants` is testable without real compilers.
    using Probe = std::function<wxString(const std::filesystem::path&)>;

    /// Extract the target architecture from an `fbc --version` line.
    /// Returns `nullopt` when neither a 32- nor 64-bit marker is present.
    [[nodiscard]] static auto parseArch(const wxString& versionLine) -> std::optional<FbcArch>;

    /// Find usable fbc variants in `folder`. Candidates are probed in the
    /// priority order `fbc64.exe`, `fbc32.exe`, `fbc.exe`; a binary is kept
    /// only when `probe` returns a non-empty version (i.e. it runs) and its
    /// architecture is not already covered — so a named variant wins over a
    /// plain `fbc.exe` of the same architecture.
    [[nodiscard]] static auto detectVariants(const std::filesystem::path& folder, const Probe& probe) -> std::vector<FbcVariant>;

    /// Build the `[compiler]` config subtree from detected variants: a
    /// canonical Default (OS-appropriate binary, generic command, hidden
    /// from the menu) plus one GUI/Console configuration pair per available
    /// architecture. The console configuration matching the canonical
    /// architecture is marked active.
    [[nodiscard]] static auto buildCompilerValue(std::span<const FbcVariant> variants, bool osIs64) -> Value;
};

#endif // __WXMSW__

} // namespace fbide
