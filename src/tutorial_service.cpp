#include "tutorial_service.h"
#include "async_log.h"
#include "character_stats.h"
#include "combat_target.h"
#include "controller_text.h"
#include "corpse_memory.h"
#include "entity_cue_audio.h"
#include "entity_radar.h"
#include "ground_items.h"
#include "input_direction.h"
#include "item_audio.h"
#include "localization.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_quest_flags.h"
#include "native_status.h"
#include "native_widgets.h"
#include "native_world.h"
#include "panel_state.h"
#include "radar.h"
#include "radar_audio.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "skill_bar_service.h"
#include "speech_priority.h"
#include "speech_strings.h"
#include "town_npcs.h"
#include "tutorial.h"
#include "tutorial_records.h"
#include "tutorial_text.h"

#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace D2RAccess::TutorialService {
namespace {
using EventKind = D2RL::Lifecycle::GameplayEventKind;
using Tutorial::Bit;
using Tutorial::Signal;
using Tutorial::Step;
enum class Command { None, On, Off, Previous, Next, First };
struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = -1; };
// Chime plays the message sound and keeps the text, Keep only keeps it; Say speaks it; Do runs `run`:
// the world's sounds let in, a step's own signal. Audition plays a lesson's `cues`, each after its name.
enum class ActionKind { Chime, Keep, Say, Do, StepDone, Audition };
struct Cue { std::string name; std::function<void()> play; };
// A `menu` action is said in the menu it is about; the rest wait for play.
struct Action {
	ActionKind kind;
	uint64_t delay = 0;
	std::string text;
	std::function<void()> run;
	bool menu = false;
	std::vector<Cue> cues;
};

// Levels.txt rows (src/generated/level_flags.h): Act 1 - Town; Act 1 - Wilderness 1, the
// Blood Moor; Act 1 - Wilderness 2, the Cold Plains; Act 1 - Cave 1, the Den of Evil.
constexpr int RogueEncampment = 1, BloodMoor = 2, ColdPlains = 3, DenOfEvil = 8;
// game-data/global/excel/skills.txt Ids: TownPortal, the pseudo-skill a bar holds, and the
// scroll's and the tome's, which the bar's getter turns it into (docs/skill-bar.md).
constexpr int TownPortalSkill = 359, PortalScrollSkill = 219, PortalTomeSkill = 220;
// The game's own names (item-names.json, ui.json), so Spain's and Mexico's Spanish each read theirs.
constexpr std::string_view PortalTomeKey = "tbk", IdentifyTomeKey = "ibk", GeneralSkillsKey = "GeneralSkills";
// The waypoint menu (WaypointsPanelExpansion, src/interaction_reader.cpp) and a shop's.
constexpr std::string_view WaypointPanelPrefix = "WaypointsPanel", VendorPanel = "VendorPanelLayout";
// Items of the world's lesson, in the player's item sounds (misc.txt, weapons.txt): gold, a
// minor healing potion, a short sword.
constexpr uint32_t GoldCode = D2RL::Items::MakeItemCode("gld"), PotionCode = D2RL::Items::MakeItemCode("hp1"),
	SwordCode = D2RL::Items::MakeItemCode("ssd");
// Akara's and Act I's Warriv's rows in game-data/global/excel/monstats.txt.
constexpr uint32_t AkaraMonStats = 148, WarrivMonStats = 155;
// The Den of Evil's quest record row: Act I's intro is row 0 in the save's order (docs/quests.md).
constexpr int ActOneIntroRow = 0, DenOfEvilRow = 1, ActOneLastRow = 6;
// MEASURED 2026-10-21 on two new characters: row 0 turns 0001 the moment Akara's dialogue opens
// (not Warriv's); the Den's row stays 0000 until the cave is clear (201e), then 2001 with the reward.
constexpr uint16_t IntroDoneBit = 0x0001;
constexpr uint16_t RewardPendingBit = 0x0002, PrimaryGoalBit = 0x2000;
constexpr std::string_view NpcDialogPanel = "NpcDialogPanel";
// The level-up lesson (the user, 2026-10-21): the character sheet, its attribute points and its
// lines, then the skill tree and its point, once the character has gone up a level and no enemy
// has been on screen for CalmMs. The game's own prompts name the pad's Menu button for both
// ("Press Menu to assign Stat Points" / "Skill Points", LevelUpStatsNotificationController and
// LevelUpNotificationController). Given once per character, kept as <key>#levelup=1.
constexpr int FirstLevelUp = 2;
constexpr uint64_t CalmMs = 3000;
constexpr std::string_view CharacterPanel = "CharacterStatsPanel", SkillTreePanel = "SkillsTreePanel";
constexpr std::string_view LevelUpSuffix = "#levelup";
// The inventory lesson (the user, 2026-10-21): walking the inventory and putting on what was picked
// up, and that an unidentified item cannot be worn, once the player picks up a weapon, armour, ring
// or amulet and it is calm. Kept as <key>#inventory=1.
constexpr std::string_view InventorySuffix = "#inventory";
// Gold picked up the first time outside the camp: the stash keeps it from death (a tester,
// 2026-10-21); a shop opened the first time: the tomes. Kept as <key>#gold=1 and <key>#shop=1.
constexpr std::string_view GoldSuffix = "#gold", ShopSuffix = "#shop";
constexpr std::string_view InventoryPanels[]{"PlayerInventoryExpansionLayout", "PlayerInventoryOriginalLayout"};
constexpr uint32_t WornTypes[]{D2RL::Items::MakeItemCode("weap"), D2RL::Items::MakeItemCode("armo"),
	D2RL::Items::MakeItemCode("ring"), D2RL::Items::MakeItemCode("amul")};
