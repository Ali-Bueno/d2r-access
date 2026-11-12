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

Spoken as `portalOwned`: "TankMan's portal" (until 2026-10-22 "Tower Cellar Level 1, TankMan's
portal"; see below). Fingerprints: `EntityLayout::PortalOwnerChecks` (`[scan] Portal owners verified`).

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

## One portal per owner, named by its owner (2026-10-22)

The testers' two-player log (Raine's and TankMan's `portal_*` files in the user's Downloads,
`d2r access test build 2026-10-22 notes.7z`): the host opened a portal from the Dry Hills, then a
new one from the Far Oasis while the client was far off in town. For about 100 s the client still
listed "Dry Hills, TankMan's portal" (the old unit, id 50, not loaded, so rule 1 waited for the new
unit to load) and its beacon kept a frozen copy of it until 13:51:49; no `[scan] portal dropped`
line, so the client's roster ids were not refreshed or were zero (rule 2 never fired).

At the tester's request the scanner now names an owned portal by its owner alone, "TankMan's
portal": where it leads is read off the portal itself on reaching it (the game's own label, through
the target reader). A portal with no owner (Andariel's, a Portal shrine's, the red ones) keeps its
destination as its name. `EntityRadar::Recreated` takes another portal of the same owner for the
same one, so the registry's entry and the beacon's lock move to the new unit as soon as the client
loads it. Not changed: whether the client can know of the new portal before loading it.

## A portal stays where it stands; Tyrael's portal is a portal (2026-10-27)

**Harrogath** (a tester's `portal confusion` log): with a town portal open, the scanner and the
route both put it at the town's west edge, 23 steps northwest of the waypoint, the Bloody Foothills
exit's place. A town portal carries the level it leads to (`ObjPortalLevel`, 110), and an exit with a
destination was moved to the area border crossing into that level (`ZoneTransitions::Crosses`,
`MoveToCrossing`, used by the scanner and `ToCrossing`). A portal is now never moved: `Crosses` is
false for the Portal category. Test: `zone_transitions_tests.cpp`.

**Tyrael's portal to Act V** was listed with the exits: `NotKiddingLastPortal` (objects.txt 566) has
no portal SubClass and is named after a level ("Harrogath"). It is drawn with the red portal's own
art (Token `pp`, `PortalPermanent`'s), and so are `ReallyTheLastPortal`, `GuildPortal`,
`PermanentPortal2` and `CainPortal`: `tools/gen_lootables.py` lists every object sharing a portal
row's Token (`Generated::PortalArtObject`), and those are portals. It keeps leading to Harrogath.

## The portal after Baal is an exit (2026-11-11, 1.0.16)

A tester: the portal that opens in the Worldstone Chamber once Baal dies had no portal sound.
`ReallyTheLastPortal` (objects.txt 565, OperateFn 72) has no portal SubClass and its name key,
`strlastcinematic` ("Destruction's End"), is no level's, so it was a `Portal` by category but an
`Object` by kind, and only exits get the loop (`GroundItemService` landmarks). An object drawn with
a portal's art (`Generated::PortalArtObject`) is now an exit: it has the portal loop and is listed
as a portal under the game's own label. It leads to no level, so it has no destination and is
not remembered between games. Test: `native_entities_tests.cpp`, `PortalAfterBaal`. Not heard live.
