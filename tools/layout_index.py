#!/usr/bin/env python3
"""layout_index.py -- Accessibility indexer for Diablo II: Resurrected UI layouts.

Walks the raw layout JSON5-ish files extracted from CASC under
`game-data/global/ui/Layouts/` and the localization string tables under
`game-data/local/lng/strings/`, and produces a machine- and human-readable
index of every widget (its text, tooltips, click/update messages, rect,
filenames, and focus/navigation-related fields) resolved against the
string tables.

Outputs (under `game-data/index/`):
  - `<relative-path-to-layout>.index.json` -- one index file per layout,
    mirroring the source directory structure (root files, `Controller/`,
    `Controller/Console/`, `Controller/Console/Switch/`, `Controller/PC/`,
    `Controller/PC/Handheld/`).
  - `all-widgets.jsonl` -- every widget from every layout, one JSON object
    per line, with a `layout_file` field added.
  - `SUMMARY.md` -- human-readable listing, per layout file (grouped root
    vs. Controller variants), of only the widgets that carry a textString
    (or text) or an onClickMessage, in document order.

Stdlib only. Usage:
    python tools\\layout_index.py
Paths default to being relative to the repository root (the parent of
this `tools/` directory); override with --repo-root / --layouts-dir /
--strings-dir / --out-dir if ever needed.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Iterator

# --------------------------------------------------------------------------
# Tolerant JSON5-ish loader: the layout files use trailing commas before
# `}`/`]` and both `//` and `/* */` comments. Comments and trailing commas
# are stripped with a small string-aware scanner (not blind regexes) so we
# never mangle a comment-like sequence that happens to live inside a
# string value (e.g. a filename containing "//").
# --------------------------------------------------------------------------


def _strip_json5_comments(text: str) -> str:
    out: list[str] = []
    i = 0
    n = len(text)
    in_string = False
    escape = False
    while i < n:
        ch = text[i]
        if in_string:
            out.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            i += 1
            continue

        if ch == '"':
            in_string = True
            out.append(ch)
            i += 1
            continue

        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            # Line comment: skip to end of line (keep the newline).
            j = text.find("\n", i)
            if j == -1:
                break
            i = j
            continue

        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            i = n if j == -1 else j + 2
            continue

        out.append(ch)
        i += 1
    return "".join(out)


_TRAILING_COMMA_RE = re.compile(r",(\s*[}\]])")


def _strip_trailing_commas(text: str) -> str:
    # Trailing commas can be nested, so repeat until stable.
    prev = None
    while prev != text:
        prev = text
        text = _TRAILING_COMMA_RE.sub(r"\1", text)
    return text


def load_json5ish(path: Path) -> Any:
    raw = path.read_text(encoding="utf-8-sig")
    cleaned = _strip_trailing_commas(_strip_json5_comments(raw))
    return json.loads(cleaned)


# --------------------------------------------------------------------------
# String tables
# --------------------------------------------------------------------------


def load_string_tables(strings_dir: Path) -> dict[str, dict[str, Any]]:
    """Load every *.json string table in strings_dir into one Key -> record
    map. Each record gets a "_table" field set to the source file's stem.
    ui-controller.json entries win over any other table on key collision
    (per spec); files are otherwise processed in alphabetical order.
    """
    table_files = sorted(p for p in strings_dir.glob("*.json") if p.is_file())
    ordered = [p for p in table_files if p.name != "ui-controller.json"]
    controller = [p for p in table_files if p.name == "ui-controller.json"]

    merged: dict[str, dict[str, Any]] = {}
    for path in ordered + controller:
        try:
            records = json.loads(path.read_text(encoding="utf-8-sig"))
        except Exception as exc:  # pragma: no cover - diagnostic path
            print(f"[strings] WARN: failed to parse {path}: {exc}", file=sys.stderr)
            continue
        if not isinstance(records, list):
            print(f"[strings] WARN: {path} is not an array, skipping", file=sys.stderr)
            continue
        for rec in records:
            key = rec.get("Key")
            if key is None:
                continue
            rec = dict(rec)
            rec["_table"] = path.stem
            merged[key] = rec
    return merged


def resolve_string_ref(raw: Any, string_table: dict[str, dict[str, Any]]) -> dict[str, Any]:
    """Resolve a textString/tooltipString-like raw value. If it starts with
    '@' it's a string-table key reference; otherwise it's a literal.
    Returns a dict with raw/key/enUS/esES/table/literal. Lists of such
    values (e.g. a TabBarWidget's "textStrings") are resolved element-wise.
    """
    if isinstance(raw, list):
        resolved = [resolve_string_ref(item, string_table) for item in raw]
        return {
            "raw": raw,
            "key": [r["key"] for r in resolved],
            "enUS": [r["enUS"] for r in resolved],
            "esES": [r["esES"] for r in resolved],
            "table": [r["table"] for r in resolved],
            "literal": False,
        }
    if not isinstance(raw, str):
        return {"raw": raw, "key": None, "enUS": None, "esES": None, "table": None, "literal": True}
    if raw.startswith("@"):
        key = raw[1:]
        rec = string_table.get(key)
        if rec is None:
            return {"raw": raw, "key": key, "enUS": None, "esES": None, "table": None, "literal": False}
        return {
            "raw": raw,
            "key": key,
            "enUS": rec.get("enUS"),
            "esES": rec.get("esES"),
            "table": rec.get("_table"),
            "literal": False,
        }
    return {"raw": raw, "key": None, "enUS": raw, "esES": raw, "table": None, "literal": True}


# --------------------------------------------------------------------------
# Widget flattening
# --------------------------------------------------------------------------

FOCUS_NAV_KEYS = {
    "navigation",
    "focusIndicatorFilename",
    "focusIndicatorOffset",
    "focusIndicatorPadding",
    "focusIndicatorVisible",
    "focusOnMouseOver",
    "defaultWidget",
    "defaultTab",
    "selectorName",
    "selected",
    "selectInactiveItems",
    "onSelect",
    "onUnselect",
    "generateFocusLinks",
    "navigationWrapsList",
    "hasNestedFocusableWidgets",
    "standaloneFocusIndicatorPadding",
}


def _collect_suffix_fields(fields: dict, suffix: str, exclude: set[str] = frozenset()) -> dict[str, Any]:
    """Recursively collect every key (at any nesting depth within `fields`)
    whose name contains `suffix`, keyed by a dotted path from `fields`.
    """
    found: dict[str, Any] = {}

    def walk(node: Any, prefix: str) -> None:
        if isinstance(node, dict):
            for k, v in node.items():
                path = f"{prefix}.{k}" if prefix else k
                if suffix in k and k not in exclude:
                    found[path] = v
                if isinstance(v, (dict, list)):
                    walk(v, path)
        elif isinstance(node, list):
            for idx, v in enumerate(node):
                walk(v, f"{prefix}[{idx}]")

    walk(fields, "")
    return found


def _collect_focus_nav_fields(fields: dict) -> dict[str, Any]:
    found: dict[str, Any] = {}
    for k, v in fields.items():
        if k in FOCUS_NAV_KEYS:
            found[k] = v
    return found


def _extract_rect(fields: dict) -> Any:
    rect = fields.get("rect")
    if rect is None:
        return None
    if isinstance(rect, dict):
        return {k: rect.get(k) for k in ("x", "y", "width", "height") if k in rect}
    # Style ref (e.g. "$OptionsPanelRect") -- keep raw.
    return {"styleRef": rect}


def flatten_layout(root: Any, string_table: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    widgets: list[dict[str, Any]] = []
    order_counter = {"n": 0}

    def visit(node: Any, depth: int, parent_path: str, sibling_index: int) -> None:
        if not isinstance(node, dict):
            return
        wtype = node.get("type")
        name = node.get("name") or f"{wtype or 'Widget'}#{sibling_index}"
        path = f"{parent_path}/{name}" if parent_path else name
        fields = node.get("fields") or {}
        if not isinstance(fields, dict):
            fields = {}

        # Primary text: prefer "textString", fall back to "text".
        text_field = None
        text_raw = None
        if "textString" in fields:
            text_field = "textString"
            text_raw = fields.get("textString")
        elif "text" in fields:
            text_field = "text"
            text_raw = fields.get("text")
        text_resolved = resolve_string_ref(text_raw, string_table) if text_field else None

        # Primary tooltip: prefer "tooltipString", fall back to "tooltip".
        tooltip_field = "tooltipString" if "tooltipString" in fields else ("tooltip" if "tooltip" in fields else None)
        tooltip_resolved = resolve_string_ref(fields.get(tooltip_field), string_table) if tooltip_field else None

        on_click = fields.get("onClickMessage")

        message_fields_raw = _collect_suffix_fields(fields, "Message")
        string_fields_raw = _collect_suffix_fields(
            fields, "String", exclude={"textString"} if text_field == "textString" else set()
        )
        # Resolve every *String field against the string table for convenience.
        string_fields_resolved = {k: resolve_string_ref(v, string_table) for k, v in string_fields_raw.items()}

        focus_nav_fields = _collect_focus_nav_fields(fields)

        order_counter["n"] += 1
        record = {
            "order": order_counter["n"],
            "depth": depth,
            "path": path,
            "type": wtype,
            "name": name,
            "text_field": text_field,
            "text_raw": text_raw,
            "text_key": text_resolved["key"] if text_resolved else None,
            "text_enUS": text_resolved["enUS"] if text_resolved else None,
            "text_esES": text_resolved["esES"] if text_resolved else None,
            "text_table": text_resolved["table"] if text_resolved else None,
            "tooltip_field": tooltip_field,
            "tooltip_raw": fields.get(tooltip_field) if tooltip_field else None,
            "tooltip_enUS": tooltip_resolved["enUS"] if tooltip_resolved else None,
            "tooltip_esES": tooltip_resolved["esES"] if tooltip_resolved else None,
            "onClickMessage": on_click,
            "rect": _extract_rect(fields),
            "filename": fields.get("filename"),
            "focusIndicatorFilename": fields.get("focusIndicatorFilename"),
            "message_fields": message_fields_raw,
            "string_fields": string_fields_resolved,
            "focus_nav_fields": focus_nav_fields,
        }
        widgets.append(record)

        children = node.get("children")
        if isinstance(children, list):
            for idx, child in enumerate(children):
                visit(child, depth + 1, path, idx)

    visit(root, 0, "", 0)
    return widgets


# --------------------------------------------------------------------------
# "basedOn" inheritance: many Controller-variant layouts are diffs against
# a base layout ("basedOn": "<path relative to Layouts/>"), overriding only
# the fields that change (e.g. adding focusIndicatorFilename/navigation)
# and leaving textString/onClickMessage/etc. to be inherited unchanged.
# Resolving this is required for accurate Controller-vs-root comparisons:
# without it, a Controller variant that only adds navigation hints looks
# like it has no text/click widgets at all.
# --------------------------------------------------------------------------


def _merge_layout_nodes(base: Any, override: Any) -> Any:
    if not isinstance(base, dict) or not isinstance(override, dict):
        return override if override is not None else base

    merged = dict(base)
    if "type" in override:
        merged["type"] = override["type"]
    if "name" in override:
        merged["name"] = override["name"]

    base_fields = base.get("fields") if isinstance(base.get("fields"), dict) else {}
    override_fields = override.get("fields") if isinstance(override.get("fields"), dict) else {}
    merged_fields = {**base_fields, **override_fields}
    if merged_fields:
        merged["fields"] = merged_fields
    else:
        merged.pop("fields", None)

    base_children = base.get("children") if isinstance(base.get("children"), list) else []
    override_children = override.get("children") if isinstance(override.get("children"), list) else []
    override_by_name = {
        c.get("name"): c for c in override_children if isinstance(c, dict) and c.get("name")
    }

    merged_children: list[Any] = []
    matched_names: set[str] = set()
    for bc in base_children:
        name = bc.get("name") if isinstance(bc, dict) else None
        if name and name in override_by_name:
            merged_children.append(_merge_layout_nodes(bc, override_by_name[name]))
            matched_names.add(name)
        else:
            merged_children.append(bc)

    for oc in override_children:
        name = oc.get("name") if isinstance(oc, dict) else None
        if name is None or name not in matched_names:
            if name is None or all(
                not (isinstance(bc, dict) and bc.get("name") == name) for bc in base_children
            ):
                merged_children.append(oc)

    if merged_children:
        merged["children"] = merged_children
    else:
        merged.pop("children", None)

    return merged


def resolve_layout_tree(
    path: Path,
    layouts_dir: Path,
    cache: dict[Path, Any],
    warnings: list[str],
    visiting: set[Path] | None = None,
) -> Any:
    """Load `path` and, if it declares "basedOn", recursively merge it onto
    its base layout. Results are cached by resolved path (base layouts get
    reused across many Controller variants).
    """
    resolved = path.resolve()
    if resolved in cache:
        return cache[resolved]
    if visiting is None:
        visiting = set()
    if resolved in visiting:
        warnings.append(f"basedOn cycle detected at {path}")
        return None
    visiting = visiting | {resolved}

    data = load_json5ish(path)
    based_on = data.get("basedOn") if isinstance(data, dict) else None
    if based_on:
        base_path = layouts_dir / based_on
        if not base_path.is_file():
            warnings.append(f"{path}: basedOn '{based_on}' not found, using override as-is")
            merged = data
        else:
            base_tree = resolve_layout_tree(base_path, layouts_dir, cache, warnings, visiting)
            merged = _merge_layout_nodes(base_tree, data) if base_tree is not None else data
    else:
        merged = data

    cache[resolved] = merged
    return merged


# --------------------------------------------------------------------------
# Driver
# --------------------------------------------------------------------------


def iter_layout_files(layouts_dir: Path) -> Iterator[Path]:
    yield from sorted(layouts_dir.rglob("*.json"))


def build_group(rel_parts: tuple[str, ...]) -> str:
    """Classify a layout's relative path into a report group (Root, or
    Controller/<sub-path> for the Controller variant trees)."""
    dirs = rel_parts[:-1]
    if not dirs:
        return "Root"
    return "Controller/" + "/".join(dirs[1:]) if dirs[0] == "Controller" and len(dirs) > 1 else "/".join(dirs)


def main() -> int:
    script_path = Path(__file__).resolve()
    default_repo_root = script_path.parent.parent

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo-root", type=Path, default=default_repo_root)
    ap.add_argument("--layouts-dir", type=Path, default=None)
    ap.add_argument("--strings-dir", type=Path, default=None)
    ap.add_argument("--out-dir", type=Path, default=None)
    args = ap.parse_args()

    repo_root: Path = args.repo_root
    layouts_dir: Path = args.layouts_dir or (repo_root / "game-data" / "global" / "ui" / "Layouts")
    strings_dir: Path = args.strings_dir or (repo_root / "game-data" / "local" / "lng" / "strings")
    out_dir: Path = args.out_dir or (repo_root / "game-data" / "index")

    if not layouts_dir.is_dir():
        print(f"ERROR: layouts dir not found: {layouts_dir}", file=sys.stderr)
        return 1
    if not strings_dir.is_dir():
        print(f"ERROR: strings dir not found: {strings_dir}", file=sys.stderr)
        return 1

    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"[strings] loading tables from {strings_dir}")
    string_table = load_string_tables(strings_dir)
    print(f"[strings] loaded {len(string_table)} keys")

    layout_files = list(iter_layout_files(layouts_dir))
    print(f"[layouts] found {len(layout_files)} layout files under {layouts_dir}")

    all_widgets_path = out_dir / "all-widgets.jsonl"
    summary_path = out_dir / "SUMMARY.md"

    total_widgets = 0
    parse_errors: list[tuple[Path, str]] = []
    basedon_warnings: list[str] = []
    tree_cache: dict[Path, Any] = {}
    # group -> list of (rel_path_str, widgets, all_widgets)
    per_layout: list[tuple[Path, str, list[dict]]] = []

    with all_widgets_path.open("w", encoding="utf-8") as jsonl_out:
        for layout_path in layout_files:
            rel = layout_path.relative_to(layouts_dir)
            rel_str = str(rel).replace("\\", "/")
            try:
                raw_data = load_json5ish(layout_path)
                based_on = raw_data.get("basedOn") if isinstance(raw_data, dict) else None
                data = resolve_layout_tree(layout_path, layouts_dir, tree_cache, basedon_warnings)
            except Exception as exc:
                parse_errors.append((layout_path, str(exc)))
                print(f"[layouts] ERROR parsing {rel_str}: {exc}", file=sys.stderr)
                continue

            widgets = flatten_layout(data, string_table)
            total_widgets += len(widgets)

            # Per-layout index file, mirroring directory structure.
            index_out_path = out_dir / (rel_str + ".index.json")
            index_out_path.parent.mkdir(parents=True, exist_ok=True)
            index_out_path.write_text(
                json.dumps(
                    {
                        "layout_file": rel_str,
                        "based_on": based_on,
                        "widget_count": len(widgets),
                        "widgets": widgets,
                    },
                    ensure_ascii=False,
                    indent=2,
                ),
                encoding="utf-8",
            )

            for w in widgets:
                rec = dict(w)
                rec["layout_file"] = rel_str
                jsonl_out.write(json.dumps(rec, ensure_ascii=False) + "\n")

            per_layout.append((layout_path, rel_str, widgets))

    # ---- SUMMARY.md -------------------------------------------------
    def sort_key(item: tuple[Path, str, list[dict]]):
        _, rel_str, _ = item
        parts = rel_str.split("/")
        group = build_group(tuple(parts))
        return (group != "Root", group, rel_str)

    per_layout_sorted = sorted(per_layout, key=sort_key)

    lines: list[str] = []
    lines.append("# D2R UI Layout Index -- Interactive/Text Widget Summary\n")
    lines.append(
        f"Generated from {len(layout_files)} layout files "
        f"({len(parse_errors)} parse errors) and {len(string_table)} string-table keys.\n"
    )
    if parse_errors:
        lines.append("## Parse errors\n")
        for p, err in parse_errors:
            lines.append(f"- `{p.relative_to(layouts_dir)}`: {err}")
        lines.append("")

    current_group = None
    for _, rel_str, widgets in per_layout_sorted:
        parts = rel_str.split("/")
        group = build_group(tuple(parts))
        if group != current_group:
            heading = "## Root Layouts" if group == "Root" else f"## Controller Variants -- {group}"
            lines.append(f"\n{heading}\n")
            current_group = group

        interesting = [w for w in widgets if w["text_raw"] not in (None, "") or w["onClickMessage"]]
        lines.append(f"### {rel_str}  ({len(interesting)}/{len(widgets)} widgets shown)\n")
        if not interesting:
            lines.append("_(no text/onClickMessage widgets)_\n")
            continue
        for w in interesting:
            text_bit = ""
            if w["text_raw"] not in (None, ""):
                es = w["text_esES"]
                es_disp = es if es not in (None, "") else "(unresolved)"
                text_bit = f' esES: "{es_disp}"'
            click_bit = f" onClick: `{w['onClickMessage']}`" if w["onClickMessage"] else ""
            lines.append(f"- **{w['name']}** (`{w['type']}`, depth {w['depth']}){text_bit}{click_bit}")
        lines.append("")

    summary_path.write_text("\n".join(lines), encoding="utf-8")

    print(f"[out] wrote {len(per_layout)} per-layout index files under {out_dir}")
    print(f"[out] wrote {all_widgets_path} ({total_widgets} widgets total)")
    print(f"[out] wrote {summary_path}")
    if basedon_warnings:
        print(f"[layouts] {len(basedon_warnings)} basedOn warnings:", file=sys.stderr)
        for w in basedon_warnings:
            print(f"  {w}", file=sys.stderr)
    if parse_errors:
        print(f"[out] {len(parse_errors)} layout files failed to parse (see stderr above)")

    return 0 if not parse_errors else 2


if __name__ == "__main__":
    raise SystemExit(main())
