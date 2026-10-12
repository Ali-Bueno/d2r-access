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
| `D2RLoader.exe`, `D2RCore.dll`, `d2rloader.mpq`, `d2rloader/config/d2rloader.toml` | `-LoaderDir`, default `../loader-backups/1.3.1-beta` (kept out of the repo) |
| `msvcp140.dll`, `msvcp140_atomic_wait.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` | newest `VC/Redist/MSVC/<version>/x64/Microsoft.VC*.CRT` of Build Tools 2022 |
| `d2rloader/plugins/d2rl-d2r-access.dll`, `d2rloader/plugins/d2r-access/prism.dll` | the build, `third_party/prism/bin` |
| `d2rloader/config/d2r-access.toml` | `radar.toml` |
| `README.txt`, `KEYBOARD.txt` | `packaging/tester/` (English; `-NewsFile` becomes the README's WHAT'S NEW section and the release notes) |
| `licenses/` | `LICENSE`, `third_party/prism/LICENSES` |
| empty `mods/`, `d2rloader/{crashes,data,logs}` | the loader's own layout |

Extracting over an existing install replaces `d2rloader.toml`, which resets `text_locale` and
`audio_locale`; the README tells testers to set the language again.

The package text is English since 2026-10-11, for testers who do not read Spanish. The asset
keeps its original `d2r-access-completo.zip` name so the always-latest link stays valid.
