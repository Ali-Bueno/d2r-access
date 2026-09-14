#!/usr/bin/env python3
"""gen_panel_titles.py -- Generates src/generated/panel_titles.h, a table
mapping a runtime panel name (the string the game sends in
"PanelManager:OpenPanel:<name>" UI messages) to the localization KEY of that
panel's human-readable title, so the plugin can resolve it in the player's
own language via the SDK's LocalizationService::getStringByKey at runtime.

Mapping rule (verified against the layout data, see module docstring in
tools/layout_index.py for the underlying JSON5-ish parser this reuses):
  - A layout file's ROOT widget "name" *is* the runtime panel name (e.g.
    SettingsPanelHD.json's root widget is named "SettingsPanel", which is
    exactly the <name> in "PanelManager:OpenPanel:SettingsPanel"). This holds
    for every real widget-tree layout; only the "_profile*.json" style/skin
    dictionaries (which have no root widget name) don't fit the rule, and
    are skipped.
  - Multiple layout files can share the same root name (a root/PC layout and
    a "HD" variant, and sometimes a Controller/ variant). The HD, non-
    Controller layout is preferred as the source of the title; a root/base
    (non-HD) layout is used if no HD one exists; a Controller-only layout
    (e.g. EmoteWheelPanel) is used as a last resort. Any Controller variant
    found alongside the chosen layout is only noted, never used for the
    title itself, per the task instructions.

Title-extraction rule, applied to the chosen layout's widgets in document
order (any depth):
  1. The first widget whose name is exactly "Title", or ends with "Title" or
     "Header" -- but NOT if its name also contains "Name", "Warning" or
     "Requirement" (those are column headers / warning banners, not the
     panel's title: e.g. MainMenuPanel's "UsabilityWarning" and
     KeyBindingMenu's "BindingNameTitle") -- AND whose primary text
     (textString/text field) is a string-table reference (starts with "@")
     that actually resolves in the string table.
  2. Failing that, the panel has no static title and gets no entry (the
     plugin falls back to speaking the raw panel name).

Stdlib only. Usage (run from the repo root):
    python tools\\gen_panel_titles.py
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_index  # noqa: E402  (reuse the JSON5-ish parser / string resolver)

GAME_VERSION = "D2R 3.3.93787"


def is_synthetic_root_name(root: dict[str, Any]) -> bool:
    """True for layout "roots" that are really style/skin dictionaries (e.g.
    the "_profile*.json" files), whose top node has neither a "name" nor a
    "type" and so falls back to flatten_layout's synthetic "Widget#0" name.
    """
    wtype = root.get("type")
    return root.get("name") == f"{wtype or 'Widget'}#0"


# Substrings that disqualify an otherwise-matching "*Title"/"*Header" widget
# name: these are column headers / warning banners, not the panel's title.
TITLE_NAME_EXCLUDE = ("Name", "Warning", "Requirement")


def find_title_widget(widgets: list[dict[str, Any]]) -> dict[str, Any] | None:
    def is_ref(w: dict[str, Any]) -> bool:
        return isinstance(w.get("text_raw"), str) and w["text_raw"].startswith("@")

    def is_candidate_name(name: str) -> bool:
        if not (name == "Title" or name.endswith("Title") or name.endswith("Header")):
            return False
        return not any(x in name for x in TITLE_NAME_EXCLUDE)

    def parent_name(path: str) -> str | None:
        # "KeyBindingMenu/HeaderTable/Row/PrimaryKeyTitle" -> "Row"
        segments = path.split("/")
        return segments[-2] if len(segments) >= 2 else None

    for w in widgets:
        name = w.get("name") or ""
        if not (is_ref(w) and is_candidate_name(name) and w.get("text_enUS") is not None):
            continue
        # A "*Title"/"*Header"-named widget whose immediate parent is a "Row"
        # is a repeated table/list column header (e.g. KeyBindingMenu's
        # "HeaderTable/Row/PrimaryKeyTitle"), not the panel's own title.
        if parent_name(w.get("path", "")) == "Row":
            continue
        return w
    return None


def variant_priority(rel_str: str, is_controller: bool) -> tuple[int, int, str]:
    is_hd = Path(rel_str).stem.endswith("HD")
    return (1 if is_controller else 0, 0 if is_hd else 1, rel_str)


def escape_c_string(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def clean_comment_text(s: str | None) -> str:
    if not s:
        return "(unresolved)"
    return " ".join(s.split())


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
    out_header: Path = args.out_header or (repo_root / "src" / "generated" / "panel_titles.h")

    if not layouts_dir.is_dir():
        print(f"ERROR: layouts dir not found: {layouts_dir}", file=sys.stderr)
        return 1
    if not strings_dir.is_dir():
        print(f"ERROR: strings dir not found: {strings_dir}", file=sys.stderr)
        return 1

    string_table = layout_index.load_string_tables(strings_dir)
    layout_files = list(layout_index.iter_layout_files(layouts_dir))

    tree_cache: dict[Path, Any] = {}
    warnings: list[str] = []

    # panel_name -> list of (rel_str, is_controller, widgets)
    groups: dict[str, list[tuple[str, bool, list[dict[str, Any]]]]] = {}
    parse_errors = 0

    for path in layout_files:
        rel = str(path.relative_to(layouts_dir)).replace("\\", "/")
        is_controller = rel.startswith("Controller/")
        try:
            tree = layout_index.resolve_layout_tree(path, layouts_dir, tree_cache, warnings)
            widgets = layout_index.flatten_layout(tree, string_table)
        except Exception as exc:
            print(f"[gen-panel-titles] ERROR parsing {rel}: {exc}", file=sys.stderr)
            parse_errors += 1
            continue
        if not widgets:
            continue
        root = widgets[0]
        if is_synthetic_root_name(root):
            continue
        groups.setdefault(root["name"], []).append((rel, is_controller, widgets))

    total_panels = len(groups)
    entries: list[tuple[str, str, str, str, str]] = []  # name, key, enUS, esES, source_rel
    controller_only: list[str] = []
    has_controller_note: list[str] = []
    skipped_no_title: list[str] = []

    for name in sorted(groups):
        variants = groups[name]
        variants_sorted = sorted(variants, key=lambda v: variant_priority(v[0], v[1]))
        primary_rel, primary_is_controller, primary_widgets = variants_sorted[0]

        if all(v[1] for v in variants):
            controller_only.append(name)
        elif any(v[1] for v in variants) and not primary_is_controller:
            has_controller_note.append(name)

        title_widget = find_title_widget(primary_widgets)
        # A key that doesn't resolve in the string table (typo, or a
        # composite/format value that isn't a plain key reference) would
        # fail the same getStringByKey lookup at runtime, so it is not a
        # usable static title either.
        if title_widget is None or not title_widget.get("text_key") or title_widget.get("text_enUS") is None:
            skipped_no_title.append(name)
            continue

        key = title_widget["text_key"]
        enUS = clean_comment_text(title_widget.get("text_enUS"))
        esES = clean_comment_text(title_widget.get("text_esES"))
        entries.append((name, key, enUS, esES, primary_rel))

    out_header.parent.mkdir(parents=True, exist_ok=True)

    lines: list[str] = []
    lines.append(f"// generated by tools/gen_panel_titles.py from game-data ({GAME_VERSION}) -- do not edit")
    lines.append("//")
    lines.append("// Maps a runtime panel name (the <name> in a \"PanelManager:OpenPanel:<name>\"")
    lines.append("// UI message) to the localization KEY of that panel's title, resolved via")
    lines.append("// Localization::Text(key) / the SDK's getStringByKey at runtime. Panels with no")
    lines.append("// static title (dynamic/dismissable dialogs) have no entry here; callers should")
    lines.append("// fall back to the raw panel name in that case.")
    lines.append("#pragma once")
    lines.append("")
    lines.append("namespace D2RAccess::Generated {")
    lines.append("")
    lines.append("struct PanelTitle {")
    lines.append("\tconst char* panel;")
    lines.append("\tconst char* stringKey;")
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr PanelTitle PanelTitles[] = {")
    for name, key, enUS, esES, _rel in entries:
        lines.append(
            f'\t{{ "{escape_c_string(name)}", "{escape_c_string(key)}" }}, // {esES} / {enUS}'
        )
    lines.append("};")
    lines.append("")
    lines.append("}")
    lines.append("")

    out_header.write_text("\r\n".join(lines), encoding="utf-8", newline="")

    print(f"[gen-panel-titles] {len(layout_files)} layout files scanned ({parse_errors} parse errors)")
    print(f"[gen-panel-titles] {total_panels} distinct panel names found")
    print(f"[gen-panel-titles] {len(entries)} panels got a title, {len(skipped_no_title)} did not")
    print(f"[gen-panel-titles] wrote {out_header}")
    if has_controller_note:
        print(f"[gen-panel-titles] {len(has_controller_note)} panels also have a Controller/ variant (same title used): {', '.join(sorted(has_controller_note))}")
    if controller_only:
        print(f"[gen-panel-titles] {len(controller_only)} panels exist only as Controller/ layouts: {', '.join(sorted(controller_only))}")
    if skipped_no_title:
        print(f"[gen-panel-titles] panels with no static title (skipped): {', '.join(sorted(skipped_no_title))}")

    return 0 if not parse_errors else 2


if __name__ == "__main__":
    raise SystemExit(main())
