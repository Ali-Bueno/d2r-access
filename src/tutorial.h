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
	RoomWays, Terrain, Gold, InDen, DenCleared, InTown, DenDone, Count };
using Signals = uint32_t;
static_assert(static_cast<size_t>(Signal::Count) <= sizeof(Signals) * 8);
constexpr Signals Bit(Signal signal) { return Signals{1} << static_cast<unsigned>(signal); }

// The steps in order; Finished is past the last and has the farewell.
enum class Step : uint8_t {
	Vitals, Radar, Sounds, Scanner, MeetWarriv, FindAkara, Route, Beacon, Talk, LeaveTown, Surroundings, Fight, FindDen,
	ClearDen, ReturnToTown, Reward, Finished };
inline constexpr size_t StepCount = static_cast<size_t>(Step::Finished);

// The signals that complete a step.
Signals Needs(Step step);

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

// Steps go in order. An event (a key, a scan, gold taken) counts once its step
// is current, so each step asks for its own action; a state (where the player
// is, the quest's progress) counts while it holds, so a quest already further
// on passes its steps.
class Guide {
public:
	void Start(Step step);
	void Notice(Signals events);
	bool Done(Signals states) const;
	void Advance();
	Step Current() const { return m_step; }
	bool Finished() const { return m_step == Step::Finished; }

private:
	Step m_step = Step::Finished;
	Signals m_seen = 0;
};

// Every message said, in order, and the one the player is reading.
class History {
public:
	// Where a move stopped because there is nothing further that way.
	enum class Edge { None, First, Last };
	struct Reading { std::string_view text; Edge edge = Edge::None; };

	void Add(std::string text); // the cursor goes to it
	void Clear();
	bool Empty() const { return m_messages.empty(); }
	size_t Size() const { return m_messages.size(); }
	std::optional<Reading> Previous();
	std::optional<Reading> Next();
	std::optional<Reading> First();

private:
	std::vector<std::string> m_messages;
	size_t m_cursor = 0;
};
}
