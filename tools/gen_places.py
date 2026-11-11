#!/usr/bin/env python3
r"""gen_places.py -- Generates src/generated/place_data.h: where landmark props and
interior volumes sit inside each map piece (DS1 preset), read from the game's own
HD layout JSONs, plus which LvlPrest presets are named places (bridge, camp...).

Every DS1 preset has an HD layout data/hd/env/preset/<path>.json listing its 3D
props. An entity at HD position (x, z) sits at DS1 subtile (x / 2, z / 2); audio
environment boxes and visual-data boxes are relative to the entity and give the
interior volumes. A prefab placement's footprint comes from the entity positions
inside the prefab JSON it names.

The Act II cliff stairs are placed by their collision mesh (the model's own
.physics file): its bounding-volume tree gives the bottom and the top landing.
The Act III jungle's bridges take the mesh's whole extent as their footprint.

A feature drawn by a game object, not a layout model, is read from the DS1's own
object list: objpreset.txt names each act's preset object, objects.txt its size.

Usage (after every game patch):
    build/casc-extract/casc_extract.exe extract "<game dir>" <root> "hd\env\preset\"
    build/casc-extract/casc_extract.exe extract "<game dir>" <root> "\pf_"
    build/casc-extract/casc_extract.exe extract "<game dir>" <root> "act2_bigcliff_stairs\"
    build/casc-extract/casc_extract.exe extract "<game dir>" <root> "act3_jungle_bridges\"
    build/casc-extract/casc_extract.exe extract "<game dir>" <root> ".ds1"
    build/casc-extract/casc_extract.exe extract "<game dir>" game-data "excel\objpreset.txt" "excel\objects.txt"
    python tools/gen_places.py --hd-root <root> [--lvlprest PATH] [--out PATH]

Stdlib only.
"""
import argparse
import csv
import json
import math
import re
import struct
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LVLPREST = REPO / "game-data/global/excel/LvlPrest.txt"
OBJPRESET = REPO / "game-data/global/excel/objpreset.txt"
OBJECTS = REPO / "game-data/global/excel/objects.txt"
OUT = REPO / "src/generated/place_data.h"

HD_PER_SUBTILE = 2.0         # 10 HD units per tile, 5 subtiles per tile
MERGE_RADIUS = 2             # subtiles; seeds of one feature closer than this merge
MIN_VOLUME_SIDE = 4          # subtiles; smaller boxes are placeholders
IDENTITY_W = 0.9999          # |w| above this means no rotation
SUBTILE_MAX = 0xFFFF
FILE_COLUMNS = [f"File{i}" for i in range(1, 7)]
MODEL_ROOT = "data/hd/env/model/"
SKIPPED_DIRS = ("ui",)
PRESET_SUBDIR = "hd/env/preset"
DS1_SUBDIR = "global/tiles"
DS1_OBJECT_TYPE = 2          # a DS1 object record of type 2 is an object (1 is a monster or NPC)
DATA_PREFIX = "data/"
MAX_FOOTPRINT_SIDE = 60      # subtiles; larger rects (prefab sub-entities far from the origin) fall back to a point

