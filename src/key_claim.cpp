#include "key_claim.h"

#include "async_log.h"
#include "xinput_hook.h"

#include <D2RLPlugin/input.h>

#include <vector>

namespace D2RAccess::KeyClaim {
namespace {

const D2RL::PluginContext*             g_context = nullptr;
const D2RL::InputService*              g_input   = nullptr;
std::vector<D2RL::Input::ActionHandle> g_actions;

// The four arrow keys, which xinput_hook's layout maps to the D-pad. The SDK
// enum stops short of them, so the virtual-key codes go in by cast: VK_LEFT
// 0x25, VK_UP 0x26, VK_RIGHT 0x27, VK_DOWN 0x28. Nothing else is claimed --
// there is no reason to take the whole keyboard, and a key the mod does not
// map should keep reaching the game exactly as it always did.
struct Claim { const char* id; const char* label; uint32_t vk; };
constexpr Claim kClaims[] = {
	{"pad-dpad-left",  "D-pad left (keyboard pad)",  0x25},
	{"pad-dpad-up",    "D-pad up (keyboard pad)",    0x26},
	{"pad-dpad-right", "D-pad right (keyboard pad)", 0x27},
	{"pad-dpad-down",  "D-pad down (keyboard pad)",  0x28},
};

// Claimed ONLY while the simulated pad is actually injecting. With the pad off
// the key is not driving anything of ours, so swallowing it would take the
// player's arrow keys away for nothing -- a controller-only player must be
// left exactly as they were.
//
// Only a press can be consumed: the loader discards the callback's result on
// release (its release dispatch throws EAX away), so a key-up still reaches
// the game. That is harmless for menu navigation, which acts on the press.
auto __cdecl OnAction(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept
	-> D2RL::Input::ActionResult {
	if (event == nullptr || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	return XInputHook::Injecting() ? D2RL::Input::ActionResult::Handled
	                               : D2RL::Input::ActionResult::Ignored;
}

}

bool Register(const D2RL::PluginContext* context) {
	if (context == nullptr) return false;
	g_context = context;
	if (context->QueryService(&g_input) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		AsyncLog::Warn(context, "[claim] input service unavailable; the D-pad keys are not claimed.");
		g_input = nullptr;
		return false;
	}
	unsigned claimed = 0;
	for (const auto& claim : kClaims) {
		const D2RL::Input::ActionRegistration action{
			.structSize     = D2RL::Input::ActionRegistrationSize,
			.logicalId      = claim.id,
			.displayName    = claim.label,
			.category       = "Accessibility",
			.defaultPrimary = {static_cast<D2RL::Input::Key>(claim.vk), D2RL::Input::Modifier::None},
			.callback       = OnAction,
			.userData       = nullptr};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) {
			g_actions.push_back(handle);
			++claimed;
		} else {
			// Expected if the loader validates against its own Key enum, which
			// does not list the arrow keys. Says so per key rather than once,
			// so a partial acceptance is not mistaken for a total refusal.
			AsyncLog::WarnF(context, "[claim] loader refused %s (virtual key 0x%02X); the game will still see it.",
				claim.id, claim.vk);
		}
	}
	AsyncLog::InfoF(context, "[claim] %u of %zu D-pad keys claimed; consumed only while the keyboard pad is injecting.",
		claimed, sizeof(kClaims) / sizeof(kClaims[0]));
	return claimed > 0;
}

void Unregister() {
	if (g_input != nullptr && g_context != nullptr)
		for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	g_input   = nullptr;
	g_context = nullptr;
}

}
