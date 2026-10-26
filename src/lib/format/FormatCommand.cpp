//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FormatCommand.hpp"
#include "FormatCheck.hpp"
#include "FormatIO.hpp"
#include "FormatSettings.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/ThemeCategory.hpp"
#include "document/DocumentIO.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include "renderers/HtmlRenderer.hpp"
#include "renderers/PlainTextRenderer.hpp"
#include "transformers/Transform.hpp"
#include "utils/ConsoleOutput.hpp"
#include "utils/PathConversions.hpp"
using namespace fbide;

FormatCommand::FormatCommand(Context& ctx, Options options)
: m_ctx(ctx)
, m_options(std::move(options)) {}

auto FormatCommand::run(const wxString& inputPath) const -> int {
    // Decode the file exactly as the editor does when opening it (BOM, UTF-8
    // check, configured default, Latin-1), or in the forced `--encoding`.
    const auto path = toFsPath(inputPath);
    const auto loaded = m_options.encoding.has_value()
                          ? DocumentIO::loadWithEncoding(path, *m_options.encoding, EolMode::LF)
                          : DocumentIO::load(path, defaultEncoding(), EolMode::LF);
    if (!loaded.has_value()) {
        ConsoleOutput::writeError(wxString::Format("fbide: cannot open '%s'", inputPath));
        return EXIT_FAILURE;
    }
    // The FB lexer consumes UTF-8 bytes, the same way the editor feeds it
    // `GetTextRaw`.
    const wxScopedCharBuffer utf8 = loaded->text.utf8_str();
    const std::string source { utf8.data(), utf8.length() };

    // The lexer classifies against the shared FB keyword tables — normally seeded
    // in OnInit once the frame exists; do it here for the headless path.
    lexer::setFbKeywords(m_ctx.getConfigManager().keywords().at("groups"));

    // Lex via FBSciLexer + StyleLexer over a headless MemoryDocument — the same
    // colouring the editor uses.
    MemoryDocument doc;
    doc.Set(std::string_view { source.data(), source.size() });
    auto* fb = FBSciLexer::Create();
    fb->Lex(0, doc.Length(), +ThemeCategory::Default, &doc);
    lexer::MemoryDocStyledSource styled(doc);
    lexer::StyleLexer adapter(styled);
    const auto tokens = adapter.tokenise();
    fb->Release();

    // Build the transform chain from the options — shared with FormatDialog and
    // the Reformat command so the three stay in lock-step.
    const auto transforms = buildTransforms(m_ctx, FormatSettings {
        .reIndent = m_options.reIndent,
        .reFormat = m_options.reFormat,
        .alignPP = m_options.alignPP,
        .applyCase = m_options.applyCase,
    });

    // Apply in order — each transform feeds the next.
    std::vector<lexer::Token> buffer;
    const std::vector<lexer::Token>* result = &tokens;
    for (const auto& transform : transforms) {
        buffer = transform->apply(*result);
        result = &buffer;
    }

    // Refuse to emit a result that lost or changed code — a silent empty or
    // mangled output is worse than a clear failure.
    std::string text;
    for (const auto& tok : *result) {
        text += tok.text;
    }
    if (!isSafeFormatResult(source, text)) {
        ConsoleOutput::writeError(wxString::Format("fbide: formatting failed: %s", inputPath));
        return EXIT_FAILURE;
    }

    // HTML declares `<meta charset="utf-8">`, so it is always UTF-8.
    if (m_options.html) {
        const HtmlRenderer renderer(m_ctx.getTheme(), source.size());
        const auto html = HtmlRenderer::decorate(renderer.render(*result));
        const wxScopedCharBuffer htmlUtf8 = html.utf8_str();
        return emit({ htmlUtf8.data(), htmlUtf8.length() }) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // Plain code is written back in the input's encoding, BOM included — the
    // bytes the editor would save.
    const auto encoded = encodeWithBom(wxString::FromUTF8(text), loaded->encoding);
    if (!encoded.has_value()) {
        ConsoleOutput::writeError(wxString::Format(
            "fbide: cannot encode output as %s: %s", wxString(loaded->encoding.toString().data()), inputPath
        ));
        return EXIT_FAILURE;
    }
    return emit(*encoded) ? EXIT_SUCCESS : EXIT_FAILURE;
}

auto FormatCommand::defaultEncoding() const -> TextEncoding {
    // Same lookup as DocumentManager::defaultEncoding — the editor's
    // configured fallback for files that are neither BOM-marked nor UTF-8.
    const auto key = m_ctx.getConfigManager().config().at("editor").get_or("encoding", "UTF-8");
    return TextEncoding::parse(key.ToStdString()).value_or(TextEncoding::UTF8);
}

auto FormatCommand::emit(const std::string_view bytes) const -> bool {
    if (m_options.outputPath.IsEmpty()) {
        ConsoleOutput::writeRaw(bytes);
        return true;
    }
    wxFFile out(m_options.outputPath, "wb");
    if (!out.IsOpened()) {
        ConsoleOutput::writeError(wxString::Format("fbide: cannot write '%s'", m_options.outputPath));
        return false;
    }
    if (!bytes.empty() && out.Write(bytes.data(), bytes.size()) != bytes.size()) {
        ConsoleOutput::writeError(wxString::Format("fbide: failed to write '%s'", m_options.outputPath));
        return false;
    }
    return true;
}
