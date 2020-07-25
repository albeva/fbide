/*
 * This file is part of FBIde, an open-source (cross-platform) IDE for
 * FreeBasic compiler.
 * Copyright (C) 2020  Albert Varaksin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Contact e-mail: Albert Varaksin <albeva@me.com>
 * Program URL   : http://fbide.sourceforge.net
 */

#include <vector>
#include "wx/wx.h"
#include "inc/main.h"
#include "inc/fbedit.h"
#include "inc/wxmynotebook.h"
#include <wx/tipwin.h>
#include <wx/tooltip.h>

#define BUTTON_BAR_SIZE 76

BEGIN_EVENT_TABLE(wxMyNotebook, wxTabbedCtrl)
        EVT_MOUSE_EVENTS(wxMyNotebook::OnMouseEvent)
END_EVENT_TABLE()

wxMyNotebook::wxMyNotebook(FBIdeMainFrame *mf, wxWindow *parent, wxWindowID id,
                           const wxPoint &pos, const wxSize &size,
                           long style, const wxString &name)
    : wxTabbedCtrl(parent, id, pos, size, style, name) {
    p = mf;
}


void wxMyNotebook::OnMouseEvent(wxMouseEvent &event) {
    wxEventType eventType = event.GetEventType();

    m_X = event.GetX();
    m_Y = event.GetY();
    int tabid = HitTest(wxPoint(m_X, m_Y));

    if (eventType == wxEVT_RIGHT_DOWN) {
        /*/
        // *  In the sake of integrity
        // *  this should be moved out of here... but How?
        // *  generate costom event and pass data along?
        // *  well for now it can stay here...
        /*/
        if (tabid == wxNOT_FOUND)
            return;
        if (tabid != GetSelection())
            SetSelection(tabid);
        FB_Edit *stc = p->stc;
        wxMenu popup("");
        popup.Append(Menu_Close, _(p->Lang[21]));
        popup.Append(Menu_CloseAll, _(p->Lang[173]));
        popup.AppendSeparator();
        popup.Append(Menu_Undo, _(p->Lang[27]));
        popup.Enable(Menu_Undo, stc->CanUndo());
        popup.Append(Menu_Redo, _(p->Lang[29]));
        popup.Enable(Menu_Redo, stc->CanRedo());
        popup.AppendSeparator();
        popup.Append(Menu_Copy, _(p->Lang[33]));
        popup.Enable(Menu_Copy, (stc->GetSelectionEnd() - stc->GetSelectionStart()));
        popup.Append(Menu_Cut, _(p->Lang[31]));
        popup.Enable(Menu_Cut, (stc->GetSelectionEnd() - stc->GetSelectionStart()));
        popup.Append(Menu_Paste, _(p->Lang[35]));
        popup.Enable(Menu_Paste, stc->CanPaste());
        popup.Append(Menu_SelectAll, _(p->Lang[37]));
        popup.Enable(Menu_SelectAll, stc->GetLength());
        wxWindow::PopupMenu(&popup, m_X, m_Y);
        return;
    } else if (eventType == wxEVT_MIDDLE_DOWN) {
        if (tabid == wxNOT_FOUND)
            return;
        if (tabid != GetSelection())
            SetSelection(tabid);
        wxCommandEvent event(wxEVT_COMMAND_MENU_SELECTED, Menu_Close);
        GetEventHandler()->ProcessEvent(event);
        return;
    }
    event.Skip();
}


// new tabs :P
//////////////////////////////////////////////////////////////////////////////////
// TabbedCtrl
//////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC_CLASS(wxTabbedCtrlEvent, wxNotifyEvent);

IMPLEMENT_DYNAMIC_CLASS(wxTabbedCtrl, wxControl);

DEFINE_EVENT_TYPE(wxEVT_COMMAND_TABBEDCTRL_PAGE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_COMMAND_TABBEDCTRL_PAGE_CHANGING)
DEFINE_EVENT_TYPE(wxEVT_COMMAND_TABBEDCTRL_PAGE_CLOSING)

BEGIN_EVENT_TABLE(wxTabbedCtrl, wxControl)
        EVT_PAINT(wxTabbedCtrl::OnPaint)
