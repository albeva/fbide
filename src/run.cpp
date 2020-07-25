/*
 * This file is part of FBIde, an open-source (cross-platform) IDE for
 * FreeBasic compiler.
 * Copyright (C) 2005  Albert Varaksin
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
 * Contact e-mail: Albert Varaksin <vongodric@hotmail.com>
 * Program URL   : http://fbide.sourceforge.net
 */

#include "inc/main.h"
#include "inc/fbedit.h"
#include <wx/process.h>

class MyProcess : public wxProcess {
public:
    MyProcess(MyFrame *parent, const wxString &cmd)
        : wxProcess(parent), m_cmd(cmd) {
        m_parent = parent;
    }

    // instead of overriding this virtual function we might as well process the
    // event from it in the frame class - this might be more convenient in some
    // cases
    virtual void OnTerminate(int pid, int status);

protected:
    MyFrame *m_parent;
    wxString m_cmd;
};


void MyFrame::AddListItem(int Linenr, int ErrorNr, wxString FileName, wxString Message) {

    // FBConsole is a pointer to wxListCtrl control

    wxString lnr;
    if (Linenr != -1)
        lnr << Linenr;
    int Itemcount = FBConsole->GetItemCount();
    long tmp = FBConsole->InsertItem(Itemcount, lnr, 0);
    FBConsole->SetItemData(tmp, 0);
    FBConsole->SetItem(Itemcount, 1, FileName);
    lnr = "";
    if (ErrorNr != -1)
        lnr << ErrorNr;
    FBConsole->SetItem(Itemcount, 2, lnr);

    FBConsole->SetItem(Itemcount, 3, Message);
}


void MyFrame::OnGoToError(wxListEvent &event) {
    if (stc == 0)
        return;
    //int idx = event.GetData();

    if (event.GetText().Len()) {
        long data = event.GetIndex();
        wxListItem list_item;
        list_item.SetId(data);
        list_item.SetColumn(1);
        list_item.SetMask(wxLIST_MASK_TEXT);
        FBConsole->GetItem(list_item);
        wxString File = list_item.GetText();

        unsigned long LineNr = 0;

        FBConsole->GetItemText(data).ToULong(&LineNr);
        LineNr--;

        GoToError(LineNr, File);

    }
}


void MyFrame::GoToError(int Linenr, wxString FileName) {
    if (wxFileNameFromPath(FileName).Lower() != "fbidetemp.bas") {
        int result = bufferList.FileLoaded(FileName);
        if (result != -1) {
            if (FBNotebook->GetSelection() != result) {
                FBNotebook->SetSelection(result);
            }
        } else {
            NewSTCPage(FileName, true);
        }
    }

    if (stc->GetCurrentLine() != (int) Linenr) {
        stc->ScrollToLine((int) Linenr - (stc->LinesOnScreen() / 2));
        stc->GotoLine((int) Linenr);

    }
    stc->SetFocus();
    stc->EnsureCaretVisible();
}


void MyFrame::OnCompilerLog(wxCommandEvent &event) {
    CompilerLog();
}


void MyFrame::MyFrame::OnConsoMouseleLeft(wxListEvent &event) {
    CompilerLog();
}


