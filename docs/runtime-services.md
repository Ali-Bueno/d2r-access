# Shared runtime, input, settings and output

Source review: 2026-10-19, integration of `origin/main` 7352a4a with the local
keyboard branch 4c88812. [Feature index](feature-index.md) maps the feature
adapters; [architecture](architecture.md) describes their native interfaces.

## Entry, order and failure boundaries

`plugin.cpp::OnUiTick` is the central dispatch. `UiThreadPump` schedules the
next SDK UI callback; `callback_boundary.h` contains allocation and other C++
failures before they escape its `noexcept` boundary. A fault cancels pending
mod actions without allocating, retains map observations, and backs off for a
second. A later successful callback queues the localized fault notice.
`Laps` samples each service and logs slow/worst updates through `AsyncLog`.

The native UI thread owns widget access, native input and transient game
pointers. The current order is input, keyboard play/menu, controller navigation,
game-text/fallback focus, multiplayer/tabs/text entry, edge cues/autodetail/modal,
native/UI messages, quests, radar/rooms/places/terrain, footstep/movement,
items/catalogue/automap/scan, entity cues/beacon, skill bars/hotkeys,
vitals/boss life, inventory arrivals, weapon swap and trade. An earlier service
can publish an owned action consumed later in that same update. Menu speech
ownership defers ambient readers; it must not be inferred from a device merely
being connected.

`NativeMemory::ReadScope` retains at most 256 permission regions within one
synchronous outer scope. Nested readers share that scope; its end invalidates
all entries. Executable and readable permissions retain their actual flags.
Every copy remains fault guarded, and a fault clears the cache. This branch
keeps its per-pass expiry instead of upstream's one-second permission cache.
`LayoutCheck` verifies complete native function fingerprints; failed checks
disable only the affected capability. `CoreSlots` resolves descriptor slots by
verified game RVA rather than fixed loader offsets. See [native UI](native-ui.md)
and [stability](stability.md) for exact fields, evidence and failure tests.

## Input and context

`KeyboardService` owns play actions, automatic walking and the settings menu;
`KeyboardMenu` owns its copied menu cursor, mouse placement and clicks. The
game's actual input-mode getter determines automatic keyboard menu placement.
The keyboard uses native game actions; there is no synthetic XInput device.
`InputDirection` copies physical controller edges and stick state. `PadFilter`
filters only the explicitly claimed native controller gestures; held/released
state must be handed back cleanly when a context closes. `PadTyping`,
`TextField` and `TextFieldEdit` share the native text-entry path for controller
typing; keyboard text boxes continue using the game's editor.

The complete bindings are [keyboard keys](keyboard-keys.md),
[keyboard menus/play](keyboard-play.md), [controller navigation](controller-navigation.md)
and [controller mode](controller-mode.md). `ControlState` and the native focus
readers provide the current context; `MenuNavigationPolicy` and
`KeyboardMenuPolicy` decide ownership. A new binding belongs in the relevant
central input dispatch and its shared settings/help registration, never in a
second polling loop. Preserve physical controllers and modifier-click behavior.

`SettingsList::Menu` operates over registered entries and groups. A row supplies
its current value, setter, wording and optional preview. Group navigation
retains the parent position; stepping reports boundary/wrap cues separately
from speech. `SettingsStore` encodes nonnegative integer values by stable key,
ignores malformed values and writes through a sibling temporary file followed
by replacement. Existing saved values take precedence over compiled defaults.
`SettingsFile` separately reads the game's `Settings.json`, including its
`Language` alias; it is not a second mod-settings producer. `RadarConfig` reads
feature configuration from `radar.toml`. Keep newly added defaults in sync with
the service reset path, config and settings registration.

## Localization and presentation

