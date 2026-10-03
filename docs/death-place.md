# Death place: the way back to where the player died

Asked for on 2026-09-28. The user died to a boss in Sewers Level 3 and pressed *Save and exit*.
In the new game the corpse lay in town, as Diablo II moves it there. The corpse memory is per game,
so nothing marked the boss room, and the user walked back blind. The player needs to find the
**place**, whether or not the corpse is still there.

## Behaviour

- **Recorded on death.** The death screen (`YouDiedModal`, a game UI layout name) marks the death. The
  place is where the scanner last read the player standing, at most one observe interval (500 ms)
  earlier. One place is kept per map (`NativeDrlg::MapSeed`, the same for every level of a map and
  across games of a single-player character). A new death replaces the map's place.
- **On disk.** `<plugin dir>\d2r-access\explored\deaths.txt`, one line per map (`seed level x y`),
  newest first, at most `DeathPlace::Memory::MaxRecords`. It survives *Save and exit*. A regenerated
  map has another seed, so its place never applies there.
- **On the death level** the scanner's corpse category lists "donde moriste" from anywhere. The beacon
  treats it as a fixed place. Your corpse, if still there, is listed beside it.
- **On any other level of the map** the corpse category lists one step: the known exit starting the
  fewest level changes toward the death level, the nearest among equals. It is named "camino a donde
  moriste, *level*: *exit*". The route uses only places the exploration memory has kept: exits and
  waypoints found on this map (`TerrainService::MapPlaces`), plus the current level's catalogue.
- **Waypoint fallback.** With no walking route known, the step is this level's waypoint. It names the
  known waypoint level nearest the death level ("…, elige *level*"). The mod cannot read which
  waypoints are activated, so a walking route always wins.
- **Forgotten on arrival.** Once the player stands within the beacon's arrival distance of the place,
  it is dropped. This only happens after the player has left the death level: the body lies on it
  until respawn.

Log: `[death] recorded level=L at=x,y`, `[death] reached … forgotten`, `[death] N death places loaded`.

## Code

- `src/death_place.{h,cpp}`: the per-map memory, its text format and `NextStep` (a breadth-first
  search over known exits by level). Pure, tested by `tests/death_place_tests.cpp`.
- `src/death_place_service.{h,cpp}`: death signal, the file, arrival, and the scanner entries.
- Wiring: `ui_message_monitor.cpp` (death screen), `map_survey_service.cpp` (`Track` on every
  observe, `Landmarks` in the catalogue), `beacon_service.cpp` (the place does not move).

## Local integration, 2026-10-03

The UI thread owns the death memory. It submits an immutable serialized snapshot to a dedicated disk worker, joined on unload; no filesystem write runs in the UI tick. The archive remains separate from character saves. Stored exit names retain local relocalization and Tal Rasha markings.
