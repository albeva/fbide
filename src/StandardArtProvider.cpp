//
//  StandardArtProvider.cpp
//  fbide
//
//  Created by Albert on 02/03/2016.
//  Copyright © 2016 Albert Varaksin. All rights reserved.
//
#include "app_pch.hpp"
#include "StandardArtProvider.hpp"

using namespace fbide;

namespace  {
    
    namespace XPM {
        #include "xpm/about.xpm"
        #include "xpm/add.xpm"
        #include "xpm/addbook.xpm"
        #include "xpm/addsrc.xpm"
        #include "xpm/arricon.xpm"
        #include "xpm/bughlp.xpm"
        #include "xpm/close.xpm"
        #include "xpm/closefl.xpm"
        #include "xpm/clrhist.xpm"
        #include "xpm/clsall.xpm"
        #include "xpm/colours.xpm"
        #include "xpm/compile.xpm"
        #include "xpm/compopt.xpm"
        #include "xpm/compres.xpm"
        #include "xpm/comprun.xpm"
        #include "xpm/copy.xpm"
        #include "xpm/cut.xpm"
        #include "xpm/debug.xpm"
        #include "xpm/dos.xpm"
        #include "xpm/empty.xpm"
        #include "xpm/envopt.xpm"
        #include "xpm/explor.xpm"
        #include "xpm/export.xpm"
        #include "xpm/gobook.xpm"
        #include "xpm/help.xpm"
        #include "xpm/homepg.xpm"
        #include "xpm/icon.xpm"
        #include "xpm/insert.xpm"
        #include "xpm/makefl.xpm"
        #include "xpm/minall.xpm"
        #include "xpm/newproj.xpm"
        #include "xpm/newsrc.xpm"
        #include "xpm/newtemp.xpm"
        #include "xpm/next.xpm"
        #include "xpm/openproj.xpm"
        #include "xpm/opnproj.xpm"
        #include "xpm/package.xpm"
        #include "xpm/packman.xpm"
        #include "xpm/paste.xpm"
        #include "xpm/prev.xpm"
        #include "xpm/print.xpm"
        #include "xpm/projopt.xpm"
        #include "xpm/rebuild.xpm"
        #include "xpm/redo.xpm"
        #include "xpm/remsrc.xpm"
        #include "xpm/reopen.xpm"
        #include "xpm/resrc.xpm"
        #include "xpm/run.xpm"
        #include "xpm/save.xpm"
        #include "xpm/saveall.xpm"
        #include "xpm/saveas.xpm"
        #include "xpm/screen.xpm"
        #include "xpm/search.xpm"
        #include "xpm/srcagain.xpm"
        #include "xpm/srchrep.xpm"
        #include "xpm/temphlp.xpm"
        #include "xpm/tile.xpm"
        #include "xpm/toolbar.xpm"
        #include "xpm/tools.xpm"
        #include "xpm/tutor.xpm"
        #include "xpm/undo.xpm"
        #include "xpm/update.xpm"
        #include "xpm/xpm_goto.xpm"
    };
    
    // map containing the icons
    std::unordered_map<wxString, wxBitmap> _icons{
        {"about",       XPM::about},
        {"add",         XPM::add},
        {"addbook",     XPM::addbook},
        {"addsrc",      XPM::addsrc},
        {"arricon",     XPM::arricon},
        {"bughlp",      XPM::bughlp},
        {"quit",        XPM::close},
        {"closefl",     XPM::closefl},
        {"clrhist",     XPM::clrhist},
        {"clsall",      XPM::clsall},
        {"colours",     XPM::colours},
        {"compile",     XPM::compile},
        {"compopt",     XPM::compopt},
        {"compres",     XPM::compres},
        {"comprun",     XPM::comprun},
        {"copy",        XPM::copy},
        {"cut",         XPM::cut},
        {"debug",       XPM::debug},
        {"dos",         XPM::dos},
        {"empty",       XPM::empty},
        {"envopt",      XPM::envopt},
        {"explor",      XPM::explor},
        {"export",      XPM::xpm_export},
        {"gobook",      XPM::gobook},
        {"help",        XPM::help},
        {"homepg",      XPM::homepg},
        {"icon",        XPM::icon},
        {"insert",      XPM::insert},
        {"makefl",      XPM::makefl},
        {"minall",      XPM::minall},
        {"newproj",     XPM::newproj},
        {"new",         XPM::newsrc},
        {"newtemp",     XPM::newtemp},
        {"next",        XPM::next},
        {"open",        XPM::openproj},
        {"opnproj",     XPM::opnproj},
        {"package",     XPM::package},
        {"packman",     XPM::packman},
        {"paste",       XPM::paste},
        {"prev",        XPM::prev},
        {"print",       XPM::print},
        {"projopt",     XPM::projopt},
        {"rebuild",     XPM::rebuild},
        {"redo",        XPM::redo},
        {"remsrc",      XPM::remsrc},
        {"reopen",      XPM::reopen},
        {"resrc",       XPM::resrc},
        {"run",         XPM::run},
        {"save",        XPM::save},
        {"saveall",     XPM::saveall},
        {"saveas",      XPM::saveas},
        {"screen",      XPM::screen},
        {"search",      XPM::search},
        {"srcagain",    XPM::srcagain},
        {"replace",     XPM::srchrep},
        {"temphlp",     XPM::temphlp},
        {"tile",        XPM::tile},
        {"toolbar",     XPM::toolbar},
        {"tools",       XPM::tools},
        {"tutor",       XPM::tutor},
        {"undo",        XPM::undo},
        {"update",      XPM::update},
        {"goto",        XPM::xpm_goto},
    };
    
    // Initialize the map
    auto init = [](decltype(_icons) & icons) {
        auto c = wxColour(191, 191, 191);
        for(auto & p : icons) {
            p.second.SetMask(new wxMask(p.second, c));
        }
        return true;
    }(_icons);
    
    // default icon size
    const wxSize _size{16, 16};
    
    // the default empty icon
    const auto & empty = _icons["empty"];
}


/**
 * Get bitmap
 */
const wxBitmap & StandardArtProvider::GetIcon(const wxString & name)
{
    auto iter = _icons.find(name);
    return iter == _icons.end() ? empty : iter->second;
}


/**
 * Get bitmap size
 */
const wxSize & StandardArtProvider::GetSize(const wxString & name)
{
    return _size;
}
