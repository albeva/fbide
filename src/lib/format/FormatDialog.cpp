//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "FormatDialog.hpp"
#include "analyses/lexer/MemoryDocument.hpp"
#include "analyses/lexer/StyleLexer.hpp"
#include "analyses/lexer/StyledSource.hpp"
#include "app/Context.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "editor/Editor.hpp"
#include "editor/lexilla/FBSciLexer.hpp"
#include "renderers/HtmlRenderer.hpp"
#include "renderers/PlainTextRenderer.hpp"
#include "transformers/case/CaseTransform.hpp"
#include "transformers/reformat/ReFormatter.hpp"
#include "ui/UIManager.hpp"
using namespace fbide;

namespace {

enum ControlId {
    ID_REINDENT = wxID_HIGHEST + 1,
    ID_REFORMAT,
    ID_ALIGN_PP,
    ID_APPLY_CASE,
    ID_RENDER_CODE,
    ID_RENDER_HTML,
    ID_BROWSER,
};

} // namespace

// clang-format off
wxBEGIN_EVENT_TABLE(FormatDialog, Layout<wxDialog>)
    EVT_CHECKBOX(ID_REINDENT,               FormatDialog::onTransformChanged)
    EVT_CHECKBOX(ID_REFORMAT,               FormatDialog::onTransformChanged)
    EVT_CHECKBOX(ID_ALIGN_PP,               FormatDialog::onTransformChanged)
    EVT_CHECKBOX(ID_APPLY_CASE,             FormatDialog::onTransformChanged)
    EVT_RADIOBUTTON(ID_RENDER_CODE,         FormatDialog::renderCode)
    EVT_RADIOBUTTON(ID_RENDER_HTML,         FormatDialog::renderHtml)
    EVT_BUTTON(wxID_OK,                     FormatDialog::onApply)
    EVT_BUTTON(ID_BROWSER,                  FormatDialog::onBrowser)
wxEND_EVENT_TABLE()
// clang-format on

FormatDialog::FormatDialog(wxWindow* parent, Context& ctx, Document* doc)
: Layout(
      parent, wxID_ANY, ctx.tr("dialogs.format.title"),
      wxDefaultPosition, wxDefaultSize,
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX
  )
, m_ctx(ctx)
, m_doc(doc)
, m_buffer(doc->getEditor()->GetTextRaw()) {}

FormatDialog::~FormatDialog() = default;

void FormatDialog::create() {
    // Options bar
    hbox("Format options", { .center = true }, [&] {
        m_reindentCheck = checkBox("Re-indent", { .expand = false }, ID_REINDENT);
        m_reindentCheck->SetToolTip("Rebuild indentation from code structure (overrides any existing indent).");

        m_alignPPCheck = checkBox("Align PP", { .expand = false }, ID_ALIGN_PP);
        m_alignPPCheck->SetToolTip(
            "Pin '#' of preprocessor directives to column 0 and indent the directive name.\n"
            "Requires Re-indent."
        );

        m_reformatCheck = checkBox("Re-format", { .expand = false }, ID_REFORMAT);
        m_reformatCheck->SetToolTip(
            "Normalise spacing between tokens (one space around binary ops, no space inside parens, etc.).\n"
            "Off = preserve the original inter-token whitespace."
        );

        separator({ .space = false });

        m_applyCaseCheck = checkBox("Apply keyword case", { .expand = false }, ID_APPLY_CASE);
        m_applyCaseCheck->SetToolTip(
            "Re-case keyword tokens using the per-group rules configured in\n"
            "Settings → Keywords (None / Lower / Upper / Mixed per group)."
        );

        separator({ .space = false });

        radio("Format", { .expand = false }, ID_RENDER_CODE, wxRB_GROUP)->SetValue(true);
        radio("As HTML", { .expand = false }, ID_RENDER_HTML);
    });

    // Preview editor
    vbox("Previw", { .proportion = 1, .border = 0 }, [&] {
        m_preview = make_unowned<Editor>(currentParent(), m_ctx, nullptr, DocumentType::FreeBASIC, true);
        m_preview->SetReadOnly(true);
        m_preview->SetMinSize(wxSize(-1, 200));
        add(m_preview, { .proportion = 1 });
    });

    // Buttons
    hbox({ .border = 0 }, [&] {
        m_browserBtn = button("Open in Browser", { .expand = false }, ID_BROWSER);

        currentSizer()->AddStretchSpacer();

        m_actionBtn = button("Apply", { .expand = false }, wxID_OK);
        button("Cancel", { .expand = false }, wxID_CANCEL);
    });
    spacer();
    SetSizerAndFit(currentSizer());
    Centre();

    // Tokenise source once via FBSciLexer + StyleLexer over a headless
    // MemoryDocument. Same colouring rules as the editor — single source
    // of truth for FB lexing.
    if (m_buffer.length() > 0) {
        MemoryDocument doc;
        doc.Set(std::string_view { m_buffer.data(), m_buffer.length() });
        auto* fb = FBSciLexer::Create();
        lexer::configureFbWordlists(*fb, m_ctx.getConfigManager().keywords().at("groups"));
        fb->Lex(0, doc.Length(), +ThemeCategory::Default, &doc);
        lexer::MemoryDocStyledSource src(doc);
        lexer::StyleLexer adapter(src);
        m_tokens = adapter.tokenise();
        fb->Release();
    }

    m_renderer = std::make_unique<PlainTextRenderer>(m_buffer.length());
    updatePreview();
}

