# Launching the modded game for a release (research, 2026-09-26; updated 2026-10-26)

Question from the user: can a released package start the game with the mod without
asking the player to start the unmodded game first to validate the license? Not
implemented; this records what was found and the plan for a public installer.

## Documented

- D2RLoader v1.0.0-beta changelog: "Restored the 30-day login check. This was never
  meant to be broken." The loader keeps Blizzard's periodic login check on purpose, and
  no config key turns it off (`d2rloader/config/d2rloader.toml` has only
  `launch_arguments`, `default_mod`, `skip_title_screen`, `check_for_updates` and
  character backups). <https://d2rloader.net/changelog.html>
- v1.1.0-beta: Steam support updated, Steam environment detection fixed, and
  `launch_arguments` added. The docs only say to extract the loader into the Steam or
  Battle.net game folder. <https://d2rloader.net/docs.html>
- The docs say nothing about the check or about logging in online. `default_mod =
  <None>` only launches without a mod, still inside the loader.

## Observed on this machine

- Steam's launch option for app 2536520 is already
  `"<game folder>\D2RLoader.exe" %command%` (per-user `localconfig.vdf`). Started that
  way, the loader logs `Steam launch detected` and receives `D2R.exe` as its argument, so
  the game runs with Steam's environment and its linked Battle.net account.
- Starting `D2RLoader.exe` directly from the folder lacks that environment. That is when
  the "no login in 30 days, start without mods" message appeared.
- 2026-10-26, the user: the Steam launch option does not avoid the message either. Once
  30 days pass without an online start, the game started by the loader shows it with a
  single Exit button. The 2026-09-26 run was most likely inside the 30-day window.
- The message text is in neither `D2RLoader.exe` nor `D2RCore.dll` (1.3.1-beta, ASCII
  and UTF-16 strings), so it is most likely the game's own; the loader lets it through.
- `Saved Games\Diablo II Resurrected\Settings.json` has `"User Last Online"`, in
  nanoseconds since the Unix epoch (here `1792163632624867200` = 2026-10-16 15:13:52
  UTC). Probably the date the check compares; unverified. The mod only ever reads it.

## Asked again, 2026-11-01: can the loader log in with the account?

No. Web research (2026-11-01):
- The 30-day rule is Blizzard's license re-authentication for offline play ("FAILED TO
  AUTHENTICATE"), on Steam too. <https://us.forums.blizzard.com/en/d2r/t/is-the-internet-connection-neccessary/15548>,
  <https://support.blizzard.com/en/article/298306>
- D2RLoader turns Battle.net online play off while it runs and keeps the check on purpose (its
  v1.0.0-beta changelog above). Its site, wiki and terms are script-rendered and were not read; the
  redistribution terms of the loader are still unknown (ask its author).
- Getting past the check by any other means than a real online start (editing `User Last Online`,
  patching) is circumventing a license check: out of scope.
