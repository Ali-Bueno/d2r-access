#!/usr/bin/env python3
r"""gen_places.py -- Generates src/generated/place_data.h: where landmark props and
interior volumes sit inside each map piece (DS1 preset), read from the game's own
HD layout JSONs, plus which LvlPrest presets are named places (bridge, camp...).

Every DS1 preset has an HD layout data/hd/env/preset/<path>.json listing its 3D
props. An entity at HD position (x, z) sits at DS1 subtile (x / 2, z / 2); audio
environment boxes and visual-data boxes are relative to the entity and give the
interior volumes. A prefab placement's footprint comes from the entity positions
inside the prefab JSON it names.

Usage (after every game patch):
    build/casc-extract/casc_extract.exe extract "<game dir>" <root> "hd\env\preset\"
    build/casc-extract/casc_extract.exe extract "<game dir>" <root> "\pf_"
    python tools/gen_places.py --hd-root <root> [--lvlprest PATH] [--out PATH]

Stdlib only.
"""
import argparse
import csv
import json
import math
import re
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LVLPREST = REPO / "game-data/global/excel/LvlPrest.txt"
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
DATA_PREFIX = "data/"
MAX_FOOTPRINT_SIDE = 60      # subtiles; larger rects (prefab sub-entities far from the origin) fall back to a point

# Feature -> (source, regex). First match in this order wins. Sources: model, audio, any.
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
    ("Fountain", "model", r"fountain"),
    ("Well", "model", r"/(?:small_|large_)?well\d"),
    ("Tents", "model", r"/tent\d"),
    ("Wagon", "model", r"/wagon\d\d(?:_pf)?$"),
    ("River", "audio", r"river"),
    ("Bridge", "model", r"_bridge/(?!bridge_tracks)"),
    ("Campfire", "model", r"campfire|bonfire"),
    ("HangmanTree", "model", r"graveyard_hanging_corpse|graveyard_props/noose_wrap"),
]
FEATURE_NAMES = [f[0] for f in FEATURES]
FEATURE_RX = [(n, s, re.compile(p)) for n, s, p in FEATURES]

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
}
PREFAB_RX = [(n, re.compile(PREFAB_FEATURES[n])) for n in FEATURE_NAMES if n in PREFAB_FEATURES]

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
    ("Cottages", r"^Act 1 - Cottages"),
    ("Graveyard", r"^Act 1 - Graveyard"),
    ("Tower", r"^Act 1 - Tower"),
    ("Ruin", r"^Act 1 - Ruin$|^Act 2 - Desert Valley Ruin"),
    ("Camp", r"^Act 1 - Camp$"),
    ("Temple", r"^Act 3 - (?:Temple \d|Burbs Temple|MetroTemple)"),
    ("CityGate", r"^Act 3 - (?:Slums|Burbs|Metro) Gate"),
    ("Oasis", r"^Act 2 - Desert Oasis"),
    ("Forge", r"^Act 1 - Barracks Forge"),
]
PLACE_NAMES = [p[0] for p in PLACES]
PLACE_RX = [(n, re.compile(p)) for n, p in PLACES]


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


def classify_feature(e):
    texts = entity_strings(e)
    for name, rx in PREFAB_RX:
        if any(rx.search(t) for t in texts["prefab"]):
            return name, "prefab"
    for name, source, rx in FEATURE_RX:
        pool = texts["model"] + texts["audio"] + texts["name"] if source == "any" else texts[source]
        if any(rx.search(t) for t in pool):
            return name, "model"
    return None, None


def basename_of(e):
    texts = entity_strings(e)
    for key in ("prefab", "model", "audio"):
        if texts[key]:
            return texts[key][0].rsplit("/", 1)[-1]
    return e["name"].lower()


def merge_seeds(seeds):
    """seeds: [(x, y, rect)] of one feature -> one rect per cluster of nearby anchors (union)."""
    clusters = []  # [sumx, sumy, n, rect]
    for x, y, rect in seeds:
        for c in clusters:
            if abs(c[0] / c[2] - x) <= MERGE_RADIUS and abs(c[1] / c[2] - y) <= MERGE_RADIUS:
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


