//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "QuickFormat.hpp"
#include "FormatSettings.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "analyses/lexer/Token.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/Editor.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include "renderers/PlainTextRenderer.hpp"
#include "transformers/Transform.hpp"
using namespace fbide;

namespace {

/// Run the token stream through the transform chain and render to plain text.
auto applyChain(
    const std::vector<std::unique_ptr<Transform>>& transforms,
    const std::vector<lexer::Token>& tokens,
    const PlainTextRenderer& renderer
) -> wxString {
    std::vector<lexer::Token> buffer;
    const std::vector<lexer::Token>* result = &tokens;
    for (const auto& transform : transforms) {
        buffer = transform->apply(*result);
        result = &buffer;
    }
    return renderer.render(*result);
}

} // namespace

void QuickFormat::run(Editor& editor) const {
    const wxCharBuffer source = editor.GetTextRaw();
    if (source.length() == 0) {
        return;
    }

    const auto settings = FormatSettings::load(m_ctx.getConfigManager());
    if (!settings.reIndent && !settings.reFormat && !settings.applyCase) {
        return; // every toggle off — nothing to do
    }

    // Lex the whole buffer — same colouring the editor uses. Token positions are
    // Scintilla document offsets (UTF-8 bytes), matching the selection positions.
    MemoryDocument doc;
    doc.Set(std::string_view { source.data(), source.length() });
    auto* fb = FBSciLexer::Create();
    fb->Lex(0, doc.Length(), +ThemeCategory::Default, &doc);
    lexer::MemoryDocStyledSource styled(doc);
    lexer::StyleLexer adapter(styled);
    const auto tokens = adapter.tokenise();
    fb->Release();
    if (tokens.empty()) {
        return;
    }

    const PlainTextRenderer renderer(source.length());
    const int selStart = editor.GetSelectionStart();
    const int selEnd = editor.GetSelectionEnd();

    // No selection → reformat the whole document, replacing all text.
    if (selStart == selEnd) {
        const auto transforms = buildTransforms(m_ctx, settings);
        const wxString formatted = applyChain(transforms, tokens, renderer);
        editor.BeginUndoAction();
        editor.SetText(formatted);
        editor.EndUndoAction();
        return;
    }

    // Selection → reformat only the lines it spans. Expand to whole lines: a
    // selection ending exactly at a line start does not include that line.
    const int firstLine = editor.LineFromPosition(selStart);
    int lastLine = editor.LineFromPosition(selEnd);
    if (lastLine > firstLine && editor.GetColumn(selEnd) == 0) {
        lastLine--;
    }
    const int rangeStart = editor.PositionFromLine(firstLine);
    const int rangeEnd = editor.GetLineEndPosition(lastLine);

    // Base indent = the first line's current indentation, in whole levels, so the
    // reformatted fragment stays at its surrounding depth rather than column 0.
    const auto tabSize =m_ctx.getConfigManager().config().get_or("editor.tabSize", 4);
    const std::size_t baseIndent = tabSize > 0
        ? static_cast<std::size_t>((editor.GetLineIndentation(firstLine) + (tabSize / 2)) / tabSize)
        : 0;

    // Slice out the tokens inside the line range (the trailing newline at
    // rangeEnd is excluded — the surrounding text keeps its own line breaks).
    std::vector<lexer::Token> slice;
    for (const auto& tok : tokens) {
        if (tok.pos >= rangeStart && tok.pos < rangeEnd) {
            slice.push_back(tok);
        }
    }
    if (slice.empty()) {
        return;
    }

    const auto transforms = buildTransforms(m_ctx, settings, baseIndent);
    wxString fragment = applyChain(transforms, slice, renderer);
    // The renderer terminates the last statement with a newline; the replaced
    // range stops at the line's end, so drop it to avoid an inserted blank line.
    if (fragment.EndsWith("\n")) {
        fragment.RemoveLast();
    }

    editor.BeginUndoAction();
    editor.SetSelection(rangeStart, rangeEnd);
    editor.ReplaceSelection(fragment);
    editor.EndUndoAction();
}
