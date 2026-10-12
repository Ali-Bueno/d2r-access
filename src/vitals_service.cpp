#include "vitals_service.h"
#include "async_log.h"
#include "equipment_census.h"
#include "input_direction.h"
#include "keyboard_menu.h"
#include "localization.h"
#include "native_memory.h"
#include "native_status.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "vitals.h"
#include "weapon_swap.h"

#include <D2RLPlugin/context.h>
#include <D2RLPlugin/input.h>
#include <D2RLPlugin/lifecycle_events.h>

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace D2RAccess::VitalsService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, On, Off, Status, Report, Life, Mana };

// The game's own HUD widgets: HUDPanelHD.json declares HealthBall and ManaBall
// as AttributeBallWidgets, each carrying the ValueDisplay text box the game
// fills while the matching "show orb text" gameplay option is on.
constexpr const char* HudPanel = "HUDPanel";
const std::string LifePath[] = {"HealthBall", "ValueDisplay"};
const std::string ManaPath[] = {"ManaBall", "ValueDisplay"};
// Those two options, named by the keys the game's own widget catalogue uses
// for them (src/generated/panel_widgets.h), so the hint speaks their real
// on-screen labels in the player's language.
constexpr const char* LifeOrbTextOption = "OptionsMenuShowLifeOrbText";
constexpr const char* ManaOrbTextOption = "OptionsMenuShowManaOrbText";

constexpr uint64_t OrbSampleMs = 100;         // life can fall within a single blow
constexpr uint64_t DurabilitySampleMs = 1000; // durability only creeps down

const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService*  g_input = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::atomic<Command> g_command{Command::None};
bool g_enabled = true;
bool g_movementAlerts = true, g_ammoAlerts = true;
bool g_statusSupported = false;
unsigned g_hintedMask = 0;
std::string_view g_lastMercSource;
uint32_t g_lastMercId = 0;
uint64_t g_nextOrb = 0, g_nextDurability = 0;
Vitals::Alerts g_alerts;
Vitals::OrbAvailability g_orbAvailability;
Vitals::Condition g_poison;
Vitals::MercenaryMeter g_mercenary;
Vitals::Breakage g_breakage;
Vitals::GaitTracker g_gait;
Vitals::StaminaMeter g_stamina;
Vitals::AmmoMeter g_ammo;
bool g_runToggled = false; // L3 pressed in play since the last orb sample
// The last census of what the character is wearing. See LogWorn.
std::string g_worn;

int ReadPercent(std::span<const std::string> path) {
	const auto orb = Vitals::ParseOrb(NativeWidgets::TextAt(HudPanel, path));
	return orb ? Vitals::Percent(orb->current, orb->maximum) : -1;
}

std::string FormattedPercent(const char* format, int percent) {
	char phrase[100]{};
	std::snprintf(phrase, sizeof(phrase), format, percent);
	return phrase;
}

void Append(std::string& text, std::string_view phrase) {
	if (phrase.empty()) return;
	if (!text.empty()) text += ". ";
	text += phrase;
}

int StaminaPercent(const NativeStatus::Movement& movement) {
	return Vitals::Percent(movement.stamina, movement.maxStamina);
}

// The stacks among the weapons in hand, with the items they came from.
std::vector<std::pair<Vitals::AmmoMeter::Stack, const Vitals::Breakage::Item*>> InHand(
	const std::vector<Vitals::Breakage::Item>& worn) {
	std::vector<std::pair<Vitals::AmmoMeter::Stack, const Vitals::Breakage::Item*>> hands;
	for (const auto& item : worn)
		if (item.bodyLocation == WeaponSwap::RightHand || item.bodyLocation == WeaponSwap::LeftHand)
			hands.push_back({{item.id, item.quantity}, &item});
	return hands;
}

std::string AmmoPhrase(const Vitals::Breakage::Item& item, int quantity) {
	char phrase[200]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().ammoLeft, EquipmentCensus::Name(item).c_str(), quantity);
	return phrase;
}

// The stacks in hand right now, "Arrows: 120 left", for the on-demand report.
std::string AmmoReport(const D2RL::PluginContext* context) {
	std::vector<Vitals::Breakage::Item> worn;
	if (EquipmentCensus::Read(context, worn) != EquipmentCensus::Result::Read) return {};
	const auto hands = InHand(worn);
	std::vector<Vitals::AmmoMeter::Stack> stacks;
	for (const auto& [stack, item] : hands) stacks.push_back(stack);
	std::string text;
	for (const auto& stack : g_ammo.Known(stacks))
		for (const auto& [held, item] : hands)
			if (held.id == stack.id) Append(text, AmmoPhrase(*item, stack.quantity));
	return text;
}

