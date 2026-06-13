//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;

/**
 * Headless `fbide format <file>` command — the command-line counterpart of
 * `FormatDialog`. Runs the same lex → transform → render pipeline against a
 * file and writes the result to stdout or an output file. No GUI.
 *
 * The transform selection mirrors the dialog's toggles (`Options`). Rendering
 * to HTML produces a complete, self-contained document.
 */
class FormatCommand final {
public:
    NO_COPY_AND_MOVE(FormatCommand)

    /// What to do, mirroring the FormatDialog toggles.
    struct Options {
        bool reIndent = false;  ///< Re-indent lines to block depth.
        bool reFormat = false;  ///< Re-flow intra-line spacing.
        bool alignPP = false;   ///< Anchor preprocessor directives to code indent (needs `reIndent`).
        bool applyCase = false; ///< Normalise keyword case per the configured rules.
        bool html = false;      ///< Render HTML instead of plain code.
        wxString outputPath;    ///< Destination file; empty → stdout.
    };

    /// Bind to the application context and the options for this run.
    FormatCommand(Context& ctx, Options options);

    /// Format `inputPath` and emit the result. Returns a process exit code
    /// (`EXIT_SUCCESS` / `EXIT_FAILURE`); failures are reported on stderr.
    [[nodiscard]] auto run(const wxString& inputPath) const -> int;

private:
    /// Write `text` to the output file, or stdout when none is set. Reports I/O
    /// failures on stderr; returns false on failure.
    [[nodiscard]] auto emit(const wxString& text) const -> bool;

    Context& m_ctx;    ///< Application context — config, theme, keywords.
    Options m_options; ///< Pinned options for this command.
};

} // namespace fbide
