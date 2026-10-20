# Keyboard tabs and shared-stash pages

Feature register, 2026-10-20: repaired shop tabs are gameplay-confirmed.
Shared-page correction is complete/offline-verified (34 native adapter checks,
28 context checks, full 128/128) and installed; revised paging gameplay pending.
The previous mouse-based Shift shortcut moved five pages;
that user-confirmed failure supersedes its earlier offline-only result.

Page Up/Down moves through enabled menu tabs in display-index order, retaining
distinct tabs with the same caption. It stops at either end as before.
While Shared page controls are shown, Shift+Page Up/Down moves one page,
Ctrl+Page Up/Down five, Ctrl+Shift+Page Up/Down ten, clamped at either end.
On other stash tabs and inventory/shop screens the existing Shift comparison
and Ctrl reading commands remain available. Text
editing and the mod's settings retain their keys. No new settings or saved
binding IDs are introduced; the existing comparison bindings supply the
context-specific page commands. Ctrl+Shift is observed by the existing direct
key watcher because the loader's binding API represents only one modifier.

## Native state and diagnosis

The user's current 2026-10-20 loader log shows repeated tab clicks after Weapons
without a selection change, while all four captions remain readable. This log
does not contain the native per-tab enable bytes; their exact runtime values
were not captured. Static native behavior supplies the missing validity rule:

| Build 3.3.93787 location | Relevant behavior |
| --- | --- |
| VendorPanel `0x2420C0` | Counts stock by category, enables/disables each tab accordingly, then selects an available category |
| TabBarWidget count `+0x558` | Native number of tab slots, bounded to eight by this adapter |
| TabBarWidget selection `+0x16C4` | Current open slot; caption and controller hover are separate state |
| TabBarWidget enable bytes `+0x1788` | Per-slot availability; captions can remain present for disabled slots |
| Native hit test `0x8779C0` and validity `0x878CA0` | Reject disabled slots |
| Native directional helper `0x877D20` | Searches for an enabled slot when a proposed slot is disabled |
| Native focus rectangle `0x877AB0` | Resolves the requested slot's active/inactive image geometry |

The previous keyboard path advanced by one caption unconditionally. An empty,
disabled second Weapons category ignored that click, so the next request
proposed the same disabled slot again and never reached Misc. The repair uses
the native availability bytes, without inferring validity from a caption,
caption uniqueness, item enumeration or previously spoken text.

## Data flow, ownership and bounds

`NativeUi::AdjacentTab` validates the visible TabBarWidget, native count and
all copied availability bytes, then searches at most eight slots. Invalid
counts, flag bytes or reads return unavailable. `KeyboardMenu::StepTab` clicks
the returned slot through `NativeSkillNavigation::RectAt` and `MenuMouse`;
the existing `TabReader` reports the resulting selected index and localized
caption. No native tab state is written directly.

`src/menus/stash_pages.cpp` owns the shared-page context adapter. It resolves
BankExpansionLayout, basicstash_container, SharedStashTabContainer and
StashNavigation afresh on the UI owner. CurrentSharedStashText must be visible,
nonempty and resolved before a page context is available. The retained Arrow
reader describes native mouse geometry but shortcuts now use native commands;
physical Shift/Ctrl must not influence their amount. The game owns page data
and binding updates; the mod never writes a page index or moves items itself.

The current loader's BankPanelMessage SharedStashLeft/Right handler at game
RVA 0x23BCF0 calls a loader replacement at instruction 0x23C2A8. Its current
D2RCore 1.3.1-beta implementation (offline RVA 0x1C9B20, 317 bytes) selects ten
for Ctrl, five for Shift, otherwise one. The native single-step flag bypasses
those physical modifiers but can wrap. src/menus/native_stash_pages.cpp resolves
the actual call/import thunk, verifies the complete game handler and loader
body, then uses the wrapper's own count/season getters and current page field
(+0x168/+0x170) to bound the single-page commands before calling. No loader RVA
is hardcoded in production. Core body fingerprint pins getter calls at +0x2B/
+0x3D and field/ABI semantics. Unfamiliar code/layout refuses shortcuts.

Each request issues at most ten native unit commands, all on the UI owner;
edge clamps prevent wrapping. Each result must advance exactly one page or
the request stops with Unavailable. A synchronous native call owns its normal
grid/data/server update; no UI pointer crosses a frame or worker boundary.
This replaces the old queued mouse page click. Tab cycling keeps its confirmed
mouse route. Native command aggregate cost is bounded by count but unmeasured
in the game; no asynchronous responsiveness/performance claim is made.

`KeyboardMenu::Tick` publishes only a copied context boolean to input callbacks.
Shift comparison and Ctrl reading callbacks post distinct direction/amount
commands while that context is active; direct key watching covers Ctrl+Shift
and loader gaps with the same identities. Modifier amount is captured on the
press, not re-read at execution after release. The UI dispatcher revalidates
the hierarchy before acting, so a request cannot change a stale Shared page after a
tab or window change. No native pointers cross this boundary. Each resolution
has a fixed four-level path plus named page lookup; it does not walk
the inventory or infer page counts from localized text.

`MenuMouse` keeps its existing move/settle/press/release/return sequence. Tab
and page commands accept no additional action while that queue is busy. The
pending action mask coalesces repeated input until the next UI dispatch. Each
of the six direction/amount bits is bounded to ten native calls, so even a
constructed all-combinations drain is at most sixty; ordinary single requests
use at most ten. Actual aggregate native command cost remains unmeasured.
Reset clears the cached context and pending inputs; menu close, foreground
loss, typing and settings preserve their established cancellation/suppression.
`InteractionReader::TickStash` remains the sole page-announcement producer.
Its current-page text uses the game locale, and failures use existing localized
messages. No speech-content deduplication or extra announcements are added.

## Validation and extension

Native UI fixtures exercise enabled duplicate-caption slots, disabled-slot
skipping in both directions, edge behavior, invalid counts/flags and hidden
bars. Stash-page fixtures exercise both arrows, localized current-page text,
tab/window changes, missing/unresolved text, hidden controls, wrong types,
missing geometry and invalid directions. Native page fixtures compile the
actual adapter with only external native getters/command replaced: all 1/5/10
amounts under all physical modifier combinations, both edges, previous season,
invalid state and unexpected multi-page results. Inputs are original mod-owned
memory/tree fixtures, with no copied game implementation. Read-only native
disassembly stays outside every Git tree in re/menu-stash-2026-10-20/.

Extend a new layout by verifying its visible ownership/path and actual input
operation before adding an adapter. Keep navigation decisions in the UI-owned
reader, callbacks limited to copied context/intent, and game actions on the
verified native page route (tabs retain their mouse route). A queue does not establish game frame
cost or audible response time; no comparable runtime baseline has been
measured for this change.

Shop cycling is confirmed; retain it. First Shared checks: Shift+Page Down then
Up must move one page, Ctrl five, Ctrl+Shift ten, stopping at either end. Release
the modifier with the key as normal. On Personal/shop, Shift still compares and
Ctrl still reads. The five-page stash can establish clamping; ten-page movement
requires a stash with enough pages, so a five-page session alone cannot confirm it.
Tests do not confirm Windows input delivery, page-edge clicks or actual audible
ordering in a running game. Local native/log evidence stays outside Git at
`../../../re/set-item-names-2026-10-20/`.
