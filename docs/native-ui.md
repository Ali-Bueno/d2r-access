# Native in-game text adapter

Implementation date: 2026-09-15. Target: D2R 3.3.93787, D2RCore 1.2.1-beta.
This is a read-only adapter; no inline hooks or additional runtime libraries.
Validated in play: testers finished Acts I to V with it (2026-11-01). Synthetic native-object and
service tests validate memory guards, classification, speech policy and lifecycle.

## Evidence

The existing plugin log recorded these live RVAs on 2026-09-15 at 07:08:39:
PanelManager global `0x3440170`, FindPanel `0x89F760`, FindChildWidget `0x855C90`,
TextBoxWidget::SetText `0x86DD50`. They match the existing analyzed Ghidra program
`re/ghidra/D2R.gpr`, `D2R_live.exe`. The reusable `re/scripts/D2RDecomp.java` script
was used to inspect the functions below. RE files/game assets are gitignored;
this document preserves the conclusions needed to maintain the adapter.

| Field or operation | Verified live code | Finding |
|---|---|---|
| Widget child vector | `0x855C90`, especially `0x855D25` | Pointer array at widget `+0x58`, **element count** at `+0x60`, 8-byte entries; recursive search compares child name at `+0x8` |
| Parent | `0x8562A0`, `0x8562F0` | Parent widget at `+0x30`; stock position calculation walks this chain |
| Visibility | `0x886840` | Selector visibility compares byte `+0x51` and changes it through vtable `+0x50`; hidden ancestors must also hide their children |
| Availability | `0x886420`, `0x8864C5` | Stock list navigation excludes rows whose byte `+0x50` is zero; read as unavailable, without hiding their text |
| Runtime type | `0x89F760`, `0x867400`, `0x8856D0` | Widget vtable `+0x58` returns a native type descriptor; game checks ancestry via descriptor `+0x10` |
| Type descriptor | `0x851030`, instructions `0x851068–0x851070` | `+0x8` holds the NUL-terminated class name, `+0x10` the base descriptor |
| Focus manager | `0x846020` | Reads PanelManager global, then manager `+0xD0` |
| Focused panel/control | `0x868230`, `0x867400`, `0x876CA0` | Focus manager `+0x188` is the active panel, `+0x190` the actual focused widget. `0x876CEE` compares the latter against the control whose selection changed |
| Button label | `0x8496CA–0x8496D6` | Stock modal code calls TextBoxWidget::SetText on button `+0x818`; the embedded textbox's text at `+0x88` gives **button text at `+0x8A0`** |
| Textbox contents | `0x86DD50` | Copies supplied string into widget `+0x88`, then refreshes its layout |
| Linked text | `0x87D950`, `0x87DAB0`, `0x87DC10`, `0x87EFE0` | TextBoxWithLinksWidget derives from **FocusableWidget**, not TextBoxWidget. Update/render delegate to the embedded textbox at `+0x570`; its string is at `+0x5F8` |
| Notification list source | `0x84CC70`, `0x8BA3A0` | ChatListWidget uses source enum at `+0x300`: 0 = ChatHistory, 1 = ChatNotificationMessages, 2 = PartyNotificationMessages. Only source 1 uses the HUD notice reader |
| Notification delivery identity | `0x84DC80`, `0x84D0B0`, `0x850060` | The stock new-entry handler asks list vtable `+0xC8` for the count, then `0x84D0B0(list, count - 1)` for the latest message. Source 1 uses a 19-slot ring (18 usable), entry stride `0x2E8`. Its first engine string is the message body; appending advances the entry address even for identical text |
| Managed panels | `0x89F760` | Additional panel pointer/count vectors at manager `+0x88/+0x90` and `+0xA0/+0xA8`, alongside root children |

`native_widgets.cpp` enables extended traversal only when the resolved FindChild
and SetText RVAs match and FindChild's child-vector instructions match at `+0x95`:
`48 8B 5F 58 48 8B 47 60 48 8D 2C C3`. These are build checks, not signatures for
searching another version. A changed layout disables the new reader and logs why.

