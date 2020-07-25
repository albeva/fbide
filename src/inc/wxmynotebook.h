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
#pragma once
#include "pch.h"


class FBIdeMainFrame;

// New tabs :P
//hit results
enum {
    wxTB_HITTEST_NOWHERE = 0,   // not on tab
    wxTB_HITTEST_ONICON = 1,   // on icon
    wxTB_HITTEST_ONLABEL = 2,   // on label
    wxTB_HITTEST_ONITEM = 4,
};

#define wxTB_TOP     0x00000001
#define wxTB_BOTTOM  0x00000002
#define wxTB_X       0x00000010
#define wxTB_DEFAULT_STYLE wxTB_TOP|wxSTATIC_BORDER
//also wxSTATIC_BORDER and wxNO_BORDER are working here

class wxTabbedCtrl : public wxControl {

DECLARE_DYNAMIC_CLASS(wxTabbedCtrl)

DECLARE_EVENT_TABLE()

    class wxTabbedPage {
    public:
        wxString text;
        int image;

        wxTabbedPage(const wxString &t, int img)
            : text(t), image(img) {}
    };

    typedef std::vector<wxTabbedPage> pages_type;

    pages_type pages;
    int active;
    wxImageList *img_list;
    long style;
    wxSize padding;
    bool hover;
    bool hover_next;
    bool hover_prev;
    bool hover_menu;
    wxRect x_rect;
    wxRect Prev_rect;
    wxRect Next_rect;
    wxRect Menu_rect;

    int m_intStartPage;
    int m_intLastPage;

    int tipTab;

    void OnMouse(wxMouseEvent &);

    void OnPaint(wxPaintEvent &);

    void OnSize(wxSizeEvent &);

    void OnEraseBackground(wxEraseEvent &);

    void DrawX(bool active, wxDC &dc);

    void DrawNext(bool active, wxDC &dc);

    void DrawPrev(bool active, wxDC &dc);

    void DrawMenu(bool active, wxDC &dc);

    bool IsVisible(int pg) {
        return pg >= m_intStartPage && pg <= m_intLastPage;
    }

    void SetVisible(int pg);

    void OnPopUpMenu(wxCommandEvent &event);

    void GenerateConextMenu(wxPoint &mouse);

public:
    wxTabbedCtrl();

    wxTabbedCtrl(wxWindow *parent, wxWindowID id,
                 const wxPoint &position = wxDefaultPosition, const wxSize size = wxDefaultSize,
                 long style = wxTB_DEFAULT_STYLE, const wxString &name = "TabbedCtrl") {
        Create(parent, id, position, size, style, name);
    }

    void Create(wxWindow *parent, wxWindowID id,
                const wxPoint &position = wxDefaultPosition, const wxSize &size = wxDefaultSize,
                long style = wxTB_DEFAULT_STYLE, const wxString &name = "TabbedCtrl");

    virtual ~wxTabbedCtrl() {}

    void AddPage(const wxString &text, bool select = false, int img = -1);

    void InsertPage(int pg, const wxString &text, bool select = false, int img = -1);

    void DeleteAllPages();

    void DeletePage(int pg);

    int GetPageCount() {
        return pages.size();
    }

    int GetSelection() {
        return active;
    }

    int HitTest(const wxPoint &pos, long *flags = 0);

    void SetSelection(int pg);

    wxString GetPageText(int pg);

    void SetPageText(int pg, const wxString &t);

    int GetPageImage(int pg);

    void SetPageImage(int pg, int img);

    wxImageList *GetImageList() {
        return img_list;
    }

    void SetImageList(wxImageList *list) {
        img_list = list;
    }

    wxSize GetPadding() {
        return padding;
    }

    void SetPadding(const wxSize &pad) {
        padding = pad;
    }

    void AdvanceSelection(bool forward = true);
};

class wxTabbedCtrlEvent : public wxNotifyEvent {
DECLARE_DYNAMIC_CLASS(wxTabbedCtrlEvent)

    size_t sel, oldsel;

public:
    wxTabbedCtrlEvent(wxEventType commandType = wxEVT_NULL, int winid = 0, int nSel = -1, int nOldSel = -1)
        : wxNotifyEvent(commandType, winid), sel(nSel), oldsel(nOldSel) {}

    void SetSelection(int s) {
        sel = s;
    }

    void SetOldSelection(int s) {
        oldsel = s;
    }

    int GetSelection() {
        return sel;
    }

    int GetOldSelection() {
        return oldsel;
    }
};


BEGIN_DECLARE_EVENT_TYPES()
DECLARE_EVENT_TYPE(wxEVT_COMMAND_TABBEDCTRL_PAGE_CHANGED, 10000)
DECLARE_EVENT_TYPE(wxEVT_COMMAND_TABBEDCTRL_PAGE_CHANGING, 10001)
DECLARE_EVENT_TYPE(wxEVT_COMMAND_TABBEDCTRL_PAGE_CLOSING, 10003)
END_DECLARE_EVENT_TYPES()

typedef void (wxEvtHandler::*wxTabbedCtrlEventFunction)(wxTabbedCtrlEvent &);

#define wxTabbedCtrlEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxTabbedCtrlEventFunction, &func)

#define EVT_TABBEDCTRL_PAGE_CHANGED(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_TABBEDCTRL_PAGE_CHANGED, winid, wxTabbedCtrlEventHandler(fn))

#define EVT_TABBEDCTRL_PAGE_CHANGING(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_TABEDDCTRL_PAGE_CHANGING, winid, wxTabbedCtrlEventHandler(fn))

#define EVT_TABBEDCTRL_PAGE_CLOSING(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_COMMAND_TABBEDCTRL_PAGE_CLOSING, winid, wxTabbedCtrlEventHandler(fn))


class wxMyNotebook : public wxTabbedCtrl {
private:
DECLARE_EVENT_TABLE()

protected:
    // for Tab Dragging
    int m_TabID;
    wxCoord m_X, m_Y;

    void OnMouseEvent(wxMouseEvent &event);

    FBIdeMainFrame *p;

public:
    wxMyNotebook(FBIdeMainFrame *mf, wxWindow *parent, wxWindowID id,
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize &size = wxDefaultSize,
                 long style = 0,
                 const wxString &name = wxNotebookNameStr);

};
