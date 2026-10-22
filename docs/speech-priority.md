# Speech priority (2026-10-19)

A tester asked how the mod weighs announcements in fast combat; the user asked for a menu where the
player chooses what may cut in. Code: `src/speech_priority.*`; the settings list builds one entry
per kind in `keyboard_service.cpp` (`Priority`), group **Speech priority**.

## How speech is delivered

All speech goes through `ScreenReader` (PRISM on its own thread). A phrase either **queues** behind
what the screen reader is saying or **interrupts** it (clears what is waiting and cuts in).
`ScreenReader::Urge` interrupts and opens a short window in which any later interrupting phrase
queues instead, so the urgent phrase is heard whole. There is no ranking beyond that: whichever
interrupting phrase comes last wins, unless an `Urge` window is open.

## Levels

| Level | Delivery |
|---|---|
| waits its turn | always queued |
| cuts in when urgent | what the mod decided before the menu existed: interrupts only when the caller marks the phrase urgent (the vitals meter's, the boss life meter's and the place tracker's own `interrupt` flags); otherwise queued |
| cuts in | always interrupts; a later interruption can still cut it off |
| cuts in and cannot be cut off | `Urge` with `SpeechPriority::HoldMs` (1.5 s) |

## Kinds and defaults

The defaults reproduce the behaviour before the menu, except status endings, which the tester
asked to cut in, and arrivals (2026-10-21): at the Cathedral's doors the door's name, said as the
cursor came onto it, cut "Arrived at Cathedral" off; held, the name follows it.

| Kind | Where it is spoken | Default |
|---|---|---|
| Status effects beginning | `vitals_service.cpp` `AnnounceEffects` | cannot be cut off |
| Status effects ending (and the shrine's end) | same, and the shrine check | cuts in |
| Life and mana alerts (mercenary life, its level) | `vitals_service.cpp` `Announce` | when urgent |
| Boss life | `boss_life_service.cpp` | when urgent |
| Enemy names (the HUD target plate) | `interaction_reader.cpp`, monster labels | when urgent (queued) |
| Game messages ("X was slain", level-up and other notices) | `game_text_reader.cpp` notices | when urgent (queued) |
| Quest updates | `quest_service.cpp` | when urgent (queued) |
| Places | `place_service.cpp` `Announce` | when urgent |
| Items on the ground | `ground_item_service.cpp` nearby items | when urgent (queued) |
| Tutorial | `tutorial_service.cpp` | waits its turn |
| Arrivals at a target ("Arrived at X": the beacon's and the route's) | `beacon_service.cpp` `AnnounceArrival` | cannot be cut off |

Menus, focus and window changes are not in the list: they interrupt because they are context
changes (§9 of the playbook). "You are fortified" stays queued so it follows the status ending.

Each entry is saved as `priority-<kind>` in `settings.txt`. A status effect's end is now said
`StatusEffects::EndHoldMs` (500 ms, was 1 s) after the state goes, the window in which a re-applied
chill or poison still counts as the same effect.

**The eclipse waits for a pause (2026-10-21).** On reaching the Lost City the area's name and the
Tainted Sun's own news came with "An unnatural darkness blankets the land" and the tester could not
tell whether it was said. It now waits until speech has paused `SpeechPriority::QuietMs` (10 s at
most, `PatienceMs`, the tutorial's wait, now shared as `SpeechPriority::Pause`), then is said as
Quest updates are. A player who wants quest news never cut off sets Quest updates to "cuts in and
cannot be cut off". Log: `[quest] eclipse said`.
