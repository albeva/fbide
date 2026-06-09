<#
.SYNOPSIS
    Build the FBIde Windows installer (Inno Setup) from a built tree.

.DESCRIPTION
    Stages the install payload (fbide.exe + ide/) via `cmake --install`,
    resolves the version from CMakeLists (the FBIDE_VERSION_ONLY fast path,
    same as CI), and compiles resources/packaging/windows/fbide.iss with ISCC.

    fbide must already be built in -BuildDir. Output:
        <OutputDir>/fbide-<version>-win{64,32}-setup.exe

.PARAMETER BuildDir
    CMake build directory containing a built fbide.exe.
    Default: build/claude/msvc/debug (relative to the repo root).

.PARAMETER Arch
    Target architecture, x64 or x86. Must match the build. Default x64.

.PARAMETER Version
    Override the display version. Default: probed from CMakeLists.

.PARAMETER OutputDir
    Where the setup EXE is written. Default: <root>/build/installer.

.PARAMETER Iscc
    Path to ISCC.exe. Default: autodetect (Inno Setup 6 install dir, then PATH).

.EXAMPLE
    resources\packaging\windows\build-installer.ps1

.EXAMPLE
    resources\packaging\windows\build-installer.ps1 -BuildDir build\claude\msvc\debug -Arch x64
#>
[CmdletBinding()]
param(
    [string] $BuildDir = 'build/claude/msvc/debug',
    [ValidateSet('x64', 'x86')]
    [string] $Arch = 'x64',
    [string] $Version,
    [string] $OutputDir,
    [string] $Iscc
)

$ErrorActionPreference = 'Stop'

# Repo root = three levels up from this script (resources/packaging/windows/).
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$iss = Join-Path $PSScriptRoot 'fbide.iss'

# Resolve the build directory to an absolute path.
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $root $BuildDir }
if (-not (Test-Path $buildPath)) {
    throw "Build dir not found: $buildPath`nBuild fbide first (see CLAUDE.md), or pass -BuildDir."
}

# Locate ISCC: the default Inno Setup 6 install, then PATH.
if (-not $Iscc) {
    $default = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
    if (Test-Path $default) {
        $Iscc = $default
    } else {
        $cmd = Get-Command iscc -ErrorAction SilentlyContinue
        if ($cmd) { $Iscc = $cmd.Source }
    }
}
if (-not $Iscc -or -not (Test-Path $Iscc)) {
    throw 'ISCC.exe not found. Install Inno Setup 6 or pass -Iscc <path>.'
}

# Resolve the version via the CMake fast path (no compiler / wxWidgets needed).
if (-not $Version) {
    $probe = Join-Path $buildPath 'version-probe'
    & cmake -B $probe -S $root -DFBIDE_VERSION_ONLY=ON | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Version probe failed ($LASTEXITCODE)." }
    $Version = (Get-Content (Join-Path $probe 'version.txt') -Raw).Trim()
}

# Numeric x.x.x.x for the VersionInfo resource: strip any tag suffix
# (e.g. 0.5.0.rc-6 -> 0.5.0) and pad to four components.
$parts = (($Version -replace '[^0-9.].*$', '').TrimEnd('.')).Split('.')
while ($parts.Count -lt 4) { $parts += '0' }
$num = ($parts[0..3] -join '.')

# Stage the install payload into a clean prefix.
$stage = Join-Path $buildPath 'installer-stage'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
& cmake --install $buildPath --prefix $stage
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed ($LASTEXITCODE)." }
if (-not (Test-Path (Join-Path $stage 'fbide.exe'))) {
    throw "Staged payload has no fbide.exe - is '$buildPath' actually built?"
}

if (-not $OutputDir) { $OutputDir = Join-Path $root 'build/installer' }
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Write-Host "Building installer: FBIde $Version ($Arch)" -ForegroundColor Cyan
& $Iscc `
    "/DFBIDE_STAGE_DIR=$stage" `
    "/DFBIDE_VERSION=$Version" `
    "/DFBIDE_VERSION_NUMERIC=$num" `
    "/DFBIDE_ARCH=$Arch" `
    "/DFBIDE_SRC_ROOT=$root" `
    "/DFBIDE_OUTPUT_DIR=$OutputDir" `
    $iss
if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)." }

$winBits = if ($Arch -eq 'x64') { '64' } else { '32' }
$setup = Join-Path $OutputDir "fbide-$Version-win$winBits-setup.exe"
Write-Host "Installer: $setup" -ForegroundColor Green