// Walking or running, from the game's L3 toggle at once and from the
// character's movement after; stamina as it falls while running.
void AnnounceMovement(const D2RL::PluginContext* context, bool toggled) {
	const auto movement = NativeStatus::PlayerMovement();
	std::optional<Vitals::Gait> gait = toggled ? g_gait.Toggle() : std::nullopt;
	int stamina = -1;
	if (movement) {
		const auto observed = movement->running ? std::optional(Vitals::Gait::Run)
			: movement->walking ? std::optional(Vitals::Gait::Walk) : std::nullopt;
		// The game applies the toggle a frame or more later: this sample still shows the old gait.
		if (!toggled)
			if (const auto changed = g_gait.Observe(observed)) gait = changed;
		stamina = g_stamina.Update(StaminaPercent(*movement));
	}
	if (!gait && stamina < 0) return;
	const auto& strings = Strings::Current();
	std::string text;
	if (stamina == 0) Append(text, strings.staminaOut);
	else if (stamina > 0) Append(text, FormattedPercent(strings.staminaPercent, stamina));
	if (gait) Append(text, *gait == Vitals::Gait::Run ? strings.gaitRunning : strings.gaitWalking);
	AsyncLog::InfoF(context, "[vitals] movement gait=%d stamina_alert=%d toggled=%d raw=%d/%d", gait ? static_cast<int>(*gait) : -1,
		stamina, toggled, movement ? movement->stamina : -1, movement ? movement->maxStamina : -1);
	// The toggle answers the button just pressed; the rest waits its turn.
	ScreenReader::Speak(text, toggled);
}

std::string MercenaryLevel(int level) {
	const auto mercenary = Localization::Text("MiniPanelHireinv");
	const auto levelLabel = Localization::Text("strchrlvl");
	if (mercenary.empty() || levelLabel.empty()) return {};
	return mercenary + ". " + levelLabel + " " + std::to_string(level);
}

void Announce(const Vitals::Alert& alert, const Vitals::MercenaryMeter::Alert& mercenary) {
	if (!alert.Any() && mercenary.life < 0 && mercenary.level < 0) return;
	const auto& strings = Strings::Current();
	std::string text;
	if (alert.life >= 0) Append(text, FormattedPercent(strings.vitalsLife, alert.life));
	if (alert.mana >= 0) Append(text, FormattedPercent(strings.vitalsMana, alert.mana));
	if (mercenary.life >= 0) Append(text, FormattedPercent(strings.mercenaryLife, mercenary.life));
	if (mercenary.level > 0) Append(text, MercenaryLevel(mercenary.level));
	AsyncLog::InfoF(g_context, "[vitals] alert life=%d mana=%d mercenary_life=%d mercenary_level=%d interrupt=%d",
		alert.life, alert.mana, mercenary.life, mercenary.level, alert.interrupt || mercenary.interrupt);
	if (!text.empty()) ScreenReader::Speak(text, alert.interrupt || mercenary.interrupt);
}

// Says which displayed orb value is unavailable, after persistent absence or
// an explicit report request. The labels come from the game's current locale.
void HintOrbText(unsigned missing, bool force = false) {
	if (!missing || (!force && missing == g_hintedMask)) return;
	const auto life = Localization::Text(LifeOrbTextOption);
	const auto mana = Localization::Text(ManaOrbTextOption);
	if (((missing & 1U) && life.empty()) || ((missing & 2U) && mana.empty())) return;
	g_hintedMask = missing;
	char phrase[300]{};
	if (missing == 3U)
		std::snprintf(phrase, sizeof(phrase), Strings::Current().vitalsOrbTextMissing, life.c_str(), mana.c_str());
	else
		std::snprintf(phrase, sizeof(phrase), Strings::Current().vitalsOrbTextMissingOne,
			(missing & 1U) ? life.c_str() : mana.c_str());
	AsyncLog::InfoF(g_context, "[vitals] orb values unreadable mask=%u", missing);
	ScreenReader::Speak(phrase, false);
}

