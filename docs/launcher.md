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
2. No `Settings.json`: it is created with only `"First Time": 1`; one without `"First Time": 1` gets
   it, the original kept once as `Settings.json.before-d2raccess` ("First run" below). `User Last
   Online` younger than 20 days: fresh. Otherwise speak that the license is validated online (about
   half a minute).
3. The store app is opened if closed, on both paths (the user, 2026-11-03). A copy in a Steam
   library (`<library>\steamapps\common\<folder>`) or with `steam_appid.txt` is a Steam copy (the
   file is not installed by Steam: D2RLoader writes it at its first run, so a new player's copy
   was taken for Battle.net, reported by drg 2026-11-03): `steam.exe -silent` if not running, then up to 60 s for
   `ActiveUser`. Any other copy is a Battle.net copy: `Battle.net.exe` from the running process
   (`QueryFullProcessImageNameW`) or from `InstallLocation` of the Uninstall entry `Battle.net`
   (32-bit view); fresh path: started without arguments if not running (no signed-in check is
   known); validation: always run with `--exec="launch OSI"`, which a running app takes over and a
   closed one opens with. Failure: an error box ("could not start the game, this copy needs Steam /
   the Battle.net app open and signed in"), exit. Fresh: start the loader.
4. Validation starts the unmodded game. Steam: `D2R.exe` without arguments, after writing a missing
   `steam_appid.txt` with the `appid` of the `steamapps\appmanifest_*.acf` whose `installdir` is the
   game folder's name (without it `D2R.exe` quits in 1.5 s and asks Steam to start it again, which
   runs the Steam launch options; measured 2026-11-03). Battle.net (a direct
   `D2R.exe` or the game's own launcher exe only opens the Battle.net app, reported 2026-11-03):
   wait up to 90 s for the `D2R.exe` it launches and open it by name. None: an error box (install
   or update the game in Battle.net first, or validate by hand: Play, main menu, close), then step 6.
5. Every 250 ms read `Settings.json`; every 1.5 s focus the game window and hold Escape 600 ms
   (SendInput, scan code). Escape never activates Play at the main menu. When the date changes: release,
   `WM_CLOSE`, wait 10 s, `TerminateProcess`, then speak "License validated" and start the loader.
6. Timeout (120 s), the game exiting by itself, or any start failure: close the game. Date 20-30 days
   old: speak that the game starts anyway and start the loader. 30 days or more, or no date at all:
   speak to check the internet and try again; no loader.

Language: `text_locale` under `[d2rloader]` in `d2rloader\config\d2rloader.toml`, else English.
Speech: PRISM from `d2rloader\plugins\d2r-access\prism.dll`; silent if it cannot load. The first
sentence interrupts the screen reader, the later ones queue; the launcher waits about 70 ms per
character of what is still queued before exiting. Progress is said as it goes (the user, 2026-11-03:
silence felt like a long wait): launcher started, opening Steam / Battle.net (only when the launcher
opens it), validating, validated, starting the game.

## First run

Measured live 2026-11-03 (the user's Steam copy, the save folder moved aside so the game started as
on a new PC; the user's idea after players failed to validate). With no `Settings.json` the game shows,
after the logos and the title screen, three screens that each have one button, "Continuar", and no
Close: "Calibración gamma", "Calibración de espacio de pantalla" and "Calibración de daltonismo" (with
a "Texto a voz" checkbox), then the full intro cinematic. Escape (tap or hold) and Enter do nothing
on them; only a click on "Continuar" passes them (or a controller's A). The validation held Escape
until its timeout, and once the game stopped responding on the gamma screen (Windows' "no responde"
box after about 18 s of Escape holds). Clicking through them, then holding Escape past the cinematic,
reached the main menu and wrote `User Last Online`; no EULA or account-linking box came (this account
had accepted and linked before).

The game writes `"First Time": 1` in `Settings.json` once they are done. A `Settings.json` holding only
that key skips all three and the cinematic: title screen, main menu, date written, the game filling
in its other defaults. So the launcher writes it when the file is missing (`CreateFileW`
`CREATE_NEW`, never over an existing file, the folder created if needed; constant
`FirstRunSettings`). Verified live end to end the same day with no save folder at all: file written,
the date 15 s after `D2R.exe` started, the loader started.

A Russian player on 1.0.3 still timed out: their log had no "Settings.json missing" line, so the file
existed without the date, and their modded game showed the gamma screen. Measured here, neither a
`WM_CLOSE` on the gamma screen nor D2RLoader writes `Settings.json` on a new PC, so theirs came from
elsewhere (perhaps an older install; not known). Since then any `Settings.json` without
`"First Time": 1` gets the key (a `0` or other number becomes `1`, a missing key is added after the
`{`, nothing else changes; written through a `.tmp` file and `MoveFileExW`), the original kept as
`Settings.json.before-d2raccess` (never overwritten). Verified live with the user's old file minus
"First Time" and the date, and no `steam_appid.txt`: marked, the appid written from the manifest, the
date 16 s after `D2R.exe` started, the loader started, one launcher run.

Not covered: a Steam account never linked to Battle.net (the game's `AccountLinkingModal`, a code to
type at a web address) and an account that never accepted the EULA (`EULAModal`, Accept / Cancel).
Both are online-only, need the player, and could not be reproduced on the user's account.

## Constants

`launcher/launcher_logic.h`: `ValidateAfterDays` 20, `OfflineLimitDays` 30, `EscapeHoldMs` 600,
`EscapePeriodMs` 1500, `PollMs` 250, `WaitTimeoutMs` 120000, `CloseWaitMs` 10000, `KillWaitMs` 5000,
`SteamTimeoutMs` 60000, `GameAppearTimeoutMs` 90000. `launcher_main.cpp`: `BattleNetLaunch`.

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

2026-11-03, the user, Steam copy: with Steam closed the launcher opened it and went on, on the
fresh path and with `--validate`.

2026-11-03, the user's Battle.net app (Diablo IV installed, no D2R): with the app running,
`Battle.net.exe --exec="launch Fen"` (Fen: Diablo IV's code, from bnetlauncher) started
`Diablo IV.exe` within 1 s. So `--exec` presses Play in the current app, not only selects the game
(bnetlauncher <https://github.com/dafzor/bnetlauncher> warned that some versions only select it).

## Not verified live

- Everything on a Battle.net copy of D2R: `OSI` as its `--exec` code (one source, a Lutris installer,
  <https://lutris.net/games/install/37245/view>), whether its `Settings.json` gets the same date,
  whether D2RLoader needs Battle.net open. A closed Battle.net app opened with `--exec`.
- The "app not open" boxes on both stores; the progress sentences heard in order.
- Rejected: `D2R.exe -uid OSI` needs a `WEB_TOKEN` that Battle.net writes under
  `HKCU\SOFTWARE\Blizzard Entertainment\Battle.net\Launch Options\OSI` and an elevated start
  (<https://github.com/shupershuff/d2r-multiboxing-without-a-script>); `-username -password` needs the
  player's credentials.
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
   `sha256`, `notes`. Tags `test-YYYY-MM-DD[-N]` and `vYYYY.MM.DD[-N]` compare as (y, m, d, n);
   numbered tags `MAJOR.MINOR[.PATCH]` (from 1.0, 2026-11-02) compare part by part and are newer
   than every dated one; only a strictly newer tag is offered. A launcher from before 1.0 cannot read
   a numbered tag, so testers on a `test-` build get 1.0 from the installer or the zip, not offered. Any failure (offline, 404, bad JSON) is logged and the launcher goes on.
3. A Yes/No `MessageBox` (topmost) in the game's language: new and installed tag, the first 1500
   characters of the notes (only the release's news since 2026-11-02, none without a news file), and that it takes about a minute without speech. No: go on to the game.
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
