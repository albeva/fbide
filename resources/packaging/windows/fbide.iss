; Inno Setup script for the FBIde Windows installer.
;
; Compiled by ISCC.exe. All inputs come in as preprocessor defines so the
; CI release job can drive arch / version / payload location, while the
; defaults below let the script compile standalone for a local smoke test.
;
;   FBIDE_STAGE_DIR        staged install payload (contains fbide.exe + ide\).
;                          Produced by `cmake --install <build> --prefix <dir>`.
;   FBIDE_VERSION          display version, e.g. 0.5.0 or 0.5.0.rc-6.
;   FBIDE_VERSION_NUMERIC  numeric x.x.x.x for the VersionInfo resource.
;   FBIDE_ARCH             x64 | x86 — selects install mode + output name.
;   FBIDE_SRC_ROOT         repo root (for the icon + LICENSE). Default "." = git root.
;   FBIDE_OUTPUT_DIR       where the setup EXE is written. Default ".".
;   FBIDE_FBC_DIR          optional. Flattened FreeBASIC payload (fbc.exe at its
;                          root, plus bin\ inc\ lib\). When defined, its whole
;                          tree is installed into {app} next to fbide.exe so the
;                          first-run auto-detect finds the bundled compiler.
;   FBIDE_FBC_VERSION      optional. FreeBASIC version shown in the wizard copy
;                          (e.g. 1.10.1). Defaults to "unknown".

#ifndef FBIDE_STAGE_DIR
  #define FBIDE_STAGE_DIR "package"
#endif
#ifndef FBIDE_VERSION
  #define FBIDE_VERSION "0.0.0-dev"
#endif
#ifndef FBIDE_VERSION_NUMERIC
  #define FBIDE_VERSION_NUMERIC "0.0.0.0"
#endif
#ifndef FBIDE_ARCH
  #define FBIDE_ARCH "x64"
#endif
#ifndef FBIDE_SRC_ROOT
  #define FBIDE_SRC_ROOT "."
#endif
#ifndef FBIDE_OUTPUT_DIR
  #define FBIDE_OUTPUT_DIR "."
#endif
#ifndef FBIDE_FBC_VERSION
  #define FBIDE_FBC_VERSION "unknown"
#endif

#if FBIDE_ARCH == "x64"
  #define FBIDE_WIN_BITS "64"
#else
  #define FBIDE_WIN_BITS "32"
#endif

#ifndef FBIDE_OUTPUT_BASE
  #define FBIDE_OUTPUT_BASE "fbide-" + FBIDE_VERSION + "-win" + FBIDE_WIN_BITS + "-setup"
#endif

