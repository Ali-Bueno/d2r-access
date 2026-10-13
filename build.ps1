# Configure + build the plugin with MSVC (Build Tools 2022) and Ninja. The CMake
# POST_BUILD step then deploys it into the game's d2rloader/plugins folder, or into
# -GameDir when given (the test release uses that to leave the install untouched).
# -Release names the build the plugin speaks at start (the test release's tag).
param([string]$Config = "Release", [string]$BuildDir, [string]$GameDir, [string]$Release)
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$root = $PSScriptRoot
$build = if ($BuildDir) { $BuildDir } else { Join-Path $root "build" }
$gameArg = if ($GameDir) { "-DD2R_GAME_DIR=`"$($GameDir -replace '\\', '/')`"" } else { "" }
$releaseArg = if ($Release) { "-DD2R_ACCESS_RELEASE=$Release" } else { "" }
# UTF-8 makes cl print /showIncludes the way CMake recorded its prefix; under the
# OEM code page the accented Spanish prefix differs and Ninja tracks no headers.
# Be gentle on the PC: child processes inherit the lower priority and a capped job count.
(Get-Process -Id $PID).PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
$jobs = [Math]::Max(1, [Environment]::ProcessorCount - 2)
cmd /c "chcp 65001 >nul && `"$vcvars`" && cmake -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config $gameArg $releaseArg && cmake --build `"$build`" --parallel $jobs"
exit $LASTEXITCODE