void FormatDialog::onTransformChanged(wxCommandEvent&) {
    rebuildTransforms();
    updatePreview();
}

void FormatDialog::renderCode(wxCommandEvent&) {
    m_renderer = std::make_unique<PlainTextRenderer>(m_buffer.length());
    updatePreview();
}

void FormatDialog::renderHtml(wxCommandEvent&) {
    m_renderer = std::make_unique<HtmlRenderer>(m_ctx.getTheme(), m_buffer.length());
    updatePreview();
}

void FormatDialog::onApply(wxCommandEvent&) {
    const auto rendered = m_preview->GetText();
    if (rendered.IsEmpty()) {
        return;
    }

    if (m_renderer->getType() == DocumentType::FreeBASIC) {
        auto* editor = m_doc->getEditor();
        editor->BeginUndoAction();
        editor->SetText(rendered);
        editor->EndUndoAction();
    } else if (!m_tokens.empty()) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(make_unowned<wxTextDataObject>(rendered));
            wxTheClipboard->Close();
        }
    }

    EndModal(wxID_OK);
}

void FormatDialog::onBrowser(wxCommandEvent&) {
    if (m_renderer->getType() != DocumentType::HTML) {
        return;
    }

    const auto rendered = m_preview->GetText();
    if (rendered.IsEmpty()) {
        return;
    }

    const auto tmpFile = wxFileName::CreateTempFileName("fbide_html") + ".html";
    wxFile file(tmpFile, wxFile::write);
    if (file.IsOpened()) {
        file.Write(HtmlRenderer::decorate(rendered));
        file.Close();
        wxLaunchDefaultBrowser("file:///" + tmpFile);
    }
}

void FormatDialog::rebuildTransforms() {
    m_transforms.clear();

    if (m_applyCaseCheck->IsChecked()) {
        std::array<CaseMode, kThemeKeywordGroupsCount> cases {};
        const auto& cfg = m_ctx.getConfigManager().keywords().at("cases");
        for (std::size_t idx = 0; idx < kThemeKeywordCategories.size(); idx++) {
            const auto key = wxString(getThemeCategoryName(kThemeKeywordCategories[idx]));
            cases[idx] = CaseMode::parse(cfg.get_or(key, "None").ToStdString())
                             .value_or(CaseMode::None);
        }
        m_transforms.push_back(std::make_unique<CaseTransform>(cases));
    }

    const bool reIndent = m_reindentCheck->IsChecked();
    const bool reFormat = m_reformatCheck->IsChecked();
    if (reIndent || reFormat) {
        m_transforms.push_back(std::make_unique<reformat::ReFormatter>(reformat::FormatOptions {
            .tabSize = static_cast<std::size_t>(m_ctx.getConfigManager().config().get_or("editor.tabSize", 4)),
            .anchoredPP = reIndent && m_alignPPCheck->IsChecked(),
            .reIndent = reIndent,
            .reFormat = reFormat,
        }));
    }
}

auto FormatDialog::isTransforming() const -> bool {
    return !m_transforms.empty() || m_renderer->getType() != DocumentType::FreeBASIC;
}

void FormatDialog::updateButtons() {
    const auto docTy = m_renderer->getType();
    if (docTy != DocumentType::FreeBASIC) {
        m_actionBtn->SetLabel("Copy");
    } else {
        m_actionBtn->SetLabel("Apply");
    }
    m_actionBtn->Enable(isTransforming());

    const auto show = docTy == DocumentType::HTML;
    if (show != m_browserBtn->IsShown()) {
        m_browserBtn->Show(show);
        if (auto* sizer = m_browserBtn->GetContainingSizer()) {
            sizer->Layout();
        }
    }

    // Align PP is only meaningful when Re-indent is active.
    m_alignPPCheck->Enable(m_reindentCheck->IsChecked());
}

void FormatDialog::updatePreview() {
    updateButtons();

    if (m_tokens.empty()) {
        return;
    }

    const int firstVisible = m_preview->GetFirstVisibleLine();
    const int xOffset = m_preview->GetXOffset();
    const int caretPos = m_preview->GetCurrentPos();

    const auto thaw = FreezeLock(this);
    m_preview->SetReadOnly(false);
    m_preview->setDocType(m_renderer->getType());
    if (isTransforming()) {
        auto tokens = m_tokens;
        for (const auto& transform : m_transforms) {
            tokens = transform->apply(tokens);
        }
        m_preview->SetText(m_renderer->render(tokens));
    } else {
        m_preview->SetTextRaw(m_buffer);
    }
    m_preview->SetReadOnly(true);

    const int lastPos = static_cast<int>(m_preview->GetLastPosition());
    m_preview->SetEmptySelection(std::min(caretPos, lastPos));
    const int lineCount = m_preview->GetLineCount();
    m_preview->SetFirstVisibleLine(std::min(firstVisible, std::max(0, lineCount - 1)));
    m_preview->SetXOffset(xOffset);
}
