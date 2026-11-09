#!/bin/sh
# Starts the mod's launcher under Proton with PRISM's Wine bridges findable. Wine loads
# prism_*_bridge.dll.so by name from WINEDLLPATH, which is read when the process
# starts, so it has to be set here; the mod cannot set it from inside the game.
# protontricks-launch does not pass WINEDLLPATH through (seen 2026-11-09: PRISM
# fell through to SAPI and stayed mute), so run Proton directly, the way the
# Grimdark wrapper does. D2RAccess.exe is the entry point, as on Windows: it
# checks the license online when it is due and then starts D2RLoader, which a
# plain D2RLoader start never does.
# Use this script as Steam's launch option ("<game folder>/d2r-access-proton.sh"
# %command%), so Steam's Play button starts it: the game needs Steam's own
# launch environment, and started from a bare shell instead it shows "You have
# not been online in the last 30 days" even with a fresh license date (seen
# 2026-11-09). Any arguments are ignored; Steam's own command is replaced by
# the launcher in the same prefix (docs/proton.md).
cd "$(dirname "$0")" || exit 1
STEAM_COMPAT_DATA_PATH="$HOME/.local/share/Steam/steamapps/compatdata/2536520"
STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
SteamAppId=2536520
SteamGameId=2536520
WINEDLLPATH="$PWD/d2rloader/plugins/d2r-access/prism/wine${WINEDLLPATH:+:$WINEDLLPATH}"
export STEAM_COMPAT_DATA_PATH STEAM_COMPAT_CLIENT_INSTALL_PATH SteamAppId SteamGameId WINEDLLPATH
exec "${PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton - Experimental/proton}" waitforexitandrun ./D2RAccess.exe
