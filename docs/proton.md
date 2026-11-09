# Linux: Steam with Proton

Speech under Wine or Proton, from [issue #5](https://github.com/Ali-Bueno/d2r-access/issues/5)
(2026-11-08). Run under Proton on 2026-11-09 (Arch, Proton Experimental, Cthulhu): the Orca
bridge loads and PRISM picks Orca. Whether it is *heard* is the player's word to give.

## Why it was silent

Inside a Proton prefix there is no NVDA or JAWS and OneCore is not implemented, so PRISM 0.18.2
found no backend on the reporter's first run: `screen reader unavailable: no screen reader backend
available`. (Wine's SAPI can present a voice — see the 1.0.11 run below — but it cannot speak.)
PRISM reaches the Linux host's speech through Wine bridges, and its Windows builds
before 0.18.3 left the bridged backends out of the registry (they were UNIX-only in its CMake;
0.18.3 builds them on Windows, ethindp/prism#126). The reporter saw the same with Grimdark, the Grim
Dawn mod, which spoke through Orca after the same upgrade.

First Proton run of 1.0.11 (2026-11-08) still did not speak, but differently: the log said
`PRISM ready, backend: SAPI`. The game had been started without `WINEDLLPATH`, so no bridge was
findable, every bridged backend reported unavailable, and `acquire_best` fell through to Wine's
SAPI. That SAPI is Proton's `protontts` over eSpeak-NG, and the Steam runtime ships no eSpeak-NG
data — the launch output's `Error processing file '/usr/share/espeak-ng-data/phontab'` — so SAPI
looked like success and stayed mute, hiding the fix. Under Wine the plugin now refuses every
non-bridged backend and logs why each bridge failed (`src/screen_reader.cpp`).

Wine Access's NVDA stand-in does not help either: since PRISM 0.16.2 the NVDA backend also requires
`RpcMgmtInqIfIds` (`source/backends/nvda.cpp`, `server_supports_interface`), which Wine's `rpcrt4`
leaves as a stub that always fails (found by the d4-assist session, 2026-11-08). d4-assist speaks
under Proton only because its Prismatoid 0.3.0 carries PRISM 0.12.2, from before that check. Under
Wine, speech therefore comes from the bridges alone, until PRISM accepts an unimplemented
`RpcMgmtInqIfIds` once `testIfRunning` has succeeded.

## What ships

`d2rloader/plugins/d2r-access/prism/`, the same as `third_party/prism/bin/`:

| File | From the PRISM v0.18.3 release |
|---|---|
| `prism.dll` | `prism-windows-x64.zip`, `dynamic/release/bin/` |
| `prism_orca_bridge.dll`, `prism_speech_dispatcher_bridge.dll`, `prism_spiel_bridge.dll` (2 KB placeholders) | `prism-linux-x64.zip`, `dynamic/release/wine/placeholders/` |
| `wine/*.dll.so` (the Linux x86-64 halves) | `prism-linux-x64.zip`, `dynamic/release/wine/` |

PRISM's [Wine bridges](https://github.com/ethindp/prism/blob/v0.18.3/doc/src/wine-bridges.md)
manual requires both halves from the same release and never renamed. The folder is new in this
change; why PRISM moved out of `d2r-access/` is in [the launcher](launcher.md#self-update).

## How PRISM finds them

- Only under Wine: each bridged backend first checks `ntdll`'s `wine_get_version`. On Windows the
  placeholders are never loaded (seen: NVDA chosen, as before).
- A placeholder is looked for on the DLL search path, then in `prism.dll`'s own folder
  (`source/optional_library.cpp` at v0.18.3), so beside `prism.dll` is enough.
- Wine then loads the matching `.dll.so` by name from `WINEDLLPATH`, read when the process starts:
  the mod cannot set it from inside the game.

## Starting the game

Set the shipped wrapper as the game's Steam launch option, then start the game from Steam's Play
button as usual. In Steam: right-click Diablo II: Resurrected → Properties → Launch Options:

```
"<game folder>/d2r-access-proton.sh" %command%
```

`d2r-access-proton.sh` (in the zip's root, beside `D2RAccess.exe`; source `packaging/`) sets
`WINEDLLPATH` to the `prism/wine` folder and runs Proton directly on `D2RAccess.exe`, the way the
reporter's working Grimdark wrapper does, ignoring Steam's command. The launcher is the entry
point, as on Windows: it validates the license online when due, then starts `D2RLoader.exe` —
starting the loader directly never renews the 30-day offline license. Set `PROTON` to another
Proton build's `proton` script if needed.

It has to go through Steam's Play button. The same wrapper run from a bare shell reaches the
title screen but the game then shows "You have not been online in the last 30 days", even with
the license date minutes old (seen 2026-11-09, plugin log: `Connecting to Battle.net` →
`DismissableModal`). Through Steam — the launch environment its reaper/pressure-vessel start
gives the game — the same start goes on to the main menu and character select, no modal. Which
part of that environment the check needs was not isolated; the date in `Settings.json` alone is
not it.

Under Wine the launcher skips its Steam check (`EnsureSteam` in `launcher/launcher_main.cpp`):
Steam is the host's Linux client, outside the prefix, so there is no `steam.exe` to open and no
`ActiveUser` in the prefix registry — that wait timed out after 60 s and the launcher exited
before starting anything (reproduced 2026-11-09, `d2rloader/logs/d2r-access-launcher.log`:
`timed out waiting for a Steam user`, `exit 1`, on an otherwise fresh license). Proton gives the
game its Steam connection itself. The rest of the flow runs as on Windows, verified on the
reporter's box the same day: `--validate` started the unmodded game, `User Last Online` changed
12 s later (the Escape holds reach the game through Wine), `License validated`, loader started.

Do not use `protontricks-launch`: it does not pass `WINEDLLPATH` through to the game (seen
2026-11-09: same prefix, same files; protontricks-launch → `backend: SAPI`, direct Proton →
`backend: Orca`, with `prism_orca_bridge.dll` loaded as a Wine builtin in the `+loaddll` trace).

The host needs Orca or Cthulhu (which owns Orca's `org.gnome.Orca.Service` bus name), Speech
Dispatcher or Spiel running; among the bridged backends PRISM's own priority picks. On the
reporter's box that is Orca. The Speech Dispatcher bridge additionally needs `libspeechd.so.2`
visible inside the Steam runtime (SteamLinuxRuntime_4 does not ship it), so a Speech-Dispatcher-only
host under Proton is unverified; the `[prism]` log lines below say whether that bridge loaded.

## Checking

`d2rloader/logs/d2r-access.log` says `PRISM ready, backend: <name>` when it works. Before it,
`[speech] wine=1 backends=<n>` and one `[speech] backend <name> priority=<n>` per registered
backend; a bridge that does not take says `[speech] <name> unavailable` (or `not supported at
runtime` / `failed to initialize`), and PRISM's own log is forwarded as `[prism] <source>: <text>`
(at debug level under Wine, where upstream logs a failed bridge load). Under Wine a failure says
`no screen reader backend available under Wine (start the game with WINEDLLPATH=<path>; ...)`, with
the bridge folder's Unix path from `wine_get_unix_file_name` (`src/screen_reader.cpp`). It never
says `backend: SAPI` under Wine any more.
