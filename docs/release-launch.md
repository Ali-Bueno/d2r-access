# Launching the modded game for a release (research, 2026-09-26)

Question from the user: can a released package start the game with the mod on the
first run, without asking the player to start the unmodded game first to validate the
license? Not implemented; this records what was found.

## Documented

- D2RLoader v1.0.0-beta changelog: "Restored the 30-day login check. This was never
  meant to be broken." The loader keeps Blizzard's periodic login check on purpose, and
  no config key turns it off (`d2rloader/config/d2rloader.toml` has only
  `launch_arguments`, `default_mod`, `skip_title_screen`, `check_for_updates` and
  character backups). <https://d2rloader.net/changelog.html>
- v1.1.0-beta: Steam support updated, Steam environment detection fixed, and
  `launch_arguments` added. The docs only say to extract the loader into the Steam or
  Battle.net game folder. <https://d2rloader.net/docs.html>

## Observed on this machine

- Steam's launch option for app 2536520 is already
  `"<game folder>\D2RLoader.exe" %command%` (per-user `localconfig.vdf`). Started that
  way, the loader logs `Steam launch detected` and receives `D2R.exe` as its argument, so
  the game runs with Steam's environment and its linked Battle.net account.
- Starting `D2RLoader.exe` directly from the folder lacks that environment. That is when
  the "no login in 30 days, start without mods" message appeared.

## Recommendation for the release

Tell players to set that Steam launch option and start the game from Steam, not to run
it unmodded first. A brand-new install or account may still need one successful online
start, which is Blizzard's requirement. Setting the option by editing Steam's
`localconfig.vdf` from an installer was not pursued, because that file belongs to Steam
and changes while Steam runs.