// Quest flags, gold and the roster are read at most this often.
constexpr uint64_t PollMs = 500;
// The step-done sound, then the next message; a message's chime, then its words.
constexpr uint64_t AfterStepMs = 1500, ChimeMs = 600;
// After the wall that ends the radar step, the radar's sounds die down before the world's
// lesson begins, so the two are not mixed (the user, 2026-10-21); then the world's sounds
// play around the player this long before the scanner's step.
constexpr uint64_t SettleMs = 4000, ListenMs = 8000;
// The game's arrival news -- the area's name, an NPC near, the radar's new places -- came
// 0.5 to 4.5 s after the player was ready, and the area's name cut the welcome off
// (MEASURED 2026-10-21). The first message waits past most of it, and every message
// waits for other speech to pause, so it is queued behind what is said, not cut by it.
// In a fight speech may never pause that long: a message waits PatienceMs at most, then goes in
// line behind it (a tester, 2026-10-21: the Den's messages never came).
constexpr uint64_t ArrivalMs = 3000;
using SpeechPriority::QuietMs, SpeechPriority::PatienceMs;
// What the tutorial says lasts about a screen reader's default pace, which news waits out. A
// lesson's sounds wait for the words before them guessed a little faster (the user, 2026-10-21:
// the wait before the radar's sounds was long), at least CueAfterNameMs; then CueGapMs to the next.
constexpr uint64_t MsPerCharacter = 60, ReadMsPerCharacter = 45, CueAfterNameMs = 1200, CueGapMs = 1400;
uint64_t ReadMs(std::string_view text) { return std::max<uint64_t>(CueAfterNameMs, text.size() * ReadMsPerCharacter); }

std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
std::atomic<bool> g_enabled{true};
// A lesson's sounds wait for the player after each one (tutorial-sound-wait, a tester, 2026-10-21);
// off, they play in turn, as the user chose, and "again" holds them.
std::atomic<bool> g_waitSounds{false};

Tutorial::Guide g_guide;
Tutorial::History g_history;
std::map<size_t, std::function<void()>> g_sounds; // a history message's sound, played when it is read back
Tutorial::Audition g_audition;
size_t g_named = 0; // the audition's names kept in the history, so one said again is kept once
struct Later { uint64_t due = 0; std::function<void()> play; };
Later g_browseSound; // a message read back plays its sound after its words
std::deque<Action> g_queue;
uint64_t g_due = 0, g_session = 0, g_lastPoll = 0;
uint64_t g_speechEnd = 0; // when what the tutorial said should be over, guessed
SpeechPriority::Pause g_pause{QuietMs, PatienceMs}; // a message waits for a pause in speech
bool g_active = false, g_decided = false;
bool g_worldQuiet = false; // the world's sounds wait for their lesson (Tutorial::QuietWorld)
// The level-up lesson: waiting for its moment; said; in the character sheet; the sheet shut before
// the tree, which is then said; in the skill tree; given.
enum class Lesson { Waiting, Told, Sheet, TreeTold, Tree, Given };
Lesson g_lesson = Lesson::Waiting;
uint64_t g_calmSince = 0; // since no enemy has been on screen; 0 while one is
bool g_sawPoints = false, g_toldSwitch = false; // in the sheet: points shown, the way to the tree said
bool g_sawSkillPoints = false, g_toldBind = false; // in the tree: a point shown, how to bind a skill said
// The inventory lesson: waiting for something to wear; said; in the inventory; given.
enum class Kit { Waiting, Told, Open, Given };
Kit g_kit = Kit::Waiting;
std::set<uint32_t> g_knownItems; // the player's items when the lesson was armed and since
bool g_kitArmed = false, g_toldWorn = false;
uint32_t g_newItem = 0; // what was picked up to wear, 0 while nothing is
// The gold lesson: waiting for gold; gold taken, waiting for calm; given. The shop's is said in the shop.
enum class News { Waiting, Armed, Given };
News g_goldNews = News::Waiting;
bool g_shopGiven = false;
std::string g_name, g_key;
Tutorial::Signals g_states = 0;
int g_gold = -1;
std::optional<uint64_t> g_experience;
std::string g_questLog; // Act I's quest rows last logged

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == EventKind::GameJoined || event->kind == EventKind::GameLeft) { g_state = {.session = event->sessionGeneration}; }
	if (event->kind == EventKind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; }
	if (event->kind == EventKind::LevelChanged) g_state.level = event->currentValue;
}

auto OnKey(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* data) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)
		|| event->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	g_command = static_cast<Command>(reinterpret_cast<uintptr_t>(data));
	return D2RL::Input::ActionResult::Handled;
}

const TutorialText::Texts& Texts() { return TutorialText::For(Strings::Selected()); }

bool Pad() { return InputDirection::LeftStick().connected; }
// The pad button the town portal is on, as the tester has it (LT X on a new character); empty when unbound.
std::string PortalButton() { return SkillBarService::ButtonOf({TownPortalSkill, PortalScrollSkill, PortalTomeSkill}); }

// The pad's words while a pad is connected, the keyboard's otherwise; places and things by the game's own names.
std::string Render(const TutorialText::Lines& lines) {
	const std::string_view text = Pad() ? lines.pad : lines.keys;
	TutorialText::Names names{NativeEntities::LevelName(DenOfEvil), NativeEntities::LevelName(BloodMoor),
		NativeEntities::LevelName(ColdPlains), Localization::Text(PortalTomeKey), Localization::Text(IdentifyTomeKey),
		Localization::Text(GeneralSkillsKey), {}};
	if (text.find("{portal}") != std::string_view::npos) names.portal = PortalButton();
	return TutorialText::Render(text, Texts(), InputDirection::RouteOnA(), names,
		[](uint32_t glyph) { return std::string(ControllerText::Button(glyph)); });
}
const TutorialText::Lines& StepLines(Step step) { return Texts().steps[static_cast<size_t>(step)]; }

