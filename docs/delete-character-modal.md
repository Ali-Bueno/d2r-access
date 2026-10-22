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

## What the controller layout actually says

`Controller/DeleteCharacterModalHD.json` is based on
`Controller/ConfirmationModalHD.json`, and that base is the whole answer:

```
Confirm  -> ButtonLegendWidget ConfirmLegend  presets: { "action": "select" }
Dismiss  -> ButtonLegendWidget DismissLegend  presets: { "action": "back"   }
```

So in controller mode this is an **action prompt, not a navigable panel**. The
two buttons are not options you move between -- they are labels showing which
*global* action does what: select confirms, back dismisses. The game gives
nothing focus because nothing is meant to take it.

That also means **do not read them as "yes" and "no"**. The string table does
carry `strdeletecharok` = YES and `strdeletecharnot` = NO as the button faces,
but reading those tells the player nothing they can act on. What they need is
the control: the legend, read live, names the button actually bound.

And it explains the functional failure, not just the silent one: with no focus
and no announcement, there is nothing to tell the player *which* button to
hold, so "press and hold" is unactionable even for a sighted player using a pad.

## The fix (implemented)

`src/modal_reader.cpp`. On the active panel changing to one that has **no
focused widget**, read it from what it shows:

1. Its `TextBoxWidget` prompt -- what is being asked.
2. Its `ButtonLegendWidget` legends, through the existing
   `NativeControllerHelp::Legend`, which decodes the U+E000 glyph range into a
   control name. Read **live**, not from the string table, so it names the
   button actually bound rather than a hardcoded guess.
3. Interrupting, because a destructive confirmation is a context change and
   should not wait behind whatever was being spoken.

**Focus is never moved.** Driving the game's own cursor is what broke the
controller focus handling before, and it is not needed: the panel is read
where it stands.

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

MEASURED 2026-10-21 (Spanish, pad): the prompt and the buttons' faces are read,
"Pulsa y mantén pulsado para borrar…, SÍ, NO", but **the legends hold no text**
(no `[modal] …Legend` line), so no button was named and the user could not
delete a character. Since then the reader names a `Confirm` or `Dismiss`
button whose `ConfirmLegend` / `DismissLegend` child is empty by its action's
pad button, A for select and B for back (`LegendGlyph` in
`src/modal_reader.cpp`): "A: SÍ, B: NO". That is the default binding, not read
from the game; the hold's length is not measured.

## The hold's count (2026-10-22)

A tester: holding A to delete counts down from five on screen, and nothing said it. While the
modal is active and A is held, the mod says 5, 4, 3, 2, 1, one a second from the press, each
cutting the last (`ModalReader::HoldCountdown`, `DeleteHoldSeconds`); letting go starts it over.
The five seconds are the tester's count, not measured. Whether the game writes the count into the
Confirm button is unknown: its text is logged while A is held, `[modal] delete: Confirm reads
"..." after N ms held`, and `[modal] delete: A let go after N ms`. A keyboard's hold is not counted.