# Feature -> (source, regex). First match in this order wins. Sources: model, audio, any, and
# object (the class of a DS1 preset object, objpreset.txt).
FEATURES = [
    ("Kitchen", "model", r"table_kitchen|cutting_board|/meat\d|cleaver|hanging_food|/kiln|kettle|sharpener"),
    ("Fireplace", "any", r"fireplace(?!_top)"),
    ("Bar", "model", r"barstool|table_bar"),
    ("Tables", "model", r"/table_?(?:\d|long|short|large|small)|/bench\d"),
    ("Stairs", "model", r"_egress/stairs|/(?:up|down)stairs|(?:palace_interior|palace_cell_stairs|palace_harem_stairs|docktown_stairs|tomb_doorways|cathedral_walls|villa_architecture|wildtemple_temple|baallair_stairs)/\w*stairs"),
    ("Dock", "model", r"act2_town_docks/dock\d|act3_docktown_docks/(?:broken_)?dock\d|hut_landing"),
    ("Boat", "model", r"_ship/|/boat|_boats/"),
    ("Stalls", "model", r"/awning\d|shop_vendors|/stall"),
    ("Forge", "model", r"/anvil|/forge\d|bellows|billows"),
    ("Altar", "model", r"/altar\d|/altar_platform|_altar$"),
    ("Throne", "model", r"/(?:bone)?throne\d+$"),
    ("Graves", "model", r"headstone|/grave_(?:cover|border)|mausoleum|/coffin\d"),
    ("Cells", "model", r"celldoor"),
    ("Torture", "model", r"act1_barracks_torture/(?:rack|table)"),
    ("Beds", "model", r"/cot\d|/bed\d"),
    ("Books", "model", r"bookshelf|bookcase"),
    ("Statues", "model", r"/(?:act2_|church_)?statue\d"),
    ("Fountain", "model", r"fountain(?!_gore)"),
    ("Well", "model", r"/(?:small_|large_)?well\d"),
    ("Tents", "model", r"/tent\d"),
    ("Wagon", "model", r"/wagon\d\d(?:_pf)?$"),
    ("River", "audio", r"river"),
    # The jungle's fallen trees and plank bridges over its rivers are bridges too (a tester, 2026-10-23).
    ("Bridge", "model", r"_bridge/(?!bridge_tracks)|act3_jungle_bridges/bridge_(?:tree|wood)"),
    ("Campfire", "model", r"campfire|bonfire"),
    ("HangmanTree", "model", r"graveyard_hanging_corpse|graveyard_props/noose_wrap"),
    # The Act II desert's cliff stairs, the only way between a level's halves: also a tracker entry.
    ("CliffStairs", "model", r"act2_bigcliff_stairs/"),
    # Tristram's fountain runs with blood (a tester, 2026-10-17); its plain fountain models are dropped there.
    ("BloodFountain", "any", r"fountain_gore"),
    ("BurningBuilding", "any", r"tristram_(?:fire_small|firewall|smolder)|building_burnt|burnt_beam"),
    # The crypts' five-pointed fire on the floor, which burns whoever crosses it.
    ("FireStar", "any", r"fx_act1crypt_fire_pentagram"),
    # The sewers' round floor grate with its steam (manhole01; a tester, 2026-10-19). The flat
    # water_channel_grate01 along the channels is not it.
    ("SewerGrate", "model", r"act2_sewer_manholes/manhole"),
    # The Countess's gilded basin of blood, by the doors to her room, candles round it (a tester,
    # 2026-10-22). Not ritual_platform01: that is the dais of her chest, in her own room.
    ("BloodCistern", "object", r"^BloodFountain$"),
    # The River of Flame's Hellforge, said with its own words the first time (a tester, 2026-10-27).
    ("Hellforge", "object", r"^SoulStoneForge$"),
    # The great pentagram at the heart of the Chaos Sanctuary (a tester, 2026-10-27): also a tracker
    # entry. Its centre is the DS1's DiabloStart, where Diablo appears; the HD star sits 7 subtiles off.
    ("Pentagram", "object", r"^DiabloStart$"),
    # Travincal's central square, before the High Council's temple: its two bloodied altars and two
    # blood pits (a tester, 2026-11-10: something to walk towards and around). Also a tracker entry.
    ("CouncilAltar", "model", r"act3_temple_props/(?:altar01|pit_small01)$"),
]
# Seeds not matched by a rule: the top landing of each cliff stairway (CliffStairs is its bottom).
DERIVED_FEATURES = ["CliffStairsTop"]
FEATURE_NAMES = [f[0] for f in FEATURES] + DERIVED_FEATURES
# A feature's seeds drop another feature's seeds on the same spot.
SUPERSEDES = {"BloodFountain": ("Fountain",)}
# Files whose own preset name already says what the feature would (a cottage on fire).
FEATURE_SKIPPED_FILES = {"BurningBuilding": re.compile(r"^act1/outdoors/cott7\.ds1$"),
                         # cryptnwarpnext.ds1 keeps an unused copy of the Countess's room.
                         "BloodCistern": re.compile(r"^(?!act1/crypt/cryptcountess[12]\.ds1$)"),
                         # The same altars and pits furnish the Kurast temples and Travincal's side halls.
                         "CouncilAltar": re.compile(r"^(?!act3/travincal/travs\.ds1$)")}
# Subtiles added around a feature's seeds: the Hangman's Tree is said under its whole canopy,
# one tile past the corpses hanging from its boughs (a tester, 2026-10-17: only at the trunk); the
# pentagram over its inner pentagon, two tiles round where Diablo appears, where a route to it ends;
# the Council's altar one tile round its altars and pits.
FEATURE_PADDING = {"HangmanTree": 5, "Pentagram": 10, "CouncilAltar": 5}
# Subtiles within which a feature's seeds are one landmark, where MERGE_RADIUS is too short: the
# square's altars stand three tiles apart, its pits between them.
FEATURE_MERGE_RADIUS = {"CouncilAltar": 15}
# An HD layout draws raised ground towards the camera: an entity h HD units up sits h times this
# many subtiles right and down of its DS1 subtile (docs/places.md, "Raised ground").
SUBTILES_PER_HD_HEIGHT = 0.64
# Features whose models stand on the floor, so their own height is the ground's: placed by it. The
# others hang, float or stand on something (food, awnings, bar tops, statues, docks) and are not.
FLOOR_FEATURES = {"Altar", "CouncilAltar", "Tables", "Beds", "Tents", "Wagon", "Campfire", "Forge"}
# HD units; a model's origin this close to zero is level ground, not a height to place it by.
LEVEL_GROUND_HEIGHT = 1.0
# HD layouts drawn off their DS1's origin: (x, y) subtiles to take off every other entity. The Pandemonium
# Fortress's hall walls (int_wall*, ext_wall*) and brazier mounts sit 22 right and 22 down of the
# DS1's hall (west wall at x 15, colonnade at x 35, braziers at y 25-55; a tester's log, 2026-10-27).
HD_LAYOUT_OFFSETS = {"act4/fort/fortress.ds1": (22, 22)}
# Models whose footprint is their collision mesh: a log over a river is anchored at one end.
MESH_FOOTPRINTS = re.compile(r"act3_jungle_bridges/")
STAIRS_COLLISION_NODES = 0x30  # .physics: node records start here, 32 bytes each (min xyz, max xyz, two words)
# HD height: nodes within one subtile of the lowest top (highest base) are one landing, the stairway's full width.
STAIRS_LANDING_SLACK = HD_PER_SUBTILE
# A rule kept to named files is the more specific one: tried first.
FEATURE_RX = sorted(((n, s, re.compile(p)) for n, s, p in FEATURES), key=lambda rule: rule[0] not in FEATURE_SKIPPED_FILES)

