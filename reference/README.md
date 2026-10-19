# Reference Library

Accumulated knowledge for our accessibility mods, written engine-agnostically so it ports to new
targets. Two kinds of material live here:

- **Reverse-engineering studies** — how real games implement the systems we want to reproduce
  (e.g. `audio-navigation/`).
- **Behavior specs** — prescriptive rules for how *our* mods must behave (e.g. `ui-accessibility/`).

This is reference material, not project code. The playbook that *uses* it is [the root CLAUDE.md](../CLAUDE.md).

## Layout

```
reference/
├── _engine-study-template.md     ← copy this to start a new game/engine study
├── _mod-status-template.md       ← copy to a mod's repo as STATUS.md (per-mod progress ledger)
├── README.md                     ← this index
└── <topic>/                      ← TECHNIQUE axis: one folder per subject area
    ├── README.md                 ← cross-game synthesis: shared model + which doc covers what
    └── <game-slug>/              ← one folder per game studied…
        └── …docs…                  …or a <technique-slug>/ folder synthesizing several mods/games

```

## Topics

| Topic | What it covers | Sources |
|-------|----------------|---------|
| [Audio navigation](audio-navigation/) | Three navigation styles (reactive radar, polar scan, wall sonification) + coordinate math, raycasting, scanning, zones, beacons, spatial audio, speech | A Hero's Call (.NET/MonoGame decompile), Swamp (native VB6 decompile), Kirandur (NativeAOT decompile), United Minecraft (Fabric/JVM — real GPL source), our own Unity mods — Core Keeper / DRG / Megabonk |
| [UI accessibility](ui-accessibility/) | Prescriptive **behavior specs** (not RE) for in-game UI: menus (entering, tabs, navigation, controls, popup/confirmation dialogs) and inventories (item readout, subsections, crafting); plus a verbatim copy of the third-party **graph navigator** spec (mod-owned parallel UI tree) | Our own accessibility playbook; WH40KRTAccess graph a11y spec (MIT, blind developers, 5 shipping mods) |
| [Token efficiency](token-efficiency/) | How we keep Claude Code usage low: how Max limits are counted, what drives usage (1M window, subagents, always-loaded instructions), the practices and settings we use, and the code-navigation tools evaluated and rejected (graphify, LSP plugins, Serena, vector search) | Anthropic's Claude Code docs and support articles, tool issue trackers and user reports (2026-10-12, updated 2026-10-15) |
| [Screen-reader integration](screen-reader-integration/) | Native **transport** for reaching the OS screen reader (PRISM) via D2RLoader's PluginSDK — the native hook `LoadLibrary`/`GetProcAddress`-loading `prism.dll` directly — and the shared `ScreenReader`/`Announcer` adapter contract | D2RLoader + PluginSDK, native |

## Adding a new engine study

1. Decide the **topic** (reuse an existing folder, or create a new `reference/<topic>/`).
2. Copy [the engine-study template](_engine-study-template.md) into
   `reference/<topic>/<game-slug>/` — one file for a compact study, or a numbered doc set
   (`00-…`, `01-…`) for a large one (see `audio-navigation/a-heros-call/`).
3. Identify the engine **before assuming** the framework (see CLAUDE.md "Investigar el Engine").
4. Fill the template; note **confidence per section** and what wasn't recovered.
5. Update the topic's `README.md` synthesis (shared model + comparison table) and add a row to the
   **Topics** table above.
