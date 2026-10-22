# Town portals (2026-10-17)

A tester (2026-10-16): a town portal in camp was said as "portal to", where the game shows
"Tower Cellar Level 1 [TankMan]": where it leads and whose it is.

## The level

The object label (0xC17E0) names a SubClass-portal object after the level the getter `0x34AD40`
gives: `ObjectData+8`. The mod read it as 16 bits, taking the next byte for D2RLoader's widening;
but **ObjectData+9 is the town portal's state flags** (getter `0x34ADF0`, set by the town portal
packet `0x1CB1C0`: level byte to +8, then `pkt[1] & 3` ORed into +9). An active town portal read
21 + 256 × state, past the 138 Levels rows, so the name was empty and the fallback took the
objects.txt key, "Portal to " (objects.json 3226). Red portals never get the state, which is why
they worked.

D2RLoader keeps the level's high byte at **ObjectData+0x78** (D2RCore exports
`SetObjectInteraction16` / `HandleSetWarpDestination16`: low byte to +8, high to +0x78). The mod
reads +8, and +0x78 only when that export exists. A portal with no level is "unnamed exit", never
the key's prefix. `destinationLevel` (routing, beacon) is now right as well.

## The owner

The label adds the owner when `dword [exe+0x2A233F4]` is set (inferred: not single player; not
checked by the mod, which says the owner whenever one is found):
1. the roster entry whose portal ids (+0x80 local, +0x84 remote; written by `0x136940`) match the
   portal's unit id (lookup `0x136670`), its name at +0 (the game shows the display name at +0xAA);
2. else the name the ownership packet left at `ObjectData+0x34` (char[0x3D], setter `0x1CB070`).

Spoken as `portalOwned`: "Tower Cellar Level 1, TankMan's portal". Fingerprints:
`EntityLayout::PortalOwnerChecks` (`[scan] Portal owners verified`).

## Remembered portals in multiplayer

The registry keeps a portal out of the loaded rooms at its last place and name
(`MapSurvey::Registry`). A party member's new portal is a new unit the client loads only
when near it, so the old one stayed (tester, 2026-10-21: Raine kept the Maggot Lair portal
after the host opened one from the Lost City). Two rules, both on `Entity::owner`:
1. A newly seen portal replaces any tracked, unloaded portal of the same owner on the level
   (a player has one town portal).
2. While not loaded, a tracked portal is dropped when its owner's roster entry
   (`RosterPortal` +0x80 / `RosterRemotePortal` +0x84, read by `NativeEntities::ReadRosterPortals`)
   is readable, not both zero, and neither id is the portal's unit id. Logged once:
   `[scan] portal dropped: owner=... tracked=... roster=local/remote`.

Not verified: that a client's roster refreshes those ids when a member opens another portal
(only the writer `0x136940` is known). If the log never shows the drop line while the old
portal lingers, the ids are not refreshed and only rule 1 applies.

## Live check

On a town portal, in town and in the field, single player and online: the scanner's name, and
whether the owner comes from the roster or +0x34. In multiplayer: host opens a second portal
while the client stays away; the client's log should show `[scan] portal dropped`. Unread: the label's format string at
`exe+0x1CC2F74` (the `.rdata` of the rebuilt image does not match the live process).
