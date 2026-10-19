# =============================================================================
#  WORKSTATION-AWARE BUILD AND DEPLOY SCRIPT
# =============================================================================
#  This replaces the upstream build.ps1, which hardcodes VS 2022 Build Tools
#  and the maintainer's Steam path. Neither exists on this machine.
#
#  Previously kept local; included on bladestorm at the user's explicit
#  request on 2026-10-19 to publish all edits. It is tracked normally now.
#
#  Usage:
#      .\build.ps1                 # Release build + deploy
#      .\build.ps1 -Config Debug
#      .\build.ps1 -Test           # build, then ctest
#      .\build.ps1 -Fresh          # wipe the build dir first
#      .\build.ps1 -Jobs 8         # more parallel compilers (default 4)
#
#  The build runs at below-normal priority with 4 compilers at a time, so the
#  rest of the PC stays responsive. Ninja's default (cores + 2 = 14 here) ran
#  the machine out of memory and made every other window lag (the user,
#  2026-09-27).
# =============================================================================
param(
    [string]$Config = "Release",
    [switch]$Test,
    [switch]$Fresh,
    [int]$Jobs = 4
)

$ErrorActionPreference = "Stop"

# Child processes (cmd, cmake, ninja, cl, link, ctest) inherit a below-normal
# priority class, so whatever the user is doing is scheduled first.
try { (Get-Process -Id $PID).PriorityClass = 'BelowNormal' } catch { }
if ($Jobs -lt 1) { $Jobs = 1 }

$root  = $PSScriptRoot
$build = Join-Path $root "build"

# --- Toolchain --------------------------------------------------------------
# Resolve whatever VS is actually installed (here: Community 2026 / v18,
# MSVC 14.50) rather than assuming Build Tools 2022.
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - is Visual Studio installed?" }

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { $vsPath = & $vswhere -latest -products * -property installationPath }
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsPath" }

# CMake and Ninja ship inside VS and are not on PATH here.
$extra = @()
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $extra += Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
}
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    $extra += Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
}

# ffmpeg is REQUIRED at configure time (cmake/GroundItems.cmake converts the
# OGG loops to WAV). It is on the persisted user PATH, but a shell started
# before it was installed will not see it - so resolve it dynamically.
if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    $pkgRoot = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    $ff = Get-ChildItem $pkgRoot -Recurse -Filter ffmpeg.exe -ErrorAction SilentlyContinue |
          Select-Object -First 1
    if (-not $ff) {
        throw "ffmpeg not found. Install it: winget install Gyan.FFmpeg.Essentials --scope user"
    }
    $extra += $ff.DirectoryName
}

if ($extra.Count) { $env:PATH = ($extra -join ';') + ';' + $env:PATH }

# --- Deploy target ----------------------------------------------------------
# Priority: explicit override -> a real install found among the known Steam
# locations -> local staging folder. The staging fallback matters because the
# POST_BUILD step is unguarded: pointed at a path with no game it would just
# create a junk tree there.
#
# Candidates, in order: this machine's install (default Steam on the system
# drive), then upstream's hardcoded path (the maintainer's library on D:).
$stage = Join-Path (Split-Path -Parent $root) "stage"
$candidates = @(
    (Join-Path ${env:ProgramFiles(x86)} "Steam\steamapps\common\Diablo II Resurrected"),
    (Join-Path $env:ProgramFiles          "Steam\steamapps\common\Diablo II Resurrected"),
    "D:/games/steam/steamapps/common/Diablo II Resurrected"
)

$found = $candidates | Where-Object { $_ -and (Test-Path (Join-Path $_ "D2R.exe")) } | Select-Object -First 1

if ($env:D2R_GAME_DIR) { $gameDir = $env:D2R_GAME_DIR; $how = "D2R_GAME_DIR override" }
elseif ($found)        { $gameDir = $found;            $how = "detected game install" }
else                   { $gameDir = $stage;            $how = "staging (game not found)" }

$gameDir = $gameDir.Replace([char]92, [char]47)

if ($Fresh -and (Test-Path $build)) { Remove-Item $build -Recurse -Force }

Write-Host "Toolchain : $vsPath"
Write-Host "Deploy to : $gameDir  [$how]"
Write-Host "Config    : $Config"
Write-Host "Jobs      : $Jobs (below-normal priority)"

# Native stderr must not become a terminating error (PS 5.1 NativeCommandError);
# vcvars64 emits a harmless 'vswhere.exe not recognized' line. Exit codes are
# checked explicitly instead.
$ErrorActionPreference = "Continue"

$cfg = "cmake -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config -DD2R_GAME_DIR=`"$gameDir`""
cmd /c "`"$vcvars`" >nul 2>nul && $cfg && cmake --build `"$build`" --parallel $Jobs"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Test) {
    cmd /c "`"$vcvars`" >nul 2>nul && ctest --test-dir `"$build`" --output-on-failure"
}
exit $LASTEXITCODE
