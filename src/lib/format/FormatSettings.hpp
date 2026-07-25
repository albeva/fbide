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
class ConfigManager;
class Transform;

/// The persistable subset of the format options — the `FormatDialog` toggles
/// minus the output-format (code vs HTML) and file I/O, which are per-invocation
/// concerns. Stored in the `[format]` config section so the `Reformat` command
/// and headless `fbide format` reuse whatever was last picked in the dialog.
struct FormatSettings {
    bool reIndent = true;   ///< Re-indent lines to block depth.
    bool reFormat = true;   ///< Re-flow intra-line spacing + blank-line policy.
    bool alignPP = false;   ///< Anchor preprocessor directives to column 0 (needs `reIndent`).
    bool applyCase = false; ///< Normalise keyword case per the configured rules.

    /// Read from the `[format]` config section, falling back to the field
    /// defaults above for any key that is not present.
    [[nodiscard]] static auto load(ConfigManager& config) -> FormatSettings;

    /// Write into the `[format]` config section and flush to disk.
    void save(ConfigManager& config) const;
};

/// Build the transform chain (keyword-case + reformat) for `settings`. Shared by
/// the dialog preview, the `Reformat` command and headless `fbide format`, so
/// the three stay in lock-step. `baseIndent` seeds the reformat indent level —
/// 0 for a whole document; the enclosing block's indent when reformatting a
/// selection, so the fragment is not flattened to column 0.
[[nodiscard]] auto buildTransforms(Context& ctx, const FormatSettings& settings, std::size_t baseIndent = 0)
    -> std::vector<std::unique_ptr<Transform>>;

} // namespace fbide
