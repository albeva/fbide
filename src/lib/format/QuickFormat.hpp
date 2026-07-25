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
class Editor;

/// The "Reformat" command's engine — reformats the active editor in place using
/// the persisted `FormatSettings`. The GUI counterpart of the headless
/// `FormatCommand`: no dialog, no preview, applied straight to the buffer.
///
/// With a non-empty selection only the spanned lines are reformatted, kept at
/// the first line's current indentation (so a nested fragment is not flattened
/// to column 0); otherwise the whole document is reformatted. One undo step.
class QuickFormat final {
public:
    NO_COPY_AND_MOVE(QuickFormat)

    explicit QuickFormat(Context& ctx)
    : m_ctx(ctx) {}

    /// Reformat `editor`. No-op on an empty buffer, when every toggle is off, or
    /// when the selection spans no tokens.
    void run(Editor& editor) const;

private:
    Context& m_ctx; ///< Application context — config, theme, keywords.
};

} // namespace fbide
