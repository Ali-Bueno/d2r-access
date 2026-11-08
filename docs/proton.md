# Linux: Steam with Proton

Speech under Wine or Proton, from [issue #5](https://github.com/Ali-Bueno/d2r-access/issues/5)
(2026-11-08). Built and checked on Windows; **not yet run under Proton**.

## Why it was silent

Inside a Proton prefix there is no NVDA or JAWS, OneCore is not implemented and SAPI has no
voices, so PRISM 0.18.2 found no backend: `screen reader unavailable: no screen reader backend
available`. PRISM reaches the Linux host's speech through Wine bridges, and its Windows builds
before 0.18.3 left the bridged backends out of the registry (they were UNIX-only in its CMake;
0.18.3 builds them on Windows, ethindp/prism#126). The reporter saw the same with Grimdark, the Grim
Dawn mod, which spoke through Orca after the same upgrade.

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

`D2RAccess.exe` does not work under Proton yet: it looks for a Windows `steam.exe` and Steam's
registry keys, which a prefix lacks. Start `D2RLoader.exe` with the bridge folder in `WINEDLLPATH`;
the reporter's chain, verified with Grimdark, from the game folder:

```sh
WINEDLLPATH="$PWD/d2rloader/plugins/d2r-access/prism/wine${WINEDLLPATH:+:$WINEDLLPATH}" \
  protontricks-launch --appid 2536520 ./D2RLoader.exe
```

The host needs Speech Dispatcher (works with any reader, Cthulhu included), Orca or Spiel running;
PRISM's own priority picks among them.

## Checking

`d2rloader/logs/d2r-access.log` says `PRISM ready, backend: <name>` when it works. Under Wine a
failure says `no screen reader backend available under Wine (start the game with
WINEDLLPATH=<path>; ...)`, with the bridge folder's Unix path from `wine_get_unix_file_name`
(`src/screen_reader.cpp`).