## Shared reading pipeline

- `native_memory.*`: guarded copies and bounded engine strings (up to 64 KiB).
  Page protection checks plus SEH contain invalid reads, including freed strings.
- `native_ui.*`: traverse visible child objects, identify widget classes through
  their native type ancestry, and snapshot current text. Cache type metadata by
  vtable, never widget objects across updates. Bounds: 512 nodes and 32 levels.
- `ui_text.*`: engine-independent menu/text archetypes. Menu entry reads heading
  and focused option together; moving focus reads only the option. Narrative and
  tutorial bodies are read in full. Recycled controls are diffed by text, not just
  by pointer; a stable 80 ms snapshot avoids partially populated reads.
- `game_text_reader.*`: UI-thread polling at 20 Hz after LocalPlayerReady. Sessions,
  panel visibility and focus decide which text owns speech. Covering menus suppress
  underlying NPC text; area banners do not interrupt a conversation. The existing
  front-end/value reader continues to handle its established controls.
- `ui_notifications.*`: notification rows are read independently of menu focus.
  New text settles for 80 ms and queues without interrupting; unchanged visible
  messages and recycled rows remain silent. A new native delivery rearms only its
  matching visible text, including identical repeated warnings. The latest ring
  identity is compared with its predecessor, so slot reuse after intervening
  deliveries is supported. Closed/disabled sessions clear the reader. Only source 1 is read;
  chat history and unrelated HUD lists are excluded.

Class mappings: NpcDialogPanel → menu; ScrollSpeechPanel/QuestLogSpeechPanel →
narrative; TutorialTooltip/DismissibleTooltipPanel → tutorial; LevelNamePanel →
area banner; NotificationPanel → temporary notice; HUDMessagePanel → notification
feed. TextBoxWidget, TextBoxWithLinksWidget, ButtonWidget, ListItemWidget/HoverListItemWidget,
ListWidget, FocusableWidget and ScrollingSpeechWidget are reusable archetypes;
subclasses inherit their readers. An unknown focused panel can expose scrolling
speech through its children. Uncatalogued buttons and runtime list rows also have
a native focus fallback.

The NPC layout has a `Templates` container; runtime clones may still be named
`ItemTemplate`, so filtering every name ending in "Template" would hide real
options. Only the container and genuinely invisible branches are omitted.

## Text sources and remaining verification

Sources are the installed game's extracted layout JSON:

- `NpcDialogPanelHD.json`: title TextTemplate, ListTemplate and row TextBox, ButtonTemplate.
- `ScrollSpeechPanelHD.json`: DialogBorder/ScrollSpeechWidget/ScrollingText.
- `QuestLogSpeechPanelHD.json` and Controller override: ScrollingSpeechParent/ScrollSpeechWidget/ScrollingText.
- `TutorialTooltipHD.json`: Title and TutorialTextDescription, Confirm and Dismiss.
- `DismissibleTooltipPanelHD.json`: TooltipText.
- `HUDLevelNamesHD.json`: NameText and DesecratedText.
- `HUDMessagePanelHD.json`: ChatListWidget with ChatNotificationMessages, cloned
  ListItemWidget rows and TextBoxWithLinksWidget/MessageText. Controller mode uses
  this same HD layout.
- `NotificationPanelHD.json`: NotificationBorder/Prompt plus a Confirm control;
  the informational prompt is readable even when the panel has no input focus.

### Graphics-toggle warning (2026-09-15)

The user reported that **G** displays an unread restriction. Localization key
`NoSDSwap`, id 27990 in `local/lng/strings/ui.json`, is the exact Spanish text
"Solo disponible en partidas de clásico y de expansión". Live `.rdata` has the
key at RVA `0x1D70418`; `0x849870` references it at `0x8498CA`, resolves the localized
text through `0x5F4990`/`0x5F4A50`, and submits it to the chat/notification manager
returned by `0x85E5C0` with message kind 4. The prior session logs repeatedly report
`ChatListWidgetMessage:ChatEntryPushed` with an empty SDK `text` field. Neither
list rows nor TextBoxWithLinksWidget were covered by the first text reader.

