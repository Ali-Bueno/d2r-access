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

## Plan for a public installer (not started)

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
