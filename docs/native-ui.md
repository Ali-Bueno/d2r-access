# Native in-game text adapter

Implementation date: 2026-09-15. Target: D2R 3.3.93787, D2RCore 1.2.1-beta.
This is a read-only adapter; no inline hooks or additional runtime libraries.
Live NVDA/controller validation is still pending. Synthetic native-object and
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
  New text settles for 80 ms and queues without interrupting; identical visible
  messages and recycled rows remain silent. A message can be read again after it
  disappears. Closed/disabled sessions clear the reader. Only source 1 is read;
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

The existing log at 2026-09-14 16:35:38–16:36:52 confirms NPC and scrolling-speech
panels opening separately and the NPC list being rebuilt after ListItemClicked.
The new build must still be exercised against live widgets; synthetic fixtures
cannot prove that all live types, visibility changes or controller hints match.
`access-text dump` logs visible nodes, types, enabled states and text on demand.
Normal `[text]` logs contain announcements/lifecycle only (SDK log lines cap at
1024 bytes; the actual speech string is not truncated to that log limit).

Inventories, item tooltips, general HUD/combat, full chat history and button-legend glyph
translation remain separate phases. No radar code is included.