void Push(Action action) {
	if (g_queue.empty()) g_due = GetTickCount64() + action.delay;
	g_queue.push_back(std::move(action));
}
// Said in the menu it is about, ahead of whatever waits for play; `said` runs once it is, and is
// dropped with it if the menu shuts first.
void MenuMessage(std::string text, std::function<void()> said = {}) {
	if (said) g_queue.push_front({ActionKind::Do, 0, {}, std::move(said), true});
	g_queue.push_front({ActionKind::Say, ChimeMs, text, {}, true});
	g_queue.push_front({ActionKind::Chime, 0, std::move(text), {}, true});
	g_due = GetTickCount64();
}
// Without the chime when it follows a message straight on, so no sound falls on its words.
void Message(std::string text, uint64_t delay, bool chime = true) {
	Push({chime ? ActionKind::Chime : ActionKind::Keep, delay, text});
	Push({ActionKind::Say, chime ? ChimeMs : 0, std::move(text)});
}
// One radar reading straight up, as the sound glossary plays it; `unexplored` adds the double pip.
std::function<void()> RadarCue(Radar::State state, bool unexplored = false, bool small = false) {
	return [=] {
		Radar::Events events{};
		Radar::Marks marks{}, smallMarks{};
		const auto up = static_cast<size_t>(Radar::Direction::Up);
		events[up] = state;
		marks[up] = unexplored;
		smallMarks[up] = small;
		RadarAudio::Play(events, {.eventMs = GetTickCount64(), .audition = true}, marks, smallMarks);
	};
}
std::function<void()> EntityCue(EntityCueAudio::Sound sound) { return [=] { EntityCueAudio::Play(sound, {}); }; }
std::function<void()> PlaceCue(GroundItems::Sound sound) { return [=] { ItemAudio::Preview(sound); }; }
// An item as it sounds in the item sounds chosen when it plays (a tester, 2026-10-21).
std::function<void()> ItemCue(uint32_t code, GroundItems::Base base) {
	return [=] { ItemAudio::Preview(GroundItems::Heard(code, static_cast<uint32_t>(D2RL::Items::Quality::Normal), base)); };
}

template <size_t N>
std::vector<Cue> Named(const char* const (&names)[N], std::array<std::function<void()>, N> sounds) {
	std::vector<Cue> cues;
	for (size_t at = 0; at < N; ++at) cues.push_back({names[at], std::move(sounds[at])});
	return cues;
}
// The sounds a step teaches, in the order of TutorialText::Texts::cues, worldCues and enemyCues.
std::vector<Cue> SoundsOf(Step step) {
	using S = Radar::State;
	using E = EntityCueAudio::Sound;
	using G = GroundItems::Sound;
	using B = GroundItems::Base;
	const auto& texts = Texts();
	switch (step) {
	case Step::Radar:
		return Named(texts.cues, std::array<std::function<void()>, TutorialText::CueCount>{RadarCue(S::Exit),
			RadarCue(S::Exit, true), RadarCue(S::Open), RadarCue(S::Open, true), RadarCue(S::Blocked), RadarCue(S::Door),
			RadarCue(S::Blocked, false, true)});
	case Step::Sounds:
		return Named(texts.worldCues, std::array<std::function<void()>, TutorialText::WorldCueCount>{EntityCue(E::Npc),
			EntityCue(E::NpcAlert), PlaceCue(G::Stash), PlaceCue(G::Portal), PlaceCue(G::ZoneTransition),
			PlaceCue(G::UnexploredRoom), ItemCue(GoldCode, B::Gold), ItemCue(PotionCode, B::HealthPotion),
			ItemCue(SwordCode, B::Equipment)});
	case Step::LeaveTown:
		return Named(texts.enemyCues, std::array<std::function<void()>, TutorialText::EnemyCueCount>{EntityCue(E::Ordinary),
			EntityCue(E::Ranged), EntityCue(E::Resurrector), EntityCue(E::Important), EntityCue(E::Reach)});
	default: return {};
	}
}
bool HasSounds(Step step) { return step == Step::Radar || step == Step::Sounds || step == Step::LeaveTown; }
// What to do once a step's sounds are heard.
const TutorialText::Lines* TaskAfterSounds(Step step) {
	const auto& texts = Texts();
	return step == Step::Radar ? &texts.radarTask : step == Step::Sounds ? &texts.worldTask
		: step == Step::LeaveTown ? &texts.leaveTask : nullptr;
}
// A step's words; one that plays sounds also says how to hear them again.
std::string StepText(Step step) {
	auto text = Render(StepLines(step));
	if (HasSounds(step)) text += " " + Render(g_waitSounds ? Texts().auditionWait : Texts().auditionAgain);
	return text;
}
void KeepCue(Cue cue) {
	g_history.Add(std::move(cue.name));
	g_sounds[g_history.Size() - 1] = std::move(cue.play);
}
// A step already given, kept unspoken for reading back, its sounds with it.
void KeepStep(Step step) {
	g_history.Add(StepText(step));
	for (auto& cue : SoundsOf(step)) KeepCue(std::move(cue));
	if (const auto* task = TaskAfterSounds(step)) g_history.Add(Render(*task));
}

// A step's message. One with sounds plays them, each after its name, then says what to do; the
// world's then lets the sounds around the player in and, after a while, ends its step.
void QueueStep(Step step, uint64_t delay, bool chime = true) {
	const auto text = StepText(step);
	Message(text, delay, chime);
	if (auto cues = SoundsOf(step); !cues.empty()) {
		Push({.kind = ActionKind::Audition, .delay = ReadMs(text), .cues = std::move(cues)});
		const auto task = Render(*TaskAfterSounds(step));
		Message(task, 0);
		if (step == Step::Sounds) {
			Push({ActionKind::Do, ReadMs(task), {}, [] { g_worldQuiet = false; }});
			Push({ActionKind::Do, ListenMs, {}, [] { Tutorial::Post(Signal::HeardSounds); }});
		}
	}
	// The quick way back, on the button the game bound it to (a tester, 2026-10-21).
	if (step == Step::ReturnToTown && Pad() && !PortalButton().empty()) Message(Render(Texts().portalButton), 0, false);
}