# Feature -> regex over the basename of a prefab placement, tried before the model rules.
PREFAB_FEATURES = {
    "Kitchen": r"pf_caudron",
    "Bar": r"pf_kurast_bar",
    "Tables": r"pf_(?:tiny)?table",
    "Stairs": r"pf_palace_stairs|pf_alcove_stairs|pf_stairs\d",
    "Dock": r"_dock\d",
    "Boat": r"pf_act2_town_ship|pf_boat_|riverbank_boat",
    "Forge": r"pf_town_blacksmith|pf_forge|pf_alcove_forge",
    "Cells": r"pf_alcove_cell",
    "Torture": r"pf_alcove_torture",
    "Beds": r"pf_bed\d",
    "Books": r"pf_bookshel",
    "Statues": r"pf_statue\d",
    "Well": r"pf_(?:bloody_)?well\d",
    "Tents": r"_tent\d",
    "Wagon": r"pf_town_wagon\d",
    "Bridge": r"pf_diab_bridge|pf_bridge\d+(?:_\d+)?$|pf_bridge_stone",
    "Campfire": r"pf_town_campfire",
    "SewerGrate": r"pf_steamy_grate",
}
PREFAB_RX = [(n, re.compile(PREFAB_FEATURES[n])) for n in FEATURE_NAMES if n in PREFAB_FEATURES]
# Prefabs that only borrow a feature's name: the sewers' "pf_table01/02" are piles of crates, vases
# and rags along the walls, not tables (a tester, 2026-10-18).
PREFAB_NOT_FEATURES = {"Tables": re.compile(r"/sewer/prefabs/perimeter_deco/")}

# Interior -> regexes over an audio environment (ESOUNDENVIRON_*) or a visbox entity name.
INTERIORS = [
    ("Tavern", r"TOWN_2_BAR$|visbox_tavern"),
    ("Palace", r"TOWN_2_PALACE$|visbox_palace"),
    ("Cottage", r"ACT1_COTTAGE$|visbox_cottage"),
    ("Tower", r"ACT1_TOWER_ENTRANCE$|ACT1_INDOOR_TOWER$|visbox_tower"),
    ("Hut", r"KURAST_HUT_INDOOR|visbox_kurast_hut|visbox_alkor_hut|visbox_asheara_hut|TOWN_3_INDOOR"),
    ("Temple", r"KURAST_INDOOR_TEMPLE|visbox_kurast_temple|XTEMPLE_INDOOR$|visbox_tempenter_roof"),
    ("Fortress", r"TOWN_4_INDOOR_FORT$|visbox_fortress"),
    ("Keep", r"XTOWN_INDOOR$|visbox_harrograth"),
    ("Farmhouse", r"BARRICADE_FARM_HOUSE$|visbox_farm"),
    ("Shack", r"visbox_shack"),
    ("Cathedral", r"visbox_act1_cathedral"),
]
INTERIOR_NAMES = [i[0] for i in INTERIORS]
INTERIOR_RX = [(n, re.compile(p)) for n, p in INTERIORS]
IGNORED_VOLUMES = re.compile(r"WILDERNESS|FRONT_END|COURTYARD|ICECAVE_RIVER|FE_ACT|visbox_court_barracks|visbox_diab|visbox_monastry")

# LvlPrest row Name -> PresetPlace; only rows with LevelId 0 (pieces inside a larger level).
PLACES = [
    ("Bridge", r"^Act \d - Bridge"),
    ("River", r"^Act 1 - River (?:Upper|Lower)"),
    ("FallenCamp", r"^Act 1 - Fallen Camp"),
    ("Cottage", r"^Act 1 - Cottages"),
    ("Graveyard", r"^Act 1 - Graveyard"),
    # Not the Moldy Tome's piece in the Stony Field: the tome is listed already, its ruin is no news (a tester, 2026-10-23).
    ("Tower", r"^Act 1 - Tower(?! Tome)"),
    ("Ruin", r"^Act 1 - Ruin$|^Act 2 - Desert Valley Ruin"),
    ("Camp", r"^Act 1 - Camp$"),
    ("Temple", r"^Act 3 - (?:Temple \d|Burbs Temple|MetroTemple)"),
    ("CityGate", r"^Act 3 - (?:Slums|Burbs|Metro) Gate"),
    ("Oasis", r"^Act 2 - Desert Oasis"),
    ("Forge", r"^Act 1 - Barracks Forge"),
]
# LvlPrest file -> PresetPlace, over the row's name: a cottage row also picks a stable or a burning cottage.
FILE_PLACES = [
    ("Stable", r"^act1/outdoors/cott6a?\.ds1$"),
    ("BurningCottage", r"^act1/outdoors/cott7\.ds1$"),
    # Cottage 5's building (cottage05 under roof05, an open timber porch; a tester's Dark Wood
    # screenshots, 2026-10-18). Cottage 1's is the plain stone cottage (a tester, 2026-10-19).
    ("CottageWithPorch", r"^act1/outdoors/cott5a?\.ds1$"),
]
# Tiles around a named piece where its name is already spoken: the ruined tower is seen from afar.
PRESET_PADDING = {"Tower": 6}
# Interiors a file's volumes must not name: the stable's visbox is the cottage's, and the porch
# cottage already says its own name (its interior alone said just "cottage").
INTERIOR_SKIPPED_FILES = {"Cottage": re.compile(r"^act1/outdoors/cott[56]a?\.ds1$")}
PLACE_NAMES = [p[0] for p in PLACES] + [p[0] for p in FILE_PLACES]
PLACE_RX = [(n, re.compile(p)) for n, p in PLACES]
FILE_PLACE_RX = [(n, re.compile(p)) for n, p in FILE_PLACES]


