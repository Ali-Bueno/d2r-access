# Crosshair beep

Raine, the tester's wife, 2026-10-22: what matters is knowing *when* the character is targeting
something. She proposed a ray from the character's front that beeps when it crosses an enemy,
like Shades of Doom's targeting sensor.

A ray of our own would disagree with the game. The pad's auto-target is not a ray: every frame
it scores each enemy in the front half by aim and distance ([controller
targeting](controller-targeting.md)). It writes the winner into the hover entry, the same one
the world target label names ([target sight](target-sight.md#the-games-own-target)). The beep
follows that entry, so it plays for the enemy a sighted player sees outlined and the next
attack takes.

## Behaviour

- `EntityCueService` reads `NativeHover::Read` on its 100 ms poll. When the entry holds a
  monster that is a read enemy in sight, and not the one last beeped for, it plays the beep,
  placed and pitched toward that enemy like every other cue (`Crosshair::Tracker`).
- The pad's target hops between a pack's members several times a second (no stickiness in
  controller mode). A monster beeped for stays quiet while it was the target within the last
  3 s (`Settings::repeatMs`, sound design, as the monster plate's repeat): held, or back after
  a hop or a moment with no target. A new monster beeps at once, unless it is within 250 ms of
  the last beep; then it waits for a later poll, and beeps then if the target held
  (`Settings::gapMs`, sound design).
- The game aims through walls. Like every automatic cue, the beep requires a line of sight.
- The hover entry holds the best target of every category, so an object or item nearby can
  win it over a monster, while an attack takes the best monster. The beep then stays silent.
- The sound: two flat 24 ms blips, 40 ms apart, at 1175 Hz with odd harmonics
  (`Sound::Sight`). It is unlike the reach ping's two rising sine notes and the pickup's single
  noisy tick. Like the other feedback cues, a presence pulse never replaces a pending one.
- Priority within one poll: corpse rattle, reach ping, crosshair beep, NPC "!", presence pulse.

## Controls

Ctrl+S `crosshair` (on by default); `access-entity-cues crosshair-on|crosshair-off`;
`radar.toml` `[entity_cues] crosshair = true`. `status` says it. The glossary plays it
after the melee reach ping, and the tutorial teaches it ([tutorial](tutorial.md#the-crosshair-2026-10-22)).

## Logs

- `[crosshair] the game's target is read: type=… id=…`: once per game, the first time the
  hover entry is set. With a pad and no mouse, this line answers the open question of whether
  the controller's auto-target fills the entry.
- `[crosshair] id=… distance=…`: each beep.

## Validation

Automated: `crosshair` (enemy only, placement, hold, loss, hop gap, the 2026-10-22 trading,
repeat time, reset, walls),
`entity-cue-service` (beep through the service, once per target, switch),
`entity-cue-audio` (length, two equal blips, rear pitch, placement).
Live 2026-10-22 (a barbarian, two hours, pad): the target was read in every game; each beep
fell on the monster the HUD plate named, at the plate's distance; the farthest at 20.8
subtiles, inside the auto-target's limit. Without the repeat rule it beeped 2077 times, 26 in
ten seconds at peak: in packs the target traded back and forth every third of a second, a beep
each. Replayed over that log, the 3 s rule leaves 1166. Not yet heard with the rule.
