# Screen-reader integration by engine

How our accessibility mods actually reach the OS screen reader (**PRISM** on every target — no
Tolk fallback) on each host we target. This is the **transport** layer — the plumbing that carries a
`Speak(text, interrupt)` call from mod code down to `prism.dll`. It answers, per engine:

- Can the host **load native code** at all, and how?
- Can a **Lua script** reach native functions directly, or does it need a native/host component?
- What do we actually **ship**?

The reusable knowledge is at the bottom: [the adapter contract](#the-part-that-ports-everywhere-the-adapter-contract)
is engine-agnostic and shared across all targets; only the transport under it changes.

> Companion to [prism-prebuilt.md](prism-prebuilt.md) (consuming the PRISM prebuilt release) and
> CLAUDE.md §6 (architecture). Those tell you *how to call PRISM*; this doc tells you *how the call
> physically gets there* on each engine.

---

## The two questions that decide everything

Every host reduces to two yes/no questions:

1. **Is the host's Lua a real native Lua** (PUC Lua or LuaJIT compiled to machine code), or a
   **managed re-implementation** running on another VM (Kahlua on the JVM, MoonSharp on .NET)?
   - Managed Lua → **cannot** load DLLs, no C modules, no FFI. You *must* bridge through the host
     VM's own native-interop (JNA on Java, P/Invoke on .NET).
   - Native Lua → native code is reachable; go to question 2.

2. **Does the host let a *script* load native code** (`package.loadlib`/`require` of a Lua C module,
   or LuaJIT `ffi`)?
   - Yes → you can ship a **pure-script mod + a bundled native module**.
   - No (sandboxed / unsupported) → the script can't reach native on its own; a **native host
     plugin** must inject the functions into the Lua state.

Everything below is those two questions answered per engine.

---

## Decision matrix (our targets)

| Host / framework | Screen-reader transport we use |
|---|---|
| **Native game** (C/C++/VB6) | Native hook `LoadLibrary("prism.dll")` directly |

---

## Native games — for completeness

- **Native game (C/C++/VB6):** the native hook (via D2RLoader's PluginSDK) links/loads `prism.dll`
  directly (`LoadLibrary` + `GetProcAddress`). No intermediary.

---

## The part that ports everywhere: the adapter contract

Regardless of transport, mod code is written against **one** small interface. Only the implementation
underneath changes per engine. This is the real reusable asset — port *this*, not the compiled DLL.

```
ScreenReader
  Output(text, interrupt)      -- route to the backend; interrupt=true on context change
  Silence()                    -- stop speech
  SetRate/SetVolume/SetPitch(v)
  BackendName()                -- log the chosen PRISM backend on init

Announcer  (built on ScreenReader — engine-agnostic, pure logic, ships as-is)
  Speak(msg, interrupt)        -- dedup / anti-spam gate (e.g. 200 ms on identical msg)
  SpeakUnique(msg) / SpeakInterrupt(msg)
```

- `Announcer` (dedup/diff-gating, verbosity) and the string/localization layer contain **no native
  calls** — the rest of the contract is identical everywhere; only `ScreenReader.Output` differs
  per engine.
- Keep a **single sink** so backend or transport swaps happen in one place (CLAUDE.md §9).

---

## One-line summary

- **Native game** — the native hook links/loads `prism.dll` directly. No intermediary.
- **Everywhere** — the `ScreenReader` + `Announcer` adapter is identical; only the transport differs.