//   EVT_LEFT_DOWN(wxTabbedCtrl::OnMouse)
        EVT_MOUSE_EVENTS(wxTabbedCtrl::OnMouse)
        EVT_SIZE(wxTabbedCtrl::OnSize)
        EVT_ERASE_BACKGROUND(wxTabbedCtrl::OnEraseBackground)
        EVT_MENU(-1, wxTabbedCtrl::OnPopUpMenu)
END_EVENT_TABLE()

wxTabbedCtrl::wxTabbedCtrl()
    : active(-1), img_list(0), style(0) {}

void wxTabbedCtrl::Create(wxWindow *parent, wxWindowID id,
                          const wxPoint &position, const wxSize &size,
                          long style, const wxString &name) {
    wxWindow::Create(parent, id, position, size, wxNO_BORDER, name);
    active = -1;
    img_list = 0;
    tipTab = -1;
    this->style = style;
    padding.x = 5;
    padding.y = 3;
    hover = false;
    hover_next = false;
    hover_prev = false;
    hover_menu = false;
    m_intStartPage = 0;
    m_intLastPage = 0;
    wxToolTip *tooltip = new wxToolTip("");
    tooltip->Enable(true);
    tooltip->SetDelay(100);
    SetToolTip(tooltip);

    wxFont normal_font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    wxFont bold_font = normal_font;
    bold_font.SetWeight(wxFONTWEIGHT_BOLD);
    wxClientDC dc(this);
    int height = 22, pom = 0;
    dc.SetFont(bold_font);
    dc.GetTextExtent("Aq", &pom, &height);
    SetSizeHints(wxSize(-1, height + padding.y * 3));
}


void wxTabbedCtrl::AddPage(const wxString &text, bool select, int img) {
    pages.push_back(wxTabbedPage(text, img));
    if (select || GetSelection() == -1)
        SetSelection(GetPageCount() - 1);
    else
        Refresh();
}


void wxTabbedCtrl::InsertPage(int pg, const wxString &text, bool select, int img) {
    wxASSERT_MSG(pg >= 0 && pg <= GetPageCount(), "Got invalid page number");
    pages_type::iterator it = pages.begin() + pg;
    pages.insert(it, wxTabbedPage(text, img));
    if (select || GetSelection() == -1)
        SetSelection(pg);
    else
        Refresh();
}

void wxTabbedCtrl::DeleteAllPages() {
    pages.clear();
    active = -1;
    Refresh();
}


void wxTabbedCtrl::DeletePage(int pg) {
    SetSelection(pg);
    wxASSERT_MSG(pg >= 0 && pg < GetPageCount(), "Got invalid page number");
    tipTab = -1;
    GetToolTip()->Enable(false);
    pages_type::iterator it = pages.begin() + pg;
    pages.erase(it);
    if (pg < active)
        active--;
    else if (active == pg && active == GetPageCount())
        active--;
    SetVisible(active);
}


void wxTabbedCtrl::SetSelection(int pg) {
    wxASSERT_MSG(pg >= 0 && pg < GetPageCount(), "Got invalid page number");

    wxTabbedCtrlEvent event(wxEVT_COMMAND_TABBEDCTRL_PAGE_CHANGING, m_windowId);
    event.SetSelection(pg);
    event.SetOldSelection(active);
    event.SetEventObject(this);
    if (!GetEventHandler()->ProcessEvent(event) || event.IsAllowed()) {
        // program allows the page change
        event.SetEventType(wxEVT_COMMAND_TABBEDCTRL_PAGE_CHANGED);
        event.SetOldSelection(active);
        event.SetSelection(pg);
        GetEventHandler()->ProcessEvent(event);
        active = pg;
        SetVisible(pg);
    }
}


void wxTabbedCtrl::AdvanceSelection(bool forward) {
}


void wxTabbedCtrl::SetVisible(int pg) {

    if (GetPageCount() == 0)
        return;
    if (!IsVisible(pg)) {
        if (pg < m_intStartPage) {
            if (m_intLastPage < (GetPageCount() - 1))
                m_intStartPage = pg;
            else
                m_intStartPage = 0;
        } else {
            int width, pom;
            wxSize size = GetSize();
            wxClientDC dc(this);
            int posx = (size.x - BUTTON_BAR_SIZE);

            wxFont normal_font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            wxFont bold_font = normal_font;
            bold_font.SetWeight(wxFONTWEIGHT_BOLD);

            int i = pg;
            for (; i > 0; i--) {
                dc.SetFont((i == pg) ? bold_font : normal_font);
                dc.GetTextExtent(GetPageText(i), &width, &pom);
                int space = padding.x;

                posx -= (space + width + space + padding.x);
                if (posx <= 0) {
                    m_intStartPage = i + 1;
                    Refresh();
                    return;
                }
            }
            m_intStartPage = 0;

        }
    }

    Refresh();

}


