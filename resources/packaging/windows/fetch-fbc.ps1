<#
.SYNOPSIS
    Download a FreeBASIC compiler release and flatten it for bundling.

.DESCRIPTION
    Fetches an official FreeBASIC binary archive from SourceForge, extracts
    it, and flattens the single top-level folder so fbc.exe lands directly in
    -DestDir (alongside bin\, inc\, lib\). build-installer.ps1 then copies that
    tree into {app} next to fbide.exe, where FBIde's first-run auto-detect
    finds it.

    Architecture selects which release is used:
      x64 -> winlibs build (ships both 32- and 64-bit target libraries, so the
             bundled fbc can compile -target win32 *and* win64).
      x86 -> plain win32 build (32-bit fbc only).

    The links are pinned and version-stamped: the winlibs asset name embeds the
    bundled gcc version (e.g. -winlibs-gcc-9.3.0), which changes per FreeBASIC
    release, so there is no clean "latest" URL to follow. Bump -Version (and the
    gcc suffix in the x64 URL below) when moving to a newer FreeBASIC, or pass
    -Url to override entirely.

.PARAMETER Arch
    x64 or x86. Selects the winlibs vs win32 release.

.PARAMETER DestDir
    Where the flattened payload is written (created/cleaned). fbc.exe ends up
    at <DestDir>\fbc.exe.

.PARAMETER Version
    FreeBASIC version to fetch. Default 1.10.1.

.PARAMETER Url
    Override the download URL entirely (ignores -Version / -Arch mapping).

.PARAMETER CacheDir
    Optional. Directory to keep the downloaded .7z in so repeat runs (and CI
    cache restores) skip the download. Default: a temp dir (no reuse).

.EXAMPLE
    resources\packaging\windows\fetch-fbc.ps1 -Arch x64 -DestDir build\fbc-x64
#>
[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string] $Arch = 'x64',
    [Parameter(Mandatory)]
    [string] $DestDir,
    [string] $Version = '1.10.1',
    [string] $Url,
    [string] $CacheDir
)

$ErrorActionPreference = 'Stop'

# Map arch -> SourceForge asset. The /fbc/ short path redirects to the current
# mirror; curl -L follows it.
if (-not $Url) {
    $Url = if ($Arch -eq 'x64') {
        "https://downloads.sourceforge.net/fbc/FreeBASIC-$Version-winlibs-gcc-9.3.0.7z"
    } else {
        "https://downloads.sourceforge.net/fbc/FreeBASIC-$Version-win32.7z"
    }
}

# Locate 7-Zip: PATH first, then the default install location.
$sevenZip = (Get-Command 7z -ErrorAction SilentlyContinue).Source
if (-not $sevenZip) {
    $default = 'C:\Program Files\7-Zip\7z.exe'
    if (Test-Path $default) { $sevenZip = $default }
}
if (-not $sevenZip) {
    throw '7z.exe not found. Install 7-Zip or add it to PATH.'
}

# Resolve the archive path: cache it when -CacheDir is given so reruns skip the
# download, otherwise drop it in a temp dir.
$archiveName = Split-Path $Url -Leaf
if ($CacheDir) {
    New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null
    $archive = Join-Path (Resolve-Path $CacheDir).Path $archiveName
} else {
    $archive = Join-Path ([System.IO.Path]::GetTempPath()) $archiveName
}

if (Test-Path $archive) {
    Write-Host "Using cached archive: $archive" -ForegroundColor Cyan
} else {
    Write-Host "Downloading $Url" -ForegroundColor Cyan
    & curl.exe -fL --retry 3 -o $archive $Url
    if ($LASTEXITCODE -ne 0) { throw "Download failed ($LASTEXITCODE): $Url" }
}

# Extract into a scratch dir, then flatten the single top-level folder into
# DestDir. Start from a clean DestDir so reruns don't merge stale trees.
$scratch = Join-Path ([System.IO.Path]::GetTempPath()) ("fbc-extract-" + [System.IO.Path]::GetFileNameWithoutExtension($archiveName))
if (Test-Path $scratch) { Remove-Item $scratch -Recurse -Force }
New-Item -ItemType Directory -Force -Path $scratch | Out-Null

Write-Host "Extracting $archiveName" -ForegroundColor Cyan
& $sevenZip x $archive "-o$scratch" -y | Out-Null
if ($LASTEXITCODE -ne 0) { throw "7z extraction failed ($LASTEXITCODE)." }

# Locate the fbc compiler anywhere in the extracted tree; its containing
# directory is the payload root (it sits next to bin\ inc\ lib\). The two
# release lines name it differently: the win32 build ships a single fbc.exe,
# while the winlibs build ships fbc32.exe + fbc64.exe (no plain fbc.exe). Match
# any of them, and stay robust whether the archive wraps everything in a
# top-level FreeBASIC-<...> folder or not.
$fbcNames = @('fbc.exe', 'fbc64.exe', 'fbc32.exe')
$fbcExe = Get-ChildItem -Path $scratch -File -Recurse |
    Where-Object { $fbcNames -contains $_.Name } | Select-Object -First 1
if (-not $fbcExe) { throw "Archive '$archiveName' contains no fbc compiler (fbc.exe/fbc64.exe/fbc32.exe)." }
$payloadRoot = $fbcExe.Directory.FullName

if (Test-Path $DestDir) { Remove-Item $DestDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
Get-ChildItem -Force $payloadRoot | Move-Item -Destination $DestDir
Remove-Item $scratch -Recurse -Force

if (-not ($fbcNames | Where-Object { Test-Path (Join-Path $DestDir $_) })) {
    throw "Flattened payload has no fbc compiler in '$DestDir' - unexpected archive layout."
}
Write-Host "FreeBASIC ready: $((Resolve-Path $DestDir).Path)" -ForegroundColor Green
