# User crash investigation — 2026-10-10

The latest gameplay crash is a confirmed `std::bad_alloc` in the mod's
comma/period selection path. A normal, bounded navigation-grid allocation
failed and escaped a `noexcept` UI callback, terminating the host. Heavy
system memory pressure is measured, but the saved counters cannot prove
which application or transient limit caused that individual allocation failure.

The previous saved dump has a different, secondary failure during process
shutdown: the mod's item-audio thread was still joinable when its static
destructor ran. That dump does not establish why the game began exiting.
Do not classify all matching Windows error codes as the same root cause.

Investigation only. No production source, settings, save, loader, or installed
DLL changes; no compilation, relink, deployment, game launch, input, or live
probe. Existing local changes remain intact. No delegation or Git publication.

## Latest gameplay crash

Own current logs were copied before another launch could overwrite them.
Windows Event 1000 records the failure at 2026-10-10 02:50:03 PDT; the saved
dump was captured at 02:50:38, so its memory counters are later measurements.

- Host: D2RLoader 1.3.1-beta; game 3.3.93787; plugin ABI 4.
- Loaded DLL and existing build both match SHA-256
  `4EE487DBDDB7D7186F9079B8B439A17019E5FE04613D921A3CB93D3782785E4A`.
- Outer exception: `0xC0000409`, subcode 7, `FAST_FAIL_FATAL_APP_EXIT`.
  This is `terminate`/`abort` here, not evidence of a stack-buffer overwrite.
- Inner C++ exception record at stack address `0x7F9300` has ThrowInfo
  RVA `0x1E9908`; PE RTTI names `std::bad_alloc` and `std::exception`.
  The exception object's message is `bad allocation`.
- Allocation requested `0x212DFF` = 2,174,463 bytes: exactly
  `(2 * Navigation::MaxRadius + 1)^2 * sizeof(Navigation::Cell) + 39`,
  with radius 150 and 24-byte cells. The 39 bytes are the STL alignment
  allowance. This was a normal 301-by-301 grid, not a corrupt huge count.

Existing COFF function bodies were matched to the installed PE while masking
linker relocations. The following addresses are confirmed byte matches; the
debugger's nearest-export label `D2RLoaderUnloadPlugin+...` is misleading for
these gameplay frames because the Release DLL has no private PDB.

| Plugin RVA | Actual function |
| --- | --- |
| `0x64C60` | `Navigation::Build`; failure at `grid.cells.resize` |
| `0x118DA0` | `KeyboardService` internal `Look` |
| `0x120700` | `KeyboardService` internal `StepPick` |
| `0x122B20` | `KeyboardService::Tick` |
| `0x50120` | plugin `OnUiTick` |
| `0xE9C00` | `UiThreadPump` internal `OnUiTick` |

`StepPick` is called by comma/period for items/objects, and brackets for NPCs.
The dump confirms this selection path; it does not retain the exact pressed
key. The user's comma/period report and log agree. The log ends after selecting
41 Gold at 02:49:49.874 and saying No enemies at 02:49:49.948. Immediately
beforehand, D successfully used Trapped Soul id 166 and it dropped gold.
An earlier vanilla walking-mode assertion at 02:43:08 was continued by the
loader; it is not the terminating exception found in the dump.

`ui_thread_pump.cpp::OnUiTick` is `noexcept` and directly invokes the
allocation-capable service tick without an exception boundary. Consequently
this allocation failure kills the whole game instead of making selection
temporarily unavailable. That is a confirmed mod robustness defect.

## Memory evidence and limits

The dump contains SystemMemoryInfoStream and ProcessVmCountersStream. The
local read-only Python tool decodes revision 2 with the installed Windows
SDK's pack(4) layouts, verifies stream sizes, and checks the independently
populated basic and legacy performance counters agree. Values below are GiB.

| Measurement at dump capture | Oct 10, 02:50:38 | Oct 9, 05:35:16 |
| --- | ---: | ---: |
| Installed physical RAM | 15.83 | 15.83 |
| Available physical RAM | 1.80 | 6.90 |
| System committed memory | 38.50 | 17.48 |
| System commit limit | 41.28 | 31.83 |
| Peak system commitment | 40.26 | 26.84 |
| Host process private commitment | 8.75 | 1.18 |
| Host process peak private commitment | 9.71 | 9.94 |
| Host process peak working set | 4.56 | 4.66 |

Commitment is memory backed by RAM/pagefile; it is not all resident RAM.
The process includes the game, loader, mod and in-process runtime components;
these counters cannot attribute its 9.71 GiB peak to a particular component
or prove a mod leak. The temporary navigation grid has ordinary scoped
ownership and a bounded size. Failed allocation is proven; its lower-level
cause is not retained, and saved last-error/status values are both zero.

At the first post-crash inspection, two DQXI compiler jobs together occupied
about 5.7 GiB resident RAM. Only one was already running during this crash:
the build began at 02:49:35 and its compiler at 02:49:38. The second compiler
started at 02:54:06, after the crash. Do not attribute both jobs to the failure.
No Windows Resource-Exhaustion-Detector events were found in the preceding
seven days; their absence does not invalidate the actual allocation failure.

## Previous shutdown failure

The Oct 9 dump reaches `ExitProcess`/loader shutdown, CRT on-exit callbacks,
then plugin RVA `0x1532B0`, then `std::terminate`/`abort`. The destructor tests
the thread id at plugin RVA `0x1F9EE8`. Relocations in the existing
`ItemAudio::Start` and `ItemAudio::Stop` object bodies independently map
`ItemAudio`'s `g_thread` to `0x1F9EE0`. The saved thread id is `0x42C8`,
which is nonzero/joinable. The installed STL thread destructor calls
`terminate` in this condition. `ItemAudio::Stop` normally joins it, but this
process-exit path reached static destruction with it still joinable.