wxString wxTabbedCtrl::GetPageText(int pg) {
    wxASSERT_MSG(pg >= 0 && pg < GetPageCount(), "Got invalid page number");
    return pages[pg].text;
}


void wxTabbedCtrl::SetPageText(int pg, const wxString &t) {
    wxASSERT_MSG(pg >= 0 && pg < GetPageCount(), "Got invalid page number");
    if (pages[pg].text != t) {
        pages[pg].text = t;
        Refresh();
    }
}


int wxTabbedCtrl::HitTest(const wxPoint &p, long *flags) {
    int height, width, pom;
    wxSize size = GetSize();
    wxClientDC dc(this);

    wxFont normal_font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    wxFont bold_font = normal_font;
    bold_font.SetWeight(wxFONTWEIGHT_BOLD);

    if (flags)
        *flags = wxTB_HITTEST_NOWHERE;
    dc.SetFont(bold_font);
    dc.GetTextExtent("Aq", &pom, &height);
    if (p.x <= 0 || p.x >= size.x)
        return wxNOT_FOUND;
    if (p.y <= 0 || p.y >= height + padding.y * 2)
        return wxNOT_FOUND;

    int posx = 3;
    for (int i = m_intStartPage; i <= m_intLastPage; i++) {
        dc.SetFont((i == GetSelection()) ? bold_font : normal_font);
        dc.GetTextExtent(GetPageText(i), &width, &pom);

        int space = padding.x;

        if (p.x > posx && p.x < posx + width + space + padding.x) {
            if (flags)
                *flags = wxTB_HITTEST_ONLABEL;

            return i;
        }

        posx += width + space + padding.x;
    }

    return wxNOT_FOUND;
}


void wxTabbedCtrl::OnMouse(wxMouseEvent &ev) {
    wxPoint mouse = ev.GetPosition();
    int page = HitTest(ev.GetPosition());
    if (ev.GetEventType() == wxEVT_MOTION) {
        bool xhover = mouse.x >= x_rect.x && mouse.x <= x_rect.x + x_rect.width && mouse.y >= x_rect.y &&
                      mouse.y <= x_rect.y + x_rect.height;
        bool nhover = (mouse.x >= Next_rect.x && mouse.x <= Next_rect.x + Next_rect.width && mouse.y >= Next_rect.y &&
                       mouse.y <= Next_rect.y + Next_rect.height) &&
                      (m_intLastPage < (GetPageCount() - 1));
        bool phover = (mouse.x >= Prev_rect.x && mouse.x <= Prev_rect.x + Prev_rect.width && mouse.y >= Prev_rect.y &&
                       mouse.y <= Prev_rect.y + Prev_rect.height) &&
                      (m_intStartPage > 0);
        bool mhover = (mouse.x >= Menu_rect.x && mouse.x <= Menu_rect.x + Menu_rect.width && mouse.y >= Menu_rect.y &&
                       mouse.y <= Menu_rect.y + Menu_rect.height);
        if (hover != xhover) {
            hover = xhover;
            wxClientDC dc(this);
            DrawX(hover, dc);
        } else if (hover_next != nhover) {
            hover_next = nhover;
            wxClientDC dc(this);
            DrawNext(hover_next, dc);
        } else if (hover_prev != phover) {
            hover_prev = phover;
            wxClientDC dc(this);
            DrawPrev(hover_prev, dc);
        } else if (hover_menu != mhover) {
            hover_menu = mhover;
            wxClientDC dc(this);
            DrawMenu(hover_menu, dc);
        } else {
            wxToolTip *tooltip = GetToolTip();
            if (page != wxNOT_FOUND) {
                if (tipTab != page) {
                    tipTab = page;
                    tooltip->Enable(true);
                    wxString info;
                    int pg = page + 1;
                    info << pg << " of " << GetPageCount() << " - " << GetPageText(page);
                    tooltip->SetTip(info);
                }
            } else {
                tipTab = -1;
                tooltip->Enable(false);
            }
        }
    } else if (ev.GetEventType() == wxEVT_LEFT_UP) {
        if (hover) {
            wxClientDC dc(this);
            DrawX(false, dc);
            SetVisible(active);
            wxCommandEvent event(wxEVT_COMMAND_MENU_SELECTED, Menu_Close);
            GetEventHandler()->ProcessEvent(event);
        } else if (hover_next) {
            wxClientDC dc(this);
            DrawNext(false, dc);
            if (GetPageCount() > (m_intStartPage + 1) && (m_intLastPage + 1) < GetPageCount()) {
                m_intStartPage++;
                Refresh();
            }
        } else if (hover_prev) {
            wxClientDC dc(this);
            DrawPrev(false, dc);
            if (m_intStartPage > 0) {
                m_intStartPage--;
                Refresh();
            }
        } else if (hover_menu) {
            hover_menu = false;
            wxClientDC dc(this);
            DrawMenu(false, dc);
            GenerateConextMenu(mouse);
        } else {
            if (page != wxNOT_FOUND)
                SetSelection(page);
        }
    } else if (ev.GetEventType() == wxEVT_RIGHT_UP && page == wxNOT_FOUND) {
        GenerateConextMenu(mouse);
    }
}


