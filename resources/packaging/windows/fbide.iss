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
Compression=lzma2/max
SolidCompression=yes
OutputDir={#FBIDE_OUTPUT_DIR}
OutputBaseFilename={#FBIDE_OUTPUT_BASE}
; File associations are registered below; let Inno notify the shell so
; icons refresh without a reboot.
ChangesAssociations=yes
; Win10 is the floor (matches the toolchain CRT / target platforms).
MinVersion=10.0
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
Name: "assocbas"; Description: "Associate FreeBASIC source files (.bas, .bi) with FBIde"; GroupDescription: "File associations:"
Name: "assocfbs"; Description: "Associate FBIde session files (.fbs) with FBIde"; GroupDescription: "File associations:"

[Files]
Source: "{#FBIDE_STAGE_DIR}\fbide.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#FBIDE_STAGE_DIR}\ide\*"; DestDir: "{app}\ide"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\FBIde"; Filename: "{app}\fbide.exe"
Name: "{group}\{cm:UninstallProgram,FBIde}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\FBIde"; Filename: "{app}\fbide.exe"; Tasks: desktopicon

[Registry]
; FreeBASIC source files (.bas, .bi) -> open in FBIde as documents.
Root: HKA; Subkey: "Software\Classes\.bas"; ValueType: string; ValueName: ""; ValueData: "FBIde.SourceFile"; Flags: uninsdeletevalue; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\.bi"; ValueType: string; ValueName: ""; ValueData: "FBIde.SourceFile"; Flags: uninsdeletevalue; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.SourceFile"; ValueType: string; ValueName: ""; ValueData: "FreeBASIC Source File"; Flags: uninsdeletekey; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.SourceFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\fbide.exe,0"; Tasks: assocbas
Root: HKA; Subkey: "Software\Classes\FBIde.SourceFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\fbide.exe"" ""%1"""; Tasks: assocbas
; FBIde session files (.fbs) -> restored via --load-session, not opened as text.
Root: HKA; Subkey: "Software\Classes\.fbs"; ValueType: string; ValueName: ""; ValueData: "FBIde.SessionFile"; Flags: uninsdeletevalue; Tasks: assocfbs
Root: HKA; Subkey: "Software\Classes\FBIde.SessionFile"; ValueType: string; ValueName: ""; ValueData: "FBIde Session"; Flags: uninsdeletekey; Tasks: assocfbs
Root: HKA; Subkey: "Software\Classes\FBIde.SessionFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\fbide.exe,0"; Tasks: assocfbs
Root: HKA; Subkey: "Software\Classes\FBIde.SessionFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\fbide.exe"" --load-session ""%1"""; Tasks: assocfbs

[Run]
Filename: "{app}\fbide.exe"; Description: "{cm:LaunchProgram,FBIde}"; Flags: nowait postinstall skipifsilent
