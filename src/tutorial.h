#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The tutorial's steps, what completes each, and its message history: pure, no game reads (docs/tutorial.md).
namespace D2RAccess::Tutorial {
// What the player did that a step can wait for.
enum class Signal : uint8_t {
	Vitals, Wall, HeardSounds, ScannedWarriv, TalkedToWarriv, ScannedAkara, RouteToAkara, AtAkara, TalkedToAkara, InBloodMoor,
	RoomWays, Terrain, Kill, InDen, DenCleared, InTown, DenDone, Waypoint, Shopped, SkillsSpent, RewardTalk, Count };
using Signals = uint32_t;
static_assert(static_cast<size_t>(Signal::Count) <= sizeof(Signals) * 8);
constexpr Signals Bit(Signal signal) { return Signals{1} << static_cast<unsigned>(signal); }

// The steps in order; Finished is past the last and has the farewell. After the reward, Akara's
// shop and the skill point, and the camp's own waypoint rather than one past the Blood Moor
// (the user, 2026-11-02: died on the way there).
enum class Step : uint8_t {
	Vitals, Radar, Sounds, Scanner, MeetWarriv, FindAkara, Route, Beacon, Talk, LeaveTown, Surroundings, Fight, FindDen,
	DenRooms, ClearDen, ReturnToTown, Reward, Shop, SpendSkill, Waypoint, Finished };
inline constexpr size_t StepCount = static_cast<size_t>(Step::Finished);

// The signals that complete a step.
Signals Needs(Step step);
// The states that show the player is already past a step: the Den cleared passes the steps
// on the way to it, so a player ahead of the tutorial is not held at one (a tester, 2026-10-21).
Signals PassedBy(Step step);

// A record saved by an older version: the step the same place has now. Version 1 had no Den's
// rooms nor waypoint steps; version 2 had no shop nor skill point steps.
Step FromVersion(int version, int step);

// From any thread: a signal for the service's next tick.
void Post(Signal signal);
Signals TakePosted();

// Until their own lesson, after the radar's, the world's other sounds -- NPCs, the stash,
// exits, unexplored rooms -- keep quiet, so the radar's are heard alone (the user, 2026-10-21).
void SetQuietWorld(bool quiet);
bool QuietWorld();

// While the tutorial talks -- a message under way or one still to come -- news that can wait,
// the radar's new places and the NPC announcer, waits for it (the user, 2026-10-21).
void SetBusy(bool busy);
bool Busy();

// Steps go in order. An event (a key, a scan, a kill) counts once its step
// is current, so each step asks for its own action; a state (where the player
// is, the quest's progress) counts while it holds, so a quest already further
// on passes its steps.
class Guide {
public:
	void Start(Step step);
	void Notice(Signals events);
	bool Done(Signals states) const;
	// Done only because the player is already past it (PassedBy): its words are not said.
	bool Passed(Signals states) const;
	void Advance();
	Step Current() const { return m_step; }
	bool Finished() const { return m_step == Step::Finished; }

private:
	Step m_step = Step::Finished;
	Signals m_seen = 0;
};

// A lesson's sounds, each after its name: one after another, or with `wait` one at a time, on
// when the player asks. "Again" plays the last sound heard once more and holds the lesson there
// until "next" (a tester, 2026-10-21: hear a sound again before the next one comes).
class Audition {
public:
	enum class Cue : uint8_t { None, Name, Sound };
	struct Due { Cue cue = Cue::None; size_t index = 0; };
	// `reads[i]`: how long the i-th name takes to say, after which its sound plays; the next
	// name comes `gap` after it. The first name is due at once.
	void Start(std::vector<uint64_t> reads, uint64_t now, uint64_t gap, bool wait);
	// `said`: the screen reader tells the name is over, so its sound need not wait out the guess.
	Due Tick(uint64_t now, bool said = false);
	// The last sound heard, to play again; none before the first.
	std::optional<size_t> Again();
	void Next(uint64_t now);
	void Reset() { m_phase = Phase::Idle; }
	bool Idle() const { return m_phase == Phase::Idle; }
	bool Holding() const { return m_phase == Phase::Hold; }
	bool Done() const { return m_phase == Phase::Done; }

private:
	enum class Phase : uint8_t { Idle, Name, Sound, Gap, Hold, Done };
	void Heard(uint64_t now);
	std::vector<uint64_t> m_reads;
	uint64_t m_due = 0, m_gap = 0;
	size_t m_next = 0; // the next cue to name
	std::optional<size_t> m_heard;
	bool m_wait = false;
	Phase m_phase = Phase::Idle;
};

// A message's sound, then its words.
inline constexpr uint64_t ChimeMs = 600;

// The question before a new character's tutorial (the user, 2026-11-02): two presses of one answer
// within `window` give it; one press alone, two different ones, or anything else ask it again.
class Offer {
public:
	enum class Press : uint8_t { Yes, No, Never, Other };
	enum class Reply : uint8_t { None, Yes, No, Never, Again };
	void Start(uint64_t window) { m_window = window; m_first.reset(); }
	Reply Feed(Press press, uint64_t now);
	// Again once a first press has waited out the window alone.
	Reply Tick(uint64_t now);

private:
	uint64_t m_window = 0, m_at = 0;
	std::optional<Press> m_first;
};

// Every message said, in order, and the one the player is reading.
class History {
public:
	// Where a move stopped because there is nothing further that way.
	enum class Edge { None, First, Last };
	struct Reading { std::string_view text; Edge edge = Edge::None; size_t at = 0; };

	void Add(std::string text); // the cursor goes to it
	void Clear();
	bool Empty() const { return m_messages.empty(); }
	size_t Size() const { return m_messages.size(); }
	std::optional<Reading> Previous();
	std::optional<Reading> Next();
	std::optional<Reading> First();

private:
	Reading At(Edge edge = Edge::None) const { return {m_messages[m_cursor], edge, m_cursor}; }
	std::vector<std::string> m_messages;
	size_t m_cursor = 0;
};
}
