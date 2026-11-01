# Shared stash pages

The shared stash's pages are not a tab bar: the game draws the current page as text between two arrows
(`StashNavigation`, `decisions.md`), so PageUp/PageDown (the tab before/after) never reach them. D2RLoader can
add up to 995 extra pages (`[d2rcore.stash] add_shared_tabs`, `d2rloader.toml`), which the controller walks with
RB/LB and a keyboard player could only reach with the mouse.

| Key | Action |
| --- | --- |
| Alt+PageUp / Alt+PageDown | previous / next page |
| Ctrl+Shift+PageUp / Ctrl+Shift+PageDown | ten pages back / forward |

Both clamp at the first and last page. At an edge the page is said again; off the Shared tab, or when the native
command is unverified, the mod says "unavailable". A move that lands is announced by the stash reader as for RB/LB.

Alt is a loader binding; Ctrl+Shift is watched directly because a loader binding holds one modifier, and the
single-modifier Ctrl/Shift reading keys are swallowed while both are held in a menu. Ctrl+Alt (AltGr) and
Shift+Alt (layout switch) are avoided.

## Native command

`src/native_stash_pages.cpp` resolves the loader's page replacement from the call at game RVA `0x23C2A8`
(inside `BankPanelMessage`, `0x23BCF0`), checks the game handler and the 317-byte loader body against
fingerprints (`LayoutCheck`), then calls it with its single-step flag, so held modifiers do not matter. It reads the
panel's count/season getters and current page (`+0x168` per season) to clamp, and verifies each step moved by
exactly one. Any mismatch leaves the keys unavailable. The reverse engineering is by bladestorm360
(commit 97cb7c4, game 3.3.93787, D2RCore 1.3.1-beta); it is not yet re-measured in this build.
