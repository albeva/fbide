//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#include "CompilerPage.hpp"
#include "app/Context.hpp"
#include "compiler/CompilerManager.hpp"
#include "config/ConfigManager.hpp"
#include "document/Document.hpp"
#include "document/DocumentManager.hpp"
#include "document/DocumentPath.hpp"
#include "help/HelpManager.hpp"
using namespace fbide;

CompilerPage::CompilerPage(Context& ctx, wxWindow* parent)
: Panel(ctx, wxID_ANY, parent) {
    const auto& cfg = getContext().getConfigManager().config();
    const auto& compiler = cfg.at("compiler");
    m_compilerPath = compiler.get_or("path", "");
    m_compileCommand = compiler.get_or("compileCommand", "");
    m_runCommand = compiler.get_or("runCommand", "");
#ifdef __WXMSW__
    m_helpFile = cfg.get_or("paths.helpFile", "");
#endif
}

void CompilerPage::create() {
    vbox(tr("dialogs.settings.compiler.compilerAndPaths"), { .proportion = 1, .border = 0 }, [&] {
        compilerPath();
        spacer();
        compilerCommand();
        spacer();
        runCommand();
#ifdef __WXMSW__
        spacer();
        helpFile();
#endif
        spacer();
        placeholderTable();
    });
    SetSizerAndFit(currentSizer());
}

void CompilerPage::apply() {
    auto& cfg = getContext().getConfigManager().config();
    auto& compiler = cfg["compiler"];
    compiler["compileCommand"] = m_compileCommand;
    compiler["runCommand"] = m_runCommand;
#ifdef __WXMSW__
    cfg["paths"]["helpFile"] = m_helpFile;
#endif
    const wxString existing = compiler.get_or("path", "");
    if (m_compilerPath != existing) {
        compiler["path"] = m_compilerPath;
        getContext().getCompilerManager().resetFbcVersion();
    }
}

void CompilerPage::compilerPath() {
    const auto [tf, btn] = makeFileEntry(m_compilerPath, tr("dialogs.settings.compiler.compilerPath"));
    m_compilerPathField = tf;
    tf->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& evt) {
        setPlaceholderVisible(false);
        evt.Skip();
    });
    btn->Bind(wxEVT_BUTTON, [&, tf](wxCommandEvent&) {
        wxFileDialog dlg(
            this, "Select compiler", "", "",
            getContext().getConfigManager().filePatterns({ "compiler", "all" }),
            wxFD_FILE_MUST_EXIST
        );
        if (dlg.ShowModal() == wxID_OK) {
            m_compilerPath = getContext().getConfigManager().relative(dlg.GetPath());
        }
        tf->SetValue(m_compilerPath);
    });
}

void CompilerPage::focusCompilerPath() {
    if (m_compilerPathField != nullptr) {
        m_compilerPathField->SetFocus();
        m_compilerPathField->SelectAll();
    }
}

void CompilerPage::compilerCommand() {
    m_compileCommandField = makeEntryField(m_compileCommand, tr("dialogs.settings.compiler.compilerCommand"));
    m_compileCommandField->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& evt) {
        m_lastFocused = m_compileCommandField;
        setPlaceholderVisible(true);
        refreshPlaceholders();
        evt.Skip();
    });
}

void CompilerPage::runCommand() {
    m_runCommandField = makeEntryField(m_runCommand, tr("dialogs.settings.compiler.runCommand"));
    m_runCommandField->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& evt) {
        m_lastFocused = m_runCommandField;
        setPlaceholderVisible(true);
        refreshPlaceholders();
        evt.Skip();
    });
}

#ifdef __WXMSW__
void CompilerPage::helpFile() {
    const auto [tf, btn] = makeFileEntry(m_helpFile, tr("dialogs.settings.compiler.helpFile"));
    tf->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& evt) {
        setPlaceholderVisible(false);
        evt.Skip();
    });
    btn->Bind(wxEVT_BUTTON, [&, tf](wxCommandEvent&) {
        wxFileDialog dlg(
            this, "Select help file", "", "",
            getContext().getConfigManager().filePattern("help"),
            wxFD_FILE_MUST_EXIST
        );
        if (dlg.ShowModal() == wxID_OK) {
            m_helpFile = getContext().getConfigManager().relative(dlg.GetPath());
            HelpManager::verifyHelpFileAccessible(this, m_helpFile);
        }
        tf->SetValue(m_helpFile);
    });
}
#endif

