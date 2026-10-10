# Allocation recovery and process shutdown

Two crash dumps identified different mod failures. A normal maximum-radius
navigation grid allocation threw `std::bad_alloc` during object selection and
escaped the SDK's `noexcept` UI callback. An earlier process exit reached a
joinable static ItemAudio thread's destructor, which called `std::terminate`.
The original reason that earlier process exit began remains unknown.

`CallbackBoundary` now contains C++ exceptions at the UI callback boundary.
The temporary snapshot unwinds normally. `KeyboardService::CancelAfterFault`
cancels active mod movement and pending actions, holds, pad approach, target
lock and picker selection without allocating. It preserves learned map data
and never substitutes an empty/fully-explored map for an unavailable snapshot.
The callback backs off one second and a later attempt queues a mod-side notice.
Normal explicit input can resume actions after recovery.

Timing-destructor formatting uses stack storage. Async logging is best effort:
an enqueue allocation failure cannot trigger another exception during recovery
or worker error reporting. Speech and disk logging remain worker operations;
native game access remains on the UI thread. These changes contain mod failures
but cannot guarantee game allocations when system RAM/commit is exhausted.

`ProcessLifetime<T>` owns all static worker threads and the terrain service's
exploration store without automatic CRT destructor cleanup. Normal plugin
unload still signals, joins and releases worker resources; the archive owner
then resets to release its cache. Forced process exit bypassing unload no
longer destroys a joinable thread or joins killed workers under the loader lock.
Windows reclaims process resources. Normal dynamic unload still requires the
plugin unload entry point; local/test Store destructors retain ordinary RAII.
This follows [Microsoft's process-detach guidance](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain).

Offline regressions cover:

- Failure of the real maximum-radius navigation-grid allocation, RAII
  unwinding, allocation-free recovery, backoff and restart.
- The actual SDK UI pump through failure/recovery/stop/restart and queue Busy.
- Failed diagnostic allocation while the real logging worker is blocked.
- An isolated worker DLL's normal stop/join/restart/unload, handle cleanup,
  and ExitProcess/CRT return while a worker remains joinable.

Focused tests passed 4/4; the complete suite passed 108/108 with a 20-second
per-test limit. No game launch or gameplay probe was used. Live confirmation
of object cycling, manual movement cancellation and game exit remains pending.
