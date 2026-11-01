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
