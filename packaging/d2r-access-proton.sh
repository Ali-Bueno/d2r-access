#!/bin/sh
# Starts the mod's launcher under Proton with PRISM's Wine bridges findable. Wine loads
# prism_*_bridge.dll.so by name from WINEDLLPATH, which is read when the process
# starts, so it has to be set here; the mod cannot set it from inside the game.
# protontricks-launch does not pass WINEDLLPATH through (seen 2026-11-09: PRISM
# fell through to SAPI and stayed mute), so run Proton directly, the way the
# Grimdark wrapper does. D2RAccess.exe is the entry point, as on Windows: it
# checks the license online when it is due and then starts D2RLoader, which a
# plain D2RLoader start never does.
# Use this script as Steam's launch option (sh "<game folder>/d2r-access-proton.sh"
# %command%; through sh, because the release zip, made on Windows, keeps no
# execute bit), so Steam's Play button starts it: the game needs Steam's own
# launch environment, and started from a bare shell instead it shows "You have
# not been online in the last 30 days" even with a fresh license date (seen
# 2026-11-09). Steam's own command is replaced by the launcher in the same prefix
# (docs/proton.md); only the Proton it names is kept.
cd "$(dirname "$0")" || exit 1
# Steam sets these for its launch option, for the library and prefix the game is
# really in (Flatpak Steam, a second library); the defaults serve a start from a shell.
: "${STEAM_COMPAT_DATA_PATH:=$(cd ../.. && pwd)/compatdata/2536520}"
: "${STEAM_COMPAT_CLIENT_INSTALL_PATH:=$HOME/.local/share/Steam}"
: "${SteamAppId:=2536520}"
: "${SteamGameId:=2536520}"
# The Proton chosen for the game, from Steam's command, so the prefix is never run
# by another Proton version; PROTON overrides it, Proton Experimental is the default.
for arg do
	case $arg in */proton) PROTON=${PROTON:-$arg} ;; esac
done
WINEDLLPATH="$PWD/d2rloader/plugins/d2r-access/prism/wine${WINEDLLPATH:+:$WINEDLLPATH}"
export STEAM_COMPAT_DATA_PATH STEAM_COMPAT_CLIENT_INSTALL_PATH SteamAppId SteamGameId WINEDLLPATH
exec "${PROTON:-$STEAM_COMPAT_CLIENT_INSTALL_PATH/steamapps/common/Proton - Experimental/proton}" waitforexitandrun ./D2RAccess.exe