The current live `.rdata` differs from the synthetic `re/D2R.rebuilt.exe` data
section. Function bodies in the analyzed live Ghidra program still match; use
live `.rdata` for name/xref lookup. Old disk-data addresses are not runtime offsets.
This investigation briefly opened and closed the game at the front end; it did
not exercise the new warning reader in a character session.

The user subsequently confirmed the first warning at 10:04:54 but reported that
further G presses stayed silent. The log contains new `ChatEntryPushed` events at
10:04:56 and later, while content deduplication suppressed speech until the old
warning disappeared. `native_widgets.cpp` now reads the latest native delivery
using the stock count/entry accessors, under SEH. It requires the existing build
checks, count-vtable target `0x84CC70`, and entry prologue at `0x84D0B0`:
`48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57`. Counts outside 1–18 fail closed.
Native strings still use guarded copies, and only bodies matching currently
visible text can rearm speech. If metadata is unavailable or a custom formatted
body does not match, ordinary visible-text change detection remains available.
No key-specific handler or hardcoded Spanish warning is used. Very fast bursts
within the 80 ms settling window coalesce into the latest delivery.

The existing log at 2026-09-14 16:35:38–16:36:52 confirms NPC and scrolling-speech
panels opening separately and the NPC list being rebuilt after ListItemClicked.
The new build must still be exercised against live widgets; synthetic fixtures
cannot prove that all live types, visibility changes or controller hints match.
`access-text dump` logs visible nodes, types, enabled states and text on demand.
Normal `[text]` logs contain announcements/lifecycle only (SDK log lines cap at
1024 bytes; the actual speech string is not truncated to that log limit).

Inventories, item tooltips, general HUD/combat, full chat history and button-legend glyph
translation remain separate phases. No radar code is included.

## Focus on a panel that draws nothing (2026-09-25)

`NativeUi::CurrentFocus` returns no focus when the focus manager names a panel
with no focused widget and no visible child. `EmoteWheelPanel` is opened at the
title screen and never closed; closing the wheel only hides its `Container`, and
the focus manager kept naming the panel. Readers that speak the world only
without a focused panel (world-target tooltip, monster plate) stayed silent
until another panel took focus (the user: open and close the inventory). The rule
is per panel shape, not per name, so any overlay that hides its content instead of
closing recovers the same way. `native-ui` suite; live check pending.

## HUD notice rows without text (2026-10-24)

A tester's "black square" (screenshot of 2026-10-23, Canyon of the Magi, 1920x1080): a dark,
empty rectangle at 20,515–570,840. It is `HUDMessagePanel/GlobalChatListContainer`
(`HUDMessagePanelHD.json`: rect x 40, bottom 910+770 = 1680 in the 3840x2160 layout, so 20 and 840
at half scale; its `MessageBackground` is black at 0.7 alpha) holding 13 rows (13 x 50 = 650 = 325
px) with no visible text.