void wxTabbedCtrl::GenerateConextMenu(wxPoint &mouse) {
    wxMenu popup("");
    for (int i = 0; i < GetPageCount(); i++) {
        if (i == active) {
            popup.AppendCheckItem(i, GetPageText(i));
            popup.Check(i, true);
        } else
            popup.Append(i, GetPageText(i));
    }

    PopupMenu(&popup, mouse.x, mouse.y);
}


void wxTabbedCtrl::OnPopUpMenu(wxCommandEvent &event) {
    int id = event.GetId();
    if (id > wxID_LOWEST) {
        event.Skip();
        return;
    }
    if (id >= 0 && id < GetPageCount())
        SetSelection(id);
}


void wxTabbedCtrl::OnSize(wxSizeEvent &) {
    Refresh();
}


void wxTabbedCtrl::OnEraseBackground(wxEraseEvent &) {}


void wxTabbedCtrl::DrawX(bool active, wxDC &dc) {
    const int SIZE = 8;
    wxSize size = GetSize();
    wxBrush back_brush = wxBrush(GetBackgroundColour());
    wxPen back_pen = wxPen(GetBackgroundColour());
    wxPen x_pen = wxPen(active ? *wxBLACK : wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
    x_pen.SetWidth(2);

    int posx = size.x - SIZE * 2, posy = (size.y - SIZE) / 2;
    x_rect = wxRect(posx, posy, SIZE, SIZE);

    dc.SetPen(back_pen);
    dc.SetBrush(back_brush);
    dc.DrawRectangle(posx - SIZE + 1, 1, SIZE * 3 - 2, size.y - 2);

    dc.SetPen(x_pen);
    dc.DrawLine(posx, posy, posx + SIZE, posy + SIZE);
    dc.DrawLine(posx, posy + SIZE, posx + SIZE, posy);
}


void wxTabbedCtrl::DrawNext(bool active, wxDC &dc) {
    const int SIZE = 8;
    wxSize size = GetSize();
    wxBrush back_brush = wxBrush(GetBackgroundColour());
    wxPen back_pen = wxPen(GetBackgroundColour());
    wxPen x_pen = wxPen(active ? *wxBLACK : wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
    x_pen.SetWidth(2);

    int posx = size.x - (SIZE * 4), posy = (size.y - SIZE) / 2;
    Next_rect = wxRect(posx, posy, SIZE, SIZE);

    dc.SetPen(back_pen);
    dc.SetBrush(back_brush);
    dc.DrawRectangle(posx - SIZE + 1, 1, SIZE * 3 - 2, size.y - 2);

    dc.SetPen(x_pen);
    dc.DrawLine(posx + 2, posy, posx + 6, posy + 4);
    dc.DrawLine(posx + 2, posy + SIZE, posx + 6, posy + 4);
}


void wxTabbedCtrl::DrawPrev(bool active, wxDC &dc) {
    const int SIZE = 8;
    wxSize size = GetSize();
    wxBrush back_brush = wxBrush(GetBackgroundColour());
    wxPen back_pen = wxPen(GetBackgroundColour());
    wxPen x_pen = wxPen(active ? *wxBLACK : wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
    x_pen.SetWidth(2);

    int posx = size.x - (SIZE * 6), posy = (size.y - SIZE) / 2;
    Prev_rect = wxRect(posx, posy, SIZE, SIZE);

    dc.SetPen(back_pen);
    dc.SetBrush(back_brush);
    dc.DrawRectangle(posx - SIZE + 1, 1, SIZE * 3 - 2, size.y - 2);

    dc.SetPen(x_pen);
    dc.DrawLine(posx + 2, posy + 4, posx + 6, posy);
    dc.DrawLine(posx + 2, posy + 4, posx + 6, posy + 8);
}


void wxTabbedCtrl::DrawMenu(bool active, wxDC &dc) {
    const int SIZE = 8;
    wxSize size = GetSize();
    wxBrush back_brush = wxBrush(GetBackgroundColour());
    wxPen back_pen = wxPen(GetBackgroundColour());
    wxPen x_pen = wxPen(active ? *wxBLACK : wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
    x_pen.SetWidth(2);

    int posx = size.x - (SIZE * 8), posy = (size.y - SIZE) / 2;
    Menu_rect = wxRect(posx, posy, SIZE, SIZE);

    dc.SetPen(back_pen);
    dc.SetBrush(back_brush);
    dc.DrawRectangle(posx - SIZE + 1, 1, SIZE * 3 - 2, size.y - 2);

    dc.SetPen(x_pen);
    dc.DrawLine(posx, posy + 4, posx + 4, posy + 8);
    dc.DrawLine(posx + 4, posy + 8, posx + 8, posy + 4);
}


void wxTabbedCtrl::OnPaint(wxPaintEvent &) {
    wxPaintDC dc(this);
    wxSize size = GetSize();
    wxBrush back_brush = wxBrush(GetBackgroundColour());
    wxBrush nosel_brush = wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    wxBrush sel_brush = wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT));
    wxPen border_pen = wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
    wxPen sel_pen = wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT));
    wxPen back_pen = wxPen(GetBackgroundColour());
    wxFont normal_font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    wxFont bold_font = normal_font;
    bold_font.SetWeight(wxFONTWEIGHT_BOLD);
    bool mirror = style & wxTB_BOTTOM;
    bool fullborder = !(style & wxNO_BORDER);

    //background
    dc.SetTextBackground(GetBackgroundColour());
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    dc.SetBrush(back_brush);
    if (fullborder) {
        dc.SetPen(border_pen);
        dc.DrawRectangle(0, 0, size.x, size.y);
    } else {
        dc.SetPen(back_pen);
        dc.DrawRectangle(0, 0, size.x, size.y);
        dc.SetPen(border_pen);
        dc.DrawLine(0, mirror ? 0 : size.y - 1, size.x, mirror ? 0 : size.y - 1);
    }

    int height, width, pom;
    dc.SetFont(bold_font);
    dc.GetTextExtent("Aq", &pom, &height);
    int posx = 3;

    //and tabs
    int i = m_intStartPage;
    for (; i < GetPageCount(); i++) {
        dc.SetPen(border_pen);
        dc.SetFont((i == GetSelection()) ? bold_font : normal_font);
        dc.SetBrush((i == GetSelection()) ? sel_brush : nosel_brush);
        dc.GetTextExtent(GetPageText(i), &width, &pom);

        int space = padding.x;
        if ((posx + width + space + padding.x + BUTTON_BAR_SIZE) > size.x) {
            break;
        }

        dc.DrawRoundedRectangle(posx, size.y - height - padding.y * 2, width + space + padding.x,
                                height + padding.y * 2 + 3, 3);
        dc.DrawText(GetPageText(i), posx + space, size.y - height - padding.y);
        if (i != GetSelection())
            dc.DrawLine(posx, size.y - 1, posx + width + space + padding.x, size.y - 1);

        posx += width + space + padding.x;
    }
    m_intLastPage = i - 1;

    //X
    DrawX(hover, dc);
    DrawNext(hover_next, dc);
    DrawPrev(hover_prev, dc);
    DrawMenu(hover_menu, dc);
}


