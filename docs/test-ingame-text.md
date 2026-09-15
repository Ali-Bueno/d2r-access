# In-game text validation (Xbox-style controller + NVDA)

Build/install: close D2R, run `./build.ps1`, then launch D2RLoader with NVDA running.
The build deploys automatically to the already configured game folder. Use the
existing offline character. The user confirmed the Rogue Encampment entry
announcement and the first G warning; NPC dialogue and repeated identical notice
deliveries still need live verification.

## Reported G warning

1. Select the existing Reign of the Warlock character with the controller and
   press **A** to enter town. No NPC needs to be found for this check.
2. Press keyboard **G**, the exact trigger in the user's report. NVDA should read
   "Solo disponible en partidas de clásico y de expansión" from the visible notice.
3. Wait without pressing anything: the notice should not repeat automatically.
   Press **G** again while the first notice remains visible. The newly delivered
   warning should be read again, despite having identical text. Repeat three times,
   allowing speech to finish between presses. Then wait until it disappears and
   press **G** once more; that new appearance should also be read.
4. Return to controller input and open **Menu/Start**, then **B** to return. Menu
   focus should still work. A new informational notice should also read without
   moving controller focus onto it.

Verify `[notice]` entries in the last plugin-log session. In Classic/Expansion
games G may switch graphics successfully and will not produce this restriction.

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
- `access-text repeat`: repeat the conversation/tutorial, otherwise the current
  notification, otherwise the visible area banner.
- `access-text off` / `access-text on`: toggle this reader for the current game process.

The console can itself change focus. For a useful dump, close it immediately
after requesting one. The reader always uses actual current game focus.

Log: `D:/games/steam/steamapps/common/Diablo II Resurrected/d2rloader/logs/d2r-access.log`.
Use the last `d2r-access loaded.` session; inspect `[notice]`, `[text]`, `[text-tree]`, `[ui]`
and `[native]` lines. The startup native-RVA line precedes `d2r-access loaded.`.

## Automated checks

`ctest --test-dir build --output-on-failure` runs five native C++ test executables
(116 checks). Coverage includes complete long strings, invalid memory, hidden
ancestors/templates, native focus and embedded button text, menu diffing, dialogue
transitions, covering modals, toggling, session readiness and listener cleanup.
Notification coverage includes native linked text and data-source filtering,
unfocused notices, coexistence with dialogue, fading/recycled/duplicate rows,
reappearance, partial text settling, repeat and feature disable. Delivery tests
cover identical successive messages, ring-slot reuse, older notices staying silent,
missing native metadata, hidden backing-store text and rapid-burst coalescing.
Tests do not launch D2R or speak through NVDA. Test executables stay in `build/`;
only the plugin and the existing PRISM DLL are deployed.