The logs of that build push one `ChatListWidgetMessage:ChatEntryPushed` after nearly every
`LoadScreenPanel` unload (45 pushes, 43 loads, 5 readable notices in one hour; the same ratio in
the user's log of 2026-10-20). Those entries give the reader no text, so no `[notice]` follows
them. The mod never writes to this list: it calls only the stock count (`0x84CC70`) and entry
(`0x84D0B0`) accessors, the ones the game's own new-entry handler calls, and sends no UI messages
or chat commands. So the empty entries come from the game or the loader; which, and why they
stay on screen, is open.

`NativeWidgets::ReadLatestNotification` now logs an entry it cannot read, once per entry:
`[notice] row without text: entry <address>, string size N, text [first 32 bytes], entry [first
0x40 bytes]`. A string size of 0 means the game pushed an empty message; anything else means the
text lives elsewhere in the entry. The tester is also asked whether the square appears with the
plugin removed.

## A closed dropdown's option (2026-10-31)

The user: moving over character creation's game-version dropdown without opening it named the
dropdown, not the version chosen. The catalog reader took a dropdown's value from
`ControlState`, which knows only what `Settings.json` holds (the options panel) and what was
picked in the popup, so `CharacterCreatePanel/GameVersionDropdown` had no value until a pick.
`NativeUi::DropdownText` now reads the option the widget draws on itself (its button caption,
`+0x818`, the row above), the same text the native fallback already read, and the reader says it
before falling back to `ControlState`. Confirmed live by the user, 2026-10-31.

## A cataloged checkbox's own state; disabled controls (2026-11-01)

The user, in the main menu's Videos list (`CinematicsModal`): "Ver vídeos clásicos" said no state,
and videos not unlocked yet did not say so. A cataloged panel took a checkbox's state from
`Settings.json` only (`ControlState`), which has no key for `LegacyCheckbox`, and marked a control
unavailable only by `DisabledWhenRules`. Now, in `focus_tracker.cpp` `Announce`:
- A `ToggleButtonWidget` with no tracked state reads the widget (`NativeUi::Toggled`, the getter
  at +0xB88 the native fallback already used) and says checked or unchecked; while it stays focused
  a change is said alone (`[focus] <id> -> "<state>"`).
- Any natively resolved control whose own enabled flag (+0x50, `NativeUi::Enabled`) is off is said
  "unavailable". **Unverified live:** that a locked video is a disabled button rather than an
  enabled one drawn with a lock.

**Every cataloged value control, widget first (2026-11-01).** The user: depending on a JSON file
for what a control shows is odd. `NativeState` in `focus_tracker.cpp` now reads the widget for
checkboxes (`NativeUi::Toggled`), option carousels (`NativeValueControls::Shown`, the text the
game draws) and dropdowns (`NativeUi::DropdownText`), on arrival and one update after a change
message (`[value] <id> -> "<state>" (widget, ...)`). Each reader checks the widget's class and its
build check, so anything unreadable falls back to `ControlState` (`Settings.json` plus the last
left/right), logged `(settings, ...)`. A dropdown's change keeps the popup's row, which is exact;
sliders keep their handle's position. `ControlState` is still updated, since `DisabledWhenRules`
read it. **Unverified live** in the Options panels.

**Locked videos, a dropdown's rows, the values' names (2026-11-01, the user's test).**
- A locked video is an *enabled* widget: the +0x50 flag stayed 1. A one-off comparison of the
  Videos list's two unlocked and five locked `ButtonWidget`s found `ButtonWidget` +0x67A, set to 1
  by RVA 0x86EEC0 (which also swaps in the disabled frame), cleared by 0x86EFB0, and tested by the
  click handler. `NativeUi::ButtonDisabled` reads it for the `ButtonWidget` class only, once
  `NativeButtons` has fingerprinted both functions (`[buttons] Verified.`); the catalog path then
  says "unavailable". **Unverified live** after the change.
- DLSS's popup said a row twice: one press brought two `ScrollController:ManualScrollInput`
  messages though nothing scrolled, so the counted index (row + scroll) ran ahead. A popup row is
  now the focused `ListItemN`'s own `TextBox` (`PopupFocusedRow`), said once per row and text,
  `[focus] popup row -> "<text>" (counted option N)`; the count stays as the fallback.
- The text language said "Francés" on arrival with Spanish on screen, and value messages with no
  input read whatever was focused. `NativeState` now reads a widget only when its native name is
  the catalog entry's, and the arrival log says `state=widget` or `state=settings`.

## Help, Chronicle, loot filter and credits (2026-11-02)

The user's log of 2026-11-02 15:43–15:45: the help screen said only its title, the Chronicle's lists
said their internal names (`ItemScrollViewContainer`, `FilterRules`, `RewardsButton, casilla`), and
several loot filter buttons borrowed a neighbour's caption (`ImportProfile` said "Perfil:"). The
credits were reported silent too (no log of them open).

- **Labels.** `tools/gen_panel_widgets.py` gave an unlabelled control the first caption among its
  siblings. Now, among several captions, it takes the one sharing the longest name prefix
  (`EliteQualityCheck` → `EliteQualityText`, `HideBtn` → `HideText`), and a `ButtonWidget` with a
  tooltip borrows nothing: the tooltip names it (every modal's X said the modal's caption; the
  stash's gold buttons said "Cofre"; `TextToSpeechToggle` said "Daltonismo"). Icon buttons with
  neither got `WIDGET_LABEL_OVERRIDES` from the game's strings: `LootFilterMoreOptions`,
  `NewProfileTitle`, `ImportProfileTitle`, `LootFilterMoveRuleUp/Down`, `UIBack`,
  `ChronicleSearchPlaceholder`, `GameListFilters`, `ViewRewardsButton`, the rewards entries'
  `Unique`/`Sets`/`Runewords`.
- **Help.** `HelpPanel` opens `HelpPanelBackground` and `MiniMenuPanel` over itself in the same
  burst, so the modal reader took the empty `MiniMenuPanel` as the active panel. It now reads the
  topmost panel opened in that burst under panels that are no menu (`PanelState::NotMenu`, the list
  moved from `keyboard_menu.cpp`). An overlay alone over an older menu still counts as active, as
  before: the front-end panels stay in the record all game, so looking further down would read the
  main menu's news in play. Help lines name keys as controller symbols; the reader decodes them
  (`ControllerText::Read`).
- **Chronicle lists (corrected after the user's test, 16:25).** Each list is a
  `FocusableListContainerWidget` the focus stops on whole first: its focus frame is the whole list
  (`ItemScrollViewContainer` 771x800, `TabScrollViewContainer` 300x605), no row is highlighted, and
  only then does the focus move to the `ChronicleItemListItem` / `ChronicleCategoryListItem` clones
  (`[field] focus name="ListItem"`), which the text reader reads. A first build looked for a
  framed row inside the list and found none; that code is gone. The lists are now named "Objetos,
  lista" (`AdvancedItems`) and "Categorías, lista" (a mod string: no game string says
  "Categories"). A row drawing `ItemSlotBackground_Locked` adds "undiscovered". Each new shape of row
  is dumped once a session (`[text-tree]`, with `toggled=`), to see what the rows hold.
- **Row checkboxes.** A row's one bare toggle (no words on or under it) gives its state: a loot
  filter rule's `EnableRuleToggle`. A Chronicle category has two toggles, one carrying its caption,
  and said "Armaduras. no marcado" in the first build; it now says its name only.
- **Icon buttons in play.** In a game, buttons belong to the interaction reader
  (`InteractionReader::HandlesFocus`), which spoke only the words a button draws, and it holds the
  speech, so the catalog reader never ran: the loot filter's `ProfileOptions`, `AddProfile` and
  `ImportProfile` were silent. An empty button now takes `FocusTracker::CatalogLabel` (its text key
  or label key, never the tooltip, which the reader already says). Borrowed captions that would have
  surfaced this way were overridden: the party panel's X, the trade's `ready_them`, unselectable
  waypoints.
- **Skill tree.** The modal reader read its counters and hotkeys on opening ("1, X, 1, B, 0 skill
  choices left"), ahead of the interaction reader's own branch and choices; it now leaves
  `SkillsTreePanel` to that reader, as it does the character panel.
- **Game classes in the catalog.** A cataloged control whose type is the game's own class (not
  `…Widget`, e.g. `ChronicleRewardsPanelEntry`) adds the words it draws after its name: a reward
  entry's completion.
- **Credits.** `CreditsModalHD.json` has one `CreditsWidget` reading `CreditsROTW.txt`.
  `CreditsReader` finds it by class and queues each line whose text box appears with text. In the
  user's test no box had text in 7 s: the widget seems to draw its lines itself. Silent 3 s after
  opening, it now logs once every widget under it (`[credits] widgets ...`) and the strings in and
  one pointer away from it (`NativeWidgets::LogStringsDeep`, `[native] deep probe credits`), to find
  where the lines live. The window is named by `strGameCredits`, not "Credits Modal".
  The probe (16:48): the `CreditsWidget` has no children at all, and its own object holds
  `"CreditsTextHolder"` at +0x200, a widget's name offset being +0x8: a widget embedded at +0x1F8.
  The reader now finds that holder by its name inside the object (only where a game vtable sits) and
  reads the holder's text boxes. **Unverified live.** Still silent, the log says
  `[credits] holder found/not found (...)` and lists the holder's widgets.
- **Back from an icon button (16:48).** Returning from the loot filter's icon buttons to the dropdown
  was silent: the focus reader still had the dropdown as its last words. When the interaction reader
  lets go of the speech, the focus reader now forgets them (`FocusTracker::Reset`).
- **Chronicle entries (16:48 dump).** An entry is `ListItem` / `ItemSlotBackground_Locked` /
  `ItemSlotText`: a unique not found yet shows only its base type ("Grebas de mirmidón") on the
  locked background, which the row reader now says as "sin descubrir".
- **Third test (16:56).** The holder at +0x1F8 was found, shown, with **no children** either; the
  deep probe found one text box named `CreditsTextEntry` holding "DIABLO II: RESURRECTED - REIGN OF
  THE WARLOCK" at its +0x88 (a text box's text), a widget of no tree. The reader now looks in the
  members of the credits and of the holder for a vector whose first element is such a box (begin and
  end pointers, the box checked for a game vtable) and reads its shown boxes; still silent, it logs
  `[credits] silent: holder ..., lines vector ..., N boxes, M shown` and probes the holder too.
  **Unverified live.**
- **Expanders (16:56).** A Chronicle set row is `ListItem` / `TabSlotBackground` / `ItemSlotText`
  plus `TabExpandButton` (`toggled=0`): the one bare toggle, said "no marcado". A bare toggle named
  `…Expand…` now says "plegado"/"desplegado" (new strings `expanded`/`collapsed`), so a parent
  category says its fold too; any other bare toggle stays checked/unchecked.
- **A tooltip with a placeholder (16:56).** Character creation's `GameVersionInfoButton` (the info
  icon by the game-version dropdown, no click message) said `Crea una versión "%s" de tu personaje.`:
  the catalog holds the raw `CharacterGameVersionTooltip`. A tooltip with `%s` is now the widget's own
  copy when the game formatted it (a button's +0x88), else the placeholder filled with the dropdown
  beside it (`NativeUi::DropdownText`). Heard right live (17:08): "Reign of the Warlock", "Expansión".
- **Categories select (17:08).** Pressing a Chronicle category sends `ChronicleListPanelMessage:
  FilterByCategory`: it selects the category (its `CategoryButton` toggles), it does not unfold it.
  Every category said "plegado" and never changed. Beside a face toggle only what is on is said now:
  "seleccionado" (new string `selected`) while it filters, "desplegado" if ever unfolded. A set row,
  with no face, keeps "plegado/desplegado".
- **Credits, fourth try (17:08).** No vector of boxes in the credits or the holder; the holder probe
  showed the one box at holder+0x980, i.e. credits+0xB78 again, a member of the `CreditsWidget`'s own
  object in both runs. The reader now scans that object (0x4000 bytes) for embedded boxes named
  `CreditsTextEntry` with a game vtable, rescans each second, and reads the shown ones top first
  (all with words when none is flagged shown). Heard working by the user (17:20).
- **Rows with the same words (17:20).** In Accessories every undiscovered unique shows its base type
  ("Anillo", ten times): the moves reached the reader (`[field] focus name="ListItem"` each time)
  but only the first of a run was said. A row read alone through the text reader's fallback is
  always at index 0 of its own snapshot, and that index was its focus key, so the same words under
  the "same" key were no change. That fallback now keys a row by its widget.