This is a confirmed cleanup fault, potentially secondary to another game
exit. Its dump has ample memory at capture and no gameplay allocation stack.
The reason process exit began is unresolved; the current log is a new session.

## Settings advice requested by the user

Current settings and the loader log confirm Texture Quality = Very High on
an RTX 3060 Laptop GPU with 5,996 MiB dedicated VRAM. Texture heap is 3,584
MiB. The extracted native tooltip says higher quality requires more video
memory and requires a restart. The available texture values are Low, Medium,
High, Very High. Texture Quality = Low, followed by a full game restart, is
the first suggested graphics experiment. Its system-RAM saving is an
inference to measure, not a result already validated in this game.

Windows permits discrete GPUs to use ordinary system RAM as shared video
memory; rendering does not avoid system RAM/commit pressure. Primary source:
[Microsoft DirectX memory explanation](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/).
Other low graphics settings can reduce rendering resource requirements;
framerate cap 60 mainly limits CPU/GPU work, not texture allocation. Current
Framerate Cap is 0. Windows already has automatic pagefile management enabled;
it provides allocation headroom rather than reducing RAM consumption.
The user clarified that playing during compiler work is the intended use.
Limit concurrent heavy compiler jobs to one while playing: each large compiler
we observed used about 3 GiB resident RAM. Lower process priority protects CPU
responsiveness but does not impose a memory limit. The local D2R build's default
four jobs/BelowNormal remains unchanged; no other project's settings/processes
or game graphics settings were changed.

Memory pressure can cause paging, allocation failure and analysis timeouts.
Ghidra provides `decompileCompleted`, `isTimedOut`, `isCancelled`, and
`getErrorMessage`. Check completion and warnings per function, the headless
process exit/log, and expected output coverage before accepting results. Reject
failed/partial output and rerun it when headroom is available; do not overwrite
good archives with failure output. These crashes provide no evidence that saved
decompiler output was corrupted. Primary contract:
[Ghidra DecompileResults](https://ghidra.re/ghidra_docs/api/ghidra/app/decompiler/DecompileResults.html).

## Follow-up boundaries

Contain allocation failure centrally and give the
affected navigation/selection service a safe unavailable result. Recovery
must not allocate speech/log strings while already handling allocation failure,
interpret an unavailable grid as fully explored, discard valid map memory,
or resume unwanted automatic movement. Existing input ownership and UI/worker
split remain required. Review the loader's process-exit contract for audio
worker lifetime separately; do not join workers under the Windows loader lock.
The dumps already prove both defects without a new gameplay probe. The user
subsequently requested both repairs and authorized pushing the tested installed
fix to origin/bladestorm.

## Repairs and validation — 2026-10-10

Implemented the central callback boundary, shared allocation-free movement
cancellation, one-second backoff, fixed-stack timing-destructor formatting,
best-effort nonthrowing diagnostics, and process-lifetime worker ownership.
All seven static threads and the terrain-owned archive store use the central
owner helper. Normal unload still stops/joins workers, flushes persistence and
releases the archive cache; local Store RAII is unchanged. No cleanup wait or
thread detach was added under the loader lock. Details and portable test
coverage: [stability repair](stability.md) and [architecture](architecture.md).

Focused allocation/pump/logging/isolated-DLL lifecycle tests: 4/4 passed.
Complete offline suite: 108/108 passed, timeout 20 per test, 69.95 seconds.
Final local build used the default four jobs/BelowNormal, staged first, then
copied the verified DLL after confirming the game/loader was closed.
Installed/build/staged SHA-256:
`5B18845CC8BF847360EAE46A705E26947948CF1859DA4723BCB0257CA5541C86`.
Previous installed DLL is retained locally as
`../../re/crash-investigation-2026-10-10/pre-fix.d2rl-d2r-access.dll`.
Build/test/deployment records are in the same evidence directory. No graphics
settings, saves, other projects' processes, or game launch/input were changed.
Gameplay confirmation remains pending. Resource exhaustion in the game itself
remains possible; recovery contains the verified mod faults.

Evidence and read-only tools: `../../re/crash-investigation-2026-10-10/`.
Original dumps remain in the user's LocalAppData CrashDumps directory.
Latest dump SHA-256:
`6D7E8134E518A2F4FC33AB89604A771ACDBA74C0258D11461DC4B3017B415DD5`.
Previous dump SHA-256:
`16DFF4BDF1E414A0AE04052AE3512732DC588EAA3AFB93473D272035392A3E60`.

Investigation validation: two dumps inspected offline, inner exception RTTI decoded,
allocation size/source matched, six gameplay function bodies matched to
existing binary, and item-worker global independently matched through Start
and Stop relocations. This evidence was recorded before recompiling; the repair
tests and installation are recorded separately above.

Publication: after the 108/108 suite and verified installation, the user-authorized
push completed to origin/bladestorm. Stability commit `4c88812`; separate existing
continuous-H prerequisite `891da55`. Remote full tip verified as
`4c8881227389893b76ff3f3cc3f040218c792076`. No force push or guard modification;
at that time this host-specific report and local instructions remained local.
On 2026-10-19 the user explicitly requested publishing all edits to bladestorm,
including this report, instructions, memory and build tooling. Raw dumps and
generated/extracted data remain outside the source publication.