- What a real online start needs is unconfirmed: a multiboxing guide says Battle.net refreshes the
  token at launch, at character select and at close
  (<https://github.com/shupershuff/d2r-multiboxing-without-a-script>). Reaching character select
  online is the safe assumption until the first test below says otherwise.
- Battle.net copies: `Battle.net.exe --exec="launch OSI"` starts the game (used by a Lutris
  installer, undocumented by Blizzard).

**First test, 2026-11-01** (the user, Steam copy, game started without the loader): the title
screen said "Connecting to Battle.net", then "Press any key"; the logo and intro videos play first
and can be skipped. Character select was not reached. `User Last Online` went from 2026-10-16
15:13:52 UTC to 2026-11-01 12:44:28 UTC.

**Watched, 2026-11-01** (D2R.exe started from the game folder with Steam running; a script
polled `Settings.json` and the process every second):
- Twice the user stayed on "Press any key" (1.5 min the second time) and closed with Alt+F4: the
  date did not change. `blz-log.txt` logs `BGSEnvironmentMap ... us.actual.battle.net` 1 to 3 s
  before the exit both times, so that line comes with shutting down, not with the login.
- A key pressed on the title screen, the main menu reached: started 09:51:57, **date written
  09:52:17 with the game still running**, closed 09:52:29 (local time, UTC-3).
- So the license is refreshed on entering the main menu online, not at exit: a launcher can close
  the game as soon as the date changes. The logo and intro videos play first and need skipping.
- A Steam copy refreshes from `D2R.exe` started directly while Steam runs; Battle.net copies untested.

The launcher built on this finding is described in [launcher.md](launcher.md).

## The installer: the user's mod-installer, adapted (2026-11-01)

The user's own installer, first made for the Kakarot mod, is reused instead of Inno Setup:
`D:\code\tools\mod-installer` (private repo `Ali-Bueno/mod-installer`, Rust + native Win32
controls), one `configs/<mod>.toml` per mod. `configs/d2r.toml` and the code it needed were added
2026-11-01 (uncommitted there until the user reviews); build with `MOD_CONFIG=d2r cargo build
--release`, copied to `dist\D2RAccessInstaller.exe`. What it does for this mod:
- Finds the game through Steam (app 2536520) or Battle.net's uninstall entry, or a chosen folder.
- Downloads the latest `d2r-access*.zip` from `Ali-Bueno/d2r-access` (public before the first
  release: a private repo answers 404 to an unsigned request), or installs a chosen zip.
- Replaces the plugin (`d2rloader/plugins/d2rl-d2r-access.dll`, `d2rloader/plugins/d2r-access/`)
  and keeps the player's `settings.txt`, `tutorial.txt`, `explored/` and `d2r-access.toml`.
- A "Game language" box (from the loader's config, else Steam's appmanifest, else Windows) sets
  `text_locale` and `audio_locale` in `d2rloader.toml`. Tested 2026-11-01: with `text_locale`
  empty the game's own option rules the text and the mod follows, but a first start through the
  loader is English whatever Steam says, so the user chose to set both
  (`[language] text_follows_game` exists in the installer, off for this mod).
- On Steam it sets the launch option `"<game>\D2RAccess.exe" %command%` (the
  [launcher](launcher.md)) in `localconfig.vdf`, replacing an older `"<game>\D2RLoader.exe"` one,
  closing Steam first with the player's OK, and keeps a backup; else it says what to type.
- A desktop shortcut, `Diablo II Resurrected Access.url`, to `D2RAccess.exe` for Steam and
  Battle.net copies alike (`[shortcut] steam_url = false`).
- Uninstall can also remove D2RLoader (never the game's `mods` folder nor the VC++ DLLs) and then
  clears the launch option if it is still the one it set.

**Unverified live:** whether the loader accepts a `d2rloader.toml` holding only the two locale keys
(written when the file is missing) and adds its defaults; Steam's shutdown and the `localconfig.vdf`
write; the Battle.net shortcut; the download once the repo is public. A `.url` file is read in the
system code page, so a game path with non-ASCII letters may break the Battle.net shortcut.
Each public release must carry the zip and `D2RAccessInstaller.exe` (the Kakarot lesson: the
installer's download link follows `/releases/latest`).

## Plan for a public installer (2026-09-26; superseded by the section above)

The user's decision: the installer validates the license once at install time, and the
player then runs a "Validate license" tool every 30 days.

- **Validation run:** announce it through PRISM, start the unmodded game, wait for
  `User Last Online` to change, close the game and announce the result. Time out with a
  clear message when the date does not change (no internet).
- **Obstacle:** once Steam's launch option points at the loader, every Steam start goes
  through it and ends at the message. Options, best first:
  1. The tool starts `D2R.exe` directly, with Steam running and Steam's app id in the
     environment. Simplest if the game accepts it; it may refuse or relaunch itself
     through Steam.
  2. A small `D2RAccess.exe` in the launch option forwards to `D2RLoader.exe`, except
     when the tool left a marker; then it starts the game unmodded once.
  3. Rejected: the tool swaps the launch option, which needs Steam closed twice.
- **Installer:** Inno Setup (works with NVDA). With Steam closed it can also write the
  launch option into `localconfig.vdf`, so the player never types it.
- **Reminder:** the plugin can read the same date and warn at startup when few days are
  left.
- **First test, before any code:** with Steam running, start `D2R.exe` directly and
  watch `Settings.json`. It answers whether option 1 works, whether the date changes on
  connecting or only on exit, and whether the title screen waits for a key.
- Copies bought on Battle.net start the unmodded game through the Battle.net app:
  second phase.
