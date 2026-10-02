# NPC and enemy sounds

Enabled by default. These short synthesized pulses use the same fixed screen
axes as the radar: stereo gives left/right, and a lower pitch marks screen-down.
Facing or turning the character does not rotate them. No audio assets or new
runtime dependencies are needed.

| Target | Sound | Selection |
|---|---|---|
| Friendly NPC | Light wooden tap | Nearest visible NPC |
| Boss or elite | Deep resonant metallic swoop | Highest enemy priority |
| Resurrecting shaman | Mid-register resonant swoop | Next enemy priority |
| Ordinary ranged enemy | Brighter resonant swoop | Third enemy priority; optional |
| Any other enemy | Short rough growl | The nearest three, in the streams left; optional |

There are at most three enemy streams plus the NPC. Each priority class has one
representative, so a crowd of archers makes one stream, not one per archer.
The streams the priority classes leave go to the nearest ordinary enemies in
sight, each heard on its own: a pack of Fallen sounds as its three nearest
members, so its spread is audible. Each rank (nearest, second, third) keeps its
own cadence, so the nearest changing does not burst. An ordinary ranged enemy
whose own cue is disabled growls instead. A special ranged enemy keeps the important-enemy sound even when
ordinary ranged cues are disabled. Selection uses native traits, not translated
names; classification evidence is in [entity units](entity-units-re.md).

The growl (2026-10-02, after the user and a friend asked for Diablo 1-style
monster sounds, so a ranged player stops shooting blind) follows
[mojsior/diablo-access](https://github.com/mojsior/diablo-access), the mod the
friend meant: one shared monster sound, the three nearest emitters, faster when
closer (100-1000 ms over 12 tiles; `Source/utils/proximity_audio.cpp`). That mod
shares its three emitters with items and doors and lowers volume with distance;
here items keep their own loops and distance stays in the tempo. It is a 90 ms band-limited
buzz falling from 175 to 125 Hz whose loudness flutters at 36 Hz: rougher and
lower than every swoop, and tonal where the radar breath is noise. The same
service plays the attack reach ping; see [attack reach](attack-reach.md).

Only loaded, living entities within 15 collision subtiles are eligible, using
the radar's terrain line-of-sight check. This has the same limitation as the
entity scan: walk-blocking fences may hide a target. An ordinary target in
another room behind a wall does not get a cue. These are direct bearings, not
routes around obstacles.

Pulse intervals shorten continuously from 450 ms at the range edge to 150 ms
at contact. The scheduler leaves 100 ms between starts, with an 84 ms NPC tap
or 90 ms enemy swoop. Due targets take turns, so the nearest boss cannot starve
the shaman or NPC. The 100 ms snapshot cadence gives one isolated target about
one pulse every 200 ms nearby or 500 ms at the range edge. Three nearby classes
each sound about every 300 ms, alternating. These are sound-design defaults,
not game constants.
Changing the nearest representative keeps that class's cadence to prevent
rapid alternating cues; the next pulse uses the newly selected target's position.

The user found the first enemy whoosh too similar to the radar's floor breath
and requested much faster NPC/enemy repetition. The latest design uses a voiced,
low descending resonance with inharmonic phase modulation and only a little air
texture, instead of a noise-dominated swell. Enemy classes keep different sweep
ranges, and rear placement lowers them. NPCs retain the quickly damped wooden
tap. Nominal waits are roughly one third of the previous 450–1450 ms, and the
shorter spacing also accelerates mixed groups.

Dialogue, menu/focus gating, lifecycle changes, unavailable audio and failed
native reads silence playback and clear timing. Dead/unloaded targets disappear
from the next snapshot. The worker keeps only one pending pulse and cancels
current output on suspension; there is no queue of stale targets.

## Controls

`access-entity-cues on|off|status` controls the feature independently of NPC
speech (`Ctrl+N`) and the navigation radar (`Ctrl+R`). The same command accepts
`npc-on`, `npc-off`, `enemy-on`, `enemy-off`, `ranged-on`, `ranged-off`,
`ordinary-on`, `ordinary-off`, and the reach controls of [attack reach](attack-reach.md).
Status and toggle feedback are localized in all 13 game languages.

Persistent settings in the plugin TOML:

```toml
[entity_cues]
enabled = true
npcs = true
enemies = true
ranged = true
ordinary = true
range = 15
volume = 100
reach = true
attack = "auto"
melee_reach = 3
ranged_reach = 15
```

Restart after edits. Volume is 0–100 percent of the deliberately soft synthesized
level, independent of radar volume. Range is in collision subtiles, greater than
zero and at most 100; the upper bound keeps this a local proximity scan.
Malformed or duplicate recognized keys discard this section and retain defaults.

## Validation

The original feature passed 67/67 CTest suites. The latest distinct-swoop/faster
cadence revision passed **4/4 affected suites** (including radar audio), with no
compiler warnings and matching build/installed DLL hashes. New checks compare
the enemy's lower resonant band against the actual radar floor synthesis, require
each cue to fit the faster spacing, and simulate repetition at the service's
100 ms cadence. This measures signal differences, not perceptual acceptance;
listening to the latest revision remains pending.

Automated coverage checks native classification, selection priorities, dead and
friendly exclusions, occlusion, fixed screen directions, changing distance,
cadence, fairness, lifecycle silence, config parsing, PCM bounds, stereo balance,
front/rear differentiation, gain and invalid inputs.

Live listening remains necessary: approach a town NPC from both sides, then
find a Fallen shaman and a ranged pack outdoors, and compare against an elite.
Move closer/farther, kill the enemy, open dialogue/pause, and change areas.
Confirm cue loudness and identity alongside the radar and game combat audio.
`[entity-cues]` logs selection and suspension; `[entity-audio]` logs device state.
