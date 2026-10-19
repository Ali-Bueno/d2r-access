# Accessibility Modding — Playbook (CLAUDE.md)

> The manual for building accessibility mods (screen reader via **PRISM** + audio cues), applied game
> by game, section by section. It rides on every request, so it holds only routing and hard rules; the
> *how* lives in [the reference library](reference/README.md) and the engineering ground rules in
> [`PRINCIPLES.md`](PRINCIPLES.md). Other docs cite these sections by number (§1–§12): keep them stable.

## Goal

Make **Diablo II: Resurrected** completable start to finish by a blind player, with the same mechanical and
narrative experience as a sighted player: screen-reader output through **PRISM**
(https://github.com/ethindp/prism), audio cues **only** where text can't convey spatial orientation or
immediate feedback, gameplay faithful to the original, and clean, modular code.

## 1. Development flow

Build section by section, testing with the screen reader at every step:

0. **Open the mod's `STATUS.md` first** (§11); on a fresh mod, create it from
   [the status template](reference/_mod-status-template.md).
1. Identify the engine and the modding framework (§2).
2. Identify the critical screens and states, then the methods/data to hook or read.
3. **Adapter + speech first:** `Speak("test", interrupt)` through PRISM plus one simple state
   (position/heading) on a key — this validates the whole pipeline.
4. Minimal hooks (no logic) that delegate to accessibility services.
5. Test with the screen reader and tune verbosity and timing; add audio cues only where they add value.

Navigation mods: section order and per-section reading list in
[the audio-navigation guide](reference/audio-navigation/README.md).

## 2. Investigate the engine BEFORE assuming anything

**Do NOT assume the modding framework.** Identify the engine and framework of THIS game before writing a
line; if it is still unclear after investigating, **ask the user**. Match the clue in the install
folder, then read [the SR integration index](reference/screen-reader-integration/README.md) plus the
docs in the last column:

| Clue in install folder | Engine | Framework | SR transport | Read before coding |
|---|---|---|---|---|
| Native C/C++/VB6 exe, no managed runtime | native | **D2RLoader + PluginSDK** | native hook `LoadLibrary("prism.dll")` | — |

- The adapter contract is identical everywhere (§4); native engines use their framework's own hooks.

## 3. Dependencies

The framework per the §2 table, plus **PRISM** — the only screen-reader
library on every target (no Tolk fallback). Its official bindings are **Prismatoid**: .NET (NuGet,
targets **.NET 10**), Python
(PyPI, for tooling) and Godot; the core is a stable C API.

## 4. PRISM — integrating the prebuilt RELEASE

Download `prism-windows-x64.zip`, `LoadLibrary("prism.dll")` + `GetProcAddress` by name (x64 exports are
undecorated) and ship **only** `prism.dll` next to the mod: the release's `tolk.dll` is not needed
(NVDA/JAWS/SAPI are built in). No x86 build exists. The verified details — exports, runtime deps, the
`prism_init → … → prism_shutdown` sequence, what to ship — are in
[prism-prebuilt.md](reference/screen-reader-integration/prism-prebuilt.md); read it once per mod and
don't re-investigate or compile the PRISM repo.

## 5. Reference library and section-by-section build

Read the relevant section of [the reference library](reference/README.md) before implementing it.

- **Audio navigation** — [the guide](reference/audio-navigation/README.md) has the decision guide, the
  per-section reading list and the convention differences behind mirrored-direction bugs. Pick one or
  more styles: reactive radar ([`a-heros-call/`](reference/audio-navigation/a-heros-call/), doc 02),
  polar scan radar ([`swamp/navigation-spec.md`](reference/audio-navigation/swamp/navigation-spec.md)
  §3), wall sonification ([`wall-sonification/`](reference/audio-navigation/wall-sonification/)). An
  obstacle is anything impassable: walls, rocks, trees, invisible barriers.
- **UI** — §8.
- **Studying a new engine:** copy [the engine-study template](reference/_engine-study-template.md) into
  `reference/<topic>/<game>/`, identify the engine first (§2), and note confidence per section.

## 6. Code architecture and best practices

- Decoupled services: `AccessibilityManager`, `ScreenReaderService` (PRISM), `AudioCueService`,
  `GameStateTracker`. **No accessibility logic inside patches** — they delegate to a service.
- **Engine adapter:** accessibility logic is written against a minimal interface (`PlayerPosition`,
  `HeadingDegrees`, `Raycast(...)`, `Speak(...)`, `PlaySound(...)`), never against the engine.
- Every feature toggles on/off. Accessible constants and strings live in one layer; offsets and names
  are derived from real game data, never hardcoded (PRINCIPLES §5).

## 7. Patches / Hooks

Hook **only** what you need and never alter the original logic unless strictly necessary. Use the
framework's own hooks, same minimalism. Every hook delegates to an accessibility service.

## 8. Accessibilizing UI (menus, inventories)

Read [the generic UI strategy](reference/ui-accessibility/generic-strategy.md) **first**. Classify
**widgets, never screens**: reusable archetype readers cover every screen (unseen ones included),
per-screen overrides only for genuine quirks, a generic tree-walk fallback and fault isolation. **Never
implement UI screen by screen.** Menus are a fixed-cost phase: build the shared pipeline once, validate
on 2–3 representative screens, sweep the critical path — 100% readable in the least time.

- **"accessibilize menus" / "accesibilizar menús"** → [the menus spec](reference/ui-accessibility/menus.md)
  (entering screens, tabs, vertical lists, sliders/checkboxes/dropdowns, popups and confirmations).
- **"accessibilize the inventory" / "accesibilizar inventario"** →
  [the inventories spec](reference/ui-accessibility/inventories.md) (item readout, subsections, crafting
  have/needed); builds on the menu rules.
- Context change → `interrupt=true`; a value change reads only what changed; follow the on-screen order.

## 9. Speech, audio cues and state tracking

- **Speech:** init PRISM on load, release on shutdown, log the backend (`prism_registry_name`). One sink
  for all output, so a backend swap or a test mock happens in one place. Concise announcements;
  `interrupt=true` only on context change (new menu, region, state), `false` for incremental info.
- **Audio cues:** only where text has no clear equivalent or spatial/immediate feedback is needed;
  simple, distinguishable, reusable. **Silence all cues during NPC dialogue and menus that pause the
  game** (keep them running if that menu doesn't pause it) and resume on close, driven from the
  centralized state — the same way the wall-sonification bed goes quiet in menus.
- **State:** one centralized accessible state (menu, submenu, combat, interaction/dialogue).
  **Diff-gate:** announce only real changes. All spoken text in one translatable layer.

## 10. Conventions, logging and publishing

- Code, comments, logs and commits in **English**; short commits (`Add screen reader support for main menu`).
- Framework logger for init, important states and patch errors; **never log every frame**.
- **Project knowledge goes in the repo, never in assistant memory** (auto-memory included):
  [PRINCIPLES §3](PRINCIPLES.md#project-knowledge-lives-in-the-repository). Mods built from this template
  and how to sync them: [`MODS.md`](<D:/code/modding projects/MODS.md>).
- Plan for game updates: no fragile offsets or names, constants centralized.
- Publishing: English `README.md` (what is accessible, requirements: framework, PRISM, screen reader); no
  unnecessary binaries; **no releases unless the user asks**.

## 11. Per-mod status ledger

Each mod keeps a short `STATUS.md` at its root ([template](reference/_mod-status-template.md)): identity
(from the §2 table), a section table (`done`/`wip`/`todo`), derived facts with their source so nothing is
re-RE'd, a *Code map*, the single next step and known issues. Open it first; update the next step and the
table whenever a chunk is done. A dashboard, not documentation — the *how* lives in `reference/`.

## 12. Working model: expensive model as director

The orchestration rules live in the user-level `~/.claude/CLAUDE.md`, which loads in every repo and wins
on conflict. For modding they mean: the Opus/Fable main session forms the RE hypotheses, decides the
architecture (adapter, services, hooks), writes the critical code and the `STATUS.md` update. Bulk output
— decompiler dumps, game-data/layout sweeps, engine-folder sweeps, build logs, web research — goes to
subagents that return `path:line` / RVA / symbol + conclusion, so the director's context stays small.
Pick the cheapest tier that gets it right: Haiku 5.5+ for simple tasks (build/test errors only, D2RLoader log
filtering, game-data/layout dump sweeps, file and symbol lists); Sonnet for reasoning (call-flow
tracing, subsystem summaries, `reference/` drafts); Opus for the hard separable parts (gnarly decompiled
functions, struct/vtable layouts, crashes and hangs in the injected mod). Independent investigations go
out in parallel in one message.

### Token efficiency

Every message re-sends the whole context, so its size is what costs. Evidence and the available settings:
[token efficiency](reference/token-efficiency/README.md).

- **Short sessions:** one task per session; resume from `STATUS.md` and its *Code map*; `/clear` between
  unrelated tasks.
- **Narrow search:** `Grep`/`Glob` and `Read` with line ranges; broad sweeps go to the Sonnet `Explore`
  agent ([setup](templates/claude-code/README.md)).
- **Delegate only bulk:** a subagent starts with a cold cache and reloads every `CLAUDE.md`; a known
  lookup is cheaper done directly. Always pass `model` — without it the subagent runs on the main model.
- **No code-index tooling:** graphify, Serena and vector-search MCPs were rejected, LSP plugins deferred.
- **Measure** with `/usage` (`w` = last 7 days) and `/context`.
