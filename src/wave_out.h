#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <cstdint>

namespace D2RAccess::WaveOut {
enum class Wait { Done, Cancelled, Stalled };
// Buffers come back at about their own length (the radar-audio audio_ms log field); one
// held a second longer means the output stalled without an error, so the caller reopens it.
inline constexpr uint64_t StallMarginMs = 1000;
// Waits for a written buffer to return; keepWaiting() returning false cancels the wait.
template <class KeepWaiting>
Wait Await(const WAVEHDR& header, HANDLE done, size_t bytesPerSecond, DWORD pollMs, KeepWaiting keepWaiting) {
	const uint64_t deadline = GetTickCount64() + uint64_t(header.dwBufferLength) * 1000 / bytesPerSecond + StallMarginMs;
	while (!(*reinterpret_cast<const volatile DWORD*>(&header.dwFlags) & WHDR_DONE)) {
		if (!keepWaiting()) return Wait::Cancelled;
		if (GetTickCount64() > deadline) return Wait::Stalled;
		WaitForSingleObject(done, pollMs);
	}
	return Wait::Done;
}
}
