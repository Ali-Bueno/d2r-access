# The delete-character modal reads nothing, and why focus cannot fix it

Observed 2026-09-19. The modal opens, the player hears nothing, and OCR of the
screen returns `RA-` where a sentence should be.

## What the screen actually says

`game-data/local/lng/strings/ui.json`:

| key | enUS |
|---|---|
| `strdeletecharmessage` | `Press and hold to permanently delete all versions of this character.` |
| `strdeletecharok` | `YES` |
| `strdeletecharnot` | `NO` |

OCR returned `RA-` because the rendered line is not plain text: a
`ButtonLegendWidget` splices the bound control in as a private-use glyph
(U+E000…U+E012), which OCR cannot render and a screen reader would read as
nothing. `controller_text.cpp` already decodes exactly that range, so the
mod is the only thing on this machine that *can* read the line correctly.

## It is catalogued. That is not the gap.

`src/generated/panel_widgets.h` has all four controls:

```
Widgets_DeleteCharacterModal[] = { Confirm, ConfirmLegend, Dismiss, DismissLegend }
```

`Confirm` carries `strdeletecharok`, `Dismiss` carries `strdeletecharnot`, and
both legends carry `strdeletecharmessage`.

## The gap: the game opens it without giving anything focus

From the live log:

```
[ui] target="PanelManager" command="OpenPanel"  text="DeleteCharacterModal"
[ui] target="PanelManager" command="ClosePanel" text="DeleteCharacterModal"
[focus] indicator at 0,0 0x0 matched nothing (best 0.00, - resolved 0/0)
```

Three facts in those lines:

- **`indicator at 0,0 0x0`** — the focus indicator has no extent, so geometry
  matching has nothing to match.
- **`resolved 0/0`** — no catalogued panel was even considered.
- **No `native identity missed` line at all**, which means `CurrentFocus()`
  returned a null widget. Identity resolution never had a candidate either.

So *both* reader paths are focus-driven, and there is no focus. The modal is
not mis-read; it is never read, because nothing ever fires.

**No amount of work on the focus reader fixes this**, and that is the point
worth recording: the ancestry fix that repaired the options panes
(`8c2ca60`) cannot help here, because the failure is upstream of matching.

## The fix

Trigger on the panel opening, not on focus.

1. **Hook the open message.** `ui_message_monitor` already observes
   `PanelManager` / `OpenPanel`, which is how the log above exists. A modal in
   a small table of "panels that open without focus" gets read on that message.
2. **Read the live legend, not the static string.** `native_controller_help.cpp`
   already reads a `ButtonLegendWidget`'s active entries in native order and
   decodes the control glyphs. That is what turns the line into "press and hold
   A" rather than "press and hold" followed by silence — and it stays correct
   if the binding changes, which a hardcoded string would not.
3. **Then the choices.** `YES` and `NO` from `Confirm` and `Dismiss`, so the
   player knows both what is being asked and what the answers are.
4. **Interrupt.** A destructive confirmation is a context change, and the
   playbook's rule is `interrupt=true` for those. Nothing queued behind it
   should delay it.

## Do it as an archetype, not as this modal

`DeleteCharacterModal` is the one we caught, not the only one. Any panel the
game opens without assigning focus is silent for the same reason, and the
project's own UI strategy is explicit that screens are never to be handled one
at a time. The reader should be "a panel opened and nothing has focus: read its
legend and its buttons", keyed on the condition rather than on the panel name,
with a per-panel override only where a genuine quirk demands it.

The diagnostic that makes those panels findable already ships: `fb7e389` logs

```
[focus] no focused widget while "<panel>" is active
```

once per panel, so a session's log now enumerates every screen with this fault
instead of surfacing one at a time as they are stumbled over.

## Status

Not implemented. The cause is confirmed from the live log and the strings and
widgets it needs are all in place; what is missing is the open-message trigger
and the archetype reader described above.