// The front action's sounds at their own pace, each name kept for reading back; true once all are heard.
bool Audit(Action& action, uint64_t now) {
	if (g_audition.Idle()) {
		std::vector<uint64_t> reads;
		for (const auto& cue : action.cues) reads.push_back(ReadMs(cue.name));
		g_audition.Start(std::move(reads), now, CueGapMs, g_waitSounds);
		g_named = 0;
	}
	const auto due = g_audition.Tick(now);
	if (due.cue == Tutorial::Audition::Cue::Name) {
		const auto& cue = action.cues[due.index];
		SpeechPriority::Say(SpeechPriority::Kind::Tutorial, cue.name);
		g_speechEnd = std::max(g_speechEnd, now) + cue.name.size() * MsPerCharacter;
		if (due.index == g_named) { KeepCue(cue); ++g_named; }
	}
	if (due.cue == Tutorial::Audition::Cue::Sound) action.cues[due.index].play();
	if (!g_audition.Done()) return false;
	g_audition.Reset();
	return true;
}
// "Previous" plays the last sound again and holds the lesson; "next" goes on.
void Steer(Command command, Action& action, uint64_t now) {
	if (command == Command::Next) { g_audition.Next(now); return; }
	const auto heard = g_audition.Again();
	if (!heard) return;
	ScreenReader::Stop();
	action.cues[*heard].play();
}

void Perform(const Action& action) {
	switch (action.kind) {
	case ActionKind::Chime: ItemAudio::PlayTutorialMessage(); g_history.Add(action.text); break;
	case ActionKind::Keep: g_history.Add(action.text); break;
	case ActionKind::Say:
		SpeechPriority::Say(SpeechPriority::Kind::Tutorial, action.text);
		g_speechEnd = std::max(g_speechEnd, GetTickCount64()) + action.text.size() * MsPerCharacter;
		break;
	case ActionKind::Do: if (action.run) action.run(); break;
	case ActionKind::StepDone: ItemAudio::PlayTutorialStep(); break;
	}
}

// The step reached by the tutorial running now.
void Save() {
	if (g_key.empty() || !g_active) return;
	TutorialRecords::SetStep(g_key, static_cast<int>(g_guide.Current()));
}

void ClearHistory() {
	g_history.Clear();
	g_sounds.clear();
	g_browseSound = {};
}

void Stop() {
	g_active = false;
	g_queue.clear();
	g_audition.Reset();
	g_speechEnd = g_pause.heldSince = 0;
	Tutorial::SetQuietWorld(false);
	Tutorial::SetBusy(false);
}

void Begin(Step step) {
	g_active = true;
	g_worldQuiet = step <= Step::Sounds;
	g_lesson = TutorialRecords::Given(g_key, LevelUpSuffix) ? Lesson::Given : Lesson::Waiting;
	g_calmSince = 0;
	g_sawPoints = g_toldSwitch = false;
	g_sawSkillPoints = g_toldBind = false;
	g_kit = TutorialRecords::Given(g_key, InventorySuffix) ? Kit::Given : Kit::Waiting;
	g_knownItems.clear();
	g_kitArmed = g_toldWorn = false;
	g_newItem = 0;
	g_goldNews = TutorialRecords::Given(g_key, GoldSuffix) ? News::Given : News::Waiting;
	g_shopGiven = TutorialRecords::Given(g_key, ShopSuffix);
	g_guide.Start(step);
	ClearHistory();
	g_queue.clear();
	g_audition.Reset();
	g_gold = -1;
	g_experience.reset();
	(void)Tutorial::TakePosted();
	const auto& texts = Texts();
	// A resumed tutorial keeps the messages already given, unspoken, for reading back.
	const bool fresh = step == Step::Vitals;
	if (fresh) Message(Render(texts.welcome), ArrivalMs);
	else {
		g_history.Add(Render(texts.welcome));
		for (size_t done = 0; done < static_cast<size_t>(step); ++done) KeepStep(static_cast<Step>(done));
	}
	QueueStep(step, fresh ? 0 : ArrivalMs, !fresh);
	Save();
	AsyncLog::InfoF(g_context, "[tutorial] %s at step %u for %s", step == Step::Vitals ? "begins" : "resumes",
		static_cast<unsigned>(step), g_name.c_str());
}

// Once per game: the character's record, else a fresh level 1 character playing alone.
void Decide(const Lifecycle& state) {
	std::vector<CorpseMemory::Owner> roster;
	if (!NativeEntities::ReadRosterCorpses(state.player, roster)) return;
	const auto self = std::find_if(roster.begin(), roster.end(), [&](const auto& owner) { return owner.id == state.player; });
	if (self == roster.end() || self->name.empty()) return;
	if (std::any_of(roster.begin(), roster.end(), [&](const auto& owner) { return owner.id != state.player; })) {
		g_decided = true;
		AsyncLog::Info(g_context, "[tutorial] other players in the game: not offered.");
		return;
	}
	g_name = self->name;
	g_key = TutorialRecords::Key(g_name);
	if (const auto dropped = TutorialRecords::DropEarlier(g_name, g_key))
		AsyncLog::InfoF(g_context, "[tutorial] %zu record(s) of an earlier %s dropped", dropped, g_name.c_str());
	if (const auto step = TutorialRecords::Step(g_key)) {
		g_decided = true;
		if (*step < static_cast<int>(Step::Finished)) Begin(static_cast<Step>(*step));
		return;
	}
	const auto experience = NativeStatus::PlayerExperience();
	if (!experience) return;
	g_decided = true;
	if (experience->level == 1) Begin(Step::Vitals);
}

// Act I's quest rows, logged as they change: which row and bits the Den's steps set is not
// measured yet. The first live test (2026-10-21) saw row 0 go 0000 -> 0001 as Akara gave her
// quest and row 1 stay 0000, so the Den may be row 0 (docs/tutorial.md).
void LogQuestRows() {
	std::string rows;
	for (int row = ActOneIntroRow; row <= ActOneLastRow; ++row) {
		const auto flags = NativeQuestFlags::Read(row);
		char word[8]{};
		std::snprintf(word, sizeof(word), flags ? "%04x " : "---- ", flags ? *flags : 0u);
		rows += word;
	}
	if (rows == g_questLog) return;
	g_questLog = rows;
	AsyncLog::InfoF(g_context, "[tutorial] quest rows 0-%d: %s", ActOneLastRow, rows.c_str());
}

