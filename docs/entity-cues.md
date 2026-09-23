# NPC and priority enemy sounds

Enabled by default. These short synthesized pulses use the same fixed screen
axes as the radar: stereo gives left/right, and a lower pitch marks screen-down.
Facing or turning the character does not rotate them. No audio assets or new
runtime dependencies are needed.

| Target | Sound | Selection |
|---|---|---|
| Friendly NPC | Soft rising pair of notes | Nearest visible NPC |
| Boss or elite | Low textured strike | Highest enemy priority |
| Resurrecting shaman | Slightly higher, fluttering strike | Next enemy priority |
| Ordinary ranged enemy | Higher textured pulse | Lowest enemy priority; optional |

There is one representative per class, with at most two enemy classes plus the
NPC. A crowd of archers therefore makes one stream, not one per archer. Regular
melee enemies are silent. A special ranged enemy keeps the important-enemy sound
even when ordinary ranged cues are disabled. Selection uses native traits, not
translated names; classification evidence is in [entity units](entity-units-re.md).

Only loaded, living entities within 15 collision subtiles are eligible, using
the radar's terrain line-of-sight check. This has the same limitation as the
entity scan: walk-blocking fences may hide a target. An ordinary target in
another room behind a wall does not get a cue. These are direct bearings, not
routes around obstacles.

Pulse intervals shorten continuously from 1.8 seconds at the range edge to
0.55 seconds at contact. The scheduler leaves at least 160 ms between starts,
and each sound is 104 ms long. Due targets take turns, so the nearest boss
cannot starve the shaman or NPC. The 100 ms snapshot cadence rounds these times
to the next update. These are tunable sound-design defaults, not game constants.
Changing the nearest representative keeps that class's cadence to prevent
rapid alternating cues; the next pulse uses the newly selected target's position.

Dialogue, menu/focus gating, lifecycle changes, unavailable audio and failed
native reads silence playback and clear timing. Dead/unloaded targets disappear
from the next snapshot. The worker keeps only one pending pulse and cancels
current output on suspension; there is no queue of stale targets.

## Controls

`access-entity-cues on|off|status` controls the feature independently of NPC
speech (`Ctrl+N`) and the navigation radar (`Ctrl+R`). The same command accepts
`npc-on`, `npc-off`, `enemy-on`, `enemy-off`, `ranged-on`, and `ranged-off`.
Status and toggle feedback are localized in all 13 game languages.

Persistent settings in the plugin TOML:

```toml
[entity_cues]
enabled = true
npcs = true
enemies = true
ranged = true
range = 15
volume = 100
```

Restart after edits. Volume is 0–100 percent of the deliberately soft synthesized
level, independent of radar volume. Range is in collision subtiles, greater than
zero and at most 100; the upper bound keeps this a local proximity scan.
Malformed or duplicate recognized keys discard this section and retain defaults.

## Validation

Build and deployment succeeded on 2026-09-22; **67/67 CTest suites passed**, with
6/6 affected suites passing again after a test-only warning correction. The
installed DLL's SHA-256 matched the build. No live listening was performed.

Automated coverage checks native classification, selection priorities, dead and
friendly exclusions, occlusion, fixed screen directions, changing distance,
cadence, fairness, lifecycle silence, config parsing, PCM bounds, stereo balance,
front/rear differentiation, gain and invalid inputs.

Live listening remains necessary: approach a town NPC from both sides, then
find a Fallen shaman and a ranged pack outdoors, and compare against an elite.
Move closer/farther, kill the enemy, open dialogue/pause, and change areas.
Confirm cue loudness and identity alongside the radar and game combat audio.
`[entity-cues]` logs selection and suspension; `[entity-audio]` logs device state.
