# Automap discovery investigation — 2026-09-23

## Result and scope

The current navigation is **not restricted to the revealed automap**. Keep its
behavior: this investigation did not establish a reliable current-build discovery
reader. The user accepts retaining the existing guidance if a stricter mode cannot
be verified. No gameplay code, installed DLL, character save or automap was changed.

A strict exploration guide remains a possible design: route through revealed
terrain to a known passage at the edge of discovery, then let the player advance.
It must not use hidden room geometry to choose which unexplored branch reaches a
destination. This is a proposal, not an implemented mode.

## Existing data is not a visibility contract

- `native_rooms.cpp:Read` walks the entire current level's logical room list.
  `room_catalogue.cpp:Entities` exposes rooms even when their visited flag is false.
- `map_survey_service.cpp:Collect` requests native fixed exits and area boundaries.
  Existing presets can supply a destination without an active unit or a revealed
  automap marker. No discovery filter is applied.
- `beacon_service.cpp:Rooms/Update` builds and searches the whole logical room
  graph. A reachable local waypoint does not prove the room sequence was revealed.
- `native_world.cpp:ReadRoom` supplies loaded collision; `beacon.cpp:Copy` rejects
  missing/blank collision, but does not check automap discovery.
- `terrain_service.cpp:Track` records a room as seen when its active pointer exists.
  Its separate visited history records physical entry. Neither reads native
  automap visibility. The current frontier also uses undiscovered logical-room
  rectangles, so it is not independently a strict-map implementation.

Generated, active, visited and revealed are separate concepts. Existing comments
that equate streamed rooms with what the automap draws are an unverified assumption.

## Current-build native evidence

Target: D2R 3.3.93787, image base `0x140000000`; static Ghidra and instruction
inspection of `re/D2R.rebuilt.exe`. SHA-256:
`90EBC0731D40EDCC314ECC7C0D00C59C929100745F190AB1A42394CFEDCD5D08`.
Addresses below are RVAs, not portable offsets for other versions.

| Anchor | Verified observation | What it does not prove |
| --- | --- | --- |
| `0x361290`, caller `0x2EF340` | Stores active Room1 at `RoomEx+0x58` | Automap discovery |
| `0x361260`, caller `0x2F0110` | Clears `RoomEx+0x58` on unloading; tests `RoomEx+0x50 & 0x00100000` during cleanup | Persistent revealed state |
| `0x361040` | Obtains presets at `RoomEx+0x98` and sets `RoomEx+0x50` bit `0x04000000` | Revealed room; this belongs to preset materialization |
| `0x3614C0` | Builds nearby-room list from the level room chain and rectangle proximity | Visibility or passable connections |
| `0x1DEE87C`, `0x1DEE8F0` | Strings `.ma%d`, `%s.ma%d` | Save/load function addresses |
| `0x1DEE890` | Blizzard source-path string ending in `D2Client/src/UI/automap.cpp` | A callable reveal function |

The existing Ghidra database and a direct RIP-relative/absolute-pointer search
found no references to those exact automap string addresses. This is a search
limit, not proof of a particular indirection mechanism or absence of the code.
The reveal/render/save call chain and persistent visibility layout remain unknown.
No D2R/D2RLoader process was running during this investigation; no live comparison
with the displayed automap was performed.

## Saved `.ma0` data: structural evidence only

Read-only inspection of two existing saves (6,618 and 41,250 bytes) found a
consistent container structure:

| File region | Observed layout |
| --- | --- |
| First 400 bytes | 100 little-endian uint32 offsets; zero means no chain |
| Node `+0x00` | Next node's absolute file offset, or zero |
| Node `+0x04` | Matches the offset-table slot index |
| Node `+0x08` | Zero in all observed nodes; semantics unknown |
| Node `+0x0C` | Varying uint32 key; semantics unknown, do not equate with map seed |
| Node `+0x10..0x1C` | Four payload byte lengths, each divisible by six |
| Node `+0x20` onward | The four payloads concatenated in order |

Following every nonzero table entry yielded 4 and 43 nodes respectively. All
chains were acyclic, every header matched its table slot, every payload fit within
the file, and the sorted node extents covered **every byte after offset 400**
without gaps or overlap. This can be reproduced with uint32 reads: each extent is
`[offset, offset + 32 + sum(four_lengths))`, and the next link is header word zero.

The four groups of six-byte entries are consistent with classic serialized
automap drawable collections. That interpretation, the key's meaning, and the
mapping from entries to world rooms have not been verified in the native parser.
Two matching files do not establish a universal format. Saved drawing data also
does not supply a verified real-time discovery mask. The plugin does not read
these files and this investigation did not modify them.

## Classic source leads and a rejected flag

These are version-specific research leads, not D2R layout definitions:

- D2MOO's Diablo II 1.10f reconstruction sets `DRLGLEVELFLAG_AUTOMAP_REVEAL` on
  [every client level](https://github.com/ThePhrozenKeep/D2MOO/blob/5596f5cb6c5251a0a07c6637d26458b06099d516/source/D2Common/src/Drlg/DrlgDrlg.cpp#L410-L418),
  then [copies it into newly allocated rooms](https://github.com/ThePhrozenKeep/D2MOO/blob/5596f5cb6c5251a0a07c6637d26458b06099d516/source/D2Common/src/Drlg/DrlgDrlgRoom.cpp#L15-L29).
  Therefore classic `DRLGROOMFLAG_AUTOMAP_REVEAL` (`0x00040000`) is **not a
  discovered-room flag**. Using it as one would silently admit hidden terrain.
- The classic [ClientInSight activation path](https://github.com/ThePhrozenKeep/D2MOO/blob/5596f5cb6c5251a0a07c6637d26458b06099d516/source/D2Common/src/Drlg/DrlgActivate.cpp#L95-L121)
  creates active rooms. This does not establish an automap visibility contract.
- A classic mod's [explicit reveal call](https://github.com/planqi/slashdiablo-maphack/blob/6226ef321c58270481b7b3d46985909f7898c904/BH/Modules/Maphack/Maphack.cpp#L841-L878)
  requires active room data and calls `D2CLIENT_RevealAutomapRoom` separately.
  It temporarily creates room data when needed. Those mutating calls are research
  evidence only and must not be used by a discovery reader.
- Classic [cell/layer definitions](https://github.com/planqi/slashdiablo-maphack/blob/6226ef321c58270481b7b3d46985909f7898c904/BH/D2Structs.h#L84-L130)
  expose floor, wall, object and extra drawable trees plus saved-state fields.
  They are not a verified current-build room mask or `.ma0` parser.

## Next step if this investigation is resumed

Trace the current-build automap renderer or save/load consumer to its real
discovery storage. Compare a read-only snapshot against the actual automap while
approaching new terrain, unloading/revisiting rooms, changing levels and returning
through a portal. Include automap-hidden play and save/reload; merely matching
loaded rooms once is insufficient.

Only after that validation should an optional strict mode filter destinations,
collision cells, room-graph edges and exploration frontiers. Room graphs would
need refreshing as discovery changes, rather than once per level. Unknown reveal
state must not be silently replaced by all generated rooms. Preserve the existing
broader guidance as a separately identified option if the user chooses that design.