// What holds now: where the player is and how far the Den of Evil's quest has come.
Tutorial::Signals States(const Lifecycle& state) {
	Tutorial::Signals states = 0;
	if (state.level == BloodMoor) states |= Bit(Signal::InBloodMoor);
	if (state.level == DenOfEvil) states |= Bit(Signal::InDen);
	if (state.level == RogueEncampment) states |= Bit(Signal::InTown);
	const auto flags = NativeQuestFlags::Read(DenOfEvilRow);
	LogQuestRows();
	// Akara has spoken, and Warriv is behind: a game resumed after it passes both talks.
	if (const auto intro = NativeQuestFlags::Read(ActOneIntroRow); intro && (*intro & IntroDoneBit))
		states |= Bit(Signal::TalkedToAkara) | Bit(Signal::TalkedToWarriv);
	if (flags) {
		// Akara's quest given is past Warriv too.
		if (EntityRadar::QuestRunning(*flags) || (*flags & EntityRadar::QuestDoneBits))
			states |= Bit(Signal::TalkedToAkara) | Bit(Signal::TalkedToWarriv);
		if (*flags & (RewardPendingBit | PrimaryGoalBit | EntityRadar::QuestDoneBits)) states |= Bit(Signal::DenCleared);
		if (*flags & EntityRadar::QuestDoneBits) states |= Bit(Signal::DenDone);
	}
	return states;
}

// An NPC's menu open: whose is the nearest town NPC's, as the game opens one only beside its NPC.
void WatchDialogue(const Lifecycle& state) {
	const auto& panels = PanelState::OpenPanels();
	if (std::find(panels.begin(), panels.end(), NpcDialogPanel) == panels.end()) return;
	Radar::World world;
	std::vector<EntityRadar::Entity> entities;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)
		|| !NativeEntities::Read(state.player, state.session, entities, false, false, false, true)) return;
	const EntityRadar::Entity* nearest = nullptr;
	double best = 0;
	for (const auto& entity : entities) {
		if (entity.kind != EntityRadar::Kind::Npc || !entity.placeId) continue;
		const double distance = std::hypot(entity.position.x - world.player.x, entity.position.y - world.player.y);
		if (!nearest || distance < best) { nearest = &entity; best = distance; }
	}
	if (!nearest) return;
	if (nearest->placeId == TownNpcs::Identity(WarrivMonStats)) Tutorial::Post(Signal::TalkedToWarriv);
	if (nearest->placeId == TownNpcs::Identity(AkaraMonStats)) Tutorial::Post(Signal::TalkedToAkara);
}

// Experience gained outside the camp: a monster killed, as nothing else gives it there so early.
// The fight step waited for gold, which may not drop for long (a tester, 2026-10-21).
void WatchKill(const Lifecycle& state) {
	const auto experience = NativeStatus::PlayerExperience();
	if (!experience) return;
	if (g_experience && experience->total > *g_experience && state.level != RogueEncampment) Tutorial::Post(Signal::Kill);
	g_experience = experience->total;
}

// Gold taken outside the camp, where selling would count too, arms the stash's lesson.
void WatchGold(const Lifecycle& state) {
	const int gold = NativeGroundItems::PlayerGold();
	if (gold < 0) return;
	if (g_gold >= 0 && gold > g_gold && state.level != RogueEncampment && g_goldNews == News::Waiting) g_goldNews = News::Armed;
	g_gold = gold;
}

bool PanelOpen(std::string_view prefix) {
	const auto& panels = PanelState::OpenPanels();
	return std::any_of(panels.begin(), panels.end(), [&](const auto& panel) { return std::string_view(panel).starts_with(prefix); });
}

// A waypoint's menu opens as it is touched; the camp's is the player's from the start.
void WatchWaypoint(const Lifecycle& state) {
	if (state.level != RogueEncampment && PanelOpen(WaypointPanelPrefix)) Tutorial::Post(Signal::Waypoint);
}

// The first shop: the tomes, said there (a tester, 2026-10-21: twenty loose scrolls fill the inventory).
void WatchShop() {
	if (g_shopGiven || !PanelOpen(VendorPanel)) return;
	g_shopGiven = true;
	MenuMessage(Render(Texts().shop), [] { TutorialRecords::Give(g_key, ShopSuffix); });
	AsyncLog::InfoF(g_context, "[tutorial] shop lesson for %s", g_name.c_str());
}

// Unread counts as not calm: the lesson never starts on a guess.
bool EnemyInSight(const Lifecycle& state) {
	Radar::World world;
	std::vector<EntityRadar::Entity> entities;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)
		|| !NativeEntities::Read(state.player, state.session, entities)) return true;
	return std::any_of(entities.begin(), entities.end(), [&](const auto& entity) {
		return entity.kind == EntityRadar::Kind::Enemy && CombatTarget::OnScreen(world.player, entity.position);
	});
}

