# Enemy growls and attack reach

Hand-ported from upstream `b3997e1` on 2026-10-03. Both channels are on by
default, as upstream had them; `ordinary = false` and `reach = false` restore
the priority-only cues. The keyboard settings list offers ordinary enemy
growls and the attack-reach ping.
The enemy master switch also controls them.

With growls enabled, up to three enemy streams prioritize important enemies,
resurrectors and archers, then fill remaining streams with nearby ordinary
enemies. Each ordinary rank has its own cadence. NPCs retain their tap; all
existing priority sounds retain their synthesis. The new ordinary cue is a
short low growl; reach is a two-note chime.

`AttackReach` derives melee/ranged style from bound controller skills and the
active weapon. When controller bindings are unavailable, `NativeAttack` uses
this branch's existing classification of the keyboard's selected native hand
skills. Skill data and equipment are read only. Melee defaults to three
subtiles plus the weapon's `rangeadder`; ranged defaults to the entity scan
range. This is a cue estimate, not a promise that every spell can hit.

The ping occurs when a visible enemy enters reach while none was within it.
It rearms after all enemies leave reach plus one subtile. UI/lifecycle changes
reset the baseline. Loadouts are refreshed once a second while the option is on.

`[entity_cues]` keys: `ordinary`, `reach`, `attack` (`"auto"`, `"melee"`,
`"ranged"`), `melee_reach`, `ranged_reach`. Console:
`access-entity-cues ordinary-on|ordinary-off|reach-on|reach-off|attack-auto|attack-melee|attack-ranged`.

Upstream's replacement monster-name admission policy is not imported. Existing
local nameplate, rank/enchantment, immunity and sight handling remain active.
Pure reach, cue scheduling and synthesis tests cover the added channels; live
listening remains user-led.