def norm_path(p):
    return p.replace("\\", "/").lower()


def load_lvlprest(path):
    with open(path, encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def lvlprest_files(rows):
    files = set()
    for r in rows:
        for c in FILE_COLUMNS:
            v = r[c].strip()
            if v and v != "0":
                files.add(norm_path(v))
    return files


def comp(entity, kind):
    for c in entity["components"]:
        if c["type"] == kind:
            return c
    return None


def quat_rotate_xz(q, x, y, z):
    """Rotates (x, y, z) by quaternion q and returns the horizontal (x, z)."""
    qx, qy, qz, qw = q["x"], q["y"], q["z"], q["w"]
    tx = 2 * (qy * z - qz * y)
    ty = 2 * (qz * x - qx * z)
    tz = 2 * (qx * y - qy * x)
    return x + qw * tx + (qy * tz - qz * ty), z + qw * tz + (qx * ty - qy * tx)


def entity_strings(e):
    """Candidate texts per source for feature matching."""
    out = defaultdict(list)
    model = comp(e, "ModelDefinitionComponent")
    if model:
        m = model["filename"].lower()
        m = m[len(MODEL_ROOT):] if m.startswith(MODEL_ROOT) else m
        out["model"].append("/" + m.removesuffix(".model"))
    pf = comp(e, "PrefabPlacementDefinitionComponent")
    if pf:
        out["prefab"].append(pf["prefab"].lower().rsplit("/", 1)[-1].removesuffix(".json"))
    audio = comp(e, "AudioEmitterComponent")
    if audio:
        out["audio"].append(audio["audioId"].lower())
    out["name"].append(e["name"].lower())
    return out


def skipped_in(name, rel):
    return name in FEATURE_SKIPPED_FILES and FEATURE_SKIPPED_FILES[name].search(rel)


def classify_feature(e, rel):
    texts = entity_strings(e)
    pf = comp(e, "PrefabPlacementDefinitionComponent")
    prefab_path = pf["prefab"].lower() if pf else ""
    for name, rx in PREFAB_RX:
        if any(rx.search(t) for t in texts["prefab"]):
            if skipped_in(name, rel) or name in PREFAB_NOT_FEATURES and PREFAB_NOT_FEATURES[name].search(prefab_path):
                return None, None
            return name, "prefab"
    for name, source, rx in FEATURE_RX:
        pool = texts["model"] + texts["audio"] + texts["name"] if source == "any" else texts[source]
        if any(rx.search(t) for t in pool) and not skipped_in(name, rel):
            return name, "model"
    return None, None


def basename_of(e):
    texts = entity_strings(e)
    for key in ("prefab", "model", "audio"):
        if texts[key]:
            return texts[key][0].rsplit("/", 1)[-1]
    return e["name"].lower()


def merge_seeds(seeds, radius=MERGE_RADIUS):
    """seeds: [(x, y, rect)] of one feature -> one rect per cluster of nearby anchors (union)."""
    clusters = []  # [sumx, sumy, n, rect]
    for x, y, rect in seeds:
        for c in clusters:
            if abs(c[0] / c[2] - x) <= radius and abs(c[1] / c[2] - y) <= radius:
                c[0] += x
                c[1] += y
                c[2] += 1
                c[3] = (min(c[3][0], rect[0]), min(c[3][1], rect[1]), max(c[3][2], rect[2]), max(c[3][3], rect[3]))
                break
        else:
            clusters.append([x, y, 1, rect])
    return [c[3] for c in clusters]


def clamp_subtile(v):
    return max(0, min(SUBTILE_MAX, v))


def point_rect(x, y):
    return (x, y, min(SUBTILE_MAX, x + 1), min(SUBTILE_MAX, y + 1))


def prefab_extent(hd_root, prefab_ref, cache):
    """Local (minX, minZ, maxX, maxZ) of the entity positions in a prefab JSON, or None."""
    if prefab_ref not in cache:
        ref = norm_path(prefab_ref)
        path = hd_root / (ref[len(DATA_PREFIX):] if ref.startswith(DATA_PREFIX) else ref)
        extent = None
        if path.is_file():
            pts = []
            for e in json.loads(path.read_text(encoding="utf-8")).get("entities", []):
                t = comp(e, "TransformDefinitionComponent")
                if t:
                    pts.append((t["position"]["x"], t["position"]["z"]))
            if pts:
                xs, zs = zip(*pts)
                extent = (min(xs), min(zs), max(xs), max(zs))
        cache[prefab_ref] = extent
    return cache[prefab_ref]


def prefab_footprint(e, t, hd_root, cache, stats):
    """Subtile rect of a prefab placement, or a point rect when the prefab is unusable."""
    pos = t["position"]
    ax, ay = round(pos["x"] / HD_PER_SUBTILE), round(pos["z"] / HD_PER_SUBTILE)
    anchor = point_rect(ax, ay)
    ext = prefab_extent(hd_root, comp(e, "PrefabPlacementDefinitionComponent")["prefab"], cache)
    if ext is None:
        stats["fp_missing"] += 1
        return anchor
    q, sc = t["orientation"], t.get("scale", {"x": 1, "y": 1, "z": 1})
    xs, zs = [], []
    for x in (ext[0], ext[2]):
        for z in (ext[1], ext[3]):
            rx, rz = quat_rotate_xz(q, x * sc["x"], 0.0, z * sc["z"])
            xs.append(pos["x"] + rx)
            zs.append(pos["z"] + rz)
    rect = (clamp_subtile(math.floor(min(xs) / HD_PER_SUBTILE)), clamp_subtile(math.floor(min(zs) / HD_PER_SUBTILE)),
            clamp_subtile(math.ceil(max(xs) / HD_PER_SUBTILE)), clamp_subtile(math.ceil(max(zs) / HD_PER_SUBTILE)))
    if not (rect[0] <= ax < rect[2] and rect[1] <= ay < rect[3]):
        stats["fp_outside"] += 1
        return anchor
    if rect[2] - rect[0] > MAX_FOOTPRINT_SIDE or rect[3] - rect[1] > MAX_FOOTPRINT_SIDE:
        stats["fp_huge"] += 1
        return anchor
    stats["fp_ok"] += 1
    return rect


def collision_nodes(e, hd_root):
    """The bounding-volume nodes of an entity's collision mesh (min xyz, max xyz), or None."""
    body = comp(e, "PhysicsBodyDefinitionComponent")
    for fixture in body.get("fixturedefs", []) if body else []:
        ref = norm_path(fixture.get("shapetype", {}).get("filename", ""))
        path = hd_root / (ref[len(DATA_PREFIX):] if ref.startswith(DATA_PREFIX) else ref)
        if not ref or not path.is_file():
            continue
        data = path.read_bytes()
        count = struct.unpack_from("<I", data, 0x18)[0]  # header word 6: the node count
        if not count or STAIRS_COLLISION_NODES + 32 * count > len(data):
            continue
        return [struct.unpack_from("<6f", data, STAIRS_COLLISION_NODES + 32 * i) for i in range(count)]
    return None


def nodes_rect(nodes, t):
    """Subtile rect holding a group of collision nodes, with the entity's scale and yaw applied."""
    pos, q, sc = t["position"], t["orientation"], t.get("scale", {"x": 1, "y": 1, "z": 1})
    xs, zs = [], []
    for node in nodes:
        for x in (node[0], node[3]):
            for z in (node[2], node[5]):
                rx, rz = quat_rotate_xz(q, x * sc["x"], 0.0, z * sc["z"])
                xs.append(pos["x"] + rx)
                zs.append(pos["z"] + rz)
    return (clamp_subtile(math.floor(min(xs) / HD_PER_SUBTILE)), clamp_subtile(math.floor(min(zs) / HD_PER_SUBTILE)),
            clamp_subtile(math.ceil(max(xs) / HD_PER_SUBTILE)), clamp_subtile(math.ceil(max(zs) / HD_PER_SUBTILE)))


def stairs_landings(e, t, hd_root, stats):
    """(bottom, top) subtile rects of a stairway from its collision mesh, or None: the
    bounding-volume nodes lowest at their top are the bottom landing, the highest at their
    base the top one (HD y is up), each across the whole width a player may climb."""
    nodes = collision_nodes(e, hd_root)
    if not nodes:
        stats["stairs_missing"] += 1
        return None
    lowest_top = min(n[4] for n in nodes)
    highest_base = max(n[1] for n in nodes)
    stats["stairs_ok"] += 1
    return (nodes_rect([n for n in nodes if n[4] <= lowest_top + STAIRS_LANDING_SLACK], t),
            nodes_rect([n for n in nodes if n[1] >= highest_base - STAIRS_LANDING_SLACK], t))


def collision_footprint(e, t, hd_root, stats):
    """A model's subtile rect from its collision mesh, or None."""
    nodes = collision_nodes(e, hd_root)
    stats["mesh_ok" if nodes else "mesh_missing"] += 1
    return nodes_rect(nodes, t) if nodes else None


def centre(rect):
    return (rect[0] + rect[2]) // 2, (rect[1] + rect[3]) // 2


def classify_interior(label):
    for name, rx in INTERIOR_RX:
        if rx.search(label):
            return name
    return None


def volume_of(e, c, stats):
    """Axis-aligned subtile bounds of a box component, with the entity's scale and yaw applied."""
    t = comp(e, "TransformDefinitionComponent")
    pos, q, sc = t["position"], t["orientation"], t.get("scale", {"x": 1, "y": 1, "z": 1})
    if abs(q["w"]) < IDENTITY_W:
        stats["rotated"] += 1
    xs, zs = [], []
    for x in (c["min"]["x"], c["max"]["x"]):
        for z in (c["min"]["z"], c["max"]["z"]):
            rx, rz = quat_rotate_xz(q, x * sc["x"], 0.0, z * sc["z"])
            xs.append(pos["x"] + rx)
            zs.append(pos["z"] + rz)
    lo = (min(xs) / HD_PER_SUBTILE, min(zs) / HD_PER_SUBTILE)
    hi = (max(xs) / HD_PER_SUBTILE, max(zs) / HD_PER_SUBTILE)
    box = (max(0, math.floor(lo[0])), max(0, math.floor(lo[1])), min(SUBTILE_MAX, math.ceil(hi[0])), min(SUBTILE_MAX, math.ceil(hi[1])))
    return box


def load_object_classes(objpreset_path, objects_path):
    """-> ({(act, preset id): class}, {class: (size x, size y)})."""
    with open(objpreset_path, encoding="utf-8-sig", newline="") as f:
        presets = {(int(r["Act"]), int(r["Index"])): r["ObjectClass"]
                   for r in csv.DictReader(f, delimiter="\t") if r["Act"].strip() and r["Index"].strip()}
    with open(objects_path, encoding="utf-8-sig", newline="") as f:
        sizes = {r["Class"]: (int(r["SizeX"] or 1), int(r["SizeY"] or 1)) for r in csv.DictReader(f, delimiter="\t")}
    return presets, sizes


def ds1_objects(path):
    """A DS1's object records -> (act from 1, [(type, preset id, subtile x, subtile y)])."""
    b = path.read_bytes()
    at = 0

    def word():
        nonlocal at
        at += 4
        return struct.unpack_from("<i", b, at - 4)[0]
    version, width, height = word(), word() + 1, word() + 1
    act = word() + 1 if version >= 8 else 1
    tag_type = word() if version >= 10 else 0
    if version >= 3:
        for _ in range(word()):
            at = b.index(b"\0", at) + 1
    if 9 <= version <= 13:
        at += 8
    walls = word() if version >= 4 else 1
    floors = word() if version >= 16 else 1
    layers = walls * 2 + floors + 1 + (1 if tag_type in (1, 2) else 0)  # walls, orientations, floors, shadow, tag
    at += layers * width * height * 4
    records = []
    for _ in range(word() if version >= 2 else 0):
        kind, index, x, y = word(), word(), word(), word()
        if version > 5:
            word()  # flags
        records.append((kind, index, x, y))
    return act, records


def object_seeds(ds1_path, rel, classes, stats):
    """Seeds of the object-sourced features in one preset: each object's own footprint."""
    presets, sizes = classes
    seeds = defaultdict(list)
    if not ds1_path:
        stats["ds1_missing"] += 1
        return seeds
    act, records = ds1_objects(ds1_path)
    for kind, index, x, y in records:
        cls = presets.get((act, index)) if kind == DS1_OBJECT_TYPE else None
        for name, source, rx in FEATURE_RX:
            if not cls or source != "object" or not rx.search(cls):
                continue
            if skipped_in(name, rel):
                continue
            sx, sy = sizes.get(cls, (1, 1))
            rect = (clamp_subtile(x - sx // 2), clamp_subtile(y - sy // 2), clamp_subtile(x - sx // 2 + sx), clamp_subtile(y - sy // 2 + sy))
            seeds[name].append((x, y, rect))
            stats["features"][name, "object"][cls] += 1
            break
    return seeds


def scan_preset(path, rel, hd_root, cache, stats, object_seeded):
    d = json.loads(path.read_text(encoding="utf-8"))
    raw_seeds = defaultdict(list, object_seeded)
    volumes = []
    offset_x, offset_y = HD_LAYOUT_OFFSETS.get(rel, (0, 0))
    for e in d["entities"]:
        t = comp(e, "TransformDefinitionComponent")
        if not t:
            continue
        feature, kind = classify_feature(e, rel)
        raised = feature in FLOOR_FEATURES and abs(t["position"]["y"]) >= LEVEL_GROUND_HEIGHT
        lift = t["position"]["y"] * SUBTILES_PER_HD_HEIGHT if raised else None
        t["position"]["x"] -= (offset_x if lift is None else lift) * HD_PER_SUBTILE
        t["position"]["z"] -= (offset_y if lift is None else lift) * HD_PER_SUBTILE
        if feature:
            x, y = round(t["position"]["x"] / HD_PER_SUBTILE), round(t["position"]["z"] / HD_PER_SUBTILE)
            if lift and not (0 <= x <= SUBTILE_MAX and 0 <= y <= SUBTILE_MAX):
                stats["lifted_out"].append((rel, feature, x, y))
            landings = stairs_landings(e, t, hd_root, stats) if feature == "CliffStairs" else None
            if landings:
                for name, rect in zip(("CliffStairs", "CliffStairsTop"), landings):
                    raw_seeds[name].append((*centre(rect), rect))
                stats["features"][feature, kind][basename_of(e)] += 1
            elif 0 <= x <= SUBTILE_MAX and 0 <= y <= SUBTILE_MAX:
                rect = prefab_footprint(e, t, hd_root, cache, stats) if kind == "prefab" else None
                if not rect and any(MESH_FOOTPRINTS.search(m) for m in entity_strings(e)["model"]):
                    rect = collision_footprint(e, t, hd_root, stats)
                rect = rect or point_rect(x, y)
                raw_seeds[feature].append((x, y, rect))
                stats["features"][feature, kind][basename_of(e)] += 1
        for kind, label in (("AudioEnvironmentOverrideComponent", None), ("VisualDataBoxDefinitionComponent", None)):
            c = comp(e, kind)
            if not c:
                continue
            tag = c["overrideEnvironment"] if kind.startswith("Audio") else e["name"]
            if IGNORED_VOLUMES.search(tag) or not tag:
                stats["ignored"][tag or "(empty)"] += 1
                continue
            interior = classify_interior(tag)
            if not interior:
                stats["unknown"][tag] += 1
                continue
            if interior in INTERIOR_SKIPPED_FILES and INTERIOR_SKIPPED_FILES[interior].search(rel):
                continue
            minx, miny, maxx, maxy = volume_of(e, c, stats)
            if maxx - minx < MIN_VOLUME_SIDE or maxy - miny < MIN_VOLUME_SIDE:
                stats["tiny"] += 1
                continue
            volumes.append((minx, miny, maxx, maxy, interior))
    for winner, losers in SUPERSEDES.items():
        for loser in losers:
            raw_seeds[loser] = [s for s in raw_seeds.get(loser, []) if not any(
                abs(s[0] - w[0]) <= MERGE_RADIUS and abs(s[1] - w[1]) <= MERGE_RADIUS for w in raw_seeds.get(winner, []))]
    seeds = []
    for feature in FEATURE_NAMES:
        pad = FEATURE_PADDING.get(feature, 0)
        for rect in merge_seeds(raw_seeds.get(feature, []), FEATURE_MERGE_RADIUS.get(feature, MERGE_RADIUS)):
            seeds.append((clamp_subtile(rect[0] - pad), clamp_subtile(rect[1] - pad),
                          clamp_subtile(rect[2] + pad), clamp_subtile(rect[3] + pad), feature))
    return seeds, merge_volumes(volumes)


def merge_volumes(volumes):
    """Overlapping boxes of the same interior (an acoustic zone and a lighting box
    of one tavern) are one room: keep their union, so its edge is not a border."""
    merged = sorted(set(volumes))
    while True:
        result = []
        for box in merged:
            for i, other in enumerate(result):
                if other[4] == box[4] and box[0] < other[2] and other[0] < box[2] and box[1] < other[3] and other[1] < box[3]:
                    result[i] = (min(box[0], other[0]), min(box[1], other[1]), max(box[2], other[2]), max(box[3], other[3]), box[4])
                    break
            else:
                result.append(box)
        if len(result) == len(merged):
            return sorted(result)
        merged = result


def match_places(rows):
    """Returns ([(def, place, name)], [excluded (def, place, name, levelid)])."""
    kept, excluded = [], []
    for r in rows:
        for place, rx in PLACE_RX:
            if rx.search(r["Name"]):
                item = (int(r["Def"]), place, r["Name"])
                if r["LevelId"].strip() in ("0", ""):
                    kept.append(item)
                else:
                    excluded.append(item + (r["LevelId"],))
                break
    return sorted(kept), excluded


def render(table, seeds_all, vols_all, names, file_names):
    o = ["// generated by tools/gen_places.py from the game's HD preset layouts -- do not edit",
         "#pragma once", "#include <cstdint>", "namespace D2RAccess::Generated {"]
    o.append("enum class PlaceFeature : uint8_t { " + ", ".join(FEATURE_NAMES) + ", Count };")
    o.append("enum class PlaceInterior : uint8_t { " + ", ".join(INTERIOR_NAMES) + ", Count };")
    o.append("enum class PresetPlace : uint8_t { None, " + ", ".join(PLACE_NAMES) + ", Count };")
    o.append("struct PlaceSeed   { uint16_t minX, minY, maxX, maxY; PlaceFeature feature; };")
    o.append("struct PlaceVolume { uint16_t minX, minY, maxX, maxY; PlaceInterior interior; };")
    o.append("struct PresetPlaces { const char* file; uint32_t firstSeed; uint16_t seedCount, firstVolume, volumeCount; };")
    o.append("struct PresetName  { int32_t def; PresetPlace place; };")
    o.append("struct PresetFileName { const char* file; PresetPlace place; };")
    o.append("struct PresetPad { PresetPlace place; uint8_t tiles; };")
    o.append("inline constexpr PlaceSeed PlaceSeeds[] = {")
    o += [f"    {{{a}, {b}, {c}, {d}, PlaceFeature::{f}}}," for a, b, c, d, f in seeds_all]
    o.append("};")
    o.append("inline constexpr PlaceVolume PlaceVolumes[] = {")
    o += [f"    {{{a}, {b}, {c}, {d}, PlaceInterior::{i}}}," for a, b, c, d, i in vols_all]
    o.append("};")
    o.append("inline constexpr PresetPlaces PresetPlacesTable[] = {")
    o += [f'    {{"{f}", {fs}, {sc}, {fv}, {vc}}},' for f, fs, sc, fv, vc in table]
    o.append("};")
    o.append("inline constexpr PresetName PresetNames[] = {")
    o += [f"    {{{d}, PresetPlace::{p}}}, // {n}" for d, p, n in names]
    o.append("};")
    o.append("// Over PresetNames: the piece a named row picked.")
    o.append("inline constexpr PresetFileName PresetFileNames[] = {")
    o += [f'    {{"{f}", PresetPlace::{p}}},' for f, p in file_names]
    o.append("};")
    o.append("// Tiles around a piece where its name is spoken already.")
    o.append("inline constexpr PresetPad PresetPads[] = {")
    o += [f"    {{PresetPlace::{p}, {n}}}," for p, n in sorted(PRESET_PADDING.items())]
    o.append("};")
    o.append("}")
    return "\n".join(o) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hd-root", required=True, type=Path, help="extraction root holding hd/env/preset and hd/env/model")
    ap.add_argument("--lvlprest", type=Path, default=LVLPREST)
    ap.add_argument("--objpreset", type=Path, default=OBJPRESET)
    ap.add_argument("--objects", type=Path, default=OBJECTS)
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()

    rows = load_lvlprest(args.lvlprest)
    known = lvlprest_files(rows)
    stats = {"rotated": 0, "tiny": 0, "features": defaultdict(Counter), "ignored": Counter(), "unknown": Counter(),
             "fp_ok": 0, "fp_missing": 0, "fp_outside": 0, "fp_huge": 0, "stairs_ok": 0, "stairs_missing": 0, "mesh_ok": 0, "mesh_missing": 0,
             "ds1_missing": 0, "lifted_out": []}
    presets_dir = args.hd_root / PRESET_SUBDIR
    prefab_cache = {}
    classes = load_object_classes(args.objpreset, args.objects)
    ds1_files = {norm_path(p.relative_to(args.hd_root / DS1_SUBDIR).as_posix()): p
                 for p in (args.hd_root / DS1_SUBDIR).rglob("*.ds1")}

    found = {}
    for p in sorted(presets_dir.rglob("*.json")):
        rel = norm_path(p.relative_to(presets_dir).as_posix())
        if rel.split("/")[0] in SKIPPED_DIRS:
            continue
        found[rel.removesuffix(".json") + ".ds1"] = p
    dropped = sorted(set(found) - known)
    missing_hd = sorted(known - set(found))
    print(f"hd presets {len(found)}; no LvlPrest file: {len(dropped)} e.g. {dropped[:5]}")
    print(f"LvlPrest files {len(known)}; no HD preset: {len(missing_hd)} e.g. {missing_hd[:5]}")

    table, seeds_all, vols_all = [], [], []
    interior_presets = defaultdict(list)
    for file in sorted(set(found) & known):
        objects = object_seeds(ds1_files.get(file), file, classes, stats)
        seeds, vols = scan_preset(found[file], file, args.hd_root, prefab_cache, stats, objects)
        if not seeds and not vols:
            continue
        table.append((file, len(seeds_all), len(seeds), len(vols_all), len(vols)))
        seeds_all += seeds
        vols_all += vols
        for v in vols:
            interior_presets[v[4]].append(file)
    table.sort(key=lambda r: r[0].encode())

    kept, excluded = match_places(rows)
    file_names = sorted(((f, next(n for n, rx in FILE_PLACE_RX if rx.search(f))) for f in known
                         if any(rx.search(f) for _, rx in FILE_PLACE_RX)), key=lambda r: r[0].encode())
    out = render(table, seeds_all, vols_all, sorted(kept), file_names)
    args.out.write_text(out, encoding="utf-8", newline="\n")

    print(f"wrote {args.out}: {len(out)} bytes, {len(table)} presets, {len(seeds_all)} seeds, {len(vols_all)} volumes")
    print(f"prefab footprints: {stats['fp_ok']} ok; point fallback: {stats['fp_missing']} prefab missing, {stats['fp_outside']} rect misses anchor, {stats['fp_huge']} over {MAX_FOOTPRINT_SIDE} subtiles")
    print(f"boxes not identity-rotated: {stats['rotated']}; tiny boxes skipped: {stats['tiny']}")
    print(f"stairs from collision: {stats['stairs_ok']}; without a collision file: {stats['stairs_missing']}")
    print(f"footprints from collision: {stats['mesh_ok']}; without a collision file: {stats['mesh_missing']}")
    print(f"DS1 files {len(ds1_files)}; presets without one: {stats['ds1_missing']}")
    print(f"floor features whose height puts them outside their piece, dropped: {stats['lifted_out']}")
    print("file names:", file_names)
    print("ignored volumes:", dict(stats["ignored"]), "unclassified:", dict(stats["unknown"]))
    for f in FEATURE_NAMES:
        for kind in ("model", "prefab", "object"):
            c = stats["features"][f, kind]
            print(f"{f} [{kind}]: raw {sum(c.values())}, top {c.most_common(6)}")
    for i in INTERIOR_NAMES:
        print(f"{i}: {sorted(set(interior_presets[i]))}")
    for d, p, n in sorted(kept):
        print(f"name {d} -> {p}  {n}")
    print("excluded (LevelId != 0):", excluded)
    for file, fs, sc, fv, vc in table:
        if file == "act2/town/lutw.ds1":
            print("lutw volumes:", vols_all[fv:fv + vc])
            print("lutw seeds:", dict(Counter(s[4] for s in seeds_all[fs:fs + sc])))


if __name__ == "__main__":
    main()