// Waiting: how long it has been calm since the first level up. Then it follows the player through
// the menus without asking them to shut one first (the user, 2026-10-21): the sheet, said as it
// opens; once its points are spent, the way on to the skill tree; the tree, said as it opens; back
// in play from the tree, the lesson is given and the step it interrupted said again. Shut from the
// sheet before the tree, the tree is said in play instead.
void WatchLesson(uint64_t now) {
	if (g_lesson == Lesson::Waiting) return;
	if (g_lesson == Lesson::Given) return;
	const auto& texts = Texts();
	const auto& panels = PanelState::OpenPanels();
	const auto shown = [&](std::string_view panel) { return std::find(panels.begin(), panels.end(), panel) != panels.end(); };
	if (shown(SkillTreePanel)) {
		if (g_lesson != Lesson::Tree) { g_lesson = Lesson::Tree; MenuMessage(Render(texts.levelUpTree)); }
		// The point spent: how to put the skill on a button (the user, 2026-10-21).
		const bool points = CharacterStats::HasPoints(CharacterStats::SkillChoices(NativeWidgets::FindPanel(SkillTreePanel)));
		if (points) g_sawSkillPoints = true;
		else if (g_sawSkillPoints && !g_toldBind) { g_toldBind = true; MenuMessage(Render(texts.levelUpBind)); }
		return;
	}
	if (shown(CharacterPanel)) {
		if (g_lesson == Lesson::Told) { g_lesson = Lesson::Sheet; MenuMessage(Render(texts.levelUpSheet)); }
		if (g_lesson != Lesson::Sheet) return;
		// The points line is the game's own; spent, it is gone or says none are left.
		const bool points = CharacterStats::HasPoints(CharacterStats::Points(NativeWidgets::FindPanel(CharacterPanel)));
		if (points) g_sawPoints = true;
		else if (g_sawPoints && !g_toldSwitch) { g_toldSwitch = true; MenuMessage(Render(texts.levelUpSwitch)); }
		return;
	}
	// Between two menus neither may show for a moment: only play counts as shut.
	if ((g_lesson != Lesson::Sheet && g_lesson != Lesson::Tree) || !RadarUi::InPlay()) return;
	if (g_lesson == Lesson::Sheet) {
		g_lesson = Lesson::TreeTold;
		Message(Render(texts.levelUpTreeLater), AfterStepMs);
		return;
	}
	g_lesson = Lesson::Given;
	TutorialRecords::Give(g_key, LevelUpSuffix);
	// Not said in the tree (no point seen spent there): said now, before the lesson ends.
	if (!g_toldBind) Message(Render(texts.levelUpBind), AfterStepMs);
	Message(Render(texts.levelUpDone), AfterStepMs);
	if (!g_guide.Finished()) Message(Render(StepLines(g_guide.Current())), 0, false);
	AsyncLog::InfoF(g_context, "[tutorial] level-up lesson given to %s", g_name.c_str());
}

// One of the player's items, as the lesson needs it.
struct Held { uint32_t id = 0, classId = 0; D2RL::Items::ItemContainer container{}; bool identified = false; };
auto __cdecl CollectHeld(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* info, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* items = static_cast<std::vector<Held>*>(userData);
	if (items && info && info->structSize >= D2RL::Items::ItemInfoRequiredSize)
		items->push_back({info->runtimeId, info->classId, info->container, (info->stateFlags & D2RL::Items::ItemStateIdentified) != 0});
	return D2RL::Inventory::IterationAction::Continue;
}
// The backpack and what is worn, through the loader's inventory service.
std::optional<std::vector<Held>> ReadHeld() {
	using D2RL::Items::ContainerBit;
	using D2RL::Items::ItemContainer;
	const D2RL::InventoryService* inventory = nullptr;
	if (!g_context || g_context->QueryService(&inventory) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceField(inventory, D2RL::InventoryServiceRequiredSize)
		|| !inventory->getLocalPlayer || !inventory->forEachInventoryItem) return std::nullopt;
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(g_context, &player) != D2RL::Inventory::Result::Success || player == D2RL::InvalidPlayerHandle)
		return std::nullopt;
	const D2RL::Inventory::ItemFilter filter{.structSize = static_cast<uint32_t>(sizeof(D2RL::Inventory::ItemFilter)), .flags = 0,
		.containerMask = ContainerBit(ItemContainer::Inventory) | ContainerBit(ItemContainer::Equipment), .reserved = 0};
	std::vector<Held> items;
	if (inventory->forEachInventoryItem(g_context, player, &filter, CollectHeld, &items) != D2RL::Inventory::Result::Success)
		return std::nullopt;
	return items;
}
bool Wearable(uint32_t classId) {
	return std::any_of(std::begin(WornTypes), std::end(WornTypes), [&](uint32_t type) { return NativeGroundItems::ClassIsType(classId, type); });
}
bool InventoryShown() {
	const auto& panels = PanelState::OpenPanels();
	return std::any_of(std::begin(InventoryPanels), std::end(InventoryPanels),
		[&](std::string_view panel) { return std::find(panels.begin(), panels.end(), panel) != panels.end(); });
}

// Waiting, in play: something to wear the backpack did not hold before. Then it follows the player
// into the inventory, says when that item is worn, and is given back in play.
void WatchKit(uint64_t now) {
	if (g_kit == Kit::Given) return;
	using D2RL::Items::ItemContainer;
	if (g_kit == Kit::Waiting) {
		if (g_newItem || !RadarUi::InPlay()) return;
		const auto items = ReadHeld();
		if (!items) return;
		for (const auto& item : *items) {
			const bool fresh = g_knownItems.insert(item.id).second;
			if (fresh && g_kitArmed && item.container == ItemContainer::Inventory && Wearable(item.classId)) g_newItem = item.id;
		}
		g_kitArmed = true;
		return;
	}
	if (InventoryShown()) {
		if (g_kit == Kit::Told) { g_kit = Kit::Open; MenuMessage(Render(Texts().kitOpen)); }
		if (g_toldWorn) return;
		const auto items = ReadHeld();
		if (!items) return;
		const bool worn = std::any_of(items->begin(), items->end(),
			[](const Held& item) { return item.id == g_newItem && item.container == ItemContainer::Equipment; });
		if (worn) { g_toldWorn = true; MenuMessage(Render(Texts().kitWorn)); }
		return;
	}
	if (g_kit != Kit::Open || !RadarUi::InPlay()) return;
	g_kit = Kit::Given;
	TutorialRecords::Give(g_key, InventorySuffix);
	Message(Render(Texts().kitDone), AfterStepMs);
	if (!g_guide.Finished()) Message(Render(StepLines(g_guide.Current())), 0, false);
	AsyncLog::InfoF(g_context, "[tutorial] inventory lesson given to %s", g_name.c_str());
	(void)now;
}