[Setup]
; Stable AppId — never change it, or upgrades install side-by-side instead
; of replacing the previous version.
AppId={{B8E9F4A2-3C7D-4E1B-9A6F-2D5C8E0A1B3F}
AppName=FBIde
AppVersion={#FBIDE_VERSION}
#ifdef FBIDE_FBC_DIR
AppVerName=FBIde {#FBIDE_VERSION} (with FreeBASIC {#FBIDE_FBC_VERSION})
#else
AppVerName=FBIde {#FBIDE_VERSION}
#endif
AppPublisher=Albert Varaksin
AppPublisherURL=https://github.com/albeva/fbide
AppSupportURL=https://github.com/albeva/fbide/issues
AppUpdatesURL=https://github.com/albeva/fbide/releases
VersionInfoVersion={#FBIDE_VERSION_NUMERIC}
VersionInfoProductVersion={#FBIDE_VERSION_NUMERIC}
DefaultDirName={autopf}\FBIde
DefaultGroupName=FBIde
DisableProgramGroupPage=yes
UninstallDisplayName=FBIde {#FBIDE_VERSION}
UninstallDisplayIcon={app}\fbide.exe
LicenseFile={#FBIDE_SRC_ROOT}\LICENSE
SetupIconFile={#FBIDE_SRC_ROOT}\resources\images\installer.ico
; Wizard side image on the Welcome/Finished pages. PNG is supported since
; Inno 6.3; the art is authored at the 164:314 wizard aspect.
WizardImageFile={#FBIDE_SRC_ROOT}\resources\images\installer-side.png
WizardStyle=modern
; Modern style disables the Welcome page by default, but that page is where the
; side image (WizardImageFile) is shown -- re-enable it.
DisableWelcomePage=no
Compression=lzma2/max
SolidCompression=yes
OutputDir={#FBIDE_OUTPUT_DIR}
OutputBaseFilename={#FBIDE_OUTPUT_BASE}
; File associations are registered below; let Inno notify the shell so
; icons refresh without a reboot.
ChangesAssociations=yes
; The optional "Add to PATH" task edits the Path environment variable from
; [Code]; this broadcasts WM_SETTINGCHANGE so new shells see it immediately.
ChangesEnvironment=yes
; Windows 7 SP1 is the floor: the x86/x64 builds statically link the (U)CRT so
; they run on Win7+, and Win7 SP1 is also Inno 6's own minimum. ARM64 has no
; installer (zip only), so the lower floor doesn't affect it.
MinVersion=6.1sp1
; Default to the unprivileged install; the dialog lets the user elevate to
; an all-users install if they want it.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
#if FBIDE_ARCH == "x64"
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
; Make the welcome copy state exactly what is installed, with versions. The
; install options live on a custom page before the licence (see [Code]).
#ifdef FBIDE_FBC_DIR
WelcomeLabel2=This will install FBIde {#FBIDE_VERSION} together with the FreeBASIC compiler {#FBIDE_FBC_VERSION} on your computer.%n%nIt is recommended that you close all other applications before continuing.
#else
WelcomeLabel2=This will install FBIde {#FBIDE_VERSION} on your computer.%n%nIt is recommended that you close all other applications before continuing.
#endif

; NOTE: there is no [Tasks] section. The install options (FreeBASIC, file
; associations, PATH, desktop shortcut) are presented on a custom wizard page
; created in [Code] and placed *before* the licence page; the [Files],
; [Registry] and [Icons] entries below gate on those choices via Check:
; functions (WantFbc / WantAssocBas / WantDesktop) and the PATH edit runs from
; CurStepChanged.

[Files]
Source: "{#FBIDE_STAGE_DIR}\fbide.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#FBIDE_STAGE_DIR}\ide\*"; DestDir: "{app}\ide"; Flags: ignoreversion recursesubdirs createallsubdirs
; Bundled FreeBASIC compiler (optional). Installed alongside fbide.exe so the
; first-run auto-detect picks it up. The READONLY sentinel stays in ide\ for
; installer builds (unlike the portable zip) so the bundle mirrors to the user
; data dir on launch.
#ifdef FBIDE_FBC_DIR
Source: "{#FBIDE_FBC_DIR}\*"; DestDir: "{app}"; Excludes: "FB-manual-*.chm"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: WantFbc
; The bundled FreeBASIC manual (CHM) installs regardless of the compiler
; choice — F1 help should work even when the user skips installing fbc. Kept
; out of the compiler glob above (Excludes) so it isn't copied twice when the
; compiler *is* installed.
Source: "{#FBIDE_FBC_DIR}\FB-manual-*.chm"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
; FreeBASIC licence text shown on its own wizard page. dontcopy = bundled with
; Setup and extracted to {tmp} for display, never installed.
Source: "{#FBIDE_SRC_ROOT}\resources\packaging\windows\freebasic-license.txt"; Flags: dontcopy
#endif

[Icons]
Name: "{group}\FBIde"; Filename: "{app}\fbide.exe"
Name: "{group}\{cm:UninstallProgram,FBIde}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\FBIde"; Filename: "{app}\fbide.exe"; Check: WantDesktop

[Registry]
; Marker telling the installed build that the installer owns the file
; associations (registered below). FileAssociations.cpp checks this and skips
; runtime self-registration, so a type the user opted out of in the wizard is
; never re-asserted on launch. Written regardless of the association choices.
Root: HKA; Subkey: "Software\FBIde"; ValueType: dword; ValueName: "Installed"; ValueData: 1; Flags: uninsdeletekey
; Per-type ProgIDs matching the runtime registration (FileAssociations.cpp).
; The icons are the document icons embedded in fbide.exe (app.rc): index 1 = bas,
; 2 = bi, 3 = fbs (0 is the app icon).
; FreeBASIC source (.bas)
Root: HKA; Subkey: "Software\Classes\.bas"; ValueType: string; ValueName: ""; ValueData: "FBIde.bas"; Flags: uninsdeletevalue; Check: WantAssocBas
Root: HKA; Subkey: "Software\Classes\FBIde.bas"; ValueType: string; ValueName: ""; ValueData: "FreeBASIC Source File"; Flags: uninsdeletekey; Check: WantAssocBas
Root: HKA; Subkey: "Software\Classes\FBIde.bas\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\fbide.exe,1"; Check: WantAssocBas
Root: HKA; Subkey: "Software\Classes\FBIde.bas\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\fbide.exe"" ""%1"""; Check: WantAssocBas
; FreeBASIC header (.bi)
Root: HKA; Subkey: "Software\Classes\.bi"; ValueType: string; ValueName: ""; ValueData: "FBIde.bi"; Flags: uninsdeletevalue; Check: WantAssocBas
Root: HKA; Subkey: "Software\Classes\FBIde.bi"; ValueType: string; ValueName: ""; ValueData: "FreeBASIC Header File"; Flags: uninsdeletekey; Check: WantAssocBas
Root: HKA; Subkey: "Software\Classes\FBIde.bi\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\fbide.exe,2"; Check: WantAssocBas
Root: HKA; Subkey: "Software\Classes\FBIde.bi\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\fbide.exe"" ""%1"""; Check: WantAssocBas
; FBIde session (.fbs) -> opened like any document. Always associated.
Root: HKA; Subkey: "Software\Classes\.fbs"; ValueType: string; ValueName: ""; ValueData: "FBIde.fbs"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\FBIde.fbs"; ValueType: string; ValueName: ""; ValueData: "FBIde Session"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\FBIde.fbs\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\fbide.exe,3"
Root: HKA; Subkey: "Software\Classes\FBIde.fbs\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\fbide.exe"" ""%1"""

[Run]
Filename: "{app}\fbide.exe"; Description: "{cm:LaunchProgram,FBIde}"; Flags: nowait postinstall skipifsilent

[Code]
// ---------------------------------------------------------------------------
// Install options live on a custom page (OptionsPage) shown after Welcome and
// *before* the licence. The [Files]/[Registry]/[Icons] entries gate on these
// via the Want* getters. When FreeBASIC is bundled, a second licence page
// (FbcLicPage) follows the FBIde licence and is skipped unless the compiler is
// being installed (ShouldSkipPage).
// ---------------------------------------------------------------------------
var
  OptionsPage: TInputOptionWizardPage;
  OptAssocBas, OptAddPath, OptDesktop: Integer;
#ifdef FBIDE_FBC_DIR
  FbcLicPage: TOutputMsgMemoWizardPage;
  OptInstallFbc: Integer;
#endif

function WantFbc: Boolean;
begin
#ifdef FBIDE_FBC_DIR
  Result := OptionsPage.Values[OptInstallFbc];
#else
  Result := False;
#endif
end;

function WantAssocBas: Boolean;
begin
  Result := OptionsPage.Values[OptAssocBas];
end;

function WantAddPath: Boolean;
begin
  Result := OptionsPage.Values[OptAddPath];
end;

function WantDesktop: Boolean;
begin
  Result := OptionsPage.Values[OptDesktop];
end;

// ---------------------------------------------------------------------------
// "Add to PATH" option. Inno has no built-in PATH editor, so append/remove the
// install dir ({app}) on the appropriate Environment key: the system key under
// HKLM for an all-users (admin) install, the per-user key under HKCU
// otherwise. ChangesEnvironment=yes makes Inno broadcast WM_SETTINGCHANGE.
// ---------------------------------------------------------------------------
const
  EnvHKLM = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  EnvHKCU = 'Environment';

function EnvRootKey: Integer;
begin
  if IsAdminInstallMode then Result := HKEY_LOCAL_MACHINE else Result := HKEY_CURRENT_USER;
end;

function EnvSubKey: string;
begin
  if IsAdminInstallMode then Result := EnvHKLM else Result := EnvHKCU;
end;

// Case-insensitive whole-segment membership test (semicolon padding avoids
// matching a substring of a longer path).
function PathContains(const PathList, Dir: string): Boolean;
begin
  Result := Pos(';' + Uppercase(Dir) + ';', ';' + Uppercase(PathList) + ';') > 0;
end;

procedure EnvAddPath(const Dir: string);
var
  PathList: string;
begin
  if not RegQueryStringValue(EnvRootKey, EnvSubKey, 'Path', PathList) then
    PathList := '';
  if PathContains(PathList, Dir) then
    exit;
  if (PathList <> '') and (PathList[Length(PathList)] <> ';') then
    PathList := PathList + ';';
  RegWriteExpandStringValue(EnvRootKey, EnvSubKey, 'Path', PathList + Dir);
end;

procedure EnvRemovePath(const Dir: string);
var
  PathList, Rebuilt: string;
  Parts: TStringList;
  I: Integer;
begin
  if not RegQueryStringValue(EnvRootKey, EnvSubKey, 'Path', PathList) then
    exit;
  if not PathContains(PathList, Dir) then
    exit;
  Parts := TStringList.Create;
  try
    Parts.StrictDelimiter := True;
    Parts.Delimiter := ';';
    Parts.DelimitedText := PathList;
    Rebuilt := '';
    for I := 0 to Parts.Count - 1 do
      if (Parts[I] <> '') and (Uppercase(Parts[I]) <> Uppercase(Dir)) then
      begin
        if Rebuilt <> '' then Rebuilt := Rebuilt + ';';
        Rebuilt := Rebuilt + Parts[I];
      end;
    RegWriteExpandStringValue(EnvRootKey, EnvSubKey, 'Path', Rebuilt);
  finally
    Parts.Free;
  end;
end;

#ifdef FBIDE_FBC_DIR
// Strip the Mark-of-the-Web (the Zone.Identifier alternate data stream) from
// the bundled CHM manual(s) so Windows doesn't block help topics. Files an
// installer writes normally carry no MOTW, so this is belt-and-suspenders;
// DeleteFile on a "<file>:<stream>" path removes just the stream and no-ops
// when it isn't there.
procedure StripChmMotw;
var
  AppDir: string;
  FindRec: TFindRec;
begin
  AppDir := ExpandConstant('{app}');
  if FindFirst(AppDir + '\FB-manual-*.chm', FindRec) then
  try
    repeat
      DeleteFile(AppDir + '\' + FindRec.Name + ':Zone.Identifier');
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;
#endif

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WantAddPath then
    EnvAddPath(ExpandConstant('{app}'));
#ifdef FBIDE_FBC_DIR
  // The manual is always installed (independent of the compiler choice), so
  // strip its MOTW unconditionally. No-op when no CHM is present.
  if CurStep = ssPostInstall then
    StripChmMotw;
#endif
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataDir: string;
begin
  // Always attempt PATH removal — EnvRemovePath no-ops when {app} isn't present.
  if CurUninstallStep = usUninstall then
    EnvRemovePath(ExpandConstant('{app}'));

  // After the program files are gone, offer to delete the per-user settings
  // folder FBIde writes under %APPDATA% (config overlays, themes, recent-file
  // history, logs, the mirrored ide\ resources). It's the current user's data
  // only — a system-wide install can't reach other users' profiles.
  // SuppressibleMsgBox returns the default (No) under a silent uninstall, so
  // unattended removals never wipe data without being asked.
  if CurUninstallStep = usPostUninstall then
  begin
    DataDir := ExpandConstant('{userappdata}\fbide');
    if DirExists(DataDir) then
      if SuppressibleMsgBox(
           'Also remove your FBIde settings and data?' + #13#10 + #13#10 +
           DataDir + #13#10 + #13#10 +
           'Choose No to keep your settings, themes and recent-file history.',
           mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDNO) = IDYES then
        DelTree(DataDir, True, True, True);
  end;
end;

// ---------------------------------------------------------------------------
// Wizard pages
// ---------------------------------------------------------------------------
procedure InitializeWizard;
#ifdef FBIDE_FBC_DIR
var
  LicText: AnsiString;
#endif
begin
  // Options page — inserted after Welcome, so it precedes the licence page(s).
  OptionsPage := CreateInputOptionPage(wpWelcome,
    'Setup Options', 'Choose what to install and configure',
    'Select the options you want, then click Next.', False, False);
#ifdef FBIDE_FBC_DIR
  OptInstallFbc := OptionsPage.Add('Install the FreeBASIC compiler {#FBIDE_FBC_VERSION} alongside FBIde');
#endif
  OptAssocBas := OptionsPage.Add('Associate .bas and .bi files with FBIde');
  OptAddPath  := OptionsPage.Add('Add the install folder to the PATH environment variable');
  OptDesktop  := OptionsPage.Add('Create a desktop shortcut');

  // Defaults: everything on except the desktop shortcut.
#ifdef FBIDE_FBC_DIR
  OptionsPage.Values[OptInstallFbc] := True;
#endif
  OptionsPage.Values[OptAssocBas] := True;
  OptionsPage.Values[OptAddPath]  := True;
  OptionsPage.Values[OptDesktop]  := False;

#ifdef FBIDE_FBC_DIR
  // Second licence page for the bundled FreeBASIC compiler, right after the
  // FBIde licence. Text ships with Setup (dontcopy) and is read from {tmp}.
  ExtractTemporaryFile('freebasic-license.txt');
  if not LoadStringFromFile(ExpandConstant('{tmp}\freebasic-license.txt'), LicText) then
    LicText := 'FreeBASIC is distributed under its own licence. See the FreeBASIC documentation in the install folder and https://www.freebasic.net.';
  FbcLicPage := CreateOutputMsgMemoPage(wpLicense,
    'FreeBASIC Licence', 'FBIde bundles the FreeBASIC compiler',
    'FreeBASIC is installed alongside FBIde and is covered by its own licence terms:',
    LicText);
#endif
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
#ifdef FBIDE_FBC_DIR
  // Skip the FreeBASIC licence when the compiler isn't being installed.
  if PageID = FbcLicPage.ID then
    Result := not WantFbc;
#endif
end;
