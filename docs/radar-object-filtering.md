# Structural radar: filtering bypassable objects

> **Since 2026-09-25 the filter applies only while the game slides** (wall-slide
> prevention off, `World::slides`). With prevention on, the default since
> 2026-09-22, a direct move stops at these objects, so they stay walls.
> `docs/radar-readings.md`.

> Current as of the 2026-09-16 redesign (docs/radar-redesign.md): this
> filtering code is unchanged, and it now feeds the radar's only
> classifier (the connected-passage `ScanPassage` scan) instead of one of
> several ray/geometry classifiers.

Implemented 2026-09-16 after the user reported that the surface-tracking build
still sounded the same and requested ignoring furniture as closed space. This
extends [surface tracking](radar-surface-tracking.md); that revision did not
resolve the reported navigation experience.

**User follow-up:** object filtering helps, but the radar still feels cluttered.
The subsequent [navigation research](D2RNavigationResearch.md) and
[current radar audit](CurrentRadarAnalysis.md) investigate connected passages.
Their isolated PoC does not replace this installed audio behavior.

## Evidence

The latest completed game session loaded at 08:26:42.895 and closed at
08:28:39.364. It generated **87 geometry events, zero retries**. Of those events,
55 had physical hit flags `0001`, 16 had `0005`, and 11 had `0400`; the remaining
five had `0015`, `0401` or `0007`. Stopping interrupted-audio retries did not
eliminate the geometry chatter. These are log events, not recorded sound.

The captured camp post at (4466,4629) is physically real: its side-body hit can
replace a far wall between neighboring player cells. The captured grid retains
only blocked/open classification; it cannot identify the original object type.
An individual `WALL` collision bit therefore cannot reliably mean a room wall.

Rechecked primary [D2MOO collision definitions](https://github.com/ThePhrozenKeep/D2MOO/blob/master/source/D2Common/include/D2Collision.h)
and the previously recovered native movement trace/body functions in this
installation. D2MOO corroborates flag meanings, not D2R memory offsets.
`PLAYER_PATH` combines wall `0001`, no-player `0008`, object `0400`, door `0800`
and no-path `1000`. Removing just the object bit would miss baked clutter and
would also lose objects that actually block a corridor.

## Classification

`radar_structure.cpp` creates a separate, bounded local collision view for
automatic boundary tracking. The original movement map and terrain probe remain
available for physical clearance and diagnostics.

1. Reuse the existing actor/terrain flag handling, now factored into
   `TerrainCollisionFlags`. Copy known local cells; preserve unknown coverage.
2. Find eight-connected blocking components, so diagonal walls stay connected.
   Retain components touching unknown coverage or the classification window edge.
3. Consider wall/object components no larger than **one native map tile**
   (five collision subtiles per side). Pure object components may extend up to
   the current local radar range (20 subtiles by default). These are conservative
   classification policies, not recovered furniture type IDs. Other movement
   restrictions, including door/no-player/standalone no-path, always remain.
4. Prove a local detour in the **unfiltered terrain view**. Expand the component
   by the native body radius plus one destination cell, test every destination
   with the complete native body pattern, and flood reachable destinations using
   the native eight-direction step model. All walkable cells must connect to an
   exterior approach. Unknown coverage fails the proof.
5. Keep components dividing two approaches, including furniture plugging a
   corridor whose visible gaps do not fit the character. Also keep enclosed
   walkable pockets: checking only the perimeter would erase small closed rooms.
6. Only the automatic structural view omits accepted components. No changes are
   made to game collision, movement, the original world snapshot or Ctrl+T.

The default window is 87x87 cells: local range + maximum candidate span + body
and detour halo. This keeps an eligible nearby component fully inspectable as
the player moves. Known neighboring rooms can complete the same component;
unloaded room seams never establish a detour. Each candidate is checked against
the original map, so removing one object cannot invent a route around another.

`Engine` advances its existing boundary tracker from `Sensor::structure`.
An opening sound still requires the **original full-body movement clearance**.
Ignoring furniture as a room boundary does not promise a clear straight ray
through it. There is no new furniture-proximity sound. Ctrl+T continues to report
the real obstacle/clearance in every direction. The four fixed axes, rear ray,
surface tracking, 120 ms cues and 50 ms gaps are unchanged.

Generated-event and Ctrl+T diagnostics now include `structure` and
`structure_flags`. `hit` identifies the structural hit driving the boundary;
`passage`, `terrain` and `flags` retain their physical meanings. Logging remains
event driven.

## Validation

- **Controlled before/after:** compiled the previous committed engine in a
  temporary directory against the same test route. Passing between two rows of
  bypassable objects generated **48 events before, zero after**. The previous
  engine fails the new regression. This is a synthetic route, not a replay of
  the latest live session. The temporary comparison files were removed.
- Zero events on both outward/return trips for wall, object and mixed flags,
  at one-, three- and four-cell strides and 50/150/200 ms polling.
- **82 new structure checks**, including all six native body patterns, larger
  furniture, buildings, thin body gaps, doors, unknown detours, actors, water,
  enclosed pockets, room seams, physical opening guards and rear reopening.
- Two additional assertions on the actual camp capture prove that the post is
  omitted structurally while its neighboring wall and physical collision remain.
- **25 suites / 1248 checks pass** with the normal Release build, including UI
  and serialized-audio integration. Radar/footsteps account for 813 checks.
- Dense synthetic clutter averaged about **0.42 ms per engine tick** over 200
  ticks on this machine, below the existing 50 ms poll period. This does not
  measure the live native snapshot or whole UI pump.
- Build/deployed SHA-256:
  `DF188862EEAC1511A7D4FBA563A27ED682D8D61725DDD0C8E5F634EC69C18C2C`.

## Remaining validation

The user leads the controller/listening test: pass ordinary furniture, approach
a real wall, pass a side doorway and return along the same route. If chatter
remains, Ctrl+T saves the local map and both physical/structural distances.

Filtering is conservative. Clutter attached to a large wall, oversized baked
components, incomplete coverage and ambiguous actor/terrain ownership can still
retain obstacles. The collision view does not expose semantic furniture names.
No live audible improvement is claimed until the user tests this build.