void CompilerPage::placeholderTable() {
    const auto trOr = [this](const wxString& key, const wxString& fallback) {
        const auto val = tr(key);
        return val.empty() ? fallback : val;
    };

    m_placeholderTitle = text(trOr("dialogs.settings.compiler.placeholders.title", "Placeholders (click to insert)"), {});
    const auto list = make_unowned<wxListCtrl>(
        currentParent(), wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL
    );
    list->AppendColumn(trOr("dialogs.settings.compiler.placeholders.placeholder", "Placeholder"), wxLIST_FORMAT_LEFT, 120);
    list->AppendColumn(trOr("dialogs.settings.compiler.placeholders.expansion", "Expansion"), wxLIST_FORMAT_LEFT, 400);
    list->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& evt) {
        int flags = 0;
        const long item = m_placeholderList->HitTest(evt.GetPosition(), flags);
        if (item != wxNOT_FOUND && (flags & wxLIST_HITTEST_ONITEM) != 0) {
            wxListItem info;
            info.SetId(item);
            info.SetColumn(0);
            info.SetMask(wxLIST_MASK_TEXT);
            m_placeholderList->GetItem(info);
            insertPlaceholder(info.GetText());
            return; // do not Skip — default handler would steal focus to the list
        }
        evt.Skip();
    });
    list->Bind(wxEVT_SIZE, [list](wxSizeEvent& evt) {
        const int total = list->GetClientSize().GetWidth();
        const int col0 = list->GetColumnWidth(0);
        constexpr int padding = 4;
        const int col1 = std::max(100, total - col0 - padding);
        list->SetColumnWidth(1, col1);
        evt.Skip();
    });
    add(list, { .proportion = 1 });
    m_placeholderList = list;
    m_lastFocused = m_runCommandField;
    refreshPlaceholders();
}

void CompilerPage::setPlaceholderVisible(const bool visible) {
    if (m_placeholderList == nullptr || m_placeholderTitle == nullptr) {
        return;
    }
    if (m_placeholderList->IsShown() == visible) {
        return;
    }
    m_placeholderTitle->Show(visible);
    m_placeholderList->Show(visible);
    Layout();
}

void CompilerPage::refreshPlaceholders() {
    if (m_placeholderList == nullptr) {
        return;
    }
    m_placeholderList->DeleteAllItems();

    auto append = [this](const wxString& key, const wxString& value) {
        const auto idx = m_placeholderList->InsertItem(m_placeholderList->GetItemCount(), key);
        m_placeholderList->SetItem(idx, 1, value);
    };

    const wxString source = getSampleSourcePath();
    const bool isRunContext = (m_lastFocused == m_runCommandField);

    if (isRunContext) {
        wxFileName exe(source);
        const auto ext = exe.GetExt().Lower();
        if (ext == "bas" || ext == "bi") {
#ifdef __WXMSW__
            exe.SetExt("exe");
#else
            exe.SetExt("");
#endif
        }
        append("<$file>", exe.GetFullPath());
        append("<$file_path>", exe.GetPath());
        append("<$file_name>", exe.GetName());
        append("<$file_ext>", exe.GetExt());
        append("<$param>", getContext().getCompilerManager().getParameters());
        append("<$terminal>", getContext().getConfigManager().getTerminalLauncher());
    } else {
        wxString fbc = m_compilerPath;
        if (fbc.empty()) {
#ifdef __WXMSW__
            fbc = "C:\\path\\to\\fbc.exe";
#else
            fbc = "/path/to/fbc";
#endif
        }
        append("<$fbc>", fbc);
        append("<$file>", source);
    }
}

void CompilerPage::insertPlaceholder(const wxString& placeholder) {
    if (m_lastFocused == nullptr) {
        return;
    }
    m_lastFocused->WriteText(placeholder);
    // Defer focus restore: native mouse-down on the list may set focus to
    // the list after our handler returns, so re-focus the text field on
    // the next event-loop tick to win the race.
    auto* target = m_lastFocused;
    CallAfter([target] {
        if (target != nullptr) {
            target->SetFocus();
        }
    });
}

auto CompilerPage::getSampleSourcePath() const -> wxString {
    if (auto* doc = getContext().getDocumentManager().getActive(); doc != nullptr && !doc->isNew()) {
        const auto& path = doc->getFilePath();
        auto ext = path.extension().string();
        if (!ext.empty() && ext.front() == '.') {
            ext.erase(0, 1);
        }
        std::ranges::transform(ext, ext.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (ext == "bas" || ext == "bi") {
            return toWxString(path);
        }
    }
#ifdef __WXMSW__
    return R"(C:\path\to\example.bas)";
#else
    return "/path/to/example.bas";
#endif
}

auto CompilerPage::makeEntryField(wxString& value, const wxString& labelText) -> Unowned<wxTextCtrl> {
    const auto lbl = text(labelText, {});
    const auto tf = textField(value, {});
    connect(lbl, tf);
    return tf;
}

auto CompilerPage::makeFileEntry(wxString& value, const wxString& labelText) -> std::pair<Unowned<wxTextCtrl>, Unowned<wxButton>> {
    const auto lbl = text(labelText, {});
    Unowned<wxButton> btn;
    Unowned<wxTextCtrl> tf;
    hbox({ .alignment = SmartBoxSizer::Alignment::Center, .border = 0 }, [&] {
        tf = textField(value, { .proportion = 1 });
        connect(lbl, tf);
        btn = button("...", {});
    });
    return std::make_pair(tf, btn);
}