// Calm: no enemy on screen since g_calmSince, while a lesson waits for its moment.
void WatchCalm(const Lifecycle& state, uint64_t now) {
	const auto experience = NativeStatus::PlayerExperience();
	const bool levelUp = g_lesson == Lesson::Waiting && experience && experience->level >= FirstLevelUp;
	const bool kit = g_kit == Kit::Waiting && g_newItem;
	const bool gold = g_goldNews == News::Armed;
	if ((!levelUp && !kit && !gold) || EnemyInSight(state)) g_calmSince = 0;
	else if (!g_calmSince) g_calmSince = now;
}

// A lesson being followed through its menus: another waits until it is done. One only said in play
// does not hold the other back, as the player may never open its menu.
bool LessonUnderWay() { return g_lesson == Lesson::Sheet || g_lesson == Lesson::Tree || g_kit == Kit::Open; }
bool Calm(uint64_t now) { return g_calmSince && now - g_calmSince >= CalmMs && now >= g_speechEnd && !LessonUnderWay(); }

bool LessonDue(uint64_t now) {
	const auto experience = NativeStatus::PlayerExperience();
	return g_lesson == Lesson::Waiting && experience && experience->level >= FirstLevelUp && Calm(now);
}
bool KitDue(uint64_t now) { return g_kit == Kit::Waiting && g_newItem && Calm(now); }
bool GoldDue(uint64_t now) { return g_goldNews == News::Armed && Calm(now); }

void TellGold() {
	g_goldNews = News::Given;
	TutorialRecords::Give(g_key, GoldSuffix);
	Message(Render(Texts().gold), 0);
	AsyncLog::InfoF(g_context, "[tutorial] gold lesson given to %s", g_name.c_str());
}

void TellKit() {
	g_kit = Kit::Told;
	Message(Render(Texts().kit), 0);
	AsyncLog::InfoF(g_context, "[tutorial] inventory lesson begins for %s (item %u)", g_name.c_str(), g_newItem);
}

void TellLesson() {
	g_lesson = Lesson::Told;
	Message(Render(Texts().levelUp), 0);
	AsyncLog::InfoF(g_context, "[tutorial] level-up lesson begins for %s", g_name.c_str());
}

void Advance() {
	g_guide.Advance();
	// A step the player is already past is kept for reading back, not said (Tutorial::PassedBy).
	while (g_guide.Passed(g_states)) {
		KeepStep(g_guide.Current());
		g_guide.Advance();
	}
	Save();
	Push({ActionKind::StepDone, 0});
	AsyncLog::InfoF(g_context, "[tutorial] step %u reached", static_cast<unsigned>(g_guide.Current()));
	if (g_guide.Finished()) {
		Message(Render(StepLines(Step::Finished)), AfterStepMs);
		return;
	}
	QueueStep(g_guide.Current(), g_guide.Current() == Step::Sounds ? SettleMs : AfterStepMs);
}

void Browse(Command command) {
	const auto reading = command == Command::Previous ? g_history.Previous()
		: command == Command::Next ? g_history.Next() : g_history.First();
	if (!reading) return;
	const auto& texts = Texts();
	std::string text;
	if (reading->edge == Tutorial::History::Edge::First) text = std::string(texts.firstMessage) + ". ";
	if (reading->edge == Tutorial::History::Edge::Last) text = std::string(texts.lastMessage) + ". ";
	text += reading->text;
	ScreenReader::Speak(text, true);
	const uint64_t now = GetTickCount64();
	g_speechEnd = now + text.size() * MsPerCharacter;
	const auto sound = g_sounds.find(reading->at);
	g_browseSound = sound == g_sounds.end() ? Later{} : Later{now + ReadMs(text), sound->second};
}

