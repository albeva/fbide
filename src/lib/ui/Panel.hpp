//
// Created by Albert Varaksin on 11/04/2026.
//
#pragma once
#include "pch.hpp"

namespace fbide {
class Context;
class Config;
enum class LangId : int;

class Panel : public wxPanel {
public:
    NO_COPY_AND_MOVE(Panel)

    Panel(Context& ctx, wxWindowID id, wxWindow* parent);
    virtual void create() = 0;
    virtual void apply() = 0;

protected:
    static constexpr int DEFAULT_PAD = 5;

    struct LayoutOptions final {
        int proportion = 0;
        int flag = 0;
        int border = DEFAULT_PAD;
    };
    static constexpr LayoutOptions defaultBoxOptions { .proportion = 0, .flag = wxALL, .border = DEFAULT_PAD };

    [[nodiscard]] auto getContext() const -> Context& { return m_ctx; }
    [[nodiscard]] auto getRootSizer() const -> wxBoxSizer* { return m_rootSizer; }
    [[nodiscard]] auto getCurrentSizer() const -> wxBoxSizer* { return m_currentSizer; }
    [[nodiscard]] auto getConfig() const -> Config&;

    void makeTitle(LangId langId);

    void add(wxWindow* view, LayoutOptions options = defaultBoxOptions);

    void text(LangId langId, LayoutOptions options = defaultBoxOptions);
    void separator(int border = DEFAULT_PAD);
    auto checkBox(bool& value, LangId langId, LayoutOptions options = defaultBoxOptions) -> Unowned<wxCheckBox>;
    auto checkBox(LangId langId, LayoutOptions options = defaultBoxOptions) -> Unowned<wxCheckBox>;
    auto spinCtrl(int& value, LangId langId, int minVal, int maxVal, LayoutOptions options = defaultBoxOptions) -> Unowned<wxSpinCtrl>;
    auto spinCtrl(LangId langId, int minVal, int maxVal, LayoutOptions options = defaultBoxOptions) -> Unowned<wxSpinCtrl>;
    auto choice(wxString& value, const wxArrayString& choices, LayoutOptions options = defaultBoxOptions) -> Unowned<wxChoice>;
    auto choice(const wxArrayString& choices, LayoutOptions options = defaultBoxOptions) -> Unowned<wxChoice>;
    auto textField(wxString& value, LayoutOptions options = defaultBoxOptions) -> Unowned<wxTextCtrl>;
    auto textField(LayoutOptions options = defaultBoxOptions) -> Unowned<wxTextCtrl>;
    auto button(LangId langId, LayoutOptions options = defaultBoxOptions) -> Unowned<wxButton>;
    auto button(const wxString& str, LayoutOptions options = defaultBoxOptions) -> Unowned<wxButton>;

    template<std::invocable Func>
    auto hbox(const LayoutOptions options, Func&& func) -> std::invoke_result_t<Func> {
        return makeBox(wxEmptyString, wxHORIZONTAL, options, std::forward<Func>(func));
    }

    template<std::invocable Func>
    auto hbox(const wxString& title, const LayoutOptions options, Func&& func) -> std::invoke_result_t<Func> {
        return makeBox(title, wxHORIZONTAL, options, std::forward<Func>(func));
    }

    template<std::invocable Func>
    auto vbox(const LayoutOptions options, Func&& func) -> std::invoke_result_t<Func> {
        return makeBox(wxEmptyString, wxVERTICAL, options, std::forward<Func>(func));
    }

    template<std::invocable Func>
    auto vbox(const wxString& title, const LayoutOptions options, Func&& func) -> std::invoke_result_t<Func> {
        return makeBox(title, wxVERTICAL, options, std::forward<Func>(func));
    }

    template<std::invocable Func>
    auto makeBox(const wxString& title, int direction, const LayoutOptions options, Func&& func) -> std::invoke_result_t<Func> {
        const ValueRestorer restore { m_currentSizer };
        const auto sizer = [&] -> wxBoxSizer* {
            if (title.empty()) {
                return make_unowned<wxBoxSizer>(direction);
            }
            return make_unowned<wxStaticBoxSizer>(direction, this, title);
        }();
        m_currentSizer->Add(sizer, options.proportion, options.flag, options.border);
        m_currentSizer = sizer;
        return std::invoke(std::forward<Func>(func));
    }

    void spacer(const int size = DEFAULT_PAD) const {
        m_currentSizer->AddSpacer(size);
    }

private:
    Unowned<wxBoxSizer> m_rootSizer;
    Unowned<wxBoxSizer> m_currentSizer;
    Context& m_ctx;
};

} // namespace fbide