void MyFrame::CompilerLog() {
    //wxMessageBox( strCompilerOutput );
    wxString strTitle("Fbc output");
    wxDialog *dlg = new wxDialog(this, -1, strTitle,
                                 wxDefaultPosition, wxSize(400, 200),
                                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxCLIP_CHILDREN | wxMAXIMIZE_BOX);
    wxBoxSizer *objSizer = new wxBoxSizer(wxVERTICAL);
    wxTextCtrl *output = new wxTextCtrl(dlg, wxID_ANY, "",
                                        wxDefaultPosition, wxDefaultSize,
                                        wxTE_MULTILINE | wxTE_READONLY | wxTE_BESTWRAP | wxTE_RICH2);
    output->SetFont(wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    objSizer->Add(output, 1, wxBOTTOM | wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 0);

    wxTextAttr normal = output->GetDefaultStyle();
    wxTextAttr bold = wxTextAttr(normal);
    bold.SetFont(bold.GetFont().Bold());

    bool nesting = false;
    wxString tag = "";
    for (int i = 0; i < (int) strCompilerOutput.Count(); i++) {
        for (int j = 0; j < (int) strCompilerOutput[i].Len(); j++) {
            wxString thechar = strCompilerOutput[i].Mid(j, 1);
            if (thechar == '[' && !nesting) {
                nesting = true;
            } else if (thechar == ']' && nesting) {
                nesting = false;
                tag = tag.MakeLower();
                if (tag == "bold") {
                    output->SetDefaultStyle(bold);
                } else if (tag == "/bold") {
                    output->SetDefaultStyle(normal);
                }

                tag = "";
            } else if (nesting) {
                tag += thechar;
            } else {
                output->WriteText(thechar);
            }
        }
        output->WriteText("\r\n");
    }
    output->SetInsertionPoint(output->XYToPosition(0, 0));


    dlg->SetSizer(objSizer);
    dlg->Show();
}

//------------------------------------------------------------------------------

void MyFrame::OnCompile(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if (ProcessIsRunning)
        return;
    if (Proceed() != 1)
        return;
    FBConsole->DeleteAllItems();
    SetStatusText("Compiling...");
    if (Compile(FBNotebook->GetSelection()) == 0)
        SetStatusText("Compilation Complete.");
    else
        SetStatusText("Compilation Failed!");
    return;
}

void MyFrame::OnCompileAndRun(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;
    if (ProcessIsRunning)
        return;
    if (Proceed() != 1)
        return;
    FBConsole->DeleteAllItems();
    SetStatusText("Compiling...");
    if (Compile(FBNotebook->GetSelection()) == 0) {
        SetStatusText("Compilation Complete.");
        wxString strFile = bufferList[FBNotebook->GetSelection()]->GetCompiledFile();
        Run(strFile);
    } else
        SetStatusText("Compilation Failed!");

    return;
}

void MyFrame::OnRun(wxCommandEvent & WXUNUSED(event)) {
    if (stc == 0)
        return;

    wxString strFile = bufferList[FBNotebook->GetSelection()]->GetCompiledFile();
    Run(strFile);

    return;
}


void MyFrame::OnCmdPromt(wxCommandEvent & WXUNUSED(event)) {

#ifdef __WXMSW__
    int major = 0, minor = 0;
    int result = wxGetOsVersion(&major, &minor);
    if (result == wxOS_WINDOWS_NT)
        wxExecute("cmd.exe");
    else if (result == wxOS_WINDOWS_9X)
        wxExecute("command.com");
#else

    if( strTerminal.Len() )
        wxExecute( strTerminal );
#endif

    return;
}

void MyFrame::OnParameters(wxCommandEvent & WXUNUSED(event)) {
    wxTextEntryDialog dialog(this, Lang[177], //"Parameters to pass to your program"
                             Lang[158],
                             ParameterList,
                             wxOK | wxCANCEL);

    if (dialog.ShowModal() == wxID_OK) {
        ParameterList = dialog.GetValue();
    }
    return;
}

void MyFrame::OnShowExitCode(wxCommandEvent & WXUNUSED(event)) {
    Prefs.ShowExitCode = !Prefs.ShowExitCode;
    return;
}

void MyFrame::OnActivePath(wxCommandEvent & WXUNUSED(event)) {
    Prefs.ActivePath = !Prefs.ActivePath;
}

int MyFrame::Proceed(void) {
    int index = FBNotebook->GetSelection();
    Buffer *buff = bufferList[index];

    if (buff->GetModified()) {
        if (wxMessageBox(Lang[183],
                         Lang[184],
                         wxICON_EXCLAMATION | wxYES_NO) == wxYES) {
            if (SaveFile(buff)) {
                SetModified(index, false);
                return 1;
            }
        }
        return 0;
    }
    return 1;
}

// -----------------------------------------------------------------------------

/**
 * MyFrame::Compile
 * This function compiles given src file. It calls MyFrame::GetCompileData to
 * receave data.
 * @param index AboutDialog file from buffer list
 * @return int 0 on success, 1 on failure
 */
int MyFrame::Compile(int index) {

    //Safety checks
    if (!stc)
        return 1;
    if (ProcessIsRunning)
        return 1;

    // these are used to get the first error
    bool isFirstErrorFound = false;
    wxString strFirstErrorFile;
    int intFirstErrorLine;

    // File that we are AboutDialog to compile
    wxFileName objFile(bufferList[index]->GetFileName());
    objFile.Normalize();

    // Get compiler cmdline and check it. if empty return.
    wxString strCompile(GetCompileData(index));
    if (!strCompile.Len())
        return 1;

    // Log compiler output and input
    strCompilerOutput.Empty();
    strCompilerOutput.Add("[bold]Command executed:[/bold]");
    strCompilerOutput.Add(strCompile);

    // If Active path is activated then set current working path
    // to show to the location of the src file.
    if (Prefs.ActivePath)
        ::wxSetWorkingDirectory(objFile.GetPath());

    // Execute fbc and retreave results.
    wxArrayString arrOutput, arrErrOutput;
    int intCompileResult = wxExecute(strCompile, arrOutput, arrErrOutput);

    // if there was any output from fbc then get output messages and put them
    // into console area
    if (arrOutput.Count() || arrErrOutput.Count()) {
        // define variables
        wxString strOutput;
        wxString strTemp;
        wxFileName objOutputFile;
        long intOutput;
        long intLine;
        int intBraceStart;
        int intBraceEnd;
        bool isOutputHeader = false;

        wxString strDebug;

        // Becouse fbc returns Outputs via both std and error channels,
        // we need to join them here.
        WX_APPEND_ARRAY(arrOutput, arrErrOutput);

        // Putput logging:
        strCompilerOutput.Add("");
        strCompilerOutput.Add("[bold]Compiler output:[/bold]");

        // Loop through arrOutput
        for (unsigned int cnt = 0; cnt < arrOutput.Count(); cnt++) {
            if (arrOutput[cnt].IsEmpty())
                continue;

            // Log compiler output
            strCompilerOutput.Add(arrOutput[cnt]);

            intBraceStart = arrOutput[cnt].First('(');
            intBraceEnd = arrOutput[cnt].First(')');

            // if intBraceStart is not -1 then probably line number was found.
            // as fbc returns things: file(linenumber): Error|Warning nr: Output message
            // As it might be any message then test if first part is a filename.
            // colon[:] - win32
            // slash[/] -linux
#ifdef __WXMSW__

            if (intBraceStart != -1 && intBraceEnd != -1 && arrOutput[cnt].GetChar(1) == ':') {
#else
                if( intBraceStart != -1 && intBraceEnd != -1 && arrOutput[cnt].GetChar(0) == '/' ) {
#endif
                // Get possible line and error number.
                strTemp = arrOutput[cnt].Mid(intBraceStart + 1, intBraceEnd - intBraceStart - 1);

                // if this is a number:
                if (strTemp.IsNumber()) {
                    strTemp.ToLong(&intLine);
                    // Get possible file name and check if it is indeed a filename
                    objOutputFile = arrOutput[cnt].Left(intBraceStart);
                    objOutputFile.Normalize();
                    if (objOutputFile.IsOk() && objOutputFile.FileExists()) {
                        //Now that it's indeed is a filename, get line, error/warning number
                        //and Output message on that line
                        strTemp = arrOutput[cnt].Mid(intBraceEnd + 4);
                        strTemp = strTemp.Mid(strTemp.Find(' ') + 1);
                        strOutput = strTemp.Mid(strTemp.Find(':') + 2);
                        strTemp = strTemp.Left(strTemp.Find(':'));
                        strTemp.ToLong(&intOutput);
                        isOutputHeader = true;
                    }
                }
            }

            // If is Output header ( includes filename, Output number and line number then
            // add generated values. Else just add original message.
            if (isOutputHeader) {
                isOutputHeader = false;
                if (intOutput == 0)
                    intOutput = -1;
                if (!isFirstErrorFound) {
                    strFirstErrorFile = objOutputFile.GetFullPath();
                    intFirstErrorLine = intLine;
                    isFirstErrorFound = true;
                }
                AddListItem(intLine, intOutput, objOutputFile.GetFullPath(), strOutput);
            } else {
                // Replace all tabs.
                arrOutput[cnt].Replace("\t", "  ");
                AddListItem(-1, -1, "", arrOutput[cnt]);
            }
        }

        // Open console area
        if (!HSplitter->IsSplit()) {
            HSplitter->SplitHorizontally(FBCodePanel, FBConsole, ConsoleSize);
            FB_View->Check(Menu_Result, true);
        }

    } else {
        // Since there was no output then close console area
        // -if no error then it is not needed.
        if (HSplitter->IsSplit()) {
            ConsoleSize = HSplitter->GetSashPosition();
            HSplitter->Unsplit(FBConsole);
            FB_View->Check(Menu_Result, false);
        }
    }

    // Log
    strCompilerOutput.Add("");
    strCompilerOutput.Add("[bold]Results:[/bold]");


    // if there was an error ( fbc returns 1 ).
    if (intCompileResult) {
        strCompilerOutput.Add("Compilation failed");

        if (isFirstErrorFound) {
            GoToError(intFirstErrorLine - 1, strFirstErrorFile);
        }
    } else {
        // Set newly compiled filename:
        // Note that under linux extension is missing, in windows
        // it's exe
        if (objFile.GetExt().Lower() == "bas" || objFile.GetExt().Lower() == "bi") {
#ifdef __WXMSW__
            objFile.SetExt("exe");
#else

            objFile.SetExt( "" );
#endif

            bufferList[index]->SetCompiledFile(objFile.GetFullPath());
        }

        strCompilerOutput.Add("Compilation successful");
        strCompilerOutput.Add("Generated executable: " + objFile.GetFullPath());
    }

    wxString strTemp;
    // Log system, fbide and fbc version
    strCompilerOutput.Add("");
    strCompilerOutput.Add("[bold]System:[/bold]");

    strTemp = "";
    strTemp << VER_MAJOR << "." << VER_MINOR << "." << VER_RELEASE;
    strCompilerOutput.Add("FBIde: " + strTemp);

    arrOutput.Empty();
#ifdef __WXMSW__

    wxFileName objFbcPath(CompilerPath);
    wxExecute("\"" + objFbcPath.GetFullPath() + "\" -version", arrOutput);
#else

    wxExecute( CompilerPath + " -version", arrOutput );
#endif

    strCompilerOutput.Add("fbc:   " + arrOutput[0]);
    strCompilerOutput.Add("OS:    " + ::wxGetOsDescription());

    return intCompileResult;
}


/**
 * MyFrame::Run
 * This function executes compiled file and adds optional parameters.
 * @param wxFileName file - file to be executed
 * @return none
 */
void MyFrame::Run(wxFileName file) {
    //Safety checks
    if (!stc)
        return;
    if (ProcessIsRunning)
        return;

    // Check if passed filename is valid. If not then it's probably not saved yet!
    if (!file.IsOk()) {
        if (wxMessageBox(Lang[180], Lang[181], wxYES_NO | wxICON_QUESTION) == wxNO)
            return;
        // hack... ugly !
        wxCommandEvent temp;
        OnCompileAndRun(temp);
        return;
    }

    // If Active path is activated then set current working path
    // to show to the location of the src file.
    if (Prefs.ActivePath)
        ::wxSetWorkingDirectory(file.GetPath());

    // Get execute command prototype
    wxString strCommand(RunPrototype.Lower().Trim(true).Trim(false));


    // Replace metatags
    strCommand.Replace("<param>", ParameterList);
    strCommand.Replace("<file>", file.GetFullPath());
    strCommand.Replace("<file_path>", file.GetPath());
    strCommand.Replace("<file_name>", file.GetName());
    strCommand.Replace("<file_ext>", file.GetExt());
    strCommand.Replace("<terminal>", strTerminal);

    // Create new process
    MyProcess *objProcess = new MyProcess(this, strCommand);

    // Execute
    int result = wxExecute(strCommand, wxEXEC_ASYNC, objProcess);

    // if process is not created... something is wrong!
    // Delete object, show error message and return
    if (!result) {
        delete objProcess;
        ProcessIsRunning = false;
        wxMessageBox(Lang[182] + strCommand + "\"", Lang[179], wxICON_ERROR);
        return;
    }

    FB_Toolbar->EnableTool(Menu_Compile, false);
    FB_Toolbar->EnableTool(Menu_CompileAndRun, false);
    FB_Toolbar->EnableTool(Menu_Run, false);
    FB_Toolbar->EnableTool(Menu_QuickRun, false);

    FB_Run->Enable(Menu_Compile, false);
    FB_Run->Enable(Menu_CompileAndRun, false);
    FB_Run->Enable(Menu_Run, false);
    FB_Run->Enable(Menu_QuickRun, false);

    ProcessIsRunning = true;
    return;

}


/**
 * MyFrame::OnQuickRun
 * This function compiles active tab as a temporary file and executes it,
 * when execution is terminated then temporary file is deleted.
 * @return none
 */
void MyFrame::OnQuickRun(wxCommandEvent & WXUNUSED(event)) {

    //Safety checks
    if (!stc)
        return;
    if (ProcessIsRunning)
        return;

    // Get data
    int index = FBNotebook->GetSelection();
    Buffer *buff = bufferList[index];

    wxString OldRunFileName = buff->GetCompiledFile();
    wxString OldFileName = buff->GetFileName();

    // Get working directory
    if (OldFileName == "" || OldFileName == FBUNNAMED) {
        CurFolder = ::wxPathOnly(EditorPath);
    } else {
        CurFolder = ::wxPathOnly(OldFileName);
    }

    CurFolder << "/";

    // Set new temprary data
    stc->SaveFile(CurFolder + "FBIDETEMP.bas");
    buff->SetFileName(CurFolder + "FBIDETEMP.bas");

    // Set status
    FBConsole->DeleteAllItems();
    SetStatusText("Compiling...");

    // Compile it
    if (Compile(index) == 0) {
        SetStatusText("Compilation Complete.");
        Run(buff->GetCompiledFile());
    } else {
        SetStatusText("Compilation Failed!");
        wxRemoveFile(CurFolder + "FBIDETEMP.bas");
        wxRemoveFile(CurFolder + "fbidetemp.asm");
        wxRemoveFile(CurFolder + "fbidetemp.o");
    }

    // Restore original data
    buff->SetCompiledFile(OldRunFileName);
    buff->SetFileName(OldFileName);

    // is temprary file
    IsTemp = true;
    return;
}


/**
 * MyFrame::GetCompileData
 * Generates compildation data that is used in compilation process
 * @param index AboutDialog file from buffer list
 * @return compiler cmd-line
 */
wxString MyFrame::GetCompileData(int index) {

    // Retreave file original name and validate it
    wxFileName objFilePath(bufferList[index]->GetFileName());
    objFilePath.Normalize();
    if (objFilePath.GetExt().Lower() != "bas" && objFilePath.GetExt().Lower() != "bi" &&
        objFilePath.GetExt().Lower() != "rc")
        return "";
    wxString strReturn(CMDPrototype.Lower().Trim(true).Trim(false));


    // Linux doesn't like quotes nor does it need exact path to fbc 
    // ( moved quotes into settings file )
#ifdef __WXMSW__

    wxFileName ObjCompilerPath(CompilerPath);
    ObjCompilerPath.Normalize();

    if (!ObjCompilerPath.FileExists()) {
        wxMessageBox(Lang[244], "FBC", wxICON_ERROR);
        return "";
    }

    strReturn.Replace("<fbc>", ObjCompilerPath.GetFullPath());
    strReturn.Replace("<filename>", objFilePath.GetFullPath());
#else

    strReturn.Replace( "<fbc>", CompilerPath );
    strReturn.Replace( "<filename>", objFilePath.GetFullPath() );
#endif
    return strReturn;
}


//------------------------------------------------------------------------------


void MyProcess::OnTerminate(int pid, int status) {
    m_parent->ProcessIsRunning = false;
    if (m_parent->Prefs.ShowExitCode) {
        wxString Temp;
        Temp << status;
        wxMessageBox(Temp, m_parent->Lang[185]);
    }
    if (m_parent->IsTemp) {
        wxRemoveFile(m_parent->CurFolder + "FBIDETEMP.bas");
        wxRemoveFile(m_parent->CurFolder + "FBIDETEMP.exe");
        m_parent->IsTemp = false;
    }
    m_parent->Raise();
    m_parent->SetFocus();

    if (m_parent->stc) {
        m_parent->stc->SetFocus();

        m_parent->FB_Toolbar->EnableTool(Menu_Compile, true);
        m_parent->FB_Toolbar->EnableTool(Menu_CompileAndRun, true);
        m_parent->FB_Toolbar->EnableTool(Menu_Run, true);
        m_parent->FB_Toolbar->EnableTool(Menu_QuickRun, true);

        m_parent->FB_Run->Enable(Menu_Compile, true);
        m_parent->FB_Run->Enable(Menu_CompileAndRun, true);
        m_parent->FB_Run->Enable(Menu_Run, true);
        m_parent->FB_Run->Enable(Menu_QuickRun, true);
    }
    m_parent->Refresh();
    m_parent->Update();
    delete this;
}