// Life and mana on demand. The automatic announcements only speak when a
// step is crossed, so without a key there is no way to ask "where am I now";
// this reads the same two orbs at the moment the key is pressed. Life or Mana
// asks for that orb alone (the keyboard's Z and X), and nothing else with it.
void Report(const D2RL::PluginContext* context, Command what = Command::Report) {
	const auto& strings = Strings::Current();
	const NativeMemory::ReadScope scope;
	if (!NativeWidgets::IsAvailable() || !NativeWidgets::FindPanel(HudPanel)) {
		// Outside a game the orbs do not exist at all.
		AsyncLog::Info(context, "[vitals] report: no HUD");
		ScreenReader::Speak(strings.unavailable, true);
		return;
	}
	const int life = ReadPercent(LifePath), mana = ReadPercent(ManaPath);
	AsyncLog::InfoF(context, "[vitals] report life=%d mana=%d", life, mana);
	if (what == Command::Life || what == Command::Mana) {
		const bool wantsLife = what == Command::Life;
		const int percent = wantsLife ? life : mana;
		if (percent >= 0) ScreenReader::Speak(FormattedPercent(wantsLife ? strings.vitalsLife : strings.vitalsMana, percent), true);
		else HintOrbText(wantsLife ? 1U : 2U, true);
		return;
	}
	const unsigned missing = (life < 0 ? 1U : 0U) | (mana < 0 ? 2U : 0U);
	std::string text;
	if (life >= 0) text = FormattedPercent(strings.vitalsLife, life);
	if (mana >= 0) {
		if (!text.empty()) text += ". ";
		text += FormattedPercent(strings.vitalsMana, mana);
	}
	if (g_statusSupported && NativeStatus::PlayerPoisoned() == true) {
		if (!text.empty()) text += ". ";
		text += strings.playerPoisoned;
	}
	if (const auto movement = g_statusSupported ? NativeStatus::PlayerMovement() : std::nullopt) {
		const int stamina = StaminaPercent(*movement);
		if (stamina >= 0 && stamina < 100) Append(text, FormattedPercent(strings.staminaPercent, stamina));
	}
	Append(text, AmmoReport(context));
	if (g_statusSupported) {
		const auto mercenary = NativeStatus::MercenaryLife();
		if (mercenary) {
			if (mercenary->lifePercent >= 0) {
				if (!text.empty()) text += ". ";
				text += FormattedPercent(strings.mercenaryLife, mercenary->lifePercent);
			}
			if (mercenary->level > 0) Append(text, MercenaryLevel(mercenary->level));
		}
	}
	// Asked for, so it interrupts: the player is waiting for this answer.
	if (!text.empty()) ScreenReader::Speak(text, true);
	HintOrbText(missing, true);
}

// What the character is wearing, logged only when it changes.
//
// The mod cannot write a save: character_saves.cpp opens a .d2s GENERIC_READ
// and reads its 315-byte header, native_memory.h has no write primitive at
// all, and every inventory call here is a read -- createItem, editItem,
// destroyItem and executeLocalPlayerMove appear nowhere in this plugin. None
// of that is a record, though. When the user's equipment went missing between
// two loads there was nothing in the log to say when, because nothing had
// ever logged what was worn. One line per change fixes that: the next time
// anything changes, the log has the second it happened and everything the mod
// was doing around it.
void LogWorn(const D2RL::PluginContext* context, const std::vector<Vitals::Breakage::Item>& worn) {
	std::vector<std::pair<int, uint32_t>> slots;
	slots.reserve(worn.size());
	for (const auto& item : worn) slots.emplace_back(item.bodyLocation, item.id);
	std::sort(slots.begin(), slots.end());
	std::string census;
	for (const auto& [location, id] : slots) {
		char entry[32]{};
		std::snprintf(entry, sizeof(entry), "%d:%u ", location, id);
		census += entry;
	}
	if (census == g_worn) return;
	g_worn = census;
	AsyncLog::InfoF(context, "[equip] worn %zu -- %s", slots.size(),
		census.empty() ? "NOTHING EQUIPPED" : census.c_str());
}

