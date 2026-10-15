# Mercenary condition and player status effects

## Player poison: verified native path

In the 2026-09-23 full mapped image of D2R 3.3.93787, the reflected
`AttributeBallWidget` has a `poisonHsvTransform` at member `+0x620`
(descriptor `0x29EAFC0`). Its color method at `0x14C9D5A` checks that the
attribute is health, obtains the local player, and at `0x14C9E25` calls
`0x3351B0(player, 2)`. A true result selects color mode 2 at `0x14C9E33`,
then copies `poisonHsvTransform` to both rendered orb layers at `0x14C9E6A`
and `0x14C9E9F`. This ties state id 2 to the actual visible poison color.

`0x3351B0` validates the state index against the active table bank, then
tail-calls `0x2F6220`, which tests the corresponding bit in the unit's active
state list at `statlist+0xAF0`. `src/native_status.cpp` calls the same checker
for the verified local player, with whole-function fingerprints of the orb
method and both state-checking functions. Unreadable or dead players yield no
state change. The pure condition tracker announces entering and leaving poison
once; on-demand vitals report includes poison while active. No status is
inferred from life loss or sprite color.

## Remaining condition sources

The mercenary correction on 2026-09-23 passed all 71 CTest suites and was
installed with a matching DLL SHA-256. The previous live startup on 3.3.93787
enabled both readers (`poison=1 mercenary=1`); the new percentage-getter and
panel-consumer fingerprints also matched read-only live memory. Corrected
combat speech and level changes still need controller listening.

The existing mercenary panel reader exposes the visible `HPTitle`/`HPStat`
row while that panel is open (`src/character_stats.cpp`; `docs/character-panel.md`).
In the mapped image, `HirelingInventoryPanel` method `0x14F2A60` obtains the
local player, calls `0x13ABC0(player, 7, 0)` for its hireling relation, then
`0x9A5D0(id, 1)` for the monster unit. It passes that unit to its stat and
equipment readers. At `0x14F2B6A`, it calls `0x2F5020(unit, 12, 0)` for the
hireling level.

The panel's stat-render function `0x14F3400` computes displayed life from
maximum life and the result of `0x13AA50(mercenaryId)`, a getter for the
hireling relation's percentage at `+0x1C`. That percentage is updated by the
game's `0x13B1E0` from incoming status. Read-only inspection of the live
2026-09-23 game found relation percentage 100 while the mercenary's raw life
stat was `0x8000` and maximum life was `0x6300`. Dividing those two stats
incorrectly clamps to 100 and can hide damage; they do not form a valid
mercenary life fraction on this client. `src/native_status.cpp` now follows
the panel's percentage getter, guarded by whole-function fingerprints of
the getter and panel consumer, while retaining the verified relation and unit
checks. An absent unit is unknown; native monster death modes 0 and 12 report
zero life. A new unit id gets fresh life and level baselines. Health uses the
player's percentage steps and critical interrupt rule, with simultaneous
player and mercenary changes in one utterance. A level increase announces the
new level once. The on-demand report includes both values when available.
Controller equipment focus still needs a live check with an available mercenary.

The loader's `DataTables::States` table describes possible states, but it does
not report which are active on the player. The `Unit+0x124 & 0x200` flag only
marks player-owned minions (`docs/entity-units-re.md`); it is not needed for
hireling identification now that the panel's relation call is known.

Before adding further player-effect speech, tie any additional state id to
the game's own rendered effect. Announcing freeze or other effects from
unverified state ids would risk false alerts. 2026-10-14: freeze, chill, stun,
slowing and the monster curses are now spoken by their `states.txt` rows, the
game's own state ids for the same checker ([status effects](status-effects.md)).
