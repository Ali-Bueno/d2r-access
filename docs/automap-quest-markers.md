# Quest landmarks on the D2R automap

Research, 2026-09-24. Local game: D2R 3.3.93787. No navigation behavior changed.
See [discovery research](automap-discovery-research.md) for the separate question
of reading which terrain and objects the native automap has revealed.

## Finding

The stock D2R automap depicts some quest landmarks, including the Cairn Stones,
Tree of Inifuss and Horadric Malus. This supports exposing such landmarks as
navigation destinations. It does not establish a universal current-quest arrow,
automatic objective selection, or a route across undiscovered terrain.

The Cairn Stones are relevant to the user's current Cain quest in Stony Field.
Their map glyphs locate the stone formation; they do not establish the activation
sequence. Preserve the separate scroll/symbol interaction and player choice.

## Evidence and limits

| Evidence | Supported conclusion | Limit |
| --- | --- | --- |
| Local `game-data/global/excel/objects.txt`, `AutoMap` column | Specific objects have map drawing cell IDs | Not a visibility, discovery, quest-stage or navigation rule |
| [D2R automap guide](https://www.wowhead.com/diablo-2/guide/auto-map-guide-overlay), [original quest-feature image](https://wow.zamimg.com/uploads/screenshots/normal/1035187.jpg) | Visually inspected Cairn formation on the D2R map | Single frame, no first-reveal timing |
| [Inifuss screenshot](https://editors.dexerto.com/wp-content/uploads/2021/09/22/diablo-2-resurrected-tree-of-inifuss.jpeg) | Visually inspected small tree glyph | Large red circle/arrow are editorial annotations |
| [Malus screenshot](https://editors.dexerto.com/wp-content/uploads/2021/09/22/diablo-2-horadric-malus-location.jpeg) | Visually inspected hammer glyph at the forge | Red circle/line are editorial annotations, not native routing |
| [D2R patch 2.3](https://us.forums.blizzard.com/en/d2r/t/diablo-ii-resurrected-122-pc-patch-notes-23-build-67314/74170) | Blizzard fixed explored areas disappearing after portals and exit-loading problems | Does not specify quest-marker discovery distance |
| [Public D2R table](https://github.com/pinkufairy/D2R-Excel/blob/main/objects.txt), [D2R data guide](https://locbones.github.io/D2R_DataGuide/#objectstxt) | Corroborate object cell semantics | Public table version can differ from the installed build |

Other visuals were not conclusive: a Tal Rasha screenshot showed the journal's
true-tomb symbol and a doorway carving, not an automap objective; inspected
storyboard frames around the tree encounter in [this gameplay](https://www.youtube.com/watch?v=T-2n47u5Y1Y)
had the automap off. Neither proves a native map guidance feature.

[d2r-mapview](https://github.com/joffreybesos/d2r-mapview) explicitly adds exit,
waypoint, boss and quest-item guidance. Its route lines and quest markers cannot
be used as evidence of unmodified D2R behavior.

## Local table catalogue

These are drawing cell IDs from the local extracted table, not runtime visibility
checks. The verified in-memory field is `ObjectsTxt+0x164`; provenance is in
[entity unit research](entity-units-re.md). Object names below identify table rows.

| Act | Objects | AutoMap cell |
| --- | --- | --- |
| I | InifussTree | 313 |
| I | StoneAlpha through StoneTheta (six stone classes) | 314 |
| I | Malus | 316 |
| I | TowerTome | 427 |
| II | Horadric cube, scroll and staff quest chests | 318 |
| II | ArcaneTome | 427 |
| III | GidBInnDecoy | 315 |
| III | LamEsensTome | 427 |
| III | Khalim heart, brain and eye chests | 318 |
| III | CompellingOrb | 305 |
| IV | Five Diablo seals | 306 |
| V | AncientsAltar / Colossal altar | 1467 |
| V | Frozen Anya (`AnyaYetAgain`) | 1468 |

Examples with zero include Cain's gibbet, the Forgotten Tower chest, the Tainted
Sun shrine, Tal Rasha's receptacle and the Hellforge. Zero rules out this table's
object glyph; it does not rule out a recognizable terrain outline, a special
renderer path, or another object at the same site. Some waypoints also have zero.
Do not use this table alone as an exhaustive list of visible destinations.

## Local session and saved map evidence

Before launching, the existing saves and settings were copied outside the repo
to `%LOCALAPPDATA%/D2RAccess/research/automap-20260924-0938/save-baseline/`.
Captures are in the parent research directory. The session used the existing
accessibility plugin and NVDA; no map-reveal tools or memory writes were used.

The game was opened in the Rogue Encampment, then the Dark Wood waypoint was
visited. The fullscreen map showed previously explored terrain. No reliable
before/after Inifuss visibility comparison was obtained. The user subsequently
took control and moved to Stony Field; the 10:02 UTC capture confirms the area
but has the controller inventory open. It does not establish Cairn visibility.
Game inputs were stopped while the user was controlling the character.

Session cleanup pending: inspection changed automap size from right minimap
(`AutoMapMode=2`) to fullscreen (`0`). It still read zero when the user resumed
control and the game lost focus at 10:04 UTC. Restore the size through the game's
options when control is available; do not overwrite the settings or saved game
with the baseline copy. The running game was left with the user.

Read-only inspection of the baseline 52,706-byte `yarkey.ma0` found 53 nodes and
8,435 six-byte entries in the container described by the discovery research.
The third payload group has strong drawable-ID candidates: cell 313 twice,
six distinct cell-314 records repeated in four nodes, and cell 427 once.
Well/shrine cell values corroborate this interpretation. Repeated entries do
not represent additional stones. Coordinates, node keys, current visibility and
quest-stage associations remain unverified; the plugin does not read this file.

## Remaining work before matching native visibility

Observe the stones' glyphs while approaching, leaving their loaded rooms and
returning, then after save/reload. Record quest state and compare native rendered
cells with read-only game state. Existing active-room and visited-room flags are
not a verified substitute. The static native investigation did not establish a
reliable automap consumer or discovery reader for this build.

For pathfinder design, distinguish identifying a quest landmark from deciding
when to offer it. The former has D2R evidence; faithfully matching the latter
still needs the native visibility check. No implementation was requested here.
