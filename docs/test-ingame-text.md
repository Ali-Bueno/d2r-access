# In-game text validation (Xbox-style controller + NVDA)

Build/install: close D2R, run `./build.ps1`, then launch D2RLoader with NVDA running.
The build deploys automatically to the already configured game folder. Use the
existing offline character. This feature is implemented but not yet confirmed
in a live controller session.

## Controller test sequence

1. On the character list, select the existing character and press **A** to play.
   In town, listen for the localized area-name banner. Front-end menus should
   still read as before; no NPC text should be spoken before loading the character.
2. Approach an NPC and use the game's interaction button (**A** when its prompt
   appears). Movement to the NPC still needs the current manual assistance; this
   build does not add radar/navigation. Akara and Warriv are useful Act I checks.
3. If a greeting starts, hear its full displayed text once. Wait several seconds:
   scrolling should not restart it. Press **B** to leave the speech when the game
   permits it. Expect the NPC heading and currently selected option together.
4. Press **D-pad Down**, wait, then **D-pad Up**. Expect only the newly selected
   option each time. Other options should not be enumerated. If the game uses
   the left stick here, use the same sequence with that control.
5. On **Hablar**, press **A**. Move through the available topics with **Up/Down**;
   the new labels must be read even if the selector stays in the same place.
   Press **A** on a topic. Hear the complete displayed dialogue once.
6. Press **B** to return, then **B** to leave the NPC. Old dialogue should stop,
   and returning to the NPC should announce its heading and selected option again.
7. Repeat the interaction with a second NPC. The same reader should handle it.
8. When a tutorial/automatic hint appears naturally, hear its displayed text once
   and, if one exists, the focused button. Use **A/B** as the game's prompt indicates.
   Optional keyboard check: trigger the graphics-mode hint with **G** if that hint
   has not already been dismissed in this save; toggling G does not guarantee it reappears.
9. Cross from town into another area, then return. Expect each displayed area name
   once. If possible, also check a dungeon entrance. Do not expect navigation cues.
10. Open **Menu/Start** during gameplay and return with **B**. Existing pause-menu
    focus should still work. Exit and reenter the character using the normal menu;
    no text from the previous session should be replayed during loading.

Stop if there is a crash or sustained input delay and retain the log. It is enough
to report the NPC/menu and last button pressed; screen captures are unnecessary.

## Optional diagnostic commands

Developer console, `Ctrl+backquote`:

- `access-text dump`: capture visible native text trees/focus on the next gameplay poll.
- `access-text repeat`: repeat the current conversation/tutorial or visible area banner.
- `access-text off` / `access-text on`: toggle this reader for the current game process.

The console can itself change focus. For a useful dump, close it immediately
after requesting one. The reader always uses actual current game focus.

Log: `D:/games/steam/steamapps/common/Diablo II Resurrected/d2rloader/logs/d2r-access.log`.
Use the last `d2r-access loaded.` session; inspect `[text]`, `[text-tree]`, `[ui]`
and `[native]` lines. The startup native-RVA line precedes `d2r-access loaded.`.

## Automated checks

`ctest --test-dir build --output-on-failure` runs four native C++ test executables
(73 checks). Coverage includes complete long strings, invalid memory, hidden
ancestors/templates, native focus and embedded button text, menu diffing, dialogue
transitions, covering modals, toggling, session readiness and listener cleanup.
Tests do not launch D2R or speak through NVDA. Test executables stay in `build/`;
only the plugin and the existing PRISM DLL are deployed.
