#include "input_actions.h"
#include "async_log.h"
#include "binding_store.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::InputActions {
namespace {
// The Options window's layout, open over every one of its tabs.
constexpr std::string_view OptionsPanel = "SettingsPanel";
// The loader's file of saved plugin keys, beside the plugin's own config (READ in D2RCore.dll's strings).
constexpr std::string_view SavedFile = "input-bindings.toml";
constexpr uint64_t PollMs = 250;

struct Tracked {
	const D2RL::InputService* input;
	D2RL::Input::ActionHandle handle;
	std::string id;
	BindingStore::Keys defaults;
};

std::mutex g_mutex;
std::vector<Tracked> g_tracked;
bool g_watching = false, g_changed = false, g_hinted = false;
uint64_t g_polled = 0;
std::vector<BindingStore::Keys> g_seen;
std::filesystem::file_time_type g_stamp{};

BindingStore::Keys KeysOf(D2RL::Input::Binding primary, D2RL::Input::Binding secondary) {
	return {static_cast<uint32_t>(primary.key), static_cast<uint32_t>(primary.modifier),
		static_cast<uint32_t>(secondary.key), static_cast<uint32_t>(secondary.modifier)};
}

std::vector<BindingStore::Action> Current(const D2RL::PluginContext* context) {
	std::lock_guard lock(g_mutex);
	std::vector<BindingStore::Action> actions;
	actions.reserve(g_tracked.size());
	for (const auto& tracked : g_tracked) {
		D2RL::Input::Binding primary{}, secondary{};
		if (!tracked.input->getBinding
			|| tracked.input->getBinding(context, tracked.handle, D2RL::Input::BindingSlot::Primary, &primary) != D2RL::Input::Result::Success
			|| tracked.input->getBinding(context, tracked.handle, D2RL::Input::BindingSlot::Secondary, &secondary) != D2RL::Input::Result::Success)
			continue;
		actions.push_back({tracked.id, tracked.defaults, KeysOf(primary, secondary)});
	}
	return actions;
}

std::vector<BindingStore::Keys> KeysOnly(const std::vector<BindingStore::Action>& actions) {
	std::vector<BindingStore::Keys> keys;
	for (const auto& action : actions) keys.push_back(action.current);
	return keys;
}

std::filesystem::path SavedPath(const D2RL::PluginContext* context) {
	return context->pluginConfigPath ? std::filesystem::path(context->pluginConfigPath).replace_filename(SavedFile) : std::filesystem::path{};
}

std::filesystem::file_time_type Stamp(const D2RL::PluginContext* context) {
	std::error_code error;
	const auto stamp = std::filesystem::last_write_time(SavedPath(context), error);
	return error ? std::filesystem::file_time_type{} : stamp;
}

void WarnIfUnsaved(const D2RL::PluginContext* context) {
	if (!context->pluginConfigPath || !context->pluginId) return;
	std::ifstream in(SavedPath(context), std::ios::binary);
	const std::string toml{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	const auto unsaved = BindingStore::Unsaved(Current(context), BindingStore::Parse(toml, context->pluginId));
	if (unsaved == 0) return;
	AsyncLog::InfoF(context, "[keys] Options closed with %zu key change(s) not applied; the loader keeps them until the game closes", unsaved);
	ScreenReader::Speak(Strings::Current().bindingsNotSaved, false);
}
}

D2RL::Input::Result Register(const D2RL::InputService* input, const D2RL::PluginContext* context,
	const D2RL::Input::ActionRegistration* action, D2RL::Input::ActionHandle* handle) {
	const auto result = input->registerAction(context, action, handle);
	if (result == D2RL::Input::Result::Success && action->logicalId) {
		std::lock_guard lock(g_mutex);
		g_tracked.push_back({input, *handle, action->logicalId, KeysOf(action->defaultPrimary, action->defaultSecondary)});
	}
	return result;
}

D2RL::Input::Result Unregister(const D2RL::InputService* input, const D2RL::PluginContext* context, D2RL::Input::ActionHandle handle) {
	{
		std::lock_guard lock(g_mutex);
		std::erase_if(g_tracked, [&](const Tracked& tracked) { return tracked.handle == handle && tracked.input == input; });
	}
	return input->unregisterAction(context, handle);
}

void Tick(const D2RL::PluginContext* context, uint64_t now) {
	const auto& open = PanelState::OpenPanels();
	if (std::find(open.begin(), open.end(), OptionsPanel) == open.end()) {
		if (g_watching && g_changed) WarnIfUnsaved(context);
		g_watching = false;
		return;
	}
	if (now - g_polled < PollMs) return;
	g_polled = now;
	auto keys = KeysOnly(Current(context));
	if (!g_watching) {
		g_watching = true;
		g_changed = g_hinted = false;
		g_seen = std::move(keys);
		g_stamp = Stamp(context);
		return;
	}
	// Apply wrote the file: a later change needs Apply again.
	if (const auto stamp = Stamp(context); stamp != g_stamp) { g_stamp = stamp; g_hinted = false; }
	if (keys == g_seen) return;
	g_seen = std::move(keys);
	g_changed = true;
	if (g_hinted) return;
	g_hinted = true;
	AsyncLog::InfoF(context, "[keys] a key changed in Options; only Apply saves it");
	ScreenReader::Speak(Strings::Current().bindingsApplyHint, false);
}

}