void SweepDurability(const D2RL::PluginContext* context) {
	std::vector<Vitals::Breakage::Item> worn;
	switch (EquipmentCensus::Read(context, worn)) {
	case EquipmentCensus::Result::Unavailable: return;
	case EquipmentCensus::Result::NoPlayer: g_breakage.Reset(); g_worn.clear(); return;
	case EquipmentCensus::Result::Read: break;
	}
	LogWorn(context, worn);
	const auto hands = InHand(worn);
	std::vector<Vitals::AmmoMeter::Stack> stacks;
	for (const auto& [stack, item] : hands) stacks.push_back(stack);
	if (const auto low = g_ammo.Update(stacks); low && g_ammoAlerts)
		for (const auto& [held, item] : hands)
			if (held.id == low->id) {
				AsyncLog::InfoF(context, "[vitals] stack in hand low: id=%u quantity=%d", low->id, low->quantity);
				ScreenReader::Speak(AmmoPhrase(*item, low->quantity), false);
			}
	const auto& labels = Strings::Current().equipmentSlots;
	for (const int location : g_breakage.Sweep(worn)) {
		const char* slot = location > 0 && static_cast<size_t>(location) < labels.size()
			? labels[location] : Strings::Current().equipment;
		char phrase[200]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().equipmentBroken, slot);
		AsyncLog::InfoF(context, "[vitals] worn item broken at body location %d", location);
		ScreenReader::Speak(phrase, false);
	}
}

auto __cdecl OnAction(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* userData) noexcept
	-> D2RL::Input::ActionResult {
	if (event == nullptr || event->kind != D2RL::Input::ActionEventKind::Pressed) {
		return D2RL::Input::ActionResult::Ignored;
	}
	// A letter typed into a box of text the keyboard's cursor is on is the box's.
	if (KeyboardMenu::Typing()) return D2RL::Input::ActionResult::Ignored;
	g_command = static_cast<Command>(reinterpret_cast<uintptr_t>(userData));
	return D2RL::Input::ActionResult::Handled;
}

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	Reset();
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "status") g_command = Command::Status;
	else if (arg == "report") g_command = Command::Report;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_statusSupported = NativeStatus::Initialize(context);
	(void)context->RegisterConsoleCommand("access-vitals", Console, "Life, mana and broken equipment announcements: on|off|status|report.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		using D2RL::Input::Key;
		struct Entry { const char* id; const char* label; Key key; Command command; };
		// Z and X are the keyboard's own: each orb by itself, and no key for
		// both (the user, 2026-09-27). R is left to the game.
		for (const Entry& entry : {
			Entry{"vitals-life", "Say life", Key::Z, Command::Life},
			Entry{"vitals-mana", "Say mana", Key::X, Command::Mana}}) {
			const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
				.logicalId = entry.id, .displayName = entry.label, .category = "Accessibility",
				.defaultPrimary = {entry.key, D2RL::Input::Modifier::None}, .callback = OnAction,
				.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entry.command))};
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			// Both keys are the game's own as well (Gordan.key: Z is its action 43); a refusal must show.
			const auto result = g_input->registerAction(context, &action, &handle);
			if (result == D2RL::Input::Result::Success) {
				g_actions.push_back(handle);
			} else {
				AsyncLog::WarnF(context, "[vitals] %s was refused by the loader: result %u", entry.id, static_cast<unsigned>(result));
			}
		}
	}
	if (context->QueryService(&g_lifecycle) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) {
		for (const auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady}) {
			const D2RL::Lifecycle::GameplayEventListener listener{
				.structSize = D2RL::Lifecycle::GameplayEventListenerSize,
				.kind = kind, .callback = OnGameplay};
			D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
			if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) == D2RL::Lifecycle::Result::Success)
				g_listeners.push_back(handle);
			else AsyncLog::Warn(context, "[vitals] Gameplay lifecycle baseline reset unavailable.");
		}
	} else g_lifecycle = nullptr;
	AsyncLog::Info(context, "[vitals] Life, mana and equipment announcements registered.");
	return true;
}

void Unregister() {
	if (g_lifecycle && g_context)
		for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	g_lifecycle = nullptr;
	if (g_input && g_context)
		for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	g_input = nullptr;
	g_context = nullptr;
	g_statusSupported = false;
	g_command = Command::None;
	Reset();
}

void Reset() {
	g_alerts.Reset();
	g_orbAvailability.Reset();
	g_poison.Reset();
	g_mercenary.Reset();
	g_breakage.Reset();
	g_gait.Reset();
	g_stamina.Reset();
	g_ammo.Reset();
	g_worn.clear();
	g_hintedMask = 0;
	g_lastMercSource = {};
	g_lastMercId = 0;
	g_nextOrb = 0;
	g_nextDurability = 0;
}

