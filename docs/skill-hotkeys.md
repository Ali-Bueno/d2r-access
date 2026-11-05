# Skill hotkey announcements

A keyboard skill hotkey (F1 to F8, the game's own binding) or a quick cast says the skill's name
when it selects a different skill than the last one named. Repeats stay silent; switching back
announces again. Mouse picks, the game restoring a skill after a quick cast, releases and
controller buttons are silent. Settings list (Ctrl+S): `skill-hotkeys`, on by default;
`[skill_hotkey] enabled` in `radar.toml` and `access-skill-hotkeys on|off`. Needs `[native]` on.

Credit: bladestorm360, branch `bladestorm`, commits 6ddca2e (announce native hotkeys) and ced5352
(only when the skill changes), with 4c1ce8a's notes. Ported by hand, not merged.

## How

- `src/native_skill_hotkeys.cpp` hooks two game functions and copies the slot and skill id of an
  accepted call into a queue: `Select(slot, defaultAttack)` at `0x118D50`, only when called from the
  keyboard dispatcher (return `0x120373`), and `QuickCast` at `0x149550` on key down (return
  `0x122F6A`). Both return sites come from the whole functions `0x11FF30` and `0x122F30`.
- The skill id is read with the binding getter `0x117D20` that `NativeSkillBar` already uses,
  skipped in controller mode (`0x77E10`).
- `src/skill_hotkey_service.cpp` drops the queue in menus, text entry and the settings list, names
  the skill with `NativeSkillBar::SkillName`, and speaks it. `src/skill_hotkey.h` holds the
  change test (`tests/skill_hotkey_tests.cpp`).

## Evidence and limits

- The four fingerprints (`Checks` in the .cpp) match `re/D2R_live.exe` (3.3.93787, code identical
  on 3.3.94995). If one differs at run time, or a hook fails, the feature stays off with one
  `[skill-hotkey]` log line.
- Differences from the branch: the hand (`0x117DE0`) is not read, as only the name is spoken; the
  last skill is forgotten when the player object changes (a new game).

## Quick cast

The game's option Quick Cast Skills (`KeyboardQuickCastEnabled`, a player's translation "Automatically
cast linked skills"). With it on a skill key casts and leaves the skill as it was: `0x149550` selects
the slot's skill, casts at the pointer, and restores on the key's release. All of it is the game's;
the pointer is the problem, as the mod parks it ([why](keyboard-play.md#the-pointer-at-rest)).

So `ObserveQuick`, as the key goes down:

- **A skill aimed at a monster:** before the game casts, `KeyboardService` picks the monster the
  skill's own key would, and `NativeHover::Hold` makes the game take it for what the pointer is on,
  until the key comes up. `0xF1A60`/`0xF15C0` are what the pad and the game's tests use. The game's
  own cast then does the rest, walking up included.
- **A corpse skill:** the game's filter refuses a corpse, silently: `0xF1A60` calls `0x9B840` with no
  skill and clears the entry (MEASURED: `aimed=1` under the first version, no skeleton, the corpse still
  there). `Hold` now checks the entry, so `aimed=` is true. The game's cast at the corner does nothing,
  and once the skill is in hand the mod sends the same `0x0D` the skill's key sends. MEASURED: `Skeleton: 1`
  from the key with the option on, and "Usable corpses nearby: 0" with none.
- **Anything else** (a buff, Teleport): left to the game, at the pointer. A buff needs no aim;
  **Teleport goes to the corner**.

`[skill-hotkey] quick cast slot=.. skill=.. accepted=.. aimed=..` is logged for every press that
arrives. No such line for a key means it did not reach the function.

MEASURED at a Zombie 18 subtiles off, with Attack bound to a key: `aimed=1`, then the game's own
`0x02 a=1 b=15` (walk to it) and, on arrival, `0x0D a=1 b=15`, what a click on it sends. Hook and
tick share a thread (`thread=14132 ui_thread=14132`), so reading the world from the hook is safe.

**Not run:** a skill that flies, as Miasma Bolt, at a monster. In a log, `aimed=1` means the game took
the target.