def scan_preset(path, hd_root, cache, stats):
    d = json.loads(path.read_text(encoding="utf-8"))
    raw_seeds = defaultdict(list)
    volumes = []
    for e in d["entities"]:
        t = comp(e, "TransformDefinitionComponent")
        if not t:
            continue
        feature, kind = classify_feature(e)
        if feature:
            x, y = round(t["position"]["x"] / HD_PER_SUBTILE), round(t["position"]["z"] / HD_PER_SUBTILE)
            if 0 <= x <= SUBTILE_MAX and 0 <= y <= SUBTILE_MAX:
                rect = prefab_footprint(e, t, hd_root, cache, stats) if kind == "prefab" else point_rect(x, y)
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
            minx, miny, maxx, maxy = volume_of(e, c, stats)
            if maxx - minx < MIN_VOLUME_SIDE or maxy - miny < MIN_VOLUME_SIDE:
                stats["tiny"] += 1
                continue
            volumes.append((minx, miny, maxx, maxy, interior))
    seeds = []
    for feature in FEATURE_NAMES:
        for rect in merge_seeds(raw_seeds.get(feature, [])):
            seeds.append((*rect, feature))
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


def render(table, seeds_all, vols_all, names):
    o = ["// generated by tools/gen_places.py from the game's HD preset layouts -- do not edit",
         "#pragma once", "#include <cstdint>", "namespace D2RAccess::Generated {"]
    o.append("enum class PlaceFeature : uint8_t { " + ", ".join(FEATURE_NAMES) + ", Count };")
    o.append("enum class PlaceInterior : uint8_t { " + ", ".join(INTERIOR_NAMES) + ", Count };")
    o.append("enum class PresetPlace : uint8_t { None, " + ", ".join(PLACE_NAMES) + ", Count };")
    o.append("struct PlaceSeed   { uint16_t minX, minY, maxX, maxY; PlaceFeature feature; };")
    o.append("struct PlaceVolume { uint16_t minX, minY, maxX, maxY; PlaceInterior interior; };")
    o.append("struct PresetPlaces { const char* file; uint32_t firstSeed; uint16_t seedCount, firstVolume, volumeCount; };")
    o.append("struct PresetName  { int32_t def; PresetPlace place; };")
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
    o.append("}")
    return "\n".join(o) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hd-root", required=True, type=Path, help="extraction root holding hd/env/preset and hd/env/model")
    ap.add_argument("--lvlprest", type=Path, default=LVLPREST)
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()

    rows = load_lvlprest(args.lvlprest)
    known = lvlprest_files(rows)
    stats = {"rotated": 0, "tiny": 0, "features": defaultdict(Counter), "ignored": Counter(), "unknown": Counter(),
             "fp_ok": 0, "fp_missing": 0, "fp_outside": 0, "fp_huge": 0}
    presets_dir = args.hd_root / PRESET_SUBDIR
    prefab_cache = {}

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
        seeds, vols = scan_preset(found[file], args.hd_root, prefab_cache, stats)
        if not seeds and not vols:
            continue
        table.append((file, len(seeds_all), len(seeds), len(vols_all), len(vols)))
        seeds_all += seeds
        vols_all += vols
        for v in vols:
            interior_presets[v[4]].append(file)
    table.sort(key=lambda r: r[0].encode())

    kept, excluded = match_places(rows)
    out = render(table, seeds_all, vols_all, sorted(kept))
    args.out.write_text(out, encoding="utf-8", newline="\n")

    print(f"wrote {args.out}: {len(out)} bytes, {len(table)} presets, {len(seeds_all)} seeds, {len(vols_all)} volumes")
    print(f"prefab footprints: {stats['fp_ok']} ok; point fallback: {stats['fp_missing']} prefab missing, {stats['fp_outside']} rect misses anchor, {stats['fp_huge']} over {MAX_FOOTPRINT_SIDE} subtiles")
    print(f"boxes not identity-rotated: {stats['rotated']}; tiny boxes skipped: {stats['tiny']}")
    print("ignored volumes:", dict(stats["ignored"]), "unclassified:", dict(stats["unknown"]))
    for f in FEATURE_NAMES:
        for kind in ("model", "prefab"):
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
