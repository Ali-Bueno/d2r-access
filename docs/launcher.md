# D2RAccess.exe, the launcher

Why: D2RLoader runs the game offline, and Blizzard blocks offline play 30 days after the account
last reached the main menu online. Measured live: starting the unmodded `D2R.exe` and reaching the main
menu writes `"User Last Online"` in `Saved Games\Diablo II Resurrected\Settings.json`
([release-launch.md](release-launch.md), "Asked again, 2026-11-01"). The launcher does that
automatically, without sight, then starts `D2RLoader.exe`. It also offers newer releases ("Self-update" below).

Shipped in the game folder next to `D2R.exe` and `D2RLoader.exe`. Sources: `launcher/` (pure logic in
`launcher_logic.*`, tested by `tests/launcher_logic_tests.cpp`; Win32 in `launcher_system.*`; flow in
`launcher_main.cpp`). Sentences: the `launcher*` fields of `src/speech_strings.h`, all 13 languages.

## Flow

1. Game folder is the exe's own folder. Arguments (Steam's `%command%` adds the `D2R.exe` path) go to
   `D2RLoader.exe` unchanged, environment inherited.
2. `User Last Online` younger than 20 days: start the loader, silent.
3. Otherwise speak that the license is validated online (about half a minute). A Steam copy
   (`steam_appid.txt`) starts `steam.exe -silent` if needed and waits for `ActiveUser` (60 s). Start
   `D2R.exe` without arguments.
4. Every 250 ms read `Settings.json`; every 1.5 s focus the game window and hold Escape 600 ms
   (SendInput, scan code). Escape never activates Play at the main menu. When the date changes: release,
   `WM_CLOSE`, wait 10 s, `TerminateProcess`, then speak "License validated" and start the loader.
5. Timeout (120 s), the game exiting by itself, or any start failure: close the game. Date 20-30 days
   old: speak that the game starts anyway and start the loader. 30 days or more, or no date at all:
   speak to check the internet and try again; no loader.

Language: `text_locale` under `[d2rloader]` in `d2rloader\config\d2rloader.toml`, else English.
Speech: PRISM from `d2rloader\plugins\d2r-access\prism.dll`; silent if it cannot load. The launcher waits
about 70 ms per character of the last sentence before exiting.

## Constants

`launcher/launcher_logic.h`: `ValidateAfterDays` 20, `OfflineLimitDays` 30, `EscapeHoldMs` 600,
`EscapePeriodMs` 1500, `PollMs` 250, `WaitTimeoutMs` 120000, `CloseWaitMs` 10000, `KillWaitMs` 5000,
`SteamTimeoutMs` 60000.

## Log

`d2rloader\logs\d2r-access-launcher.log`, appended: start, language, the date's age, each step and
sentence, the result. Nothing per poll.

## Build

`D2RAccess.exe` is a CMake target; the build copies it to `D2R_GAME_DIR` best effort
(`cmake/BestEffortCopy.cmake`, warns if the folder or file is unavailable). `tools/test-release.ps1`
stages it at the zip root. The installer's desktop shortcut is the installer repo's job.

## Verified live (2026-11-01, the user, Steam copy, `--validate`)

`D2R.exe` started 10:18:27, the date written 10:18:43 (Escape held past the videos and "Press any
key", window brought forward from the background), `WM_CLOSE` closed the game in 1.4 s, "License
validated" said, D2RLoader started; the fresh path (0 days) started the loader at once.

## Not verified live

- Battle.net copies: no `steam_appid.txt`, so the game is started directly; untested.
- With the date missing from `Settings.json` a failed validation counts as past the limit.

## Self-update

Port of the Path of Exile 2 launcher's updater. Sources: `launcher_update.*` (flow), `launcher_download.*`
(WinHTTP, BCrypt SHA-256), `launcher_install.*` (copy with rollback), `update_logic.*` (pure, tested by
`tests/launcher_update_tests.cpp`). Boxes: the `launcherUpdate*` fields of `src/speech_strings.h`.

Flow, at each start before the license step:

1. Skipped, logged, when the launcher has no tag (a dev build: `D2R_ACCESS_RELEASE` empty, set by
   `build.ps1 -Release` and `tools/test-release.ps1`), when `update-check=0` is in
   `d2rloader\plugins\d2r-access\settings.txt` (the plugin's Ctrl+S switch "update-check", on by
   default and so absent until changed), or when `D2R.exe` is running.
2. Download `releases/latest/download/release.json` (no API, no rate limit; 5 s): `tag`, `asset`,
   `sha256`, `notes`. Tags `test-YYYY-MM-DD[-N]` and `vYYYY.MM.DD[-N]` compare as (y, m, d, n); only a
   strictly newer one is offered. Any failure (offline, 404, bad JSON) is logged and the launcher goes on.
3. A Yes/No `MessageBox` (topmost) in the game's language: new and installed tag, the first 1500
   characters of the notes, and that it takes about a minute without speech. No: go on to the game.
4. Download `releases/download/<tag>/<asset>` (pinned to the tag; 10 min) to `%TEMP%\D2RAccessUpdate\<tag>\`,
   check the SHA-256, extract with `System32	ar.exe -xf` (exit code checked).
5. Copy every file over the game folder. Never overwritten when they exist: any `*.toml`, and the plugin's
   `settings.txt`, `tutorial.txt` and `explored\`. Each replaced file is backed up first; the running
   `D2RAccess.exe` is renamed to `D2RAccess.exe.old`. Any failure restores the backups, deletes created
   files, moves the exe back, shows a failure box and goes on to the game with the old version.
6. Success: an "updated" box, the new `D2RAccess.exe` starts with the same arguments, this one exits.
   At start a leftover `.old` is deleted (10 tries, 300 ms apart).

Constants: `launcher/update_logic.h`. Log lines: `update check ...` (skipped and why, installed and
latest tag), `update declined`/`accepted`, `download:`, `update: checksum`, `tar.exe exit`, `install:`,
`updated to <tag>`. Release side: `tools/test-release.ps1` writes `release.json` and uploads it with the zip.

Not verified live: the download and install on a real release (the repo is private until the first
public release, so the manifest answers 404 until then); the boxes read by a screen reader; the
rollback paths (only the pure parts are unit tested).

## Forcing a validation

`D2RAccess.exe --validate` validates whatever the date's age (logged `validation forced by
--validate`); the flag is not passed to the loader. It is how the validation path is tried while
the license is fresh, and a way for a player to renew it before a long time offline.
