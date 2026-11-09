# Linux: Steam with Proton

Speech under Wine or Proton, from [issue #5](https://github.com/Ali-Bueno/d2r-access/issues/5)
(2026-11-08). Run under Proton on 2026-11-09 (Arch, Proton Experimental, Cthulhu): the Orca
bridge loads, PRISM picks Orca and the reporter hears Cthulhu speak the mod in the menus.

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

1.0.12 shipped that explicit selection and was silent again (2026-11-09): `[speech] wine=1
backends=15`, every bridge `unavailable`, no `[prism]` lines — the bridges were never loaded,
with `WINEDLLPATH` verified correct in the loader process. The selection probed each backend
with `prism_registry_get`, which only returns a backend something has already acquired (PRISM
0.18.3's `FrozenRegistry::get` reads a weak cache that only `acquire_entry` fills, by running
the factory that loads the bridge DLL), so the probe could never succeed. The plugin now calls
`prism_registry_acquire` directly — the acquire is the probe — and runs the
`PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` check on the acquired backend, freeing it on failure.
Proven outside the game under Wine against the shipped `prism.dll` (throwaway harness,
2026-11-09): `registry_get` null for Orca, Speech Dispatcher and Spiel; `registry_acquire`
succeeds and Orca initializes. An in-game run of a rebuilt DLL is still outstanding.

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
sh "<game folder>/d2r-access-proton.sh" %command%
```

Through `sh`: the release zip is made on Windows and keeps no execute bit, so the script cannot
be run by its path alone; `.gitattributes` keeps it LF and `tools/test-release.ps1` refuses a
staged copy with a CR, since `sh` fails on one.

`d2r-access-proton.sh` (in the zip's root, beside `D2RAccess.exe`; source `packaging/`) sets
`WINEDLLPATH` to the `prism/wine` folder and runs Proton directly on `D2RAccess.exe`, the way the
reporter's working Grimdark wrapper does, in place of Steam's command. It keeps two things of
Steam's: the `STEAM_COMPAT_*` variables Steam sets for the launch option, which name the prefix
the game really has (Flatpak Steam, a second library), and the `proton` script named in Steam's
command, so the prefix is never run by a Proton other than the game's. From a shell, without
them, it falls back to the game's own library and Proton Experimental; `PROTON` overrides the
choice. The launcher is the entry point, as on Windows: it validates the license online when due,
then starts `D2RLoader.exe` — starting the loader directly never renews the 30-day offline license.

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

The host needs Orca or Cthulhu (which owns Orca's `org.gnome.Orca.Service` bus name) running;
Speech Dispatcher alone is not a route today. Its bridge is sound — driven directly on the
reporter's host (2026-11-09, a throwaway harness dlopening the shipped
`prism_speech_dispatcher_bridge.dll.so`): ABI correct, `prism_speechd_available` OK against the
running speechd, create and speak OK. But with the 1.0.11 plugin it is unreachable in game:
upstream registers Speech Dispatcher and SAPI at the same priority (97; Orca is 100), SAPI wins
the tie, and `acquire_best` never probes the bridge — seen the same day with the Orca bridge
temporarily renamed away: `backend: SAPI`, and no bridge load attempt in a `+loaddll` trace.
The plugin's explicit bridged-backend selection (above) removes the tie; and inside Steam's runtime the bridge additionally needs `libspeechd.so.2`,
which SteamLinuxRuntime_4 does not ship. So a Speech-Dispatcher-only host stays unverified until
both a rebuilt plugin and a runtime-visible libspeechd exist; the `[speech]`/`[prism]` log lines
below will then say whether that bridge loaded.

## Checking

`d2rloader/logs/d2r-access.log` says `PRISM ready, backend: <name>` when it works. Before it,
`[speech] wine=1 backends=<n>` and one `[speech] backend <name> priority=<n>` per registered
backend; a bridge that does not take says `[speech] <name> unavailable` (or `not supported at
runtime` / `failed to initialize`), and PRISM's own log is forwarded as `[prism] <source>: <text>`
(at debug level under Wine, where upstream logs a failed bridge load). Under Wine a failure says
`no screen reader backend available under Wine (start the game with WINEDLLPATH=<path>; ...)`, with
the bridge folder's Unix path from `wine_get_unix_file_name` (`src/screen_reader.cpp`). It never
says `backend: SAPI` under Wine any more.
