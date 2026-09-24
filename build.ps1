# Configure + build the plugin with MSVC (Build Tools 2022) and Ninja. The CMake
# POST_BUILD step then deploys it into the game's d2rloader/plugins folder.
param([string]$Config = "Release")
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$root = $PSScriptRoot
$build = Join-Path $root "build"
# UTF-8 makes cl print /showIncludes the way CMake recorded its prefix; under the
# OEM code page the accented Spanish prefix differs and Ninja tracks no headers.
cmd /c "chcp 65001 >nul && `"$vcvars`" && cmake -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config && cmake --build `"$build`""
exit $LASTEXITCODE
