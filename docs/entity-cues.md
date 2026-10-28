# NPC and enemy sounds

Enabled by default. These short synthesized pulses use the same fixed screen
axes as the radar: stereo gives left/right, and a lower pitch marks screen-down.
Facing or turning the character does not rotate them. No audio assets or new
runtime dependencies are needed.

| Target | Sound | Selection |
|---|---|---|
| Friendly NPC or another player | Light wooden tap | Nearest visible NPC or player |
| Boss or elite | Deep resonant metallic swoop | Highest enemy priority |
| Resurrecting shaman (Fallen and fetish shamans, the Greater Mummy family, the Sarcophagus that spawns mummies; champions and uniques too, bosses keep theirs) | Mid-register resonant swoop | Next enemy priority |
| Ordinary ranged enemy | Brighter resonant swoop | Third enemy priority; optional |
| Any other enemy | Short rough growl | The nearest few, in the voices left; optional |

There are at most `max_voices` (default 6, 1-8) enemy streams plus the NPC. Each priority class has one
representative, so a crowd of archers makes one stream, not one per archer.
The streams the priority classes leave go to the nearest ordinary enemies in
sight, each heard on its own: a pack of Fallen sounds as its nearest
members, so its spread is audible. Each rank (nearest, second, ...) keeps its
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

Only loaded, living entities on screen are eligible (`on_screen = true`), using
`CombatTarget::OnScreen` (the extent the Tab key uses) widened by 3 subtiles for
ranged enemies, so archers and casters sound as they appear; `on_screen = false`
restores the `range` circle (15 subtiles). Both use the radar's terrain line-of-sight check. This has the same limitation as the
entity scan: walk-blocking fences may hide a target. An ordinary target in
another room behind a wall does not get a cue. These are direct bearings, not
routes around obstacles.

Pulse intervals shorten continuously from 600 ms at the screen's far corner (or the range edge) to 150 ms
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
speech (`Ctrl+N`) and the navigation radar (`Ctrl+G`). The same command accepts
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
on_screen = true
range = 15
max_voices = 6
pan_strength = 100
pitch_strength = 100
volume = 100
reach = true
crosshair = true
attack = "auto"
melee_reach = 3
ranged_reach = 15
```

Restart after edits. `pan_strength` and `pitch_strength` are percent (0-200, clamped,
100 = the compiled sound) of the stereo spread and the rear pitch drop; the cues are
synthesized live, not samples. `range` is used only with `on_screen = false`;
`ranged_reach` no longer follows it. Volume is 0–100 percent of the deliberately soft synthesized
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

## Enemy classes by layer (2026-10-13)

A tester: elite, boss, shaman and archer cues were the same swoop at different
pitches, and pitch already says up or down, so a boss above sounded like an
ordinary enemy below. Now every enemy growls in the same register and its class
adds a layer, at about half the growl's level and swelling with it:
champion/unique/minion a bright buzz chattering at 62 Hz, act boss or super
unique a metallic ring (a struck bar's partials), raiser of the dead a wailing
tone with vibrato, ordinary ranged a thin whistle. Elite and boss are now two
sounds (`Cue::boss`, from `EnemyTraits::boss`). `enemy-class-sounds` (Ctrl+S,
on, the user's choice) brings back the swoops. The pan and pitch strengths go up
to 400 percent; the pitch never drops below a quarter. The NPC "!" bell is in
[NPC proximity](npc-proximity.md). Not yet heard by the user: the glossary
plays each one.

### Elite layer louder and higher (2026-10-13)

Tester: the champion buzz was too soft or low against six growls (the archer's 1.65 kHz whistle was fine).
Fundamental 300 -> 750 Hz (ratio 5 over the 150 Hz layer base), six harmonics at 1/h with spread phases
(1.3 rad per harmonic squared, which keeps the crest at 0.41 of full scale for the 14000 peak test),
level 0.15 -> 0.28; layer RMS 0.035 -> 0.064 (1.8x, now equal to the growl's), spectral centroid
880 -> 1830 Hz, still tonal and chattering at 62 Hz, never noise. The ping test now asks for twice,
not four times, the elite's zero-crossing rate (a buzz this bright crosses zero more often); the
growl still sits four times below. Static modelling only (numpy replica of the synth); listen in the glossary.

## Critters are no enemies (2026-10-25)

After Duriel died a tester still heard growls: the larvae crawling in his lair. They are
`larva` (monstats.txt 283), one of Duriel's Lair's critters (Levels.txt `cmon1-4`: rat, bat,
scorpion, larva), killable and named, with a hostile `Align`, so the reader took them for enemies;
the other critters are friendly, unkillable or unnamed. Every class a level lists as a critter is
now left out of the scan (`Generated::CritterMonster`, generated with the level flags by
`tools/gen_level_flags.py`): chicken, rat, bat, bug, scorpion, larva, snake, parrot, minispider,
bunny. Cows and camels are not critters and stay listed in town.

## Catapults sound as archers; the battlefield's barbarians (2026-10-27)

A tester in Act V: the catapults (monstats `catapult1`-`4`, *hcIdx 497-500, AI `Catapult`, MissA1
`catapultchargedball`) are enemies that shoot from afar, but their `rangedtype` column is empty, so
the ranged flag (`MonStats+0x3F` bit 4) was never set and they had the plain enemy cue. They are now
ranged by class (`CatapultClasses` in `native_entities.cpp`), as the shamans are. Their spotters
(516-519) are not killable and stay out.

The same tester: the allied fighters on the battlefields were announced "Combatant", the game's own
name (`act5barb1`/`2`, 522-523, NameStr "Act 5 Combatant", AI `NpcBarb`). They are now named with
the game's "Barbarian" (ui.json, the class name, so every language has it), the way cows and camels
take a name from another table (`AllyNameKey`).
