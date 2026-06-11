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

#if FBIDE_ARCH == "x64"
  #define FBIDE_WIN_BITS "64"
#else
  #define FBIDE_WIN_BITS "32"
#endif

#ifndef FBIDE_OUTPUT_BASE
  #define FBIDE_OUTPUT_BASE "fbide-" + FBIDE_VERSION + "-win" + FBIDE_WIN_BITS + "-setup"
#endif

; Optional " and FreeBASIC" suffix for task labels — present only when the
; compiler is bundled (FBIDE_FBC_DIR defined).
#ifdef FBIDE_FBC_DIR
  #define FBIDE_FBC_NOTE " and FreeBASIC"
#else
  #define FBIDE_FBC_NOTE ""
#endif

[Setup]
; Stable AppId — never change it, or upgrades install side-by-side instead
; of replacing the previous version.
AppId={{B8E9F4A2-3C7D-4E1B-9A6F-2D5C8E0A1B3F}
AppName=FBIde
AppVersion={#FBIDE_VERSION}
AppVerName=FBIde {#FBIDE_VERSION}
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

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
; Bundle the FreeBASIC compiler (on by default). Only offered when a payload
; was supplied at build time. When unticked, only FBIde is installed.
#ifdef FBIDE_FBC_DIR
Name: "installfbc"; Description: "Install the FreeBASIC compiler alongside FBIde"
#endif
; .bas/.bi association is optional (on by default). .fbs is always associated
; (registered unconditionally below), so it has no task.
Name: "assocbas"; Description: "Associate FreeBASIC source files (.bas, .bi) with FBIde"; GroupDescription: "File associations:"
; Add the install dir to PATH (on by default). FBIde lives there, and so does
; fbc when the compiler is installed — one entry exposes both.
Name: "modifypath"; Description: "Add FBIde{#FBIDE_FBC_NOTE} to the PATH environment variable"

[Files]
Source: "{#FBIDE_STAGE_DIR}\fbide.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#FBIDE_STAGE_DIR}\ide\*"; DestDir: "{app}\ide"; Flags: ignoreversion recursesubdirs createallsubdirs
; Bundled FreeBASIC compiler (optional). Installed alongside fbide.exe so the
; first-run auto-detect picks it up. The READONLY sentinel stays in ide\ for
; installer builds (unlike the portable zip) so the bundle mirrors to the user
; data dir on launch.
#ifdef FBIDE_FBC_DIR
Source: "{#FBIDE_FBC_DIR}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: installfbc
#endif

[Icons]
Name: "{group}\FBIde"; Filename: "{app}\fbide.exe"
Name: "{group}\{cm:UninstallProgram,FBIde}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\FBIde"; Filename: "{app}\fbide.exe"; Tasks: desktopicon

[Registry]
; Per-type ProgIDs matching the runtime registration (FileAssociations.cpp).
; The icons are the document icons embedded in fbide.exe (app.rc): index 1 = bas,
; 2 = bi, 3 = fbs (0 is the app icon).
; FreeBASIC source (.bas)
Root: HKA; Subkey: "Software\Classes\.bas"; ValueType: string; ValueName: ""; ValueData: "FBIde.bas"; Flags: uninsdeletevalue; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.bas"; ValueType: string; ValueName: ""; ValueData: "FreeBASIC Source File"; Flags: uninsdeletekey; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.bas\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\fbide.exe,1"; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.bas\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\fbide.exe"" ""%1"""; Tasks: assocbas
; FreeBASIC header (.bi)
Root: HKA; Subkey: "Software\Classes\.bi"; ValueType: string; ValueName: ""; ValueData: "FBIde.bi"; Flags: uninsdeletevalue; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.bi"; ValueType: string; ValueName: ""; ValueData: "FreeBASIC Header File"; Flags: uninsdeletekey; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.bi\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\fbide.exe,2"; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.bi\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\fbide.exe"" ""%1"""; Tasks: assocbas
; FBIde session (.fbs) -> opened like any document. Always associated.
Root: HKA; Subkey: "Software\Classes\.fbs"; ValueType: string; ValueName: ""; ValueData: "FBIde.fbs"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\FBIde.fbs"; ValueType: string; ValueName: ""; ValueData: "FBIde Session"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\FBIde.fbs\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\fbide.exe,3"
Root: HKA; Subkey: "Software\Classes\FBIde.fbs\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\fbide.exe"" ""%1"""

[Run]
Filename: "{app}\fbide.exe"; Description: "{cm:LaunchProgram,FBIde}"; Flags: nowait postinstall skipifsilent

[Code]
// ---------------------------------------------------------------------------
// "Add to PATH" task. Inno has no built-in PATH editor, so append/remove the
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

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('modifypath') then
    EnvAddPath(ExpandConstant('{app}'));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  // Always attempt removal — EnvRemovePath no-ops when {app} isn't present.
  if CurUninstallStep = usUninstall then
    EnvRemovePath(ExpandConstant('{app}'));
end;
