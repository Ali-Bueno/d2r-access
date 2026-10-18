# D2RLoader multiplayer

The loader's native multiplayer menu was not in the game's generated widget
catalog. The shared native focus fallback now reads those controls from their
visible widget tree. This includes editable field labels, buttons, checkbox
state, selected tabs and the direct-join overlay. Password contents remain masked.
The existing text-field reader reads non-password values and edit feedback.

## Play with a friend over the internet

1. Both players launch `D2RLoader.exe` with compatible loader versions, game
   data, mods and shared extensions. This installation uses **1.3.0-beta**.
2. Select a local/offline character and open **Multijugador**, beside **Jugar**.
3. The host chooses **Dirigir partida**, enters a name and optional password,
   chooses a difficulty the character has unlocked, enables **Unlisted**, and
   leaves **LAN Only** disabled. Then choose **Crear partida**.
4. The host finds the Game ID in the in-game party panel and shares it with
   the friend, along with any password.
5. The friend opens **Browse Games → Direct Join**, enters the Game ID and
   password, and chooses **Unirse**.

Some loader captions are English even with Spanish game text; the reader uses
the actual displayed captions. Unlisted games do not appear in the public list.
TCP/IP uses local characters, rather than Battle.net characters.

The right-stick scanner has a **player** category (`jugador`) listing the
other characters nearby by name, distance and direction. Party members are
listed anywhere on the same level. A left-stick click follows the one just
named (`docs/entity-scan.md`). To be found from afar, both players must be in
the same party (the party panel's invite button). A friend anywhere in the rooms
the game has loaded is placed at their actual position. The party roster only
places a friend who is farther away (`docs/entity-units-re.md`). Following a
friend also works through rooms this player has not explored, even in
exploration mode (`docs/beacon.md#exploration-mode`, 2026-09-26).

### Diagnosing a multiplayer problem (2026-09-26)

D2RLoader overwrites `d2rloader/logs/d2r-access.log` at every launch and has no
retention setting. After anything odd, copy the log before starting the game
again. In the log:

- A lasting `[radar] status=level-mismatch` or `status=player-identity` points
  at the mod's lifecycle. `[radar] lifecycle=` now prints `eventPlayer=`, the id
  the loader's event carried.
- `[radar-audio] stage=failed reason=stalled` followed by `Output device failed;
  reopening` is an audio output that kept a buffer. Radar, beacon and entity cue
  players now give up on a buffer one second after its length and reopen the
  device (`src/wave_out.h`). Before, the radar stayed mute until a restart.
- `status=ready`, with a `[beacon] position=` frozen while the character moves,
  suggests a network desync rather than the mod.

The mod's speech and audio play locally; no accessibility audio is sent to the
other client. Navigation uses the local-player position and identity. Vitals
read this client's HUD and local character; mercenary/corpse readers check
ownership, and footstep handling compares the entity to the local player.
Nearby shared items, portals and quest changes can still produce each player's
own announcements. Quest completion checks player/session identity; packet text
is correlated with this client's quest alerts. This code audit does not replace
a live two-client audio check.

If the multiplayer button is absent, enable **Show TCP/IP Button** under
**D2RLoader settings → General**. Vanilla already shows it automatically.

For internet hosting the default port is **TCP 4000**. The loader attempts
automatic router setup and checks external reachability. During the local
2026-09-23 inspection it reported **Hosting: Issues**, with an on-screen tooltip
stating automatic port setup was unavailable. The other player can try hosting;
otherwise the host needs suitable router/firewall configuration. A custom
`game_port` must match on both computers. No network settings were changed by
this accessibility update.

Sources: [D2RLoader multiplayer guide](https://d2rloader.net/tcp-ip.html),
[release notes](https://d2rloader.net/changelog.html).

## Reading policy and validation

The existing catalog still handles its established controls. Uncatalogued
controls fall back to native types and visible text; a focused list row is read
without enumerating neighboring rows. Native checkbox and dropdown changes read
only the changed value. The direct-join subtree is treated as an overlay, even
though the loader opens it inside the existing panel.

Passive hosting and browser text is settled and diffed before being queued.
Set `[multiplayer] enabled = false` in `d2rloader/config/d2r-access.toml` to
disable those passive announcements; normal focused-control speech remains.

The loader's forms now use one D-pad press per control, following the visible
rows and columns. Release before moving again; A/cross keeps its native action.
The shared cursor adapter places the real cursor over the chosen control and
updates native hover and focus together. Disabled controls and controls behind
the direct-join modal are excluded. Native text editing and horizontal dropdown
changes keep their own input handling. Set `[menu_navigation] enabled = false`
to restore the loader's original travelling cursor in this menu.

Build/test and live validation results are recorded in `STATUS.md`. A local
menu check does not establish that a second player can connect from another
network. On 2026-09-23 the user confirmed short-press navigation, activation,
retained checkbox focus and state announcements. Live logs also confirmed
direct-join field navigation and cancellation. Selection in a populated public
game list and a two-player session still need their own live checks.

The selected character's native controls reported Normal enabled, Nightmare
and Hell disabled. Navigation skips those unavailable buttons. Difficulty
unlocking follows game progress, not an accessibility setting; see Blizzard's
[difficulty guide](https://classic.battle.net/diablo2exp/basics/difficulty.shtml).

## Native evidence (D2R 3.3.93787, loader 1.3.0-beta)

The installed `d2rloader.mpq` contains 51 files, including 19 layout JSONs, but
no host/browser/direct-join layout. The loader builds those controls at runtime.
`mainmenupanelhd.json` defines `MainMenuPanel/TCP` with
`@d2rloader:D2RLoaderTcpIp` and the action
`PanelManager:OpenPanel:D2RLoaderMultiplayerPanel`.

Runtime inspection confirmed `HostContainer/HostGameDetailsColumn` with
`HostGameNameLabel/Input`, `HostPasswordLabel/Input`, `HostDescriptionLabel/Input`,
`UnlistedToggle` and `LanOnlyToggle`. Browser fields are under
`BrowseContainer/BrowseLeftColumn`. `DirectJoinContainer/DirectJoinModal`
contains `DirectJoinTitle`, address/password label and input pairs, and
`DirectJoinConfirmButton` / `DirectJoinCancelButton`. Background widgets remain
visible while that overlay is open. `MainTabs` supplies the selected tab.

`HostStatusStrip/HostingStatusLabel` and `HostingStatusText` carry hosting state;
`BrowseContainer/BrowseLeftColumn/BrowseEmptyState` carries empty/filtered results.
These text boxes can be disabled while still visibly displaying useful text.

`ToggleButtonWidget::isToggled` is a byte at **+0xB88**. The live reflection
record's name pointer at game RVA `0x2536F80` names `isToggled`, and its member
offset at `0x2536F90` is `0xB88`. The vtable `+0x08` getter at `0x882D40`
reads it at `+6` (`0F B6 91 88 0B 00 00`); the toggle function at `0x882D70`
compares it at `0x882D76` and writes it at `0x882DA6`. The reader checks that
instruction and accepts only 0 or 1. Live states matched the displayed
checkboxes: Terrorized on, Unlisted/LAN Only/level difference/item spacing off.

`InputTextBoxWidget` stores its editable engine string at **+0x558**, unlike
`TextBoxWidget`'s displayed string at +0x88. Typing `abc` into SearchInput and
clearing it through the UI changed that exact string. Vtable `+0x70`, function
`0x889EC0`, reads it at `+6` (`48 8B 81 58 05 00 00`), also read by the native
draw/update path at `0x88E921`. The reader checks the getter and excludes
`MaskedInputTextBoxWidget`. The search placeholder is separate at +0x810.

The native difficulty dropdown retains the inherited button caption at +0x8A0
(`Normal`) and the tooltip at +0x88 (`Filter by difficulty`). Those are the same
verified button fields used by the shared native text reader.

### Controller travel versus text editing

The user confirmed host-field speech but reported that short D-pad presses did
not leave `HostGameNameInput`, whereas holding the direction moved through the
form. Live focus snapshots and screenshots agreed with the announced control;
the maximum-player button also changed 8 to 7. The active text-editor pointer
was null while this happened, so field focus alone was not evidence of edit
capture. No A-button replay or synthetic toggle activation was added.

`InputTextBoxWidget` edit callback RVA `0x88C7C0` is 70 bytes, FNV-1a
`E6E4FE9DF7F4CE48`. Its RIP-relative load at +11 reads the active editor global
at RVA `0x3440C48` (displacement at +14, instruction end +18). Initialization
verifies the whole callback before deriving that pointer. An unavailable state
or an active editor leaves text interaction to the game.

Menu navigation reuses the verified cursor positioning, hover refresh,
FocusableWidget selection and gesture cancellation documented in
[controller-navigation.md](controller-navigation.md). Only the skill grid or
the loader form owns that adapter during one UI tick. Geometry comes from the
native focus-rectangle virtual, and candidates must use the verified selection
function at vtable +0xA0. Full press/release gating prevents stock cursor travel
from continuing a gesture after the selected control has changed.

Live controller actions at 12:21:14 and 12:22:35 changed Unlisted, then reset
native focus to `HostPasswordInput` / `HostGameNameInput` on the following tick.
The value-action observer retains the activated control once after matching
the native command to its name (`ToggleUnlisted` / `UnlistedToggle`, or the
corresponding increment/decrement and difficulty pairs). It only restores a
visible, enabled control inside the current modal when focus fell back to an
input. New D-pad presses and active text editing take precedence. It never
dispatches or repeats the original action.

### Typing with the controller (2026-10-15, built, unverified)

A tester (2026-10-15): the host boxes (game name, password, description) and
the direct-join boxes take nothing from the pad, and the browser's game list
could not be entered. Two things, `src/pad_typing.cpp`, switch `pad-typing`
(Controller group, on):

- **A on a text box** that does not start the game's own editing within 250 ms
  (`NativeSkillNavigation::TextEditing` stays false) gets a mouse click on its
  focus rectangle (`MenuMouse::ClickAt`), the keyboard mode's own way into a
  box, so a keyboard then types there. Log: `[typing] A did not start editing`.
- **While a box is being edited**, the right stick picks characters: up and
  down step through `PadTyping::Charset` (lower case, digits, space, `-_.:`,
  upper case), each said as it is reached; right sends the one reached as a
  keystroke (`SendInput`, `KEYEVENTF_UNICODE`) and left sends Backspace, so the
  text field reader echoes them as typing. Log: `[typing] 'a' sent`. The game's
  own B ends editing as before.

The game list's rows (`Kind::ListItem`) are now among the D-pad navigation
nodes (`menu_navigation.cpp`). None of this has been tried live: the first
log of pressing A on a loader box will say whether the loader starts editing
by itself (`[field] focus ... editable=1` followed by typing) or needs the
click.

### Text boxes name the box being typed into (2026-09-25)

The user heard the game-name box named while typing a password, and a box's
contents cut off by its own label. D2RCore 1.3.0-beta's form refresh
(`0x2FC470`, default focus at `0x2FEBC7`–`0x2FEC13`) picks `DifficultyNormal`
under LAN Only, else `HostPasswordInput` when Unlisted, else `HostGameNameInput`
(flags `0x687F3A` / `0x687F39`), and can run while a box is edited. Typing still
goes to the active editor (`0x3440C48`, stored by begin-edit `0x88C810`), so
`NativeSkillNavigation::ActiveTextInput` names that box whenever it belongs to
the focused panel, in `FocusTracker` and `TextField`. A box is now spoken as one
phrase, label, role, then contents (`Readout::contents`; masked boxes expose
none): `TextField` used to speak the contents on arrival, and the tracker's
33 ms poll then interrupted them. After editing ends, focus is announced where
the game left it; restoring it to the edited box was not added. The refresh
trigger and whether a controller `TextInputModal` owns the editor are unverified:
next session, log `[field]`/`[focus]` while pressing A on the password box.

## Things another player takes (2026-10-15)

The host kept tracking a potion the other player had picked up (about 10 s) and Cain after the
other player freed him (about 30 s). The beacon kept leading to the last place of a target it
could no longer read. Now a target read absent from the player's room or one touching it (the
game keeps those loaded) for more than 2 s is lost at once ("[beacon] gone"); exits are left to
arrival, as not all are units. The host's log also showed "Cain's Gibbet" listed from the
remembered places for 12 minutes after the rescue: a remembered quest object whose spot the map
can see without it is forgotten for the run ("[terrain] quest place gone").

## The lobby regression, measured live (2026-10-17)

A tester (build 2026-10-16): text boxes reached erratically, typing a gamble, and Direct Join's
password box sending the player back to the main multiplayer window. The user reproduced it with
`[menu-navigation] boxes:` (each box and why it is or is not a D-pad stop) and
`[menu-navigation] editing <box>` (the game's active editor):

- **LAN Only disables all three host boxes**, and **Unlisted disables the game name**: the
  loader's rules, not the mod's (`HostGameNameInput(disabled) HostPasswordInput(disabled) ...`).
  The form then opens on `DifficultyNormal`.
- **The synthetic click was the kick.** Pressing A on a box the game did not start editing made
  `PadTyping` click it; the real mouse event made the game switch every panel to its mouse layout
  (`d2rloader.log`: `Loaded panel D2RLoaderMultiplayerPanel` without `Controller/` 90 ms after the
  click) and back on the next pad input, rebuilding every panel: a modal such as Direct Join is
  gone, and the box is never edited. The click is removed. It also fired from the A that opened
  the form; an A pressed before the form showed is now dropped.
- **The D-pad went to the nearest rectangle**, which jumps between the form's columns and stays
  put when nothing lies that way. Up and down now walk the form's own order (the widget tree's,
  column by column), wrapping at the ends (`MenuNavigationPolicy::Step`); left and right keep the
  nearest control within a row (difficulties, player count).
- **Begin-edit instead of the click.** When A leaves a box unedited for 250 ms, `PadTyping` calls
  the game's begin-edit `0x88C810(box)` (`NativeSkillNavigation::BeginEdit`, verified with
  `IsEditing` `0x88B180` and the active-editor global it stores; `[typing] Begin-edit verified.`),
  only when no box is edited, the box holds the game's focus (FocusManager+0x190) and takes input
  (+0x589). Log: `[typing] A did not start editing <box>: <reason or "begun by the mod">`. While
  the game's text entry panel holds focus over a lobby box, `MenuNavigation` keeps the right-stick
  typing running.

Measured live by the user (2026-10-17, Host Game, neither LAN Only nor Unlisted): the game name box
reached with the D-pad, A started the game's own editing (the mod's begin-edit was not needed),
keyboard and right-stick characters and Backspace reached the box, B ended editing; no panel
reload. Direct Join's boxes were not in that log.

## The tester's lobby notes on build 2026-10-17

The log showed A on a box starting the game's editing (`TextInputModal` opens), but that panel says
nothing, so A seemed dead: the start is now said ("Editing. Right stick picks a character, A types
it, B finishes"). The stick steps forward with up (most games' pickers), and A types the character
reached as right does. **Not done:** Enter (or A) ending the edit. The game ends it on B through
its text entry panel; no end-edit counterpart of begin-edit (`0x88C810`) is known, and a synthetic
Esc risks closing Direct Join. LAN Only and Unlisted on at start are the loader's, not the mod's.
