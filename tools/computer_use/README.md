# Local computer use for D2R

An assistant or developer can inspect the running game, send one short input,
and compare the next screenshot with the plugin's native radar diagnostics.
This is a local command-line runtime, using Python's standard library and the
Windows desktop APIs. It needs no packages, API key, server, controller driver,
or additional DLL. Tested with Windows, Python 3.13 and D2RLoader 1.2.1-beta.

## Start a session

1. Start NVDA and D2RLoader, enter the offline game, and leave the controller idle.
2. Run `python -B tools/computer_use/desktop.py list` from the repository root.
3. Use the returned **current PID** in every targeted command. It changes when
   the game restarts. The game process can be named `D2RLoader.exe`.
4. Put Diablo II in the foreground. The optional `focus` action attempts this,
   but Windows may refuse; in that case the user must use Alt+Tab.
5. Capture and inspect the game before sending input. After switching from
   controller to mouse/keyboard, wait for the new layout and capture again.
   The tested transition took about 640 ms.

```powershell
python -B tools/computer_use/desktop.py --pid 10420 capture
python -B tools/computer_use/desktop.py --pid 10420 key ctrl+t
python -B tools/computer_use/desktop.py --pid 10420 key f13 --seconds 0.25
python -B tools/computer_use/desktop.py --pid 10420 capture
```

Replace the example PID. Each invocation prints JSON, returns a nonzero exit
code on failure, and appends a timestamped record to `actions.jsonl`. Capture
results contain an absolute PNG path and its original width/height. Artifacts
default to `%LOCALAPPDATA%\D2RAccess\computer-use`; `--output-dir` overrides it.
Keep captures and session logs outside the repository.

## Controls

| Command | Meaning |
|---|---|
| `list` | Find visible D2R windows, without sending input |
| `focus` | Attempt to restore/foreground the chosen window |
| `capture` | Save the visible game client as PNG |
| `key ctrl+t` | Hold a key/chord for 80 ms, then release |
| `key f13 --seconds 0.25` | Hold for an explicit duration, up to two seconds |
| `move X Y` | Move the pointer within the game client |
| `click X Y [--button right]` | Click a client pixel |
| `scroll X Y -3` | Scroll down three notches at that client position |

Mouse coordinates are **original client pixels**, with origin at its top left.
An image viewer may scale a 2560×1440 capture to 2048×1152: convert displayed
coordinates back to the original dimensions before clicking. Inspect the actual
UI; menu positions are not built into this tool.

Supported keys: ASCII letters/digits, F1–F24, arrows, PageUp, PageDown, Home,
End, Ctrl, Shift, Alt, Enter, Escape, Space, Tab and Backspace. Separate up to three keys with `+`.
Global application shortcuts still apply. In particular, F12 triggered Steam
screenshots during setup and was replaced. The numeric keypad can be consumed
by NVDA's review commands and is not used.

The local character's previously unbound movement commands were assigned in
**Options → Controls**, then saved with **Apply**, on 2026-09-15:

| Game action | Test key |
|---|---|
| Move up / Hacia arriba | F9 |
| Move right / Hacia la derecha | F10 |
| Move down / Hacia abajo | F11 |
| Move left / Hacia la izquierda | F13 |

F13 is a Windows function key that the tool can send without a physical F13 key.
These are game bindings, not plugin shortcuts. They were verified in gameplay;
inspect bindings again for another character or machine. Game/controller
configuration files are never edited by the tool.

## Guards and limits

- Target selection requires the expected game title and executable name.
  Actions recheck the selected window's PID, executable path and foreground state.
- During a held key/click, focus is checked every 10 ms. **Pause** stops the hold.
  Previously pressed keys/buttons are released in reverse order in `finally`,
  including when an action fails. Already-held requested keys/modifiers prevent
  accidental chords. Windows can still reject an input or release.
- Scroll is limited to eight notches, clicks to the client rectangle, and key
  holds to two seconds. Each action ends; there is no background navigation loop.
- Desktop focus checks reduce accidental input; they cannot atomically lock
  Windows focus. Let each action finish before taking over the mouse/controller.
- Capture uses visible screen pixels, including overlays. It cannot inspect a
  hidden/minimized game. It does not capture audio or emulate analog stick values.
- Read `Ctrl+T` results in `d2rloader/logs/d2r-access.log`; a successful SendInput
  call alone is not proof that the game moved. Match screenshots and positions.
  Allow the player to stop before taking a stationary diagnostic. A zero-velocity
  dump does not validate the moving reaction forecast or audible cue timing.

## Verification

```powershell
python -B tests/computer_use_tests.py
```

Tests mock all input calls. They cover target/focus guards, duration limits,
release after failures, held modifiers, scroll limits and PNG pixels/checksums.
The live smoke test covered capture, mouse menus/scrolling, all four movement
bindings and Ctrl+T: [measurements](../../docs/computer-use-radar-check.md).

Implementation references: Windows [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
and [BitBlt](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-bitblt).
