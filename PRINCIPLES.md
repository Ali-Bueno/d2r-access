# Engineering & Accessibility Principles

> **What this file is:** the general working principles that sit *underneath* the playbook in
> [`CLAUDE.md`](CLAUDE.md). The playbook tells you *how to build an accessibility mod*; this file tells
> you *how to write the code and treat the codebase* while you do it. These rules are engine-agnostic
> and apply to every project in this workspace.
>
> **If you use an AI coding assistant** (Claude Code, Cursor, etc.): drop this file (and `CLAUDE.md`)
> into the project so the assistant follows the same rules. You can also copy the sections you like into
> your own global assistant config.

## Table of contents

1. [General code principles](#1-general-code-principles)
2. [Scope discipline](#2-scope-discipline)
3. [Testing & build](#3-testing--build)
4. [No magic numbers](#4-no-magic-numbers)
5. [Accessibility library choice (PRISM)](#5-accessibility-library-choice-prism)
6. [Framework-specific rules](#6-framework-specific-rules)
7. [Reverse-engineering tools](#7-reverse-engineering-tools)
8. [Publishing](#8-publishing)

---

## 1. General code principles

- **Prefer simple solutions.** Readability over cleverness; composition over inheritance.
- **Don't duplicate logic.** Factor shared behavior into one place (services, adapters, helpers).
- **Keep the codebase clean and organized.** Files, folders and naming should stay coherent as the mod
  grows.
- **Refactor files that grow past ~200–300 lines.** Long files are a smell — split by responsibility.
- **Avoid one-off throwaway scripts and files** living in the repo. Temporary/experimental files belong
  in a scratch location outside the project, and should be deleted once the task is done.
- **Consider Dev, Test and Prod separately** — don't let debug-only paths or hardcoded test values leak
  into a release build.
- **Match the surrounding code.** New code should read like the code already there: same naming,
  comment density and idioms.

## 2. Scope discipline

- **Only change what was requested.** Don't make unrequested changes, even if you spot something you'd
  do differently.
- **Fix only the bug you were asked to fix.** Don't introduce a new library, pattern or technology to
  fix a bug — solve it within the existing architecture.
- **Avoid major changes to established patterns and architecture** unless that *is* the task.
- **Think about ripple effects.** Before a change, consider what other methods and areas of the code it
  could affect.
- **Never overwrite the `.env` file** (or equivalent local config).
- **Don't add new DLLs / native dependencies silently.** If a task genuinely needs one, flag it and
  agree on it first.

## 3. Testing & build

- **Compile after every code change.** A change that doesn't build isn't done.
- **Write thorough tests for all major functionality.** Accessibility mods are hard to test blind-first;
  where you can, cover the state-tracking and formatting logic (the parts that don't need the game
  running).
- Use the framework's own build command (see §6) and make the build copy the artifact to the game's mod
  folder automatically, so the test loop is one step.

## 4. No magic numbers

- **Never hardcode unexplained constants** — offsets, IDs, thresholds, indices, timings.
- **Derive values from the real source**: the game's own data/components/config/APIs, or the library's
  headers. Read the authoritative value at runtime instead of guessing a literal.
- If a literal is truly unavoidable, give it a **named, documented constant** stating where it comes
  from and why.

## 5. Accessibility library choice (PRISM)

- **PRISM** (<https://github.com/ethindp/prism>) is the only screen-reader library — no fallback.
  Cross-platform C++23 library unifying NVDA/JAWS (Windows), VoiceOver (macOS/iOS), Orca/Speech-dispatcher
  (Linux), native TTS (Android) and WebSpeech (Web) behind one API. Integration details for the prebuilt
  release are in [`CLAUDE.md` §4](CLAUDE.md).
- **Route all speech through a single sink** so the backend (NVDA ↔ JAWS ↔ SAPI, all via PRISM) can be
  swapped in one place. The same code must work with NVDA, JAWS, VoiceOver, Orca, etc.

## 6. Framework-specific rules

> Identify the engine and framework **before** writing code — see [`CLAUDE.md` §2](CLAUDE.md). These
> rules apply once you've confirmed the framework.

This is a native C/C++ target (D2RLoader + PluginSDK): use its own hooking system and build process. See
the table in [`CLAUDE.md` §2](CLAUDE.md).

## 7. Reverse-engineering tools

Keep a set of tools handy and pick by binary type. All of these are free.

### First step — identify what you're looking at

Before decompiling, figure out the engine/language/packer so you pick the right tool (see the engine
clues in [`CLAUDE.md` §2](CLAUDE.md)).

| Tool | What it's for | Download |
|------|---------------|----------|
| **Detect It Easy (DIE)** | Detects compiler, language, packer and often the engine of a PE/ELF binary. | <https://github.com/horsicq/Detect-It-Easy> |
| **PE-bear** | Inspect PE headers, imports/exports, sections. | <https://github.com/hasherezade/pe-bear> |

### Static decompilers — pick by binary type

| Target | Tool | Notes | Download |
|--------|------|-------|----------|
| Native C/C++ / PE binaries | **Ghidra** (the main one) | Enable the MSVC RTTI analyzer + demangler so vtables/classes get real names instead of `FUN_`/`DAT_`. | <https://github.com/NationalSecurityAgency/ghidra> · <https://ghidra-sre.org> |
| Native C/C++ (alternative) | **IDA Free** | Great decompiler UI; Free edition covers x64. | <https://hex-rays.com/ida-free> |

### Dynamic / runtime analysis — find offsets and structures live

Often faster than static analysis for finding a struct offset or a live pointer, and the *only* way to
derive values at runtime (which is what we want — see §4 No magic numbers).

| Tool | What it's for | Download |
|------|---------------|----------|
| **Cheat Engine** | Scan memory, find structures/pointers/offsets while the game runs, walk pointer chains. | <https://github.com/cheat-engine/cheat-engine> · <https://cheatengine.org> |
| **x64dbg** | Native user-mode debugger for stepping through the game's code. | <https://github.com/x64dbg/x64dbg> · <https://x64dbg.com> |
| **ReClass.NET** | Reconstruct in-memory class/struct layouts interactively. | <https://github.com/ReClassNET/ReClass.NET> |
| **Frida** | Scriptable dynamic instrumentation — hook and trace functions at runtime. | <https://frida.re> · <https://github.com/frida/frida> |

### Modding framework (the host for your mod)

Per the table in [`CLAUDE.md` §2](CLAUDE.md), this repo's target uses **D2RLoader**
(<https://d2rloader.net>) with its **PluginSDK** (<https://github.com/D2RLoader/PluginSDK>).
*(The install path on your own machine is up to you; the tool choice is what matters.)*

## 8. Publishing

- Ship a clear, documented repo with a `README.md` in English: what the mod does, which parts are
  accessibilized, and the requirements (framework, PRISM, system screen reader).
- Avoid committing unnecessary binaries.
- **Don't create GitHub releases automatically** — only when explicitly asked. This applies to every
  mod.