`Localization::Text` uses the SDK two-call UTF-8 copy contract and prefixes bare
game keys with `d2r:`. Returned text is owned; an absent service/key returns
empty. `DetectSpeechLanguage` waits for the game's `close` label, identifies
its distinct translation, then chooses one of thirteen `Strings::Table`s;
unknown probes fall back to English. `ForgetSpeechLanguage` allows detection
after a language change. `speech_strings.cpp` statically requires every table
field to be populated, and `speech-strings` checks placeholders and locale
mapping. The supported catalogs are enUS, deDE, esES, esMX, frFR, itIT, jaJP,
koKR, plPL, ptBR, ruRU, zhCN and zhTW.

Game text remains game-localized; mod wording belongs in every catalog.
`UiText::Clean`, `ControllerText` and `ClickWords` handle markup and effective
input hints. `UiText::Reader` retains candidate/spoken readouts and context/focus
identity to settle dependent panes. These are existing event/focus guards;
this integration adds no speech-content deduplicator. Tooltip, detail, repeat
and line-reading consumers use the same copied description. `AutoDetail`
coordinates delayed detail with foreground speech; see [native tooltips](native-tooltips.md),
[descriptions](descriptions.md), [progression](progression.md) and
[waypoints](waypoints.md).

There is currently no persistent independent mod-language override in the
settings registry: language follows the game probe. This is an existing gap
against the portable template's Auto/manual language requirement, not a claim
of implemented manual selection. The loader's separate text/audio locale
configuration and its historical Steam-language issue are documented in STATUS.

## Output ownership and shutdown

`ScreenReader::Speak` queues owned text to the sole PRISM thread; that worker
owns backend initialization, selection, speech and shutdown. `AsyncLog`
copies bounded diagnostic messages to its worker; producer enqueue is best
effort and nonthrowing. The SDK logger can flush disk, so native callbacks must
not deliver slow logging synchronously. `ModSounds` is the shared sound master;
individual audio services retain their category/volume gates.

All static worker threads, including upstream low-life and wall-tone audio,
use `ProcessLifetime`. Normal service unload still stops/joins workers and
flushes persistence. Forced process exit does not destroy joinable threads or
join killed workers under loader lock. The terrain archive store follows that
same ownership rule. Audio workers own their backend/device state and consume
copied commands, never game pointers. The established Windows output is PRISM
and XAudio2/wave output; this merge does not establish HRTF coverage or another
platform port. See [audio/language audit](audio-language-audit.md),
[ground items](ground-items.md), [entity cues](entity-cues.md),
[radar readings](radar-readings.md) and [stability](stability.md).

## Extension and verification limits

Add feature behavior in its service/policy, register it once in the central
dispatch, supply lifecycle reset and settings/help/catalog entries, and extend
the relevant production-reader or pure-policy fixtures. Native layouts need
matching function evidence and owned bounded snapshots. Do not introduce a
game-thread wait, worker use of native pointers, or unbounded retained state.

Focused checks cover `callback-boundary`, `ui-thread-pump`, `process-lifetime`,
`native-memory`, `async-log`, `speech-strings`, `settings-list`, `settings-store`,
`menu-cursor`, `menu-navigation`, `native-ui`, `ui-text`, `game-text-reader`,
`text-field-edit` and `pad-filter`; the full CTest run validates integration.
Current results belong in STATUS and the merge record. Fixture success does not
measure game frame cost, load time, speech-device latency or actual audible
response. Whole-pipeline runtime baselines and gameplay checks remain user-led.

The central `keyboard_service.cpp`, `keyboard_menu.cpp`, `interaction_reader.cpp`,
`focus_tracker.cpp` and `native_entities.cpp` exceed the general 1,000-line budget. They retain shared
context/gesture and dependent-tooltip state whose ordering is exercised by the
existing tests. This merge keeps those central owners intact to avoid combining
a behavioral merge with a new state-ownership refactor. In particular, negative
NPC-presence evidence is produced beside the same raw unit traversal whose
completeness it validates, using its private table and lifetime checks; moving
that observation independently would lose this guarantee. The larger files remain
explicit organization debt. Focused policy modules already separate routing,
menu selection, formatting and native readers. No new parallel subsystem or
unused wrapper is introduced to satisfy a line count.
