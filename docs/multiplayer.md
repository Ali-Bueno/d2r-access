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
