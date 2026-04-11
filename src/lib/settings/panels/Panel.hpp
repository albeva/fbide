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
    Panel(Context& ctx, wxWindowID id, wxWindow* parent);
    virtual void layout() = 0;
    virtual void apply() = 0;

protected:
    [[nodiscard]] auto getContext() const -> Context& { return m_ctx; }
    [[nodiscard]] auto getVBox() const -> wxBoxSizer* { return m_vbox; }
    [[nodiscard]] auto getConfig() const -> Config&;

    void makeTitle(LangId langId);
    void makeText(wxSizer* sizer, LangId langId, int flags = wxGROW | wxALL);
    void makeSeparator(wxSizer* sizer, long style);
    void makeCheckBox(wxSizer* sizer, bool& value, LangId langId);
    void makeSpinCtrl(wxSizer* sizer, int& value, LangId langId, int minVal, int maxVal, int width = 50);
    void makeChoice(wxSizer* sizer, wxString& value, LangId langId, const wxArrayString& choices);
    void makeTextField(wxSizer* sizer, wxString& value, LangId langId, std::function<void()> browseFn = {});

private:
    Unowned<wxBoxSizer> m_vbox;
    Context& m_ctx;
};

} // namespace fbide
