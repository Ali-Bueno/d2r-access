#include "message_hold.h"

#include "async_log.h"

#include <windows.h>

#include <atomic>
#include <bitset>
#include <mutex>
#include <utility>

namespace D2RAccess::MessageHold {
namespace {
std::atomic<uint64_t> g_until{0};
HHOOK g_hook = nullptr;
bool g_tried = false;
std::mutex g_mutex;
std::vector<uint32_t> g_presses;
// Touched only on the game window's thread, where the hook runs.
std::bitset<256> g_kept;

void Keep(uint32_t press) {
	std::lock_guard lock(g_mutex);
	g_presses.push_back(press);
}

// Bit 30 of a key message's lParam: the key was already down (a repeat).
constexpr LPARAM RepeatBit = LPARAM{1} << 30;

LRESULT CALLBACK Filter(int code, WPARAM wParam, LPARAM lParam) {
	if (code == HC_ACTION && wParam == PM_REMOVE) {
		auto* message = reinterpret_cast<MSG*>(lParam);
		const bool hold = GetTickCount64() < g_until.load(std::memory_order_relaxed);
		const UINT kind = message->message;
		if (kind == WM_KEYDOWN || kind == WM_SYSKEYDOWN || kind == WM_KEYUP || kind == WM_SYSKEYUP) {
			const auto key = static_cast<uint32_t>(message->wParam & 0xFF);
			const bool down = kind == WM_KEYDOWN || kind == WM_SYSKEYDOWN;
			const bool closing = kind == WM_SYSKEYDOWN && key == VK_F4;
			if (hold && down && !closing) {
				if ((message->lParam & RepeatBit) == 0) Keep(key);
				g_kept.set(key);
			}
			const bool kept = (hold && !closing) || g_kept.test(key);
			if (!down) g_kept.reset(key);
			if (kept) message->message = WM_NULL;
		} else if (hold) {
			switch (kind) {
			case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: Keep(VK_LBUTTON); message->message = WM_NULL; break;
			case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: Keep(VK_RBUTTON); message->message = WM_NULL; break;
			case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: Keep(VK_MBUTTON); message->message = WM_NULL; break;
			case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: Keep(VK_XBUTTON1); message->message = WM_NULL; break;
			case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: Keep(0); message->message = WM_NULL; break;
			case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP: message->message = WM_NULL; break;
			default: break;
			}
		}
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}

// On the thread of the game's window, which is in front when the question opens.
void Install(const D2RL::PluginContext* context) {
	if (g_hook || g_tried) return;
	DWORD process = 0;
	const DWORD thread = GetWindowThreadProcessId(GetForegroundWindow(), &process);
	if (process != GetCurrentProcessId() || !thread) return;
	g_tried = true;
	g_hook = SetWindowsHookExW(WH_GETMESSAGE, Filter, nullptr, thread);
	if (g_hook) AsyncLog::InfoF(context, "[hold] keys and mouse buttons can be kept from the game (thread %lu)", thread);
	else AsyncLog::WarnF(context, "[hold] keys and mouse buttons cannot be kept from the game: error %lu", GetLastError());
}
}

void Hold(const D2RL::PluginContext* context, uint64_t now) {
	Install(context);
	g_until.store(now + HoldMs, std::memory_order_relaxed);
}

void Release() { g_until.store(0, std::memory_order_relaxed); }

std::vector<uint32_t> TakePresses() {
	std::lock_guard lock(g_mutex);
	return std::exchange(g_presses, {});
}

void Remove() {
	g_until = 0;
	if (g_hook) UnhookWindowsHookEx(g_hook);
	g_hook = nullptr;
	g_tried = false;
	std::lock_guard lock(g_mutex);
	g_presses.clear();
}
}
