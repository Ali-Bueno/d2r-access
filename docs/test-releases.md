# Test releases

Testers who cannot build download one ZIP with everything needed to play, extracted over the
game folder. The newest one is always at:

<https://github.com/Ali-Bueno/d2r-access/releases/latest/download/d2r-access-completo.zip>

The repository is private, so the link works only for a signed-in GitHub account with access.

## Publishing one

```powershell
.\tools\test-release.ps1 -NewsFile <news.txt>   # build, test, package, publish
.\tools\test-release.ps1 -NoPublish              # same, but stop at the local ZIP
```

The script builds `HEAD` in `build/test-release/cmake`, whose deploy step points at
`build/test-release/deploy`, so the plugin installed in the game is left alone. It refuses to
publish while build inputs (`src`, `cmake`, `sounds`, `third_party`, …) have uncommitted changes
or `HEAD` is not pushed. Every ZIP entry is checked against its source before publishing.

Each run creates a normal release tagged `test-YYYY-MM-DD` (`-2`, `-3`… on the same day) and marks
it latest. The tag is chosen before the build and compiled in (`build.ps1 -Release`), so the
plugin says it at start; a local build says "development build". They are not prereleases on purpose: GitHub's `releases/latest` link skips
prereleases. The asset name never changes, so the link above stays valid.

## Package contents

| Entry | Source |
|---|---|
| `D2RLoader.exe`, `D2RCore.dll`, `d2rloader.mpq` | `-LoaderDir`, default `../loader-backups/1.3.1-beta` (kept out of the repo) |
| `msvcp140.dll`, `msvcp140_atomic_wait.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` | newest `VC/Redist/MSVC/<version>/x64/Microsoft.VC*.CRT` of Build Tools 2022 |
| `d2rloader/plugins/d2rl-d2r-access.dll`, `d2rloader/plugins/d2r-access/prism.dll` | the build, `third_party/prism/bin` |
| `README.txt`, `KEYBOARD.txt` | `packaging/tester/` (English; `-NewsFile` becomes the README's WHAT'S NEW section and the release notes) |
| `licenses/` | `LICENSE`, `third_party/prism/LICENSES` |
| empty `mods/`, `d2rloader/{crashes,data,logs}` | the loader's own layout |

**No config files in the package** (2026-10-31). Until test-2026-10-31 the zip carried the
loader's default `d2rloader.toml` and `radar.toml` as `d2r-access.toml`, so every update reset the
tester's language, extra shared stash pages (`[d2rcore.stash] add_shared_tabs`) and own mod edits.
Neither is needed: `D2RLoader.exe` embeds a byte-identical copy of the default `d2rloader.toml`
(9,235 bytes) and logs `Configuration synchronized.` at start, and the PluginSDK README says the
loader writes `<plugin-id>.toml` from the DLL's embedded `radar.toml` only when it is missing.
`test-release.ps1` refuses to package any `*.toml`. Consequence: a key already in a player's
`d2r-access.toml` keeps its value, so a changed default reaches existing installs only through a
new key; a missing key falls back to the compiled default (`radar_config.cpp`). Not yet seen live:
a fresh install starting without `d2rloader.toml`.

The package text is English since 2026-10-11, for testers who do not read Spanish. The asset
keeps its original `d2r-access-completo.zip` name so the always-latest link stays valid.
