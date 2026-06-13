//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FormatCommand.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "config/ThemeCategory.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include "renderers/HtmlRenderer.hpp"
#include "renderers/PlainTextRenderer.hpp"
#include "transformers/Transform.hpp"
#include "transformers/case/CaseTransform.hpp"
#include "transformers/reformat/ReFormatter.hpp"
#include "utils/ConsoleOutput.hpp"
using namespace fbide;

FormatCommand::FormatCommand(Context& ctx, Options options)
: m_ctx(ctx)
, m_options(std::move(options)) {}

auto FormatCommand::run(const wxString& inputPath) const -> int {
    // Read the source as raw UTF-8 bytes — the FB lexer consumes bytes the same
    // way the editor feeds it `GetTextRaw`.
    wxFFile file(inputPath, "rb");
    if (!file.IsOpened()) {
        ConsoleOutput::writeError(wxString::Format("fbide: cannot open '%s'", inputPath));
        return EXIT_FAILURE;
    }
    std::string source(static_cast<std::size_t>(file.Length()), '\0');
    if (!source.empty() && file.Read(source.data(), source.size()) != source.size()) {
        ConsoleOutput::writeError(wxString::Format("fbide: failed to read '%s'", inputPath));
        return EXIT_FAILURE;
    }

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

    // Build the transform chain from the options (mirrors FormatDialog).
    std::vector<std::unique_ptr<Transform>> transforms;
    if (m_options.applyCase) {
        std::array<CaseMode, kThemeKeywordGroupsCount> cases {};
        const auto& cfg = m_ctx.getConfigManager().keywords().at("cases");
        for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
            const auto key = wxString(getThemeCategoryName(kThemeKeywordCategories[idx]));
            cases[idx] = CaseMode::parse(cfg.get_or(key, "None").ToStdString()).value_or(CaseMode::None);
        }
        transforms.push_back(std::make_unique<CaseTransform>(cases));
    }
    if (m_options.reIndent || m_options.reFormat) {
        transforms.push_back(std::make_unique<reformat::ReFormatter>(reformat::FormatOptions {
            .tabSize = static_cast<std::size_t>(m_ctx.getConfigManager().config().get_or("editor.tabSize", 4)),
            .anchoredPP = m_options.reIndent && m_options.alignPP,
            .reIndent = m_options.reIndent,
            .reFormat = m_options.reFormat,
        }));
    }

    // Apply in order — each transform feeds the next.
    std::vector<lexer::Token> buffer;
    const std::vector<lexer::Token>* result = &tokens;
    for (const auto& transform : transforms) {
        buffer = transform->apply(*result);
        result = &buffer;
    }

    // Render to plain code or a complete HTML document.
    if (m_options.html) {
        const HtmlRenderer renderer(m_ctx.getTheme(), source.size());
        return emit(HtmlRenderer::decorate(renderer.render(*result))) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    const PlainTextRenderer renderer(source.size());
    return emit(renderer.render(*result)) ? EXIT_SUCCESS : EXIT_FAILURE;
}

auto FormatCommand::emit(const wxString& text) const -> bool {
    if (m_options.outputPath.IsEmpty()) {
        ConsoleOutput::write(text);
        return true;
    }
    wxFFile out(m_options.outputPath, "wb");
    if (!out.IsOpened()) {
        ConsoleOutput::writeError(wxString::Format("fbide: cannot write '%s'", m_options.outputPath));
        return false;
    }
    const auto utf8 = text.ToStdString(wxConvUTF8);
    if (!utf8.empty() && out.Write(utf8.data(), utf8.size()) != utf8.size()) {
        ConsoleOutput::writeError(wxString::Format("fbide: failed to write '%s'", m_options.outputPath));
        return false;
    }
    return true;
}