// The settings list says the change itself; on, the next poll decides again.
void Toggle(bool on) {
	if (!on) Stop();
	g_decided = false;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) { Unregister(); return false; }
	for (auto kind : {EventKind::GameJoined, EventKind::GameLeft, EventKind::LocalPlayerReady, EventKind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize,
			.kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		const auto data = [](Command command) { return reinterpret_cast<void*>(static_cast<uintptr_t>(command)); };
		const D2RL::Input::ActionRegistration actions[]{
			{.structSize = D2RL::Input::ActionRegistrationSize, .logicalId = "tutorial-previous",
				.displayName = "Tutorial: previous message", .category = "Accessibility",
				.defaultPrimary = {D2RL::Input::Key::K, D2RL::Input::Modifier::None}, .callback = OnKey, .userData = data(Command::Previous)},
			{.structSize = D2RL::Input::ActionRegistrationSize, .logicalId = "tutorial-next",
				.displayName = "Tutorial: next message", .category = "Accessibility",
				.defaultPrimary = {D2RL::Input::Key::L, D2RL::Input::Modifier::None}, .callback = OnKey, .userData = data(Command::Next)},
			{.structSize = D2RL::Input::ActionRegistrationSize, .logicalId = "tutorial-first",
				.displayName = "Tutorial: first message", .category = "Accessibility",
				.defaultPrimary = {D2RL::Input::Key::K, D2RL::Input::Modifier::Shift}, .callback = OnKey, .userData = data(Command::First)}};
		for (const auto& action : actions) {
			D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
			if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
			else AsyncLog::WarnF(context, "[tutorial] key %s not registered.", action.logicalId);
		}
	}
	const auto records = TutorialRecords::Load(context, std::filesystem::path(context->pluginDirectory) / "d2r-access" / "tutorial.txt");
	AsyncLog::InfoF(context, "[tutorial] registered, %zu character record(s)", records);
	return true;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	// The chords and keys are taken every tick, in a menu too, so none is answered late.
	const auto side = InputDirection::TakeBackWithRightStickSide();
	const bool first = InputDirection::TakeBackWithDpadUp();
	auto command = g_command.exchange(Command::None);
	if (command == Command::On || command == Command::Off) { Toggle(command == Command::On); command = Command::None; }
	if (side == InputDirection::StickPush::Left) command = Command::Previous;
	else if (side == InputDirection::StickPush::Right) command = Command::Next;
	else if (first) command = Command::First;
	const auto posted = Tutorial::TakePosted();

	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	// Each game decides again, and the history belongs to the game it was said in.
	if (!state.ready || state.session != g_session) {
		Stop();
		g_decided = false;
		ClearHistory();
		g_name.clear();
		g_key.clear();
		g_questLog.clear();
		g_session = state.session;
		if (!state.ready) { TutorialRecords::DropDeleted(GetTickCount64()); return; }
	}
	const uint64_t now = GetTickCount64();
	// Read in a dialogue too: talking to Akara is a step.
	if (g_enabled && now - g_lastPoll >= PollMs) {
		g_lastPoll = now;
		if (!g_decided) Decide(state);
		if (g_active) {
			g_states = States(state);
			WatchDialogue(state);
			WatchKill(state);
			WatchGold(state);
			WatchWaypoint(state);
			WatchShop();
			WatchLesson(now);
			WatchKit(now);
			WatchCalm(state, now);
		}
	}
	if (g_active) g_guide.Notice(posted);
	// While a lesson's sounds play, the world's keep quiet so they are heard alone, and previous and
	// next steer them; otherwise those browse the messages.
	const bool auditioning = g_active && !g_queue.empty() && g_queue.front().kind == ActionKind::Audition && !g_audition.Idle();
	Tutorial::SetQuietWorld(g_active && (g_worldQuiet || auditioning));
	Tutorial::SetBusy(g_active && (!g_queue.empty() || now < g_speechEnd));
	// The in-play gate walks the panel tree: only when there is something to do.
	if (command == Command::None && !g_active) return;
	const bool inPlay = RadarUi::InPlay();
	if (command != Command::None && inPlay) {
		if (auditioning && command != Command::First) Steer(command, g_queue.front(), now);
		else Browse(command);
	}
	if (g_browseSound.play && now >= g_browseSound.due) {
		if (inPlay) g_browseSound.play();
		g_browseSound = {};
	}
	if (!g_active) return;
	// New words wait for play, the lesson's for its menu. In play they wait while the game's own text
	// is read; in a menu the reader of what has focus always owns speech (GameTextReader::Tick), so
	// there they wait only for a pause, as every new message does (the lesson was never said: the
	// user, 2026-10-21).
	while (!g_queue.empty() && now >= g_due && (inPlay ? !uiOwnsSpeech : g_queue.front().menu)) {
		// Words for a menu already shut are dropped: they would point at what is no longer there.
		if (inPlay && g_queue.front().menu) {
			g_queue.pop_front();
			if (!g_queue.empty()) g_due = now + g_queue.front().delay;
			continue;
		}
		if (g_queue.front().kind == ActionKind::Audition) {
			if (!Audit(g_queue.front(), now)) break;
			g_queue.pop_front();
			if (!g_queue.empty()) g_due = now + g_queue.front().delay;
			continue;
		}
		const auto kind = g_queue.front().kind;
		if (kind == ActionKind::Chime || kind == ActionKind::Keep) {
			uint64_t waited = 0;
			if (!g_pause.Due(now, ScreenReader::LastSpoken(), &waited)) break;
			if (waited) AsyncLog::InfoF(context, "[tutorial] no pause in speech for %llu ms: the message goes in line",
				static_cast<unsigned long long>(waited));
		}
		g_pause.heldSince = 0;
		const Action action = std::move(g_queue.front());
		g_queue.pop_front();
		Perform(action);
		if (!g_queue.empty()) g_due = now + g_queue.front().delay;
	}
	if (!g_queue.empty() || !inPlay) return;
	if (g_guide.Finished()) { Stop(); AsyncLog::InfoF(context, "[tutorial] finished for %s", g_name.c_str()); return; }
	if (g_guide.Done(g_states)) Advance();
	else if (LessonDue(now)) TellLesson();
	else if (KitDue(now)) TellKit();
	else if (GoldDue(now)) TellGold();
}

bool Enabled() { return g_enabled; }
void SetEnabled(bool on) {
	g_enabled = on;
	g_command = on ? Command::On : Command::Off;
}
bool WaitForSounds() { return g_waitSounds; }
void SetWaitForSounds(bool on) { g_waitSounds = on; }

void Scanned(const EntityRadar::Entity& entity) {
	if (entity.kind != EntityRadar::Kind::Npc) return;
	if (entity.placeId == TownNpcs::Identity(WarrivMonStats)) Tutorial::Post(Signal::ScannedWarriv);
	if (entity.placeId == TownNpcs::Identity(AkaraMonStats)) Tutorial::Post(Signal::ScannedAkara);
}

void Routed(const EntityRadar::Entity& entity) {
	if (entity.kind == EntityRadar::Kind::Npc && entity.placeId == TownNpcs::Identity(AkaraMonStats)) Tutorial::Post(Signal::RouteToAkara);
}

void Arrived(const EntityRadar::Entity& entity) {
	if (entity.kind == EntityRadar::Kind::Npc && entity.placeId == TownNpcs::Identity(AkaraMonStats)) Tutorial::Post(Signal::AtAkara);
}

void Unregister() {
	if (g_lifecycle && g_context) for (auto handle : g_listeners)
		(void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_input && g_context) for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear();
	g_actions.clear();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	Stop();
	ClearHistory();
	TutorialRecords::Clear();
	g_context = nullptr; g_lifecycle = nullptr; g_input = nullptr;
	g_command = Command::None;
	g_enabled = true;
	g_session = g_lastPoll = g_due = 0;
	g_decided = false;
	g_name.clear();
	g_key.clear();
	g_states = 0;
	g_gold = -1;
	g_experience.reset();
	g_questLog.clear();
}
}