namespace {
// BACK+X on the controller, while no menu is up (the user, 2026-09-21: BACK
// alone became the modifier, "back+X: speak life+mana as it does now"). The
// user reports BACK does nothing in play, so holding it costs the game
// nothing; the X press is kept from the game during play by pad_filter.h.
bool BackRequested() {
	// Always taken, so a press made in a menu cannot be heard later.
	const bool pressed = InputDirection::TakeBackWith(InputDirection::FaceButton::X);
	return pressed && RadarUi::InPlay();
}
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	// Always taken, so a press made in a menu is never heard later.
	if (InputDirection::TakeLeftStickClick() && RadarUi::InPlay()) g_runToggled = true;
	if (command == Command::Life || command == Command::Mana) Report(context, command);
	else if (command == Command::Report || BackRequested()) Report(context);
	if (command == Command::On || command == Command::Off) {
		g_enabled = command == Command::On;
		Reset();
	}
	// Report answers with the reading itself, so it must not also say whether
	// the automatic announcements are on.
	if (command == Command::On || command == Command::Off || command == Command::Status)
		ScreenReader::Speak(g_enabled ? Strings::Current().vitalsEnabled : Strings::Current().vitalsDisabled, true);
	if (!g_enabled || !NativeWidgets::IsAvailable()) {
		g_runToggled = false;
		return;
	}
	const uint64_t now = GetTickCount64();
	if (now >= g_nextOrb) {
		g_nextOrb = now + OrbSampleMs;
		const NativeMemory::ReadScope scope;
		if (!NativeWidgets::FindPanel(HudPanel)) {
			// Outside a game the orbs do not exist: take a fresh baseline on
			// the next character instead of comparing against the last one.
			g_alerts.Reset();
			g_orbAvailability.Reset();
			g_poison.Reset();
			g_mercenary.Reset();
			g_gait.Reset();
			g_stamina.Reset();
			g_hintedMask = 0;
			g_lastMercSource = {};
			g_lastMercId = 0;
		} else {
			const int life = ReadPercent(LifePath), mana = ReadPercent(ManaPath);
			const auto playerAlert = g_alerts.Update(life, mana);
			Vitals::MercenaryMeter::Alert mercenaryAlert;
			HintOrbText(g_orbAvailability.Update(life >= 0, mana >= 0));
			if (g_statusSupported) {
				const auto mercenary = NativeStatus::MercenaryLife();
				const std::string_view source = !mercenary ? "unavailable"
					: mercenary->lifePercent < 0 ? "life-unreadable" : "ready";
				const uint32_t id = mercenary ? mercenary->id : 0;
				if (source != g_lastMercSource || id != g_lastMercId) {
					g_lastMercSource = source;
					g_lastMercId = id;
					AsyncLog::InfoF(context, "[vitals] mercenary source=%s id=%u life_percent=%d level=%d",
						source.data(), mercenary ? mercenary->id : 0,
						mercenary ? mercenary->lifePercent : -1, mercenary ? mercenary->level : -1);
				}
				mercenaryAlert = g_mercenary.Update(
					mercenary ? std::optional<uint32_t>(mercenary->id) : std::nullopt,
					mercenary ? mercenary->lifePercent : -1, mercenary ? mercenary->level : -1);
			}
			Announce(playerAlert, mercenaryAlert);
			if (g_statusSupported) {
				const auto changed = g_poison.Update(NativeStatus::PlayerPoisoned());
				if (changed) ScreenReader::Speak(*changed ? Strings::Current().playerPoisoned
					: Strings::Current().playerPoisonCleared, false);
				if (g_movementAlerts) AnnounceMovement(context, g_runToggled);
				g_runToggled = false;
			}
		}
	}
	if (now >= g_nextDurability) {
		g_nextDurability = now + DurabilitySampleMs;
		const NativeMemory::ReadScope scope;
		SweepDurability(context);
	}
}


// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled() { return g_enabled; }
void SetEnabled(bool enabled) { g_command = enabled ? Command::On : Command::Off; }
bool MovementAlerts() { return g_movementAlerts; }
void SetMovementAlerts(bool enabled) {
	g_movementAlerts = enabled;
	g_gait.Reset();
	g_stamina.Reset();
}
bool AmmoAlerts() { return g_ammoAlerts; }
void SetAmmoAlerts(bool enabled) { g_ammoAlerts = enabled; }
}
