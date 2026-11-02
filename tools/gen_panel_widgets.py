#!/usr/bin/env python3
"""gen_panel_widgets.py -- Generates src/generated/panel_widgets.h, a catalog
of the FOCUSABLE widgets of every runtime panel (root widget name -> a list
of widget entries), used by the plugin to match the controller focus
indicator's rect against a widget's own rect and announce what's focused.

Reuses tools/layout_index.py for the JSON5-ish parser, the "basedOn" merge
and the string-table resolver -- see that module's docstring. Panel/layout
grouping (root widget name -> candidate layout files, HD preferred) mirrors
tools/gen_panel_titles.py; this generator additionally merges in the PC
controller variant of the SAME file, when present, using layout_index's own
"basedOn" merge function so the Controller variant's fields/widgets win on
conflict.

Controller-variant merge scope (PC only -- confirmed by in-game diagnostics
that non-PC controller widgets, e.g. a Console-only QuickPlayButton inside
MainMenuButtonRibbon, do not exist at runtime on PC and fail to resolve):
merge ONLY the chosen root/HD layout, "Controller/<same file name>" (if
present) and "Controller/PC/<same file name>" (if present, applied last so
it wins over the plain Controller/ variant). "Controller/Console/...",
"Controller/Console/Switch/..." and "Controller/PC/Handheld/..." are NEVER
merged. A panel that exists ONLY under one of those excluded paths (no root,
no direct Controller/, no Controller/PC/) has no PC widget source and is
skipped entirely (gets no entry, same as a panel with zero focusable
widgets).

Focusable-widget rule (applied to every non-root node of the merged tree):
  - has a "navigation" field, or
  - has any field whose name starts with "focusIndicator", or
  - has a "focusOnMouseOver" field, or
  - its widget "type" contains one of:
    Button, Toggle, Tab, Slider, Dropdown, Checkbox, Edit, Row, Slot, Grid,
    List, Scroll, Selector (plain substring match, as specified).

For each such widget, records:
  - name: its own widget name (or a synthetic "Type#index" if the JSON node
    has no "name", matching layout_index.flatten_layout's own convention).
  - type: its widget type.
  - textKey / text: from its primary text field (textString, else text): a
    "@Key" reference resolves to textKey="Key", text=nullptr; a literal
    string is text="...", textKey=nullptr; no text field -> both nullptr.
  - path: the chain of NAMED ancestors from the panel root's first-level
    child down to the widget itself (root name excluded), each paired with
    ITS OWN "anchor" fraction (the SDK's widget rects are local to the
    parent and do NOT apply "anchor" themselves -- {"x": 0.5} means
    "offset from the parent's horizontal center", default {0,0} when the
    field is absent). An ancestor with no "name" in the JSON contributes no
    path segment (it is skipped, not included as "Widget#n") since the
    SDK's findWidget navigates by real names only; each such unnamed
    ancestor is counted once (by node identity) and reported at the end.
    The panel root's own anchor fraction is recorded once per panel.
    An "anchor" value that is a "$Name" profile reference is resolved
    against the _profile*.json files (_profileHD.json first); an
    unresolvable reference falls back to {0,0} and is reported.
  - labelKey: the string-table key of the widget's own label, used only
    when the widget has no textString/text of its own (if it does, labelKey
    is left nullptr rather than repeating textKey -- a documented choice,
    not a limitation of the data). Search order:
      1. Among the widget's OWN SIBLINGS (its parent's other children),
         prefer the first (in document order) TextBoxWidget with an "@key"
         text whose name starts with the widget's own name, or ends with
         "Text"/"Label" (matches the game's own "<Name>Text"/"<Name>Label"
         convention, e.g. "WindowModeText" next to "WindowMode"). If no
         sibling matches that preference, fall back to the first sibling
         TextBoxWidget with an "@key" text at all (e.g. "Row NPCSpeech"'s
         label is named "NPC Speech", which matches neither preference but
         is still the row's only text sibling).
      2. Failing that, the same search repeated one ancestor level up (the
         grandparent's children, i.e. the parent's siblings) -- "up to 2
         levels" total (parent, then grandparent).
      3. Failing that, nullptr.
    This is a plain name heuristic, not a semantic one: when a widget's
    parent has no per-widget label (e.g. two toggles sharing a container
    with an unrelated "*Label" sibling), it can pick an unrelated text --
    flagged case by case in the generation report.
  - tooltipKey: from the widget's OWN "tooltipString" field only (not the
    "tooltip" fallback used elsewhere): a "$Name" profile reference is
    resolved first; an "@key" result yields that key; a literal string (or
    an unresolvable reference) yields nullptr.
  - updateTarget / updateCommand: the first two ":"-separated parts of the
    widget's "onUpdateMessage" (e.g. "OptionsPanelMessage:SetMasterVolume"),
    or of its "onClickMessage" when onUpdateMessage is absent AND the
    widget is a ToggleButtonWidget (the VSync toggle only has an
    onClickMessage). A third ":"-separated part is dropped from the header
    but tallied for the report. nullptr/nullptr when neither field applies.
  - states / stateCount: the widget's "states" array (a "$Name" profile
    reference is resolved first), each entry with a leading "@" stripped
    (entries without "@", e.g. "OptionsMenuNPCSpeechAudioOnly", are kept
    as-is), emitted as its own "States_<Panel>_<n>" array. nullptr/0 when
    absent, unresolvable, or resolves to an empty list (a zero-length
    array is not valid to declare here).

Stdlib only. Usage (run from the repo root):
    python tools\\gen_panel_widgets.py
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_index  # noqa: E402  (reuse the JSON5-ish parser / basedOn merge / string resolver)

GAME_VERSION = "D2R 3.3.93787"

FOCUSABLE_TYPE_KEYWORDS = (
    "Button", "Toggle", "Tab", "Slider", "Dropdown", "Checkbox",
    "Edit", "Row", "Slot", "Grid", "List", "Scroll", "Selector",
)

# MiniMenuButtonWidget entries (the 7 HUD mini-menu icons in HUDPanelHD.json's
# "MiniMenu" group) carry no textString/label sibling of their own -- their
# accessible name lives only in their own "menu" field (e.g. "character"),
# which must be mapped to the matching string-table key from
# game-data/local/lng/strings/ui.json (confirmed against each key's enUS
# value, e.g. "menu": "party" -> minipanelparty -> "Party Screen").
# Controls whose label find_label_key() cannot get right, because they have no
# label of their own in the layout and no sibling whose name relates to them.
# In that situation find_label_key() falls back to the first text reference
# among the siblings, which is a *confidently wrong* name rather than a missing
# one -- on CharacterCreatePanel the three option toggles sit beside the
# character-name field, so all three announced themselves as "Character Name"
# (live log 2026-09-18). Keyed by (panel, widget); values confirmed against
# each key's enUS value in game-data/local/lng/strings/ui.json, as with
# MINI_MENU_LABEL_KEYS below.
# An empty value means "this control has no label of its own": clear the
# borrowed one so Label() falls through to the tooltip, which is what the
# game itself shows for the control.
WIDGET_LABEL_OVERRIDES: dict[tuple[str, str], str] = {
    # "Hardcore"; its tooltip TooltipHardcoreCharacter opens with the same word.
    ("CharacterCreatePanel", "HardcoreCheckbox"): "strHardcore",
    # "Ladder"; matches the tooltip title in TooltipLadderCharacter.
    ("CharacterCreatePanel", "LadderToggle"): "LobbyLadder",
    # "Pre-Expansion"; matches the tooltip title in TooltipPreExpansionCharacter.
    ("CharacterCreatePanel", "ClassicToggle"): "CharacterSelectPanelPreExpansionCharacter",
    # Icon-only help button (PANEL\Loot_Filter\Chronicle_Btn_Info) with a
    # tooltipString and no labelString, so it borrowed the dropdown beside it
    # and both announced themselves as "Game Version" (live log 2026-09-19).
    # Every other use of that icon in the layouts is likewise a help button
    # carrying only a tooltip, so the tooltip is what describes it.
    ("CharacterCreatePanel", "GameVersionInfoButton"): "",
    # Loot filter icon buttons (PANEL\Loot_Filter icons, no text or tooltip): the
    # game's own strings for what each does; the three profile buttons all
    # borrowed "Profile:" from the caption beside them (a tester, 2026-11-02).
    ("LootFilterProfilePanel", "ProfileOptions"): "LootFilterMoreOptions",
    ("LootFilterProfilePanel", "AddProfile"): "NewProfileTitle",
    ("LootFilterProfilePanel", "ImportProfile"): "ImportProfileTitle",
    ("LootFilterProfilePanel", "EnableRuleToggle"): "LootFilterEnableRule",
    ("LootFilterProfilePanel", "RuleUpArrow"): "LootFilterMoveRuleUp",
    ("LootFilterProfilePanel", "RuleDownArrow"): "LootFilterMoveRuleDown",
    ("LootFilterProfilePanel", "RuleOptions"): "LootFilterMoreOptions",
    ("LootFilterRuleDetailsPanel", "BackButton"): "UIBack",
    # The Equipment/Items tab strip: its open tab names it.
    ("LootFilterRuleDetailsPanel", "ItemListTabs"): "",
    ("LootFilterItemPanel", "CategoryExpandButton"): "LootFilterExpandCategory",
    # The dropdown sits under "Copy Existing Profile (Optional)", not the name above it.
    ("LootFilterNewProfileModal", "ProfileDropdown"): "CopyExistingProfile",
    # Borrowed captions in-game icon buttons would now say through the interaction
    # reader (FocusTracker::CatalogLabel): the party panel's X is no "Game Name", the
    # other trader's ready mark no "Trade", an unreachable waypoint no "Waypoint".
    ("PartyPanel", "CloseButton"): "strClose",
    ("TradeLayout", "ready_them"): "",
    ("WaypointsPanelExpansion", "UnselectableButtonTemplate"): "",
    ("WaypointsPanelOriginal", "UnselectableButtonTemplate"): "",
    ("ChronicleRewardsPanel", "UniquesRewards"): "Unique",
    ("ChronicleRewardsPanel", "SetsRewards"): "Sets",
    ("ChronicleRewardsPanel", "RunewordsRewards"): "Runewords",
}

# The Chronicle's list pages share ChronicleBaseListPanelHD.json's controls,
# none named: the search box shows only its placeholder, the dropdown filters
# the list, and the toggle's caption is a child the label search never visits.
for _panel in ("ChronicleBaseListPanel", "ChronicleUniquesListPanel", "ChronicleSetsListPanel", "ChronicleRunewordsListPanel"):
    WIDGET_LABEL_OVERRIDES[(_panel, "SearchInput")] = "ChronicleSearchPlaceholder"
    WIDGET_LABEL_OVERRIDES[(_panel, "FilterRules")] = "GameListFilters"
    WIDGET_LABEL_OVERRIDES[(_panel, "RewardsButton")] = "ViewRewardsButton"
    # The list of entries the open category shows ("Items"); the categories beside it have
    # no game string and are named by the mod (focus_tracker.cpp ModLabels).
    WIDGET_LABEL_OVERRIDES[(_panel, "ItemScrollViewContainer")] = "AdvancedItems"

MINI_MENU_LABEL_KEYS: dict[str, str] = {
    "character": "minipanelchar",
    "inventory": "minipanelinv",
    "skillTree": "minipaneltree",
    "autoMap": "minipanelautomap",
    "party": "minipanelparty",
    "questLog": "minipanelquest",
    "pauseMenu": "minipanelmenubtn",
}


class AnchorStats:
    """Accumulates anchor-resolution facts across the whole run, for the
    final report: how many "$Name" profile references were seen/resolved,
    which ones (if any) failed to resolve, and any anchor key other than
    x/y (none expected, per the SDK's anchor contract).
    """

    def __init__(self) -> None:
        self.ref_count = 0
        self.unresolved: list[str] = []
        self.other_keys: set[str] = set()


def load_anchor_profiles(layouts_dir: Path) -> list[tuple[str, dict[str, Any]]]:
    """Loads every "_profile*.json" style/constants dictionary, HD first (as
    instructed), the rest alphabetically as a fallback chain for a "$Name"
    anchor reference not defined in the HD profile.
    """
    profile_files = sorted(layouts_dir.glob("_profile*.json"), key=lambda p: (p.name != "_profileHD.json", p.name))
    profiles: list[tuple[str, dict[str, Any]]] = []
    for p in profile_files:
        try:
            data = layout_index.load_json5ish(p)
        except Exception:
            continue
        if isinstance(data, dict):
            profiles.append((p.name, data))
    return profiles


def lookup_profile(key: str, profiles: list[tuple[str, dict[str, Any]]]) -> Any | None:
    for _, data in profiles:
        if key in data:
            return data[key]
    return None


def resolve_anchor(value: Any, profiles: list[tuple[str, dict[str, Any]]], stats: AnchorStats) -> tuple[float, float]:
    if value is None:
        return 0.0, 0.0
    if isinstance(value, str):
        if not value.startswith("$"):
            stats.unresolved.append(f"non-$ string anchor: {value!r}")
            return 0.0, 0.0
        stats.ref_count += 1
        resolved = lookup_profile(value[1:], profiles)
        if resolved is None:
            stats.unresolved.append(value)
            return 0.0, 0.0
        value = resolved
    if isinstance(value, dict):
        for k in value:
            if k not in ("x", "y"):
                stats.other_keys.add(k)
        try:
            return float(value.get("x", 0.0)), float(value.get("y", 0.0))
        except (TypeError, ValueError):
            stats.unresolved.append(f"non-numeric anchor dict: {value!r}")
            return 0.0, 0.0
    stats.unresolved.append(f"unexpected anchor shape: {value!r}")
    return 0.0, 0.0

_IDENT_RE = re.compile(r"[^0-9A-Za-z_]")


def sanitize_ident(name: str) -> str:
    ident = _IDENT_RE.sub("_", name)
    if ident and ident[0].isdigit():
        ident = "_" + ident
    return ident or "_"


def escape_c_string(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def is_synthetic_root_name(root: dict[str, Any]) -> bool:
    wtype = root.get("type")
    return root.get("name") == f"{wtype or 'Widget'}#0"


def is_focusable(fields: dict[str, Any], wtype: str | None) -> bool:
    if "navigation" in fields or "focusOnMouseOver" in fields:
        return True
    if any(k.startswith("focusIndicator") for k in fields):
        return True
    if wtype and any(kw in wtype for kw in FOCUSABLE_TYPE_KEYWORDS):
        return True
    return False


def primary_text(fields: dict[str, Any], string_table: dict[str, dict[str, Any]]) -> tuple[str | None, str | None]:
    """Returns (textKey, text) from the widget's primary text field
    (textString preferred over text), matching layout_index.flatten_layout.
    """
    raw = fields.get("textString") if "textString" in fields else fields.get("text")
    if not isinstance(raw, str):
        return None, None
    if raw.startswith("@"):
        resolved = layout_index.resolve_string_ref(raw, string_table)
        return resolved["key"], None
    return None, raw


def _text_ref_candidates(siblings: list[Any], exclude: Any) -> list[tuple[str, str]]:
    """(name, key) for every TextBoxWidget child in `siblings` (skipping
    `exclude`, the widget itself) whose primary text is an "@key" ref, in
    document order.
    """
    out: list[tuple[str, str]] = []
    for c in siblings:
        if not isinstance(c, dict) or c is exclude or c.get("type") != "TextBoxWidget":
            continue
        cfields = c.get("fields") or {}
        raw = cfields.get("textString") if "textString" in cfields else cfields.get("text")
        if isinstance(raw, str) and raw.startswith("@"):
            out.append((c.get("name") or "", raw[1:]))
    return out


def find_label_key(widget_name: str, widget_node: Any, ancestor_siblings: list[list[Any]]) -> str | None:
    """ancestor_siblings[0] = the widget's own siblings (parent's children),
    ancestor_siblings[1] = the parent's siblings (grandparent's children),
    i.e. "up to 2 levels" of ancestor as specified.
    """
    for level, siblings in enumerate(ancestor_siblings[:2]):
        candidates = _text_ref_candidates(siblings, widget_node if level == 0 else None)
        if not candidates:
            continue
        if level == 0:
            preferred = [
                (n, k) for (n, k) in candidates
                if n.startswith(widget_name) or n.endswith("Text") or n.endswith("Label")
            ]
            if preferred:
                # Several captions side by side (NormalQualityText, EliteQualityText):
                # each control takes the one its name shares the most with.
                return max(preferred, key=lambda c: len(os.path.commonprefix([c[0], widget_name])))[1]
        return candidates[0][1]
    return None


def compute_tooltip_key(fields: dict[str, Any], profiles: list[tuple[str, dict[str, Any]]]) -> str | None:
    raw = fields.get("tooltipString")
    if isinstance(raw, str) and raw.startswith("$"):
        raw = lookup_profile(raw[1:], profiles)
    if isinstance(raw, str) and raw.startswith("@"):
        return raw[1:]
    return None


def compute_update_message(fields: dict[str, Any], wtype: str | None) -> tuple[str | None, str | None, str | None]:
    """Returns (target, command, extra) where `extra` is the 3rd ":"-part,
    if any (report-only, never written to the header).
    """
    raw = fields.get("onUpdateMessage")
    if raw is None and wtype == "ToggleButtonWidget":
        raw = fields.get("onClickMessage")
    if not isinstance(raw, str) or not raw:
        return None, None, None
    parts = raw.split(":")
    target = parts[0] if parts and parts[0] else None
    command = parts[1] if len(parts) >= 2 else None
    extra = parts[2] if len(parts) >= 3 else None
    return target, command, extra


# A placeholder list ([""]) means the game fills the states at runtime; some
# layouts keep the source list in a field of their root (CharacterCreatePanelHD:
# "gameVersionNames", per the comment next to its GameVersionDropdown).
RUNTIME_STATE_SOURCES = ("gameVersionNames",)


def compute_states(fields: dict[str, Any], profiles: list[tuple[str, dict[str, Any]]], root_fields: dict[str, Any]) -> list[str] | None:
    raw = fields.get("states")
    if isinstance(raw, str) and raw.startswith("$"):
        raw = lookup_profile(raw[1:], profiles)
    if not isinstance(raw, list) or not raw:
        return None
    if all(item == "" for item in raw):
        for source in RUNTIME_STATE_SOURCES:
            if isinstance(root_fields.get(source), list) and root_fields[source]:
                raw = root_fields[source]
                break
    return [item[1:] if isinstance(item, str) and item.startswith("@") else str(item) for item in raw]


def variant_priority(rel_str: str) -> tuple[int, str]:
    is_hd = Path(rel_str).stem.endswith("HD")
    return (0 if is_hd else 1, rel_str)


class Walker:
    def __init__(
        self,
        string_table: dict[str, dict[str, Any]],
        profiles: list[tuple[str, dict[str, Any]]],
        anchor_stats: AnchorStats,
    ) -> None:
        self.string_table = string_table
        self.profiles = profiles
        self.anchor_stats = anchor_stats
        self.entries: list[dict[str, Any]] = []
        self.unnamed_ancestor_ids: set[int] = set()
        self.root_anchor: tuple[float, float] = (0.0, 0.0)
        self.root_fields: dict[str, Any] = {}

    def walk(
        self,
        node: Any,
        depth: int,
        named_path: list[tuple[str, float, float]],
        sibling_index: int,
        ancestor_siblings: list[list[Any]],
    ) -> None:
        if not isinstance(node, dict):
            return
        wtype = node.get("type")
        real_name = node.get("name")
        fields = node.get("fields")
        fields = fields if isinstance(fields, dict) else {}
        display_name = real_name if real_name else f"{wtype or 'Widget'}#{sibling_index}"
        anchor_x, anchor_y = resolve_anchor(fields.get("anchor"), self.profiles, self.anchor_stats)

        if depth == 0:
            # Panel root: never an entry, never contributes to the path --
            # only its own anchor fraction is recorded.
            self.root_anchor = (anchor_x, anchor_y)
            self.root_fields = fields
            child_named_path = named_path
        else:
            if real_name:
                child_named_path = named_path + [(real_name, anchor_x, anchor_y)]
            else:
                self.unnamed_ancestor_ids.add(id(node))
                child_named_path = named_path

            if is_focusable(fields, wtype):
                path = child_named_path if real_name else child_named_path + [(display_name, anchor_x, anchor_y)]
                text_key, text = primary_text(fields, self.string_table)
                tooltip_key = compute_tooltip_key(fields, self.profiles)
                if text_key is not None:
                    label_key = None
                elif wtype == "MiniMenuButtonWidget":
                    label_key = MINI_MENU_LABEL_KEYS.get(fields.get("menu"))
                elif wtype == "ButtonWidget" and tooltip_key is not None:
                    # An icon button the game names by its tooltip (a modal's X:
                    # "Close"), not by the caption that happens to sit beside it.
                    label_key = None
                else:
                    label_key = find_label_key(display_name, node, ancestor_siblings)
                update_target, update_command, update_extra = compute_update_message(fields, wtype)
                states = compute_states(fields, self.profiles, self.root_fields)
                self.entries.append({
                    "name": display_name,
                    "type": wtype or "Widget",
                    "textKey": text_key,
                    "text": text,
                    "labelKey": label_key,
                    "tooltipKey": tooltip_key,
                    "updateTarget": update_target,
                    "updateCommand": update_command,
                    "updateExtra": update_extra,  # report-only, not emitted
                    "states": states,
                    "path": path,
                })

        children = node.get("children")
        if isinstance(children, list):
            next_ancestor_siblings = [children] + ancestor_siblings[:1]
            for idx, child in enumerate(children):
                self.walk(child, depth + 1, child_named_path, idx, next_ancestor_siblings)


def collect_panel_widgets(
    root_node: Any,
    string_table: dict[str, dict[str, Any]],
    profiles: list[tuple[str, dict[str, Any]]],
    anchor_stats: AnchorStats,
) -> tuple[list[dict[str, Any]], tuple[float, float], int]:
    walker = Walker(string_table, profiles, anchor_stats)
    walker.walk(root_node, 0, [], 0, [])
    return walker.entries, walker.root_anchor, len(walker.unnamed_ancestor_ids)


def main() -> int:
    script_path = Path(__file__).resolve()
    default_repo_root = script_path.parent.parent

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo-root", type=Path, default=default_repo_root)
    ap.add_argument("--layouts-dir", type=Path, default=None)
    ap.add_argument("--strings-dir", type=Path, default=None)
    ap.add_argument("--out-header", type=Path, default=None)
    args = ap.parse_args()

    repo_root: Path = args.repo_root
    layouts_dir: Path = args.layouts_dir or (repo_root / "game-data" / "global" / "ui" / "Layouts")
    strings_dir: Path = args.strings_dir or (repo_root / "game-data" / "local" / "lng" / "strings")
    out_header: Path = args.out_header or (repo_root / "src" / "generated" / "panel_widgets.h")

    if not layouts_dir.is_dir():
        print(f"ERROR: layouts dir not found: {layouts_dir}", file=sys.stderr)
        return 1
    if not strings_dir.is_dir():
        print(f"ERROR: strings dir not found: {strings_dir}", file=sys.stderr)
        return 1

    string_table = layout_index.load_string_tables(strings_dir)
    layout_files = list(layout_index.iter_layout_files(layouts_dir))
    profiles = load_anchor_profiles(layouts_dir)
    anchor_stats = AnchorStats()

    tree_cache: dict[Path, Any] = {}
    warnings: list[str] = []

    # panel_name -> {"root": [...], "controller_direct": [Controller/<file>, ...],
    #                "controller_pc": [Controller/PC/<file>, ...]}
    # "Controller/Console/...", "Controller/Console/Switch/..." and
    # "Controller/PC/Handheld/..." are intentionally never bucketed: they are
    # not valid PC widget sources (see module docstring).
    groups: dict[str, dict[str, list[str]]] = {}
    parse_errors = 0

    for path in layout_files:
        rel = str(path.relative_to(layouts_dir)).replace("\\", "/")
        parts = rel.split("/")
        try:
            tree = layout_index.resolve_layout_tree(path, layouts_dir, tree_cache, warnings)
        except Exception as exc:
            print(f"[gen-panel-widgets] ERROR parsing {rel}: {exc}", file=sys.stderr)
            parse_errors += 1
            continue
        if not isinstance(tree, dict):
            continue
        root_name_probe = layout_index.flatten_layout(tree, string_table)
        if not root_name_probe or is_synthetic_root_name(root_name_probe[0]):
            continue
        name = root_name_probe[0]["name"]
        bucket = groups.setdefault(name, {"root": [], "controller_direct": [], "controller_pc": []})
        if parts[0] != "Controller":
            bucket["root"].append(rel)
        elif len(parts) == 2:
            bucket["controller_direct"].append(rel)
        elif len(parts) == 3 and parts[1] == "PC":
            bucket["controller_pc"].append(rel)
        # else: Console / Console/Switch / PC/Handheld -- not a PC source, ignored.

    total_unnamed_ancestors = 0
    panel_entries: dict[str, list[dict[str, Any]]] = {}
    panel_root_anchor: dict[str, tuple[float, float]] = {}

    def tree_for(rel: str) -> Any:
        return tree_cache[(layouts_dir / rel).resolve()]

    for name in sorted(groups):
        bucket = groups[name]
        root_rels = sorted(bucket["root"], key=variant_priority)

        sources: list[Any] = []
        if root_rels:
            sources.append(tree_for(root_rels[0]))
            root_basename = Path(root_rels[0]).name
            direct = next((r for r in bucket["controller_direct"] if Path(r).name == root_basename), None)
            pc = next((r for r in bucket["controller_pc"] if Path(r).name == root_basename), None)
            if direct:
                sources.append(tree_for(direct))
            if pc:
                sources.append(tree_for(pc))  # applied last: PC-specific wins over plain Controller/
        else:
            # No root/HD layout at all: the only valid PC sources are a
            # direct Controller/ or Controller/PC/ file for this same name
            # (a panel that exists only under Console/Switch/Handheld has no
            # PC source and is skipped -- sources stays empty).
            direct_rels = sorted(bucket["controller_direct"], key=variant_priority)
            pc_rels = sorted(bucket["controller_pc"], key=variant_priority)
            if direct_rels:
                sources.append(tree_for(direct_rels[0]))
            elif pc_rels:
                sources.append(tree_for(pc_rels[0]))

        if not sources:
            continue

        merged = sources[0]
        for extra in sources[1:]:
            merged = layout_index._merge_layout_nodes(merged, extra)

        entries, root_anchor, unnamed_count = collect_panel_widgets(merged, string_table, profiles, anchor_stats)
        total_unnamed_ancestors += unnamed_count
        if entries:
            for entry in entries:
                override = WIDGET_LABEL_OVERRIDES.get((name, entry["name"]))
                if override is not None and entry["textKey"] is None:
                    entry["labelKey"] = override or None
            panel_entries[name] = entries
            panel_root_anchor[name] = root_anchor

    out_header.parent.mkdir(parents=True, exist_ok=True)

    lines: list[str] = []
    lines.append(f"// generated by tools/gen_panel_widgets.py from game-data ({GAME_VERSION}) -- do not edit")
    lines.append("#pragma once")
    lines.append("#include <cstdint>")
    lines.append("namespace D2RAccess::Generated {")
    lines.append("struct PathStep { const char* name; float anchorX; float anchorY; };")
    lines.append(
        "struct WidgetEntry { const char* name; const char* type; const char* textKey; const char* text; "
        "const char* labelKey; const char* tooltipKey; const char* updateTarget; const char* updateCommand; "
        "const char* const* states; uint32_t stateCount; const PathStep* path; uint32_t pathLength; };"
    )
    lines.append("struct PanelWidgets { const char* panel; float rootAnchorX; float rootAnchorY; const WidgetEntry* widgets; uint32_t count; };")
    lines.append("")

    def c_str_or_null(s: str | None) -> str:
        return "nullptr" if s is None else f'"{escape_c_string(s)}"'

    def c_float(v: float) -> str:
        return f"{v!r}f"

    used_idents: set[str] = set()
    panel_idents: dict[str, str] = {}

    def unique_ident(base: str) -> str:
        ident = sanitize_ident(base)
        candidate = ident
        n = 2
        while candidate in used_idents:
            candidate = f"{ident}_{n}"
            n += 1
        used_idents.add(candidate)
        return candidate

    for panel_name in sorted(panel_entries):
        panel_ident = unique_ident(panel_name)
        panel_idents[panel_name] = panel_ident
        entries = panel_entries[panel_name]
        widget_lines: list[str] = []
        for i, entry in enumerate(entries):
            path_var = f"Path_{panel_ident}_{i}"
            step_literals = ", ".join(
                f'{{ "{escape_c_string(step_name)}", {c_float(step_x)}, {c_float(step_y)} }}'
                for step_name, step_x, step_y in entry["path"]
            )
            lines.append(f"inline constexpr PathStep {path_var}[] = {{ {step_literals} }};")

            states = entry["states"]
            if states:
                states_var = f"States_{panel_ident}_{i}"
                state_literals = ", ".join(f'"{escape_c_string(s)}"' for s in states)
                lines.append(f"inline constexpr const char* const {states_var}[] = {{ {state_literals} }};")
                states_ref, state_count = states_var, len(states)
            else:
                states_ref, state_count = "nullptr", 0

            widget_lines.append(
                "\t{ "
                f'"{escape_c_string(entry["name"])}", '
                f'"{escape_c_string(entry["type"])}", '
                f'{c_str_or_null(entry["textKey"])}, '
                f'{c_str_or_null(entry["text"])}, '
                f'{c_str_or_null(entry["labelKey"])}, '
                f'{c_str_or_null(entry["tooltipKey"])}, '
                f'{c_str_or_null(entry["updateTarget"])}, '
                f'{c_str_or_null(entry["updateCommand"])}, '
                f"{states_ref}, {state_count}, "
                f"{path_var}, {len(entry['path'])} "
                "},"
            )
        lines.append(f"inline constexpr WidgetEntry Widgets_{panel_ident}[] = {{")
        lines.extend(widget_lines)
        lines.append("};")
        lines.append("")

    lines.append("inline constexpr PanelWidgets PanelCatalog[] = {")
    for panel_name in sorted(panel_entries):
        panel_ident = panel_idents[panel_name]
        count = len(panel_entries[panel_name])
        root_x, root_y = panel_root_anchor[panel_name]
        lines.append(
            f'\t{{ "{escape_c_string(panel_name)}", {c_float(root_x)}, {c_float(root_y)}, '
            f"Widgets_{panel_ident}, {count} }},"
        )
    lines.append("};")
    lines.append("")
    lines.append("}")
    lines.append("")

    out_header.write_text("\r\n".join(lines), encoding="utf-8", newline="")

    all_entries = [e for entries in panel_entries.values() for e in entries]
    total_entries = len(all_entries)
    with_label = [e for e in all_entries if e["labelKey"]]
    with_tooltip = [e for e in all_entries if e["tooltipKey"]]
    with_update = [e for e in all_entries if e["updateTarget"] or e["updateCommand"]]
    with_states = [e for e in all_entries if e["states"]]
    three_part = [e for e in all_entries if e["updateExtra"]]
    update_target_counts = Counter(e["updateTarget"] for e in with_update if e["updateTarget"])
    update_types = sorted({e["type"] for e in with_update})

    print(f"[gen-panel-widgets] {len(layout_files)} layout files scanned ({parse_errors} parse errors)")
    print(f"[gen-panel-widgets] {len(groups)} distinct panel names, {len(panel_entries)} with focusable widgets")
    print(f"[gen-panel-widgets] {total_entries} widget entries total")
    print(f"[gen-panel-widgets] {total_unnamed_ancestors} unnamed-ancestor nodes skipped from paths (counted once per node)")
    print(f"[gen-panel-widgets] {anchor_stats.ref_count} \"$Name\" profile anchor references resolved ({len(anchor_stats.unresolved)} unresolved -> fell back to 0,0)")
    if anchor_stats.unresolved:
        print(f"[gen-panel-widgets] unresolved anchors: {anchor_stats.unresolved}")
    if anchor_stats.other_keys:
        print(f"[gen-panel-widgets] anchor keys other than x/y seen: {sorted(anchor_stats.other_keys)}")
    else:
        print("[gen-panel-widgets] no anchor used a key other than x/y")
    print(f"[gen-panel-widgets] labelKey: {len(with_label)}, tooltipKey: {len(with_tooltip)}, update message: {len(with_update)}, states: {len(with_states)}")
    print(f"[gen-panel-widgets] {len(three_part)} update messages had a 3rd \":\"-part (dropped): {[ (e['name'], e['updateTarget'], e['updateCommand'], e['updateExtra']) for e in three_part ]}")
    print(f"[gen-panel-widgets] updateTarget counts: {dict(sorted(update_target_counts.items()))}")
    print(f"[gen-panel-widgets] widget types carrying an update message: {update_types}")
    print(f"[gen-panel-widgets] wrote {out_header}")

    return 0 if not parse_errors else 2


if __name__ == "__main__":
    raise SystemExit(main())
