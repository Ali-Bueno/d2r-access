#include "native_cairn.h"

#include "async_log.h"
#include "cairn_order_cache.h"
#include "layout_check.h"
#include "native_memory.h"

#include <D2RLPlugin/lifecycle_events.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

namespace D2RAccess::NativeCairn {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;

// D2R 3.3.93787: opcode 0x50 wrapper calls this quest-order dispatcher.
constexpr uintptr_t PacketHandlerRva = 0x232EA0;
constexpr uint8_t PacketHandlerPrefix[] = {
	0x48, 0x83, 0xEC, 0x48, 0x48, 0x8B, 0x05, 0x1D, 0x84, 0x79, 0x02, 0x48, 0x33, 0xC4};
constexpr size_t PacketSize = 15;
constexpr uint8_t PacketOpcode = 0x50;
constexpr uint16_t CairnQuestId = 4;
struct CodeCheck { uintptr_t rva; size_t size; uint64_t hash; };
constexpr CodeCheck NativeChecks[] {
	{0x12CC30, 81, 0xA732112A62FFE554ULL}, // opcode 0x50 wrapper
	{PacketHandlerRva, 283, 0x8F3407E4271E9F07ULL},
	{0x245210, 30, 0x5C7E7CAECCAD2354ULL}, // client order setter
};

const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
CairnStones::OrderCache g_order;
std::atomic<bool> g_active{false};
void* g_original = nullptr;

void CapturePacket(uint64_t epoch, const std::array<uint8_t, PacketSize>& bytes) {
	if (bytes[0] != PacketOpcode) return;
	uint16_t questId = 0;
	std::memcpy(&questId, bytes.data() + 1, sizeof(questId));
	if (questId != CairnQuestId) return;
	std::array<uint32_t, CairnStones::Count> symbols{};
	for (size_t i = 0; i < symbols.size(); ++i) {
		uint16_t symbol = 0;
		std::memcpy(&symbol, bytes.data() + 3 + i * sizeof(symbol), sizeof(symbol));
		symbols[i] = symbol;
	}
	if (g_active.load(std::memory_order_acquire)) (void)g_order.CompleteCapture(epoch, symbols);
}

void __cdecl HandlePacket(const uint8_t* packet) noexcept {
	std::array<uint8_t, PacketSize> bytes{};
	std::optional<uint64_t> epoch;
	bool copied = false;
	try {
		if (g_active.load(std::memory_order_acquire)) epoch = g_order.BeginCapture();
		copied = epoch && packet && NativeMemory::Copy(reinterpret_cast<uintptr_t>(packet), bytes.data(), bytes.size());
	} catch (...) { /* The stock packet handler must still run. */ }
	reinterpret_cast<void(__cdecl*)(const uint8_t*)>(g_original)(packet);
	try {
		if (copied) CapturePacket(*epoch, bytes);
	} catch (...) { /* The stock packet handler must not depend on this capture. */ }
}

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	try {
		switch (event->kind) {
		case Kind::GameJoined: g_order.Reset(event->sessionGeneration, false); break;
		case Kind::LocalPlayerReady: g_order.Reset(event->sessionGeneration, true); break;
		case Kind::GameLeft: g_order.Reset(0, false); break;
		default: break;
		}
	} catch (...) { /* A lifecycle callback must not interrupt the game. */ }
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context || !context->exeBase || context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) {
		Unregister();
		return false;
	}
	g_context = context;
	if (!LayoutCheck::Verify(context, context->exeBase, NativeChecks, "cairn", "scroll order capture disabled")) {
		Unregister();
		return false;
	}
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady}) {
		const D2RL::Lifecycle::GameplayEventListener listener{
			.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) {
			Unregister();
			return false;
		}
		g_listeners.push_back(handle);
	}
	if (!context->InstallInlineHook(PacketHandlerRva, PacketHandlerPrefix, sizeof(PacketHandlerPrefix),
		reinterpret_cast<void*>(&HandlePacket), &g_original)) {
		AsyncLog::Warn(context, "[cairn] Scroll order packet hook unavailable.");
		Unregister();
		return false;
	}
	g_active.store(true, std::memory_order_release);
	AsyncLog::Info(context, "[cairn] Verified deciphered-scroll packet capture installed.");
	return true;
}

void Unregister() {
	g_active.store(false, std::memory_order_release);
	g_order.Reset(0, false);
	if (g_context && g_lifecycle) {
		for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	}
	g_listeners.clear();
	g_context = nullptr;
	g_lifecycle = nullptr;
}

std::optional<CairnStones::Order> Read(uint64_t session) {
	if (!g_active.load(std::memory_order_acquire)) return {};
	return g_order.Read(session);
}

bool Available() {
	return g_active.load(std::memory_order_acquire);
}
}
