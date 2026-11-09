#!/bin/sh
# Starts D2RLoader under Proton with PRISM's Wine bridges findable. Wine loads
# prism_*_bridge.dll.so by name from WINEDLLPATH, which is read when the process
# starts, so it has to be set here; the mod cannot set it from inside the game.
# protontricks-launch does not pass WINEDLLPATH through (seen 2026-11-09: PRISM
# fell through to SAPI and stayed mute), so run Proton directly, the way the
# Grimdark wrapper does. Any arguments are ignored: as a Steam launch option
# ("<game folder>/d2r-access-proton.sh" %command%) Steam's own command is
# replaced by D2RLoader in the same prefix (docs/proton.md).
cd "$(dirname "$0")" || exit 1
STEAM_COMPAT_DATA_PATH="$HOME/.local/share/Steam/steamapps/compatdata/2536520"
STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
SteamAppId=2536520
SteamGameId=2536520
WINEDLLPATH="$PWD/d2rloader/plugins/d2r-access/prism/wine${WINEDLLPATH:+:$WINEDLLPATH}"
export STEAM_COMPAT_DATA_PATH STEAM_COMPAT_CLIENT_INSTALL_PATH SteamAppId SteamGameId WINEDLLPATH
exec "${PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton - Experimental/proton}" waitforexitandrun ./D2RLoader.exe
