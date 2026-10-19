# Custom/D2R/d2r-access — memory entry

Complete current retrieval map: [feature/source index](docs/feature-index.md),
[shared runtime contracts](docs/runtime-services.md), and
[2026-10-19 upstream/waypoint/NPC integration](docs/upstream-2026-10-19.md).
Read STATUS and parent HANDOFF for current validation/deployment, rather than
archived test totals below.

2026-10-19: local merge 4ad6316 includes upstream main 7352a4a and preserves
keyboard/navigation/stability work. Localized terror waypoint label and scoped
absent-NPC filtering installed; 124/124 pass. The user then requested every
edit be committed and pushed to origin/bladestorm, including this memory,
instructions, crash report and build script. STATUS records publication scope;
parent HANDOFF records the verified remote result. Actual gameplay/transition
confirmation and client-presence/color-only limits remain in the linked record.

Shared project context and imported topic index: [Custom/D2R/CODEX_MEMORY.md](../CODEX_MEMORY.md).

2026-10-04: rune/gem stash slots now read their localized fixed identity in the
first poll, without item cursor settle; repeated native permission queries and
same-tick menu builds were reduced. Built/deployed, 104/104 plus final 8/8;
live listening pending. [Evidence](docs/navigation-latency.md#rune-and-gem-stash-browsing--2026-10-04).

Ownership: Third-party collaboration; user's local keyboard work.

This is the source entry for that workspace. Read the linked memory once, then only task-relevant topics. Add durable source-specific facts below with a date and evidence; the imported Claude notes are preserved, not revalidated runtime results.

2026-10-03: [architecture guide](docs/architecture.md) maps current source and game
interfaces. [Tal Rasha symbols](docs/tal-rasha-symbols.md) records verified journal
frame/entrance mapping and the visible-image adapter; built/deployed, 96/96 pass,
live speech pending (see parent HANDOFF).

2026-10-03: [additive upstream review](docs/upstream-2026-10-03.md) records
retreat collision evidence/fix and unresolved repair attribution. Full 103/103
suites pass; see STATUS/HANDOFF for deployment and actual loader version.

2026-10-03 follow-up: user explicitly authorized commit/push; `ad7743c` and
`2c61673` are on `origin/bladestorm`, verified at the latter full tip. Local
`bladestorm` fast-forwarded and tracks the remote; working checkout remains
`keyboard-local`, clean. No force push or guard modification.

2026-10-04: `6ddca2e` / `ca2fb67` add native keyboard skill hotkey speech and
correct town NPC unloading/reload regressions from the additive integration.
104/104 pass; staged, awaiting game close. See STATUS and feature documents.

2026-10-04 user follow-up: skill hotkeys, including quick cast, announce once
until a different skill id is used; switching back announces again. No
speech text deduplication or timeout. Implemented in local `ced5352`,
104/104 pass, staged awaiting game close. See HANDOFF/STATUS for deployment.

2026-10-04 deployment follow-up: user authorized deployment, game verified
closed, normal four-job/BelowNormal build completed. Installed plugin
`7FBE2FE2...` matches the build; PRISM verified. No push or launch.
Live skill/NPC checks remain user-led; see HANDOFF/STATUS.

2026-10-04: Alt+A/Alt+D menu clicks built/deployed, 104/104 pass.
Argument-only live check found native respec OFF and plus group hidden.
User then approved loader `enable_respec = true`; installed with game closed,
backup/exact bytes verified, `-enablerespec` retained. User confirmed reset works;
latest log shows plus-button targets and Alt+A on StrengthPointButton. [Controls/setup](docs/keyboard-play.md#offline-respec-and-alt-clicks-2026-10-04);
see STATUS/HANDOFF. Earlier stash work preserved.

2026-10-04: Active-table skill prerequisite names now join J and Ctrl+arrow
descriptions. Built/deployed, 105/105 pass; user confirmed working in game.
[Evidence](docs/progression.md#textual-skill-prerequisites---2026-10-04).

2026-10-04: User-authorized push of all pending project work completed at
`75e454f` on `origin/bladestorm`, independently verified. Local bladestorm
fast-forwarded; keyboard-local remains checked out/clean. 105/105 pass.

2026-10-04: Menu initial placement and Kurast town exploration corrected and
deployed (105/105). Native input mode governs automatic placement; H uses town
visits and reachable entry cells. Live checks pending; see STATUS and feature docs.

2026-10-04: [Act 3-5 routing](docs/exploration.md#act-3-5-routing-corrections---2026-10-04)
keeps measured borders across streaming/revisits (archive v4, backward reader),
routes H to any room floor and preserves logical-room holes/native warp targets.
105/105 pass; deployed with game closed; live checks pending, see STATUS/HANDOFF.

2026-10-04: User clarified unexplored territory explains the Flayer-route stop;
absolute "No way" speech is misleading. Implemented "No known route" in all
locales/maps and all-neighbor shared-border sections. Test/deploy status and
evidence limits live in STATUS and docs/exploration.md.

2026-10-04 follow-up: user reports all current behavior working as intended and
authorized deploying the staged routing/text update and committing/pushing all
project updates to origin/bladestorm. Deployment verified closed, 105/105 plus
8/8, unchanged PRISM/config/archive; see STATUS/HANDOFF for final Git results.

2026-10-04 deployment/push completed: menus 6201df6, routing 7f2824f, docs
bc31b60 pushed to origin/bladestorm and remotely verified. Local bladestorm
tracks the same tip; keyboard-local remains clean/current. Main, pre-push guard
and machine-specific build script unchanged; records/backups kept locally.

2026-10-05: User confirmed Kurast NPC/floor exploration fixes work in game.
Commit 8077d2c pushed to origin/bladestorm and independently verified; local
bladestorm fast-forwarded. Keyboard-local remains clean; main, pre-push guard
and machine-specific build.ps1 unchanged. Evidence/deployment: STATUS/HANDOFF
and entity-scan/exploration docs; records re/kurast-routing-rootcause-2026-10-05/.

## Engineering and model policy

Use [project engineering/delegation instructions](AGENTS.override.md#engineering-and-model-delegation) and the [shared model policy](../../../codex/MODEL_POLICY.md) for Sol-first ownership, bounded low-risk Luna implementation with a clear Sol handoff/review, and emergency-only Astra analysis after at least two distinct unsuccessful Sol approaches to a specific genuinely unfamiliar blocker. Subject matter/complexity alone never qualifies; Sol/Astra cooperation is reserved for extreme RE whose code Sol cannot understand after those attempts. Sol performs the overwhelming majority of work and owns implementation/validation. Record important discoveries and their reasoning in this memory or linked project documentation so later agents can reuse the investigation.

Summarize each delegation to another model when it finishes. Give the user a concise tracking summary naming the model, assigned task, outcome, review/validation and remaining issues. Include failed, interrupted or inconclusive outcomes honestly; retain durable findings in project documentation linked from memory.

2026-10-10: Allocation/UI-callback and process-exit worker faults repaired,
108/108 offline tests pass and verified DLL installed. Gameplay pending.
[Stability contract/tests](docs/stability.md) and parent HANDOFF contain current
hash, push authorization/result and local crash-evidence pointer. User intends
to play during compiler work; preserve bounded concurrency and validate each
decompiler result rather than accepting failed/partial output.

2026-10-10 push completed: 4c88812 stability plus separate existing continuous-H
commit 891da55 are on origin/bladestorm; full remote tip verified. No force push
or guard change. Private crash records and local instructions remain local.
